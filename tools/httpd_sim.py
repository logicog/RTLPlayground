#!/usr/bin/env python3
"""Serve files from the current directory and provide the example device API."""

import os
import secrets
import time
import re
from email.parser import BytesParser
from email.policy import default
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlsplit

HOST = "0.0.0.0"
PORT = 8081
PASSWORD = "1234"

PORTS = 6
NSFP = 1
N_COUNTERS = 52
VLAN_MAX_ID = 4094
L2_MAX_TRANSFER = 5

ROOT = Path.cwd().resolve()

phys_to_log_port = []

if PORTS == 9:
    phys_to_log_port = [ 0, 1, 2, 3, 4, 5, 6, 7, 8 ]
else:
    phys_to_log_port = [ 4, 5, 6, 7, 3, 8 ]

l2_entries = [
    { "mac": "00:01:2f:00:00:01", "vlan":"001", "type":'l', "port":7, "idx":0x10 },
    { "mac": "00:06:78:00:00:02", "vlan":"001", "type":'l', "port":7, "idx":0x20 },
    { "mac": "00:01:2e:00:00:03", "vlan":"001", "type":'s', "port":7, "idx":0x30 },
    { "mac": "60:7d:09:00:00:04", "vlan":"001", "type":'l', "port":7, "idx":0x40 },
    { "mac": "1c:2a:a3:00:00:05", "vlan":"001", "type":'l', "port":7, "idx":0x50 },
    { "mac": "1c:2a:a3:00:00:06", "vlan":"001", "type":'s', "port":7, "idx":0x60 },
    { "mac": "02:e0:4c:00:00:07", "vlan":"001", "type":'l', "port":7, "idx":0x70 },
    { "mac": "00:e0:4c:00:00:08", "vlan":"001", "type":'l', "port":4, "idx":0x80 },
    { "mac": "1c:2a:a3:00:00:09", "vlan":"001", "type":'l', "port":9, "idx":0x90 },  # CPU-Port
    { "mac": "3c:8c:f8:00:00:0a", "vlan":"001", "type":'l', "port":7, "idx":0xa0 },
    { "mac": "00:01:2e:00:00:0b", "vlan":"001", "type":'l', "port":7, "idx":0xb0 },
    { "mac": "00:01:2f:00:01:01", "vlan":"001", "type":'l', "port":7, "idx":0xc0 }
]

state = {
    "lldp_enabled": False,
    "vlans": {1: {"active": True, "name": "", "untagged":[1,2,3,4,5,6,7,8], "tagged":[], "pvid":[1,2,3,4,5,6,7,8]}},
    "cmd_history": [],
    "mgmt_vlan": 1
}


tx_g = [0] * PORTS
rx_g = [0] * PORTS
tx_b = [0] * PORTS
rx_b = [0] * PORTS
last_called = int(time.time())

sessions = set()
uploaded_config = None


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def api_handlers(self,query):
        return {
            "/status.json": self._api_status,
            "/eee.json": self._api_eee,
            "/bandwidth.json": self._api_bandwidth,
            "/information.json": self._api_information,
            "/mirror.json": self._api_mirror,
            "/lag.json": self._api_lag,
            "/l2.json": lambda: self._api_l2(query),
            "/mtu.json": self._api_mtu,
            "/vlan.json": lambda: self._api_vlan(query),
            "/vlanlist": self._api_vlanlist,
            "/lldp.json": self._api_lldp,
            "/cmd_log": self._api_cmd_log,
            "/counters.json": self._api_counters,
            "/debug": self._api_debug,
        }

    def _send(self, status, body=b"", content_type="text/plain",
              headers=()):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        for name, value in headers:
            self.send_header(name, value)
        self.end_headers()
        if body:
            self.wfile.write(body)

    def _json(self, value, headers=()):
        import json

        body = json.dumps(value, separators=(",", ":")).encode("utf-8")
        return self._send(
            200, body, "application/json", headers=headers
        )

    def _redirect(self, location, headers=()):
        return self._send(
            302, headers=(("Location", location), *headers)
        )

    def _authenticated(self):
        for item in self.headers.get("Cookie", "").split(";"):
            name, _, value = item.strip().partition("=")
            if name == "session" and value in sessions:
                return True
        return False

    def _protected(self):
        if not self._authenticated():
            self._send(401, b"Unauthorized\n")
            return True
        return False

    @staticmethod
    def _int_param(query, name):
        try:
            return int(query.get(name, ["0"])[0], 0)
        except ValueError:
            return 0

    def _api_response(self, path, query):
        handler = self.api_handlers(query).get(path)
        return handler() if handler else {}

    def do_GET(self):
        global uploaded_config

        parsed = urlsplit(self.path)
        path = unquote(parsed.path)
        query = parse_qs(parsed.query)

        if path in self.api_handlers(None).keys():
            if self._protected():
                return
            result = self._api_response(path, query)
            if isinstance(result, tuple):
                body, content_type = result
                return self._send(200, body, content_type)
            return self._json(
                result,
                headers=(("Cache-Control", "no-cache"),)
                if path in ("/status.json", "/counters.json")
                else (),
            )

        if path == "/config" and uploaded_config is not None:
            if self._protected():
                return
            return self._send(200, uploaded_config, "text/plain")

        if not self._authenticated() and path not in ("/login.html", "/favicon.ico"):
            return self._redirect("/login.html")

        # Resolve files under ROOT only; reject path traversal.
        relative = path.lstrip("/") or "index.html"
        target = (ROOT / relative).resolve()
        if target != ROOT and ROOT not in target.parents:
            return self._send(404, b"Not found\n")

        if not target.is_file():
            return self._send(404, b"Not found\n")

        try:
            data = target.read_bytes()
        except OSError:
            return self._send(404, b"Not found\n")

        import mimetypes

        mime = mimetypes.guess_type(str(target))[0] or "application/octet-stream"
        return self._send(
            200,
            data,
            f"{mime}",
            headers=(("Cache-Control", "max-age=60, must-revalidate"),),
        )

    def do_POST(self):
        global uploaded_config, state

        path = urlsplit(self.path).path

        if path == "/login":
            form = self._read_urlencoded_form()
            if form.get("pwd", [""])[0] == PASSWORD:
                token = secrets.token_urlsafe(32)
                sessions.add(token)
                return self._redirect(
                    "/index.html",
                    headers=((
                        "Set-Cookie",
                        f"session={token}; Path=/; SameSite=Strict; HttpOnly",
                    ),),
                )
            return self._redirect("/login.html")

        if self._protected():
            return

        if path == "/cmd":
            command = self._read_raw()
            state["cmd_history"].append(command)
            print(f"Received command: '{command}'")
            self.parse_cmd(command.strip());

            return self._send(
                200,
                b"<!DOCTYPE html> <html><head><title>Upload OK</title></head>"
                b"<body><h1>Command executed successully</h1></html>",
                "text/html",
            )

        if path in ("/upload", "/config"):
            if self.headers.get_content_type() != "multipart/form-data":
                return self._send(400, b"Expected multipart/form-data\n")

            parts = self._read_multipart()
            if parts is None:
                return self._send(400, b"Bad multipart request\n")

            file_data = next(
                (
                    data for content_type, data in parts
                    if content_type == "application/octet-stream"
                ),
                None,
            )
            if file_data is None:
                return self._send(
                    400, b"No application/octet-stream part\n"
                )

            if path == "/config":
                uploaded_config = file_data

            return self._send(
                200,
                b"<!DOCTYPE html> <html><head><title>Upload OK</title></head>"
                b"<h1>File uploaded successfully</h1></body></html>",
                "text/html",
            )

        return self._send(404, b"Not found\n")

    def _read_raw(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length).decode("utf-8", errors="replace")
        return raw

    def _read_urlencoded_form(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length).decode("utf-8", errors="replace")
        return parse_qs(raw)

    def _read_multipart(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length)
        content_type = self.headers.get("Content-Type", "")
        message = BytesParser(policy=default).parsebytes(
            b"Content-Type: "
            + content_type.encode("utf-8")
            + b"\r\nMIME-Version: 1.0\r\n\r\n"
            + raw
        )

        if not message.is_multipart():
            return None

        parts = []
        for part in message.iter_parts():
            parts.append((
                part.get_content_type(),
                part.get_payload(decode=True) or b"",
            ))
        return parts

    def parse_cmd(self, cmd):
        pieces = cmd.split(" ")
        handler = self.get_parsers().get(pieces[0])
        return handler(pieces) if handler else {}

    def get_parsers(self):
        return {
            "vlan": self._parse_vlan,
            "lldp": self._parse_lldp,
            "pvid": self._parse_pvid
        }

    def newVlan(self, name):
        return {"active":True, "name":name, "untagged":[], "tagged":[], "pvid":[]}

    def _parse_vlan(self, pieces):
        global state
        vid = int(pieces[1]);
        startIndex=2
        name=""
        if pieces[2] == "d":
            return remove_vlan(vid)
        elif pieces[2] == "mgmt":
            state["mgmt_vlan"] = vid
            return
        elif not re.match('^[0-9]+$', pieces[2]):
            name = pieces[2]
            startIndex=3

        state["vlans"][vid] = self.newVlan(name)

        for i in pieces[startIndex:]:
            if i[-1] == "t":
                state["vlans"][vid]["tagged"].append(phys_to_log_port[int(i[:-1])-1])
            else:
                state["vlans"][vid]["untagged"].append(phys_to_log_port[int(i)-1])

    def _parse_pvid(self, pieces):
        global state
        port = int(pieces[1])
        vid = int(pieces[2])
        if(vid in state["vlans"]):
            state["vlans"][vid]["pvid"].append(phys_to_log_port[port-1])
        else :
            state["vlans"][vid] = self.newVlan("")

    def _parse_lldp(self, pieces):
        global state
        state.lldp_enabled = (pieces[0] == "on")

    def _api_status(self):
        global last_called
        now = max(int(time.time()), last_called + 1)
        elapsed = now - last_called
        ports = []

        for i in range(1, PORTS + 1):
            n = i - 1
            if i % 2:
                rate = 2_400_000_000 if i == 1 else 950_000_000
                tx_g[n] += rate * elapsed
                rx_g[n] += rate * elapsed
                tx_b[n] += rate * elapsed // 10_000_000
                rx_b[n] += rate * elapsed // 10_000_000

            item = {
                "portNum": i,
                "logPort": phys_to_log_port[n],
                "isSFP": int(i > PORTS - NSFP),
                "enabled": int(i % 4 != 0),
                "link": (5 if i == 1 else 2) if i % 2 else 0,
                "txG": f"0x{tx_g[n]:016x}",
                "txB": f"0x{tx_b[n]:016x}",
                "rxG": f"0x{rx_g[n]:016x}",
                "rxB": f"0x{rx_b[n]:016x}",
            }

            if i >= PORTS - NSFP:
                item.update({
                    "sfp_vendor": "OEM",
                    "sfp_model": "10G-SFP+",
                    "sfp_serial": "12345678",
                    "sfp_options": "0x68",
                    "sfp_temp": f"0x{0x28fb + secrets.randbelow(100):04x}",
                    "sfp_vcc": f"0x{0x7eda + secrets.randbelow(100):04x}",
                    "sfp_txbias": f"0x{0x0d24 + secrets.randbelow(100):04x}",
                    "sfp_txpower": f"0x{0x14bd + secrets.randbelow(100):04x}",
                    "sfp_rxpower": "0x0000",
                    "sfp_laser": "0x0000",
                })
            else:
                item["adv"] = (
                    "100000" if i == 1
                    else "000011" if i == 2
                    else "000100"
                )

            ports.append(item)

        last_called = now
        return ports

    def _api_eee(self):
        return [
            {
                "portNum": i,
                "isSFP": int(i >= 5),
                "eee": "00000010",
                "eee_lp": "00000100",
                "active": int(i % 2 == 1),
            }
            for i in range(1, PORTS + 1)
        ]

    def _api_bandwidth(self):
        ports = []
        for i in range(1, PORTS + 1):
            ingress_limited = i % 2 == 1
            egress_limited = i % 4 != 0
            ports.append({
                "portNum": i,
                "iLimited": int(ingress_limited),
                "iFC": int(i % 4 != 0) if ingress_limited else 0,
                "iBW": f"{0x100 if ingress_limited else 0xfffff:08x}",
                "eLimited": int(egress_limited),
                "eBW": f"{0x1000 if egress_limited else 0xfffff:08x}",
            })
        return ports

    def _api_mirror(self):
        return {
            "mPort": 1,
            "enabled": 1,
            "mirror_tx": f"{0b000110:016b}",
            "mirror_rx": f"{0b000010:016b}",
        }

    def _api_lag(self):
        return [
            {
                "lagNum": i,
                "members": f"{(0x30 << (i * 2)) & 0x1f8:016b}",
                "hash": "0x7e",
            }
            for i in range(4)
        ]

    def _api_mtu(self):
        return [
            {
                "portNum": i,
                "mtu": f"0x{16383 if i % 2 else 1522:04x}",
            }
            for i in range(1, PORTS + 1)
        ]

    def _api_lldp(self):
        return {
            "on": 1,
            "portStatus": [
                {
                    "portNum": i,
                    "logPort": i - 1,
                    "name": "",
                    "permitted": 1,
                }
                for i in range(1, PORTS + 1)
            ],
        }

    def _api_counters(self):
        return [f"0x{0x1234 + i:016x}" for i in range(N_COUNTERS)]

    def _api_vlanlist(self):
        global state
        vlan_list = []
        for i in state["vlans"].keys():
            if i in state["vlans"] and state["vlans"][i]["active"]:
                vlan_list.append({
                    "id": i,
                    "name": state["vlans"][i]["name"],
                })
        return {"mgmt": state["mgmt_vlan"], "vlan": vlan_list}

    def _api_vlan(self,query):
        global state
        vlan_id = self._int_param(query, "vid")

        vlan = state["vlans"][vlan_id]
        if vlan and vlan["active"]:
            return {
                "name": vlan["name"],
                "members": self.vlanMembers(vlan),
                "pvid": self.vlanPvid(vlan),
            }
        return {}

    def vlanMembers(self, vlan):
        member_mask = 0
        for port in vlan["tagged"] + vlan["untagged"]:
            member_mask |= 1 << port

        untagged_mask = 0
        for port in vlan["untagged"]:
            untagged_mask |= 1 << port

        members = member_mask | (untagged_mask << 10)
        return f"0x{members:08x}"

    def vlanPvid(self, vlan):
        pvid_mask = 0
        for port in vlan["pvid"]:
            pvid_mask |= 1 << port
        return f"0x{pvid_mask:04x}"

    def _api_l2(self,query):
        idx = self._int_param(query, "idx")
        entries = sorted(l2_entries, key=lambda entry: entry["idx"])
        start = next(
            (n for n, entry in enumerate(entries) if entry["idx"] >= idx),
            len(entries),
        )
        result = []
        for entry in entries[start:start + L2_MAX_TRANSFER]:
            result.append({
                "mac": entry["mac"],
                "vlan": entry["vlan"],
                "type": "l" if entry.get("type") == "l" else "s",
                "port": entry["port"],
                "idx": f"{entry['idx']:04x}",
            })
        return result

    def _api_cmd_log(self):
        global state
        text = "\n".join(state["cmd_history"])
        if state["cmd_history"]:
            text += "\n"
        return text.encode("utf-8"), "text/plain"

    def _api_information(self):
        return {
            "ip_address":"192.168.10.247",
            "ip_gateway":"192.168.2.22",
            "ip_netmask":"255.255.255.0",
            "mac_address":"1c:2a:a3:23:00:02",
            "sw_ver":"0.1-sim",
            "hw_ver":"SWGT024-V2.0"
        }

    def _api_debug(self):
        global state
        return {
            "state": state
        }

if __name__ == "__main__":
    print(f"Serving {ROOT} on http://{HOST}:{PORT}")
    ThreadingHTTPServer((HOST, PORT), Handler).serve_forever()
