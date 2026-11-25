#!/usr/bin/env python3
"""
Fetch hardware info and configs from RTLPlayground switches, sort configs safely,
compile per-switch firmware using Docker, and optionally upload firmware.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path

import requests

# =============================================================================
# Configuration
# =============================================================================
SWITCH_IPS = [
    "192.168.10.247",
]
SWITCH_PASSWORD = os.environ.get("RTLP_PASSWORD", "1234")

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_DIR = SCRIPT_DIR.parent
MACHINE_DIR = REPO_DIR / "machine"
SWITCHES_DIR = REPO_DIR / "output" / "switches"
DOCKER_IMAGE = "rtlplayground-dev"

# =============================================================================
# Switch communication helpers
# =============================================================================


def ping_switch(ip: str, timeout: int = 1) -> bool:
    result = subprocess.run(
        ["ping", "-c", "1", "-W", str(timeout), ip],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return result.returncode == 0


def create_switch_session(ip: str, password: str) -> requests.Session:
    """Authenticate against /login and return an authenticated requests.Session."""
    session = requests.Session()
    # The switch's embedded httpd closes the TCP connection after every response.
    session.headers["Connection"] = "close"

    resp = session.post(f"http://{ip}/login", data={"pwd": password}, timeout=10)
    resp.raise_for_status()
    if "session" not in session.cookies:
        raise RuntimeError(f"Login to {ip} did not return a session cookie (wrong password?)")
    return session


def upload_firmware(ip: str, password: str, firmware_path: Path) -> None:
    """Upload firmware following RTLPlayground/doc/automation.md."""
    with create_switch_session(ip, password) as session:
        print(f"  Uploading {firmware_path.name} to http://{ip}/upload ...")
        try:
            with firmware_path.open("rb") as fw:
                resp = session.post(
                    f"http://{ip}/upload",
                    files={
                        "uploadedfile": (
                            firmware_path.name,
                            fw,
                            "application/octet-stream",
                        )
                    },
                    timeout=60,
                )
            if not resp.ok:
                detail = resp.text.strip()
                raise RuntimeError(
                    f"Upload to {ip} failed (HTTP {resp.status_code}): {detail or resp.reason}"
                )
        except requests.exceptions.ConnectionError:
            # Per doc/automation.md, the switch may close the connection without
            # sending an HTTP response while flashing and rebooting.
            pass

    print(f"  Waiting for {ip} to reboot and come back online ...")
    time.sleep(5)
    deadline = time.time() + 120
    while time.time() < deadline:
        if ping_switch(ip, timeout=2):
            try:
                probe = requests.get(
                    f"http://{ip}/login.html",
                    headers={"Connection": "close"},
                    timeout=3,
                )
                if probe.ok:
                    print(f"  {ip} is back online.")
                    return
            except requests.RequestException:
                pass
        time.sleep(2)
    raise RuntimeError(f"Timed out waiting for {ip} to come back online after upload")


# =============================================================================
# Machine detection from ./machine/*.h
# =============================================================================


def load_machine_catalog(machine_dir: Path) -> list[dict]:
    """Load machine catalog from machine/machines.json (or legacy machine/*.h)."""
    registry = machine_dir / "machines.json"
    if registry.is_file():
        entries = json.loads(registry.read_text(encoding="utf-8"))
        return [
            {
                "machine": e["machine"],
                "model": e.get("friendly_name", ""),
                "pcb": e.get("pcb", ""),
            }
            for e in entries
            if "machine" in e
        ]

    model_re = re.compile(r'^\s*#define\s+MACHINE_MODEL\s+"([^"]+)"', re.MULTILINE)
    catalog = []
    for hdr in sorted(machine_dir.glob("*.h")):
        machine_name = hdr.stem
        c_file = machine_dir / f"{machine_name}.c"
        if not c_file.exists():
            continue
        text = hdr.read_text(errors="ignore")
        m = model_re.search(text)
        if not m:
            continue
        pcb_name = ""
        if c_file.is_symlink():
            pcb_name = Path(os.readlink(c_file)).stem
        catalog.append(
            {
                "machine": machine_name,
                "model": m.group(1),
                "pcb": pcb_name,
            }
        )
    return catalog


def resolve_machine(hw_ver: str, pcb_ver: str, catalog: list[dict]) -> str:
    """
    Match hw_ver (and optional pcb_ver) from /information.json against machine/*.h.

    Handles both:
    - Current firmware: hw_ver == MACHINE_MODEL (plus pcb_ver for multi-PCB models)
    - Legacy firmware: hw_ver includes a trailing revision like "V3.1" or omits a
      minor suffix (e.g. "keepLink KP-9000-6XHML V2.1" -> KEEPLINK_KP_9000_6XHML_X2_2M_PCB43_V2_1)
    """
    hw_clean = hw_ver.strip()
    pcb_clean = pcb_ver.strip()

    # 1. Exact or case-insensitive match on MACHINE_MODEL
    matches = [c for c in catalog if c["model"] == hw_clean]
    if not matches:
        matches = [c for c in catalog if c["model"].lower() == hw_clean.lower()]

    if len(matches) == 1:
        return matches[0]["machine"]

    if len(matches) > 1 and pcb_clean and pcb_clean.lower() != "unknown":
        pcb_matches = [c for c in matches if c["pcb"].lower() == pcb_clean.lower()]
        if len(pcb_matches) == 1:
            return pcb_matches[0]["machine"]

    # 2. Legacy hw_ver with trailing version token, e.g. "... V3.1" or "... V2.1"
    ver_match = re.match(r"^(.+?)\s+[vV](\d+(?:\.\d+)+)$", hw_clean)
    if ver_match:
        base_model = ver_match.group(1).strip().lower()
        ver_token = "v" + ver_match.group(2).replace(".", "_")

        candidates = [c for c in catalog if c["model"].lower() == base_model]
        if not candidates:
            candidates = [
                c
                for c in catalog
                if c["model"].lower().startswith(base_model)
                and (
                    len(c["model"]) == len(base_model)
                    or c["model"][len(base_model)] in (" ", "-", "_")
                )
            ]

        if candidates:
            ver_candidates = [
                c
                for c in candidates
                if c["machine"].lower().endswith("_" + ver_token)
                or c["pcb"].lower().endswith("_" + ver_token)
            ]
            if len(ver_candidates) == 1:
                return ver_candidates[0]["machine"]
            if len(ver_candidates) > 1:
                names = ", ".join(c["machine"] for c in ver_candidates)
                raise RuntimeError(
                    f"Ambiguous machine match for hw_ver={hw_ver!r} pcb_ver={pcb_ver!r}: {names}"
                )

    candidates_str = ", ".join(c["machine"] for c in matches) if matches else "none"
    raise RuntimeError(
        f"Could not uniquely match hw_ver={hw_ver!r} (pcb_ver={pcb_ver!r}) "
        f"in {MACHINE_DIR} (candidates: {candidates_str})"
    )


# =============================================================================
# Safe config sorting
# =============================================================================


def _int_or_max(s: str) -> int:
    try:
        return int(s)
    except ValueError:
        return 999999


def _sort_ingress_line(line: str) -> str:
    """Sort per-port tokens in `ingress 1u 2t ...` numerically by port."""
    parts = line.split()
    if len(parts) <= 2:
        return line
    port_tokens = parts[1:]
    if all(re.match(r"^\d+[uta]$", tok) for tok in port_tokens):
        port_tokens.sort(key=lambda t: int(t[:-1]))
        return "ingress " + " ".join(port_tokens)
    return line


def _config_sort_key(line: str) -> tuple:
    """
    Order startup config commands cleanly while respecting execution dependencies:
      1. Identity & access (hostname, passwd, session, mac)
      2. Network (ip, netmask, gw, dns)
      3. Time & logging (ntp settings -> ntp on/off; syslog settings -> syslog on/off)
      4. VLAN definitions (sorted by VLAN ID) -> vlan <id> mgmt -> vlan <id> d
      5. Port VLANs & ingress (pvid by port; global ingress before per-port ingress)
      6. Port settings (port <n> name -> lldp -> speed/state; mtu; eee; bw; storm; isolate; mirror)
      7. Aggregation (lag -> laghash -> lacp)
      8. Protocols (igmp -> lldp -> stp settings -> stp on/off)
      9. Anything else (kept in original relative order via Python's stable sort)
    """
    parts = line.split()
    cmd = parts[0].lower() if parts else ""
    arg1 = parts[1].lower() if len(parts) > 1 else ""
    arg2 = parts[2].lower() if len(parts) > 2 else ""

    # 1. Identity & access
    if cmd == "hostname":
        return (10, 0, 0, "")
    if cmd == "passwd":
        return (11, 0, 0, "")
    if cmd == "session":
        return (12, 0, 0, "")
    if cmd == "mac":
        return (13, 0, 0, "")

    # 2. Network
    if cmd == "ip":
        return (20, 0, 0, "")
    if cmd == "netmask":
        return (21, 0, 0, "")
    if cmd == "gw":
        return (22, 0, 0, "")
    if cmd == "dns":
        return (23, 0, 0, arg1)

    # 3. Time & logging (keep on/off after configuration lines)
    if cmd in ("ntp", "time"):
        sub_order = {
            "server": 0,
            "interval": 1,
            "timezone": 2,
            "dst": 3,
            "on": 9,
            "off": 9,
        }.get(arg1, 5)
        return (30, sub_order, 0, arg1)
    if cmd == "syslog":
        sub_order = {"on": 9, "off": 9}.get(arg1, 1)
        return (31, sub_order, 0, arg1)

    # 4. VLANs: create VLANs first (by numeric ID), then mgmt VLAN, then deletes
    if cmd == "vlan":
        vid = _int_or_max(arg1)
        if arg2 == "mgmt":
            return (41, vid, 0, "")
        if arg2 == "d":
            return (42, vid, 0, "")
        return (40, vid, 0, "")

    # 5. PVID & ingress
    if cmd == "pvid":
        return (50, _int_or_max(arg1), 0, "")
    if cmd == "ingress":
        # Global mode (`ingress a|t|u`) must come before per-port overrides
        is_global = len(parts) == 2 and arg1 in ("a", "t", "u")
        return (51, 0 if is_global else 1, 0, "")

    # 6. Per-port settings
    if cmd == "port":
        pnum = _int_or_max(arg1)
        # Keep `name` first, then `lldp`, then speed/state (stable sort preserves
        # original order if multiple speed/state lines exist for the same port).
        if arg2 == "name":
            sub = 0
        elif arg2 == "lldp":
            sub = 1
        else:
            sub = 2
        return (60, pnum, sub, "")
    if cmd == "mtu":
        return (61, _int_or_max(arg1), 0, "")
    if cmd == "eee":
        # e.g. `eee off 1` or `eee on 1`
        return (62, _int_or_max(arg2), 0, arg1)
    if cmd == "bw":
        return (63, _int_or_max(arg1), 0, "")
    if cmd == "storm":
        return (64, _int_or_max(arg1), 0, "")
    if cmd == "isolate":
        return (65, _int_or_max(arg1), 0, "")
    if cmd == "mirror":
        return (66, 0, 0, "")

    # 7. LAG / LACP (must precede STP which maps ports to LAGs)
    if cmd == "lag":
        return (70, _int_or_max(arg1), 0, "")
    if cmd == "laghash":
        return (71, _int_or_max(arg1), 0, "")
    if cmd == "lacp":
        return (72, 0, 0, "")

    # 8. Protocols
    if cmd == "igmp":
        return (80, 0, 0, "")
    if cmd == "lldp":
        return (81, 0, 0, "")
    if cmd == "stp":
        # STP parameters/port/lag settings before `stp on`/`stp off`
        sub = 9 if arg1 in ("on", "off") else 1
        port_or_lag = _int_or_max(arg2) if arg1 in ("port", "lag") else 0
        return (82, sub, port_or_lag, arg1)

    # 9. Any other commands: keep at end in original relative order
    return (99, 0, 0, "")


def sort_config(raw_config: str) -> str:
    lines = []
    for raw_line in raw_config.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.lower().startswith("ingress "):
            line = _sort_ingress_line(line)
        lines.append(line)

    sorted_lines = sorted(lines, key=_config_sort_key)
    return "\n".join(sorted_lines) + ("\n" if sorted_lines else "")


# =============================================================================
# Docker build helpers
# =============================================================================


def ensure_docker_image() -> None:
    check = subprocess.run(
        ["docker", "image", "inspect", DOCKER_IMAGE],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    if check.returncode == 0:
        return
    print(f"Building Docker image '{DOCKER_IMAGE}' from {REPO_DIR / 'Dockerfile'} ...")
    res = subprocess.run(["docker", "build", "-t", DOCKER_IMAGE, str(REPO_DIR)])
    if res.returncode != 0:
        raise RuntimeError(f"Failed to build Docker image '{DOCKER_IMAGE}'")


def compile_firmware(machine: str, switch_dir: Path, config_file: Path) -> Path:
    """Compile firmware inside Docker and return the generated .bin path."""
    # Remove old .bin files in this switch's directory so we clearly identify the new build
    for old_bin in switch_dir.glob("*.bin"):
        old_bin.unlink()

    rel_dir = switch_dir.relative_to(REPO_DIR)
    container_dir = f"/workspace/{rel_dir.as_posix()}"
    container_cfg = f"{container_dir}/{config_file.name}"

    cmd = [
        "docker",
        "run",
        "--rm",
        "-u",
        f"{os.getuid()}:{os.getgid()}",
        "-e",
        "GIT_CONFIG_COUNT=1",
        "-e",
        "GIT_CONFIG_KEY_0=safe.directory",
        "-e",
        "GIT_CONFIG_VALUE_0=/workspace",
        "-v",
        f"{REPO_DIR}:/workspace",
        "-w",
        "/workspace",
        DOCKER_IMAGE,
        "make",
        f"MACHINE={machine}",
        f"CONFIG={container_cfg}",
        f"OUTPUT_DIR={container_dir}",
    ]
    print(f"  Compiling MACHINE={machine} CONFIG={config_file.name} ...")
    res = subprocess.run(cmd)
    if res.returncode != 0:
        raise RuntimeError(f"Firmware compilation failed for MACHINE={machine}")

    bins = sorted(switch_dir.glob("*.bin"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not bins:
        raise RuntimeError(f"No .bin firmware produced in {switch_dir}")
    return bins[0]


# =============================================================================
# Main workflow
# =============================================================================


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Fetch configs, sort them, and build RTLPlayground firmware for each switch."
    )
    parser.add_argument(
        "--upload",
        action="store_true",
        help="Upload the compiled firmware to each switch after building.",
    )
    parser.add_argument(
        "--ips",
        nargs="+",
        default=SWITCH_IPS,
        help="Override the list of switch IPs to process.",
    )
    parser.add_argument(
        "--password",
        default=SWITCH_PASSWORD,
        help="Override the switch web UI password.",
    )
    args = parser.parse_args()

    if not MACHINE_DIR.is_dir():
        print(f"ERROR: Machine directory not found: {MACHINE_DIR}", file=sys.stderr)
        return 1

    SWITCHES_DIR.mkdir(parents=True, exist_ok=True)
    catalog = load_machine_catalog(MACHINE_DIR)
    ensure_docker_image()

    results = []

    for ip in args.ips:
        print(f"\n=== Switch {ip} ===")
        if not ping_switch(ip):
            print(f"  Offline (ping failed), skipping.")
            results.append((ip, "OFFLINE", "-", "-", "-"))
            continue

        try:
            with create_switch_session(ip, args.password) as session:
                info_resp = session.get(f"http://{ip}/information.json", timeout=10)
                info_resp.raise_for_status()
                info = info_resp.json()

                hw_ver = info.get("hw_ver", "")
                pcb_ver = info.get("pcb_ver", "")
                hostname = info.get("hostname", "")

                machine = resolve_machine(hw_ver, pcb_ver, catalog)
                print(f"  Hostname: {hostname or '(none)'}")
                print(f"  hw_ver:   {hw_ver}" + (f" (pcb_ver: {pcb_ver})" if pcb_ver else ""))
                print(f"  Machine:  {machine}")

                cfg_resp = session.get(f"http://{ip}/config", timeout=10)
                cfg_resp.raise_for_status()
                sorted_config = sort_config(cfg_resp.text)

            switch_dir = SWITCHES_DIR / ip
            switch_dir.mkdir(parents=True, exist_ok=True)

            (switch_dir / "information.json").write_text(
                json.dumps(info, indent=2) + "\n", encoding="utf-8"
            )
            config_file = switch_dir / "config.txt"
            config_file.write_text(sorted_config, encoding="utf-8")
            print(f"  Saved sorted config to {config_file}")

            firmware_path = compile_firmware(machine, switch_dir, config_file)
            print(f"  Built firmware: {firmware_path} ({firmware_path.stat().st_size} bytes)")

            status = "BUILT"
            if args.upload:
                upload_firmware(ip, args.password, firmware_path)
                status = "UPLOADED"

            results.append((ip, status, hostname or "-", machine, firmware_path.name))
        except Exception as exc:
            print(f"  ERROR: {exc}", file=sys.stderr)
            results.append((ip, f"ERROR: {exc}", "-", "-", "-"))

    print("\n=== Summary ===")
    for ip, status, hostname, machine, fw in results:
        print(f"  {ip:<12} {status:<10} {hostname:<16} {machine:<42} {fw}")

    return 0 if all(r[1] in ("BUILT", "UPLOADED", "OFFLINE") for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
