#!/usr/bin/env python3
"""Small allowlisted HTTP reader; never upload, execute commands or follow redirects."""

import argparse
from datetime import datetime, timezone
import getpass
import hashlib
import http.client
import ipaddress
import json
import os
from pathlib import Path
import re
import sys
from urllib.parse import urlencode


READ_PATHS = {
    "/information.json", "/status.json", "/vlanlist", "/config",
    "/eee.json", "/bandwidth.json", "/mirror.json", "/mtu.json",
    "/lag.json", "/stp.json",
}


class Switch:
    def __init__(self, host):
        self.host = str(ipaddress.IPv4Address(host))
        self.cookie = None

    def request(self, method, path, body=None):
        if not ((method == "POST" and path == "/login") or
                (method == "GET" and (path in READ_PATHS or
                 re.fullmatch(r"/vlan\.json\?vid=([1-9][0-9]{0,3})", path)
                 and 1 <= int(path.split("=")[1]) <= 4094))):
            raise ValueError("Request is outside the read-only allowlist")
        headers = {"Connection": "close"}
        if self.cookie:
            headers["Cookie"] = self.cookie
        if body is not None:
            headers["Content-Type"] = "application/x-www-form-urlencoded"
        conn = http.client.HTTPConnection(self.host, timeout=10)
        try:
            conn.request(method, path, body, headers)
            response = conn.getresponse()
            payload = response.read(65537)
            if len(payload) > 65536:
                raise ValueError("Response exceeds 64 KiB limit")
            return response.status, response.getheader("Set-Cookie"), payload
        finally:
            conn.close()

    def login(self, password):
        status, cookie, _ = self.request("POST", "/login", urlencode({"pwd": password}))
        if status != 302 or not cookie or not cookie.startswith("session="):
            raise ValueError("Login failed; no authenticated session")
        self.cookie = cookie.split(";", 1)[0]

    def get(self, path):
        status, _, payload = self.request("GET", path)
        if status != 200:
            raise ValueError(f"{path}: HTTP {status}; no redirect/retry attempted")
        return payload


def snapshot(switch, root):
    os.umask(0o077)
    root.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    directory = root / f"{stamp}-{switch.host}"
    directory.mkdir(mode=0o700)
    records, errors = {}, []
    manifest = {"host": switch.host, "captured_utc": stamp, "files": {},
                "errors": errors, "complete_restore_backup": False}

    def save(path, name):
        try:
            payload = switch.get(path)
            (directory / name).write_bytes(payload)
            manifest["files"][name] = {"endpoint": path, "bytes": len(payload),
                                       "sha256": hashlib.sha256(payload).hexdigest()}
            records[path] = payload
            if path != "/config":
                return json.loads(payload)
        except (OSError, ValueError, http.client.HTTPException) as exc:
            errors.append(str(exc))
        return None

    for path in sorted(READ_PATHS):
        save(path, "config.txt" if path == "/config" else path[1:])
    vlans = json.loads(records.get("/vlanlist", b"{}")) if "/vlanlist" in records else {}
    for vlan in vlans.get("vlan", []):
        vid = vlan["id"]
        save(f"/vlan.json?vid={vid}", f"vlan-{vid}.json")
    # Repeat the persistent config read to detect an unstable export.
    save("/config", "config-second-read.txt")
    first = directory / "config.txt"
    second = directory / "config-second-read.txt"
    if first.exists() and second.exists() and first.read_bytes() != second.read_bytes():
        errors.append("Persistent config changed between reads")
    config = first.read_bytes() if first.exists() else b""
    if not config or any(b < 32 and b not in (9, 10, 13) for b in config) or any(b > 126 for b in config):
        errors.append("Persistent config is empty or contains unexpected bytes")
    missing = [str(v["id"]) for v in vlans.get("vlan", [])
               if not re.search(rb"(?m)^vlan\s+" + str(v["id"]).encode() + rb"(?:\s|$)", config)]
    if missing:
        errors.append("Active VLANs absent from persistent config: " + ", ".join(missing))
    manifest["warning"] = "Diagnostic snapshot only; completeness and restoration are NOT verified. No reboot or flash."
    (directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Snapshot: {directory}\nDiagnostic snapshot only; NOT a verified restore backup.")
    for error in errors:
        print(f"WARNING: {error}")
    return 1 if errors else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("status", "storage", "snapshot"))
    parser.add_argument("--host", default="192.168.5.9")
    parser.add_argument("--backup-dir", type=Path, default=Path(".switch-backups"))
    args = parser.parse_args()
    switch = Switch(args.host)
    switch.login(os.environ.get("SWITCH_PASSWORD") or getpass.getpass("Switch password: "))
    if args.action == "snapshot":
        return snapshot(switch, args.backup_dir)
    info = json.loads(switch.get("/information.json"))
    if args.action == "status":
        print(json.dumps(info, indent=2))
    else:
        print(f"Device: {info['hw_ver']}\nFirmware: {info['sw_ver']}")
        print(f"Physical flash (device report): {info['flash_size']}")
        print("Local source layout: image 512 KiB; update requires at least 1 MiB flash.")
        print("Default config: 0x6f000..0x6ffff; user config: 0x70000..0x70fff.")
        print("Staging: 0x80000..0xfffff. Remaining chip space is NOT filesystem free space.")
        print("Runtime free RAM: unavailable via HTTP. Use the local SDCC RAM report.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, http.client.HTTPException) as exc:
        sys.exit(f"ERROR: {exc}")
