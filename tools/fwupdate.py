#!/usr/bin/env python3
"""Read switch information or flash a 512 KiB RTLPlayground image.

    fwupdate.py 192.168.10.247                           # Read model and version
    fwupdate.py 192.168.10.247 output/rtlplayground.bin  # Flash and wait for reboot

Set RTLP_PASSWORD or enter the password at the prompt. Python callers can use the same updater directly.

    switch = fwupdate.FirmwareUpdater(host, password)
    info = switch.status()
    result = switch.update(image_path)

fwupdate.Failure reports switch-specific problems such as a model mismatch or an unconfirmed upload.

Exit status is 0 on success and 1 on error.
"""

import argparse
import getpass
import http.client
import http.cookies
import json
import os
import sys
import time
from contextlib import closing
from datetime import timedelta
from functools import partial
from pathlib import Path
from urllib.parse import quote, urlencode


class Failure(Exception):
    """A switch-specific failure during a status check or firmware update."""


_UPDATE_ERRORS = (Failure, OSError, ValueError, http.client.HTTPException, http.cookies.CookieError)


class FirmwareUpdater:
    """Read status and update firmware over HTTP.

    Supply a password string or a function that returns it at first login.
    Pass a text stream for progress messages.
    """

    DEFAULT_TIMEOUT = float(os.getenv("RTLP_FWUPDATE_TIMEOUT", "60"))
    DEFAULT_WAIT_TIMEOUT = float(os.getenv("RTLP_FWUPDATE_WAIT_TIMEOUT", "180"))

    IMAGE_SIZE = 512 * 1024
    IMAGE_MAGIC = b"\x00\x40\x02"
    IMAGE_CRC = 0xB001

    CRC_TABLE = list(range(256))
    for _ in range(8):
        CRC_TABLE = [(crc >> 1) ^ (0xA001 if crc & 1 else 0) for crc in CRC_TABLE]

    def __init__(self, host, password, *, timeout=DEFAULT_TIMEOUT, wait_timeout=DEFAULT_WAIT_TIMEOUT, progress=None):
        if not all(0 < value < float("inf") for value in (timeout, wait_timeout)):
            raise ValueError("timeout and wait_timeout must be positive and finite")

        host = host.removeprefix("http://").removesuffix("/")
        self.host = f"[{host}]" if host.count(":") > 1 and not host.startswith("[") else host
        self.password = password
        self.timeout = timeout
        self.wait_timeout = wait_timeout
        self.progress = progress
        self.update_warning = None
        self._session_cookie = None

    def status(self, *, image=None, timeout=None):
        """Return switch information and check the model if firmware bytes are supplied."""
        status, _, body = self._request("GET", "/information.json", timeout=timeout)
        if status != 200:
            raise Failure(f"/information.json returned HTTP {status}")

        info = json.loads(body)
        if not isinstance(info, dict) or not info:
            raise Failure("/information.json did not return switch information")

        if image is not None:
            model = info.get("hw_ver")
            hint = "use --force only if you know the target board"
            if not isinstance(model, str) or not model.strip() or not (model.isascii() and model.isprintable()):
                raise Failure(f"cannot read the switch model from /information.json; {hint}")
            if model.encode("ascii") + b"\0" not in image:
                raise Failure(f"image does not match {model!r}; {hint}")

        return info

    @staticmethod
    def describe(info):
        host = info.get("hostname") or info.get("ip_address") or "?"
        hardware = [info[key] for key in ("hw_ver", "flash_size") if info.get(key)]
        if hardware:
            host += f" ({', '.join(hardware)})"

        summary = f"{host} running {info.get('sw_ver') or '?'}"
        if build_date := info.get("build_date"):
            summary += f" ({build_date})"
        if (uptime := FirmwareUpdater._uptime(info)) is not None:
            summary += f" up for {timedelta(seconds=uptime)}"
        return summary

    def update(self, image_path, *, force=False, wait=True):
        """Return the upload reply after the requested reboot or recovery status check."""
        self.update_warning = None
        path = Path(image_path)
        image = self._read_image(path)

        previous_uptime = None
        previous_version = None
        if not force or wait:
            try:
                info = self.status(image=None if force else image)
                if wait and self._uptime(info) == 0:
                    # Give a freshly booted switch a nonzero baseline before uploading.
                    time.sleep(2)
                    info = self.status(image=None if force else image)
            except _UPDATE_ERRORS:
                if not force:
                    raise
            else:
                previous_uptime = self._uptime(info)
                previous_version = info.get("sw_ver")
                self._report(self.describe(info))

        self._report(f"Uploading {len(image)} bytes to {self.host}...")
        upload_started = time.monotonic()
        accepted, result = self._upload(image, path.name, wait=wait)
        self._report(f"Switch: {result}")

        if wait:
            status_only = force and previous_uptime is None and accepted
            self._wait_after_upload(upload_started, previous_uptime, previous_version, status_only=status_only)
        return result

    def _read_image(self, path):
        image = path.read_bytes()
        if len(image) != self.IMAGE_SIZE:
            raise ValueError(f"{path}: expected {self.IMAGE_SIZE} bytes, got {len(image)}")
        if not image.startswith(self.IMAGE_MAGIC):
            raise ValueError(f"{path}: missing RTLPlayground bank header")

        crc = 0
        for byte in image:
            crc = (crc >> 8) ^ self.CRC_TABLE[(crc ^ byte) & 0xFF]
        if crc != self.IMAGE_CRC:
            raise ValueError(f"{path}: expected CRC16 {self.IMAGE_CRC:#06x}, got {crc:#06x}")
        return image

    def _wait_after_upload(self, upload_started, previous_uptime, previous_version, *, status_only):
        deadline = time.monotonic() + self.wait_timeout
        self._report("Waiting for switch status..." if status_only else "Waiting for the switch to reboot...")
        previous_cookie = self._session_cookie
        observed_at = upload_started
        while (remaining := deadline - time.monotonic()) > 0:
            timeout = min(self.timeout, remaining, 5)
            probe_started = time.monotonic()
            had_cookie = self._session_cookie is not None
            try:
                info = self.status(timeout=timeout)
            except _UPDATE_ERRORS:
                if had_cookie and self._session_cookie is None:
                    continue  # The old cookie was rejected. Read uptime with a fresh session.
            else:
                received = time.monotonic()
                if received >= deadline:
                    break
                if status_only:
                    self._report(self.describe(info))
                    return
                uptime = self._uptime(info)
                if uptime is not None:
                    # A small forward step across the 32-bit counter wrap is still the same boot.
                    if previous_uptime is not None and uptime < previous_uptime:
                        rebooted = (1 << 32) - previous_uptime + uptime > received - observed_at + 1
                    else:
                        # After a missed reboot, even a higher uptime can date the new boot
                        # after our last reading. Allow one second for counter resolution.
                        rebooted = (uptime != previous_uptime and self._session_cookie != previous_cookie
                                    and uptime + 1 < probe_started - observed_at)
                    # The reported boot must also fit inside this update.
                    if rebooted and uptime <= received - upload_started + 1:
                        self._report(self.describe(info))
                        version = info.get("sw_ver")
                        if version and version == previous_version:
                            self.update_warning = f"Warning: firmware version is still {version} after reboot."
                        return
                    previous_uptime, previous_cookie, observed_at = uptime, self._session_cookie, received
            time.sleep(min(2, max(0, deadline - time.monotonic())))

        self._session_cookie = None
        if status_only:
            raise Failure(f"upload accepted, but switch status was unavailable after {self.wait_timeout:g} seconds")
        raise Failure(f"could not confirm reboot from switch uptime within {self.wait_timeout:g} seconds")

    @staticmethod
    def _uptime(info):
        try:
            return int(info.get("uptime"), 16)
        except (TypeError, ValueError):
            return None

    def _upload(self, image, filename, *, wait):
        boundary = "----RTLPlayground"
        while boundary.encode("ascii") in image:
            boundary = os.urandom(24).hex()

        filename = quote(os.fsencode(filename), safe=" ")
        head = (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="uploadedfile"; filename="{filename}"\r\n'
            "Content-Type: application/octet-stream\r\n\r\n"
        ).encode("ascii")
        body = head + image + f"\r\n--{boundary}--\r\n".encode("ascii")
        total = len(body)

        def chunks():
            """Report each block after http.client has sent it."""
            block_size = 8192
            for offset in range(0, total, block_size):
                chunk = body[offset:offset + block_size]
                yield chunk

                sent = offset + len(chunk)
                print(f"\rUploading {sent * 100 // total:3d}% ({sent}/{total} bytes)",
                      end="\n" if sent == total else "", file=self.progress, flush=True)

        payload = chunks() if self.progress is not None and self.progress.isatty() else body
        try:
            status, _, data = self._request(
                "POST", "/upload", payload, content_type=f"multipart/form-data; boundary={boundary}",
                content_length=total, allow_no_reply=wait)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError,
                TimeoutError, http.client.HTTPException) as error:
            if self._session_cookie is None:  # Login failed before the upload started.
                raise
            self._session_cookie = None
            raise Failure(f"upload outcome unconfirmed: {error}. Check the running firmware before retrying.") from error

        if status == 401:
            raise Failure("the switch rejected the session cookie")
        result = data.decode(errors="replace").strip()
        if status not in (200, None):
            raise Failure(f"upload returned HTTP {status}: {result or 'upload rejected'}")

        # The switch may already be rebooting with the old session invalidated.
        if not wait:
            self._session_cookie = None
        if status is None:
            return False, "connection closed without a result"
        return True, result or "upload accepted"

    def _request(self, method, path, body=None, *, content_type=None,
                 content_length=None, allow_no_reply=False, timeout=None):
        """Make one request; older firmware may reboot after receiving all upload bytes."""
        if path != "/login" and self._session_cookie is None:
            started = time.monotonic()
            self._login(timeout)
            if timeout is not None:
                timeout -= time.monotonic() - started
                if timeout <= 0:
                    raise TimeoutError("login used the request timeout")

        headers = {
            "Connection": "close",
            "Content-Type": content_type,
            "Content-Length": content_length,
            "Cookie": self._session_cookie,
        }
        timeout = self.timeout if timeout is None else timeout
        with closing(http.client.HTTPConnection(self.host, timeout=timeout)) as conn:
            conn.request(method, path, body, {name: value for name, value in headers.items() if value is not None})

            try:
                response = conn.getresponse()
            except (ConnectionResetError, BrokenPipeError):
                if allow_no_reply:
                    return None, {}, b""
                raise

            with response:
                data = response.read()
                if response.status == 401:
                    self._session_cookie = None
                return response.status, response.headers, data

    def _login(self, timeout=None):
        if callable(self.password):
            self.password = self.password()

        body = urlencode({"pwd": self.password}).encode("ascii")
        status, headers, _ = self._request("POST", "/login", body,
                                          content_type="application/x-www-form-urlencoded", timeout=timeout)

        # Both login outcomes redirect. A session cookie signals success.
        if status == 302:
            session = http.cookies.SimpleCookie(headers.get("Set-Cookie", "")).get("session")
            if session is not None and session.value:
                self._session_cookie = f"session={session.coded_value}"
                return
            if headers.get("Location", "").rsplit("/", 1)[-1] == "login.html":
                raise Failure("login failed: wrong password")
        raise Failure(f"login returned HTTP {status} without a valid session")

    def _report(self, message):
        if self.progress is not None:
            print(message, file=self.progress)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Read switch information or flash firmware.",
                                     epilog="Recover a known board: %(prog)s --force --no-wait HOST IMAGE")

    parser.add_argument("host", metavar="HOST", help="switch address or HTTP URL, optionally with a port")
    parser.add_argument("image", metavar="IMAGE", nargs="?", type=Path,
                        help="RTLPlayground firmware image (.bin). Omit for status.")

    parser.add_argument("--force", action="store_true", help="skip the board-model check")
    parser.add_argument("--no-wait", action="store_true", help="return once the switch accepts the upload")
    parser.add_argument("-p", "--password", default=os.getenv("RTLP_PASSWORD"), help="uses RTLP_PASSWORD or prompts")

    try:
        args = parser.parse_intermixed_args(argv)

        password = (
            args.password if args.password is not None
            else partial(getpass.getpass, f"Password for {args.host}: ")
        )
        switch = FirmwareUpdater(args.host, password, progress=sys.stderr)

        if args.image is None:
            print(switch.describe(switch.status()))
        else:
            switch.update(args.image, force=args.force, wait=not args.no_wait)
            if args.no_wait:
                print("Firmware staged; the switch is rebooting to apply it.")
            else:
                summary = "Switch is responding after upload." if args.force else "Firmware update complete."
                if switch.update_warning:
                    summary += f" {switch.update_warning}"
                print(summary)
        return 0

    except SystemExit as exit_status:
        return int(bool(exit_status.code))
    except _UPDATE_ERRORS as error:
        print(f"error: {error}", file=sys.stderr)
    except (EOFError, KeyboardInterrupt):
        print("\naborted", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
