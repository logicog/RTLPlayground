#!/usr/bin/env python3
"""Small allowlisted HTTP reader; never upload, execute commands or follow redirects."""

import argparse
import getpass
import http.client
import ipaddress
import json
import os
import sys
from urllib.parse import urlencode


class Switch:
    def __init__(self, host):
        self.host = str(ipaddress.IPv4Address(host))
        self.cookie = None

    def request(self, method, path, body=None):
        if not ((method == "POST" and path == "/login") or
                (method == "GET" and path == "/information.json")):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("status", "storage"))
    parser.add_argument("--host", default="192.168.5.9")
    args = parser.parse_args()
    switch = Switch(args.host)
    switch.login(os.environ.get("SWITCH_PASSWORD") or getpass.getpass("Switch password: "))
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
