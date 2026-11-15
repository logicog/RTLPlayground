#!/usr/bin/env python3
"""Update an RTLPlayground switch over its web API.

The web UI logs in with a form POST to /login (field ``pwd``), keeps the
``session`` cookie from the reply, and posts the image as the ``uploadedfile``
part of a multipart/form-data request to /upload.  This tool performs the same
exchange, checks the image locally before sending it, prints the switch's
verdict, and waits for the switch to reboot and answer again.

The image must be the 512 KiB RTLPlayground build
(output/rtlplayground_*.bin), not the OEM upgrade image.  The switch only
accepts an image whose size, header and CRC16 are valid; checking them here
avoids a 512 KiB upload just to be rejected.

Usage:
    fwupdate.py [options] IMAGE [HOST]

    fwupdate.py -p secret output/rtlplayground_....bin 192.168.10.247
    fwupdate.py -p secret --health output/rtlplayground_....bin 192.168.10.247
    fwupdate.py -p secret --check 192.168.10.247
    RTLP_PASSWORD=secret fwupdate.py output/rtlplayground_....bin --no-wait

Exit status:
    0  image accepted (and, unless --no-wait, the switch came back)
    1  the local image is not a valid RTLPlayground image
    2  login failed
    3  the switch rejected the upload
    4  transfer, reboot or health read failed
"""

import argparse
import getpass
import http.client
import json
import os
import re
import secrets
import sys
import time
import urllib.parse

IMAGE_SIZE = 512 * 1024
IMAGE_MAGIC = b"\x00\x40\x02"
IMAGE_CRC = 0xB001
DEFAULT_HOST = "192.168.10.247"
DEFAULT_WAIT_TIMEOUT = 180
LINK_SPEEDS = ("down", "10M", "100M", "1000M", "500M", "10G", "2.5G", "5G")


class Failure(Exception):
    """An error that ends the run with a specific exit status."""

    def __init__(self, message, status=4):
        super().__init__(message)
        self.status = status


def _crc_table():
    """Reflected CRC-16 table for poly 0xA001 (the switch's crc16_bank1)."""
    table = []
    for value in range(256):
        crc = value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
        table.append(crc)
    return table


CRC_TABLE = _crc_table()


def crc16(data):
    crc = 0
    for byte in data:
        crc = (crc >> 8) ^ CRC_TABLE[(crc ^ byte) & 0xFF]
    return crc


def load_image(path):
    try:
        with open(path, "rb") as image_file:
            data = image_file.read()
    except OSError as error:
        raise Failure(f"cannot read {path}: {error}", 1)

    if len(data) != IMAGE_SIZE:
        raise Failure(
            f"{path}: size is {len(data)}, expected {IMAGE_SIZE} "
            f"({IMAGE_SIZE // 1024} KiB)", 1)
    if data[:len(IMAGE_MAGIC)] != IMAGE_MAGIC:
        raise Failure(
            f"{path}: missing bank header / LJMP magic: not an "
            "RTLPlayground firmware image", 1)
    crc = crc16(data)
    if crc != IMAGE_CRC:
        raise Failure(
            f"{path}: CRC16 is 0x{crc:04x}, expected 0x{IMAGE_CRC:04x}; "
            "the image is corrupt", 1)
    return data


def parse_target(target, port_override):
    """Return (host, port) for a host, host:port or http://host URL."""
    if "://" in target:
        try:
            url = urllib.parse.urlsplit(target)
            port = url.port
        except ValueError as error:
            raise Failure(f"invalid target {target!r}: {error}", 1)
        if url.scheme.lower() != "http":
            raise Failure("only the switch's plain HTTP API is supported", 1)
        if url.path not in ("", "/"):
            raise Failure("the target must not include a path", 1)
        host = url.hostname
    else:
        host, port = target, None
        if target.count(":") == 1:
            host, possible_port = target.split(":", 1)
            if possible_port.isdigit():
                port = int(possible_port)

    if not host:
        raise Failure("no switch address given", 1)
    if port_override is not None:
        port = port_override
    if port is None:
        port = 80
    if not 0 < port < 65536:
        raise Failure(f"invalid port {port}", 1)
    return host, port


def http_request(host, port, timeout, method, path, body=None, cookie=None,
                 content_type=None):
    """Make one request; the switch closes the connection after every reply."""
    connection = http.client.HTTPConnection(host, port, timeout=timeout)
    headers = {"Connection": "close"}
    if cookie:
        headers["Cookie"] = f"session={cookie}"
    if content_type:
        headers["Content-Type"] = content_type
    try:
        connection.request(method, path, body=body, headers=headers)
        response = connection.getresponse()
        data = response.read()
        return (response.status, response.getheader("Location"),
                response.getheader("Set-Cookie"), data)
    except (OSError, http.client.HTTPException) as error:
        raise Failure(f"{method} {path}: {error}", 4)
    finally:
        connection.close()


def login(host, port, timeout, password):
    body = ("pwd=" + urllib.parse.quote_plus(password)).encode("ascii")
    status, location, set_cookie, _ = http_request(
        host, port, timeout, "POST", "/login", body=body,
        content_type="application/x-www-form-urlencoded")
    if status != 302:
        raise Failure(f"login returned HTTP {status}", 2)

    if "index.html" not in (location or ""):
        raise Failure("login failed: wrong password", 2)
    match = re.search(r"(?:^|;\s*)session=([^;]*)", set_cookie or "")
    if not match or not match.group(1):
        raise Failure("login succeeded but the reply had no session cookie", 2)
    return match.group(1)


def fetch_info(host, port, timeout, cookie):
    try:
        status, _, _, body = http_request(
            host, port, timeout, "GET", "/information.json", cookie=cookie)
    except Failure:
        return None
    if status != 200:
        return None
    try:
        return json.loads(body.decode("utf-8", "replace"))
    except ValueError:
        return None


def describe(info):
    if not info:
        return "unknown version"
    return (f"{info.get('sw_ver', '?')} "
            f"(built {info.get('build_date', '?')}, "
            f"hardware {info.get('hw_ver', '?')})")


def fetch_status(host, port, timeout, cookie):
    """Return the port list from /status.json."""
    status, _, _, body = http_request(
        host, port, timeout, "GET", "/status.json", cookie=cookie)
    if status != 200:
        raise Failure(f"/status.json returned HTTP {status}", 4)
    try:
        ports = json.loads(body.decode("utf-8", "replace"))
    except ValueError:
        raise Failure("/status.json is not valid JSON", 4)
    if not isinstance(ports, list):
        raise Failure("/status.json did not return a port list", 4)
    return ports


def print_health(host, port, timeout, password, cookie,
                 label="Post-update health"):
    """Print the port health from /status.json (best effort).

    Returns True when the summary was printed, False when the read failed.
    """
    if cookie is None:
        try:
            cookie = login(host, port, timeout, password)
        except Failure as error:
            print(f"warning: cannot read health data: {error}", file=sys.stderr)
            return False
    try:
        ports = fetch_status(host, port, timeout, cookie)
    except Failure as error:
        print(f"warning: cannot read health data: {error}", file=sys.stderr)
        return False

    def number(entry, key, base=10):
        try:
            return int(str(entry.get(key, "0")), base)
        except (TypeError, ValueError):
            return 0

    rows = []
    up = tx_errors = rx_errors = learned = 0
    for entry in ports:
        link = number(entry, "link")
        speed = (LINK_SPEEDS[link] if 0 <= link < len(LINK_SPEEDS)
                 else f"link {link}")
        if link:
            up += 1
        enabled = entry.get("enabled")
        enabled = ("?" if enabled is None
                   else ("yes" if number(entry, "enabled") else "no"))
        tx_good = number(entry, "txG", 16)
        tx_bad = number(entry, "txB", 16)
        rx_good = number(entry, "rxG", 16)
        rx_bad = number(entry, "rxB", 16)
        macs = number(entry, "macs", 16)
        tx_errors += tx_bad
        rx_errors += rx_bad
        learned += macs
        rows.append((entry.get("portNum", "?"), entry.get("name", ""), speed,
                     enabled, tx_good, tx_bad, rx_good, rx_bad, macs))

    print()
    print(f"{label} (/status.json): {len(ports)} ports, {up} up, "
          f"{learned} MACs learned, TX bad pkts {tx_errors}, "
          f"RX bad pkts {rx_errors}")
    print(f"{'Port':>4} {'Name':<12} {'Link':>6} {'En':>3} "
          f"{'TX pkts':>10} {'TX bad':>7} {'RX pkts':>10} {'RX bad':>7} "
          f"{'MACs':>6}")
    for port_num, name, speed, enabled, tx_good, tx_bad, rx_good, rx_bad, macs in rows:
        print(f"{str(port_num):>4} {str(name)[:12]:<12} {speed:>6} {enabled:>3} "
              f"{tx_good:>10} {tx_bad:>7} {rx_good:>10} {rx_bad:>7} "
              f"{macs:>6}")
    return True


def image_matches_hardware(image, hardware):
    """True if the image carries the switch's NUL-terminated machine name."""
    if not hardware:
        return True
    return hardware.encode("ascii", "replace") + b"\0" in image


def build_multipart(image, path):
    boundary = "----RTLPlayground" + secrets.token_hex(12)
    marker = b"\r\n--" + boundary.encode("ascii")
    while marker in image:
        boundary = "----RTLPlayground" + secrets.token_hex(12)
        marker = b"\r\n--" + boundary.encode("ascii")

    filename = os.path.basename(path) or "firmware.bin"
    filename = (filename.replace('"', "_").replace("\r", "_")
                .replace("\n", "_"))
    filename = filename.encode("ascii", "replace").decode()
    head = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="uploadedfile"; '
        f'filename="{filename}"\r\n'
        "Content-Type: application/octet-stream\r\n"
        "\r\n"
    ).encode("ascii")
    return boundary, head + image + f"\r\n--{boundary}--\r\n".encode("ascii")


def upload(host, port, timeout, cookie, image, path):
    """Send the image; return (accepted, verdict).

    accepted is None when the switch closed the connection without a verdict
    (older firmware did that).  The caller then falls back to watching the
    reboot.
    """
    boundary, body = build_multipart(image, path)
    connection = http.client.HTTPConnection(host, port, timeout=timeout)

    print(f"Uploading {len(image)} bytes to http://{host}:{port}/upload ...",
          file=sys.stderr)
    try:
        connection.putrequest("POST", "/upload")
        connection.putheader(
            "Content-Type", f"multipart/form-data; boundary={boundary}")
        connection.putheader("Content-Length", str(len(body)))
        connection.putheader("Cookie", f"session={cookie}")
        connection.putheader("Connection", "close")
        connection.endheaders()

        sent = 0
        last_percent = -1
        for offset in range(0, len(body), 8192):
            chunk = body[offset:offset + 8192]
            connection.send(chunk)
            sent += len(chunk)
            if sys.stderr.isatty():
                percent = sent * 100 // len(body)
                if percent != last_percent:
                    last_percent = percent
                    print(f"\rUploading {percent:3d}% "
                          f"({sent}/{len(body)} bytes)", end="",
                          file=sys.stderr)
        if sys.stderr.isatty():
            print(file=sys.stderr)

        try:
            response = connection.getresponse()
        except (http.client.HTTPException, ConnectionResetError,
                BrokenPipeError):
            return None, "connection closed without a verdict"

        verdict = response.read().decode("utf-8", "replace").strip()
        if response.status == 200:
            return True, verdict
        if response.status == 400:
            return False, verdict or "HTTP 400 Bad Request"
        if response.status == 401:
            raise Failure("the switch rejected the session cookie", 2)
        raise Failure(
            f"upload returned HTTP {response.status}"
            + (f": {verdict}" if verdict else ""), 3)
    except (BrokenPipeError, ConnectionResetError) as error:
        raise Failure(f"connection lost during upload: {error}", 4)
    except OSError as error:
        raise Failure(f"upload failed: {error}", 4)
    finally:
        connection.close()


def probe(host, port, timeout):
    """Return the HTTP status, or None when the switch is not answering."""
    try:
        status, _, _, _ = http_request(
            host, port, timeout, "GET", "/information.json")
        return status
    except Failure:
        return None


def wait_for_switch(host, port, timeout, wait_timeout, password, before,
                    require_down=False):
    """Wait until the switch has rebooted and answers again.

    require_down is set when the switch closed the upload connection without
    a verdict: then only an observed reboot proves that the image was taken.
    Returns (info, cookie) from the fresh login after the reboot; both are None
    when that login or the /information.json read failed.
    """
    start = time.monotonic()
    seen_down = False
    print("Waiting for the switch to reboot ...", file=sys.stderr)
    while time.monotonic() - start < wait_timeout:
        status = probe(host, port, min(timeout, 5))
        elapsed = time.monotonic() - start
        if status is None:
            if not seen_down:
                print("Switch is down (rebooting) ...", file=sys.stderr)
                seen_down = True
            time.sleep(2)
            continue
        if (status in (200, 401) and not seen_down and elapsed >= 9
                and require_down):
            raise Failure(
                "switch never rebooted and gave no verdict; the image was "
                "probably rejected", 3)
        # Stay reachable for a few seconds before believing the boot is
        # complete; otherwise a fast reply may just be the old session.
        if status in (200, 401) and (seen_down or elapsed >= 9):
            print("Switch is back.", file=sys.stderr)
            after = None
            cookie = None
            try:
                cookie = login(host, port, timeout, password)
                after = fetch_info(host, port, timeout, cookie)
            except Failure:
                pass
            if after:
                print(f"Now running {describe(after)}", file=sys.stderr)
                if before and (after.get("sw_ver") == before.get("sw_ver")
                               and after.get("build_date")
                               == before.get("build_date")):
                    print("Note: the switch reports the same version; this "
                          "may be the image that was already installed.",
                          file=sys.stderr)
            return after, cookie
        time.sleep(2)
    raise Failure(f"switch did not answer within {wait_timeout} seconds", 4)


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Update RTLPlayground switch firmware via its web API.",
        epilog="The image is the 512 KiB RTLPlayground build "
               "(output/rtlplayground_*.bin), not the OEM upgrade image. "
               "The password can also be set in RTLP_PASSWORD.")
    parser.add_argument("image", nargs="?",
                        help="RTLPlayground firmware image (.bin); with --check, "
                             "the single positional is the host")
    parser.add_argument(
        "host", nargs="?", default=None,
        help=f"switch address, optionally host:port or http://host "
             f"(default: {DEFAULT_HOST})")
    parser.add_argument("-p", "--password",
                        help="web interface password (prompted if not given)")
    parser.add_argument("--port", type=int,
                        help="HTTP port (overrides the port in HOST)")
    parser.add_argument("--timeout", type=float, default=60,
                        help="socket timeout in seconds (default: 60)")
    parser.add_argument("--wait-timeout", type=float,
                        default=DEFAULT_WAIT_TIMEOUT,
                        help="seconds to wait for the reboot "
                             f"(default: {DEFAULT_WAIT_TIMEOUT})")
    parser.add_argument("--no-wait", action="store_true",
                        help="do not wait for the switch to reboot")
    parser.add_argument("--health", action="store_true",
                        help="after the reboot, fetch /status.json and print "
                             "a port health summary")
    parser.add_argument("--check", action="store_true",
                        help="read-only: print the version and /status.json "
                             "health, then exit without uploading; give the "
                             "host as the only positional "
                             f"(default: {DEFAULT_HOST})")
    parser.add_argument("--force", action="store_true",
                        help="upload even if the image does not name the "
                             "switch's hardware")
    return parser.parse_args(argv)


def resolve_password(args, host):
    password = args.password
    if password is None:
        password = os.environ.get("RTLP_PASSWORD")
    if password is None:
        password = getpass.getpass(f"Password for {host}: ")
    return password


def check_switch(args, target):
    """Read-only mode: print the running version and /status.json health."""
    host, port = parse_target(target, args.port)
    password = resolve_password(args, host)
    cookie = login(host, port, args.timeout, password)
    info = fetch_info(host, port, args.timeout, cookie)
    if not info:
        raise Failure("could not read /information.json", 4)

    name = info.get("hostname") or host
    print(f"Switch: {name} at {host}:{port}")
    print(f"Version: {describe(info)}")
    print(f"Flash: {info.get('flash_size', '?')}, "
          f"chip temperature: {info.get('chip_temp', '?')}")
    return (0 if print_health(host, port, args.timeout, password, cookie,
                              "Health") else 4)


def run(args):
    if args.timeout <= 0:
        raise Failure("--timeout must be positive", 1)
    if not args.check and args.wait_timeout <= 0:
        raise Failure("--wait-timeout must be positive", 1)

    if args.check:
        if args.host is not None:
            raise Failure("--check takes only the host; do not pass an image",
                          1)
        if args.no_wait:
            raise Failure("--no-wait has no effect with --check", 1)
        target = args.image if args.image is not None else DEFAULT_HOST
        if args.image is not None and os.path.isfile(target):
            raise Failure("--check takes a host, not a firmware image", 1)
        return check_switch(args, target)

    if args.image is None:
        raise Failure("IMAGE is required unless --check is used", 1)
    if args.health and args.no_wait:
        raise Failure("--health needs the reboot; remove --no-wait", 1)
    image = load_image(args.image)
    print(f"{args.image}: {len(image)} bytes, magic and CRC16 0x"
          f"{IMAGE_CRC:04x} OK", file=sys.stderr)
    host, port = parse_target(args.host or DEFAULT_HOST, args.port)
    password = resolve_password(args, host)

    cookie = login(host, port, args.timeout, password)
    print(f"Logged in to {host}:{port}", file=sys.stderr)

    before = fetch_info(host, port, args.timeout, cookie)
    if before:
        print(f"Currently running {describe(before)}", file=sys.stderr)
        if not args.force and not image_matches_hardware(
                image, before.get("hw_ver")):
            raise Failure(
                f"the image does not contain the switch's hardware name "
                f"({before.get('hw_ver')}); it was probably built for another "
                "board. Use --force to upload it anyway.", 1)
    else:
        print("Could not read /information.json; skipping the hardware check",
              file=sys.stderr)

    accepted, verdict = upload(
        host, port, args.timeout, cookie, image, args.image)

    if accepted is False:
        raise Failure(f"switch rejected the image: {verdict}", 3)
    if accepted is None:
        print(f"Switch: {verdict}", file=sys.stderr)
    else:
        print(f"Switch: {verdict or 'upload accepted'}", file=sys.stderr)

    if args.no_wait:
        print("Firmware staged; the switch is rebooting to apply it.")
        return 0

    _, cookie = wait_for_switch(host, port, args.timeout, args.wait_timeout,
                                password, before,
                                require_down=accepted is None)
    if args.health:
        print_health(host, port, args.timeout, password, cookie)
    print("Firmware update complete.")
    return 0


def main(argv=None):
    args = parse_args(argv)
    try:
        return run(args)
    except Failure as error:
        print(f"error: {error}", file=sys.stderr)
        return error.status
    except KeyboardInterrupt:
        print("\naborted", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(main())
