#!/usr/bin/env python3
"""Host side of the RTL8370N Ethernet console (console.c), for macOS BPF (needs sudo).

    sudo ./console.py en7 listen              beacons and log lines
    sudo ./console.py en7 info
    sudo ./console.py en7 reg 0x1b03 [0x0960]  read [write] a switch register
    sudo ./console.py en7 phy 0 0xa402 [VAL]   read [write] a PHY OCP register
    sudo ./console.py en7 xdata 0x2000 64      read XDATA
    sudo ./console.py en7 flash 0x1fc000 256 [-o out.bin]
    sudo ./console.py en7 log                  the switch's boot log
    sudo ./console.py en7 reboot
    sudo ./console.py en7 flash-write 0x0 image.bin   below 0x1fc000 only

Frames use EtherType 0x88B5: DA, SA, type, op, seq, status, data. Requests
carry their arguments where the status byte would be. Replies set op bit 7.
"""
import argparse
import fcntl
import os
import random
import re
import struct
import subprocess
import sys
import time

ETHERTYPE = 0x88B5
BIOCGBLEN, BIOCSETIF, BIOCIMMEDIATE, BIOCSHDRCMPLT, BIOCSRTIMEOUT = 0x40044266, 0x8020426C, 0x80044270, 0x80044275, 0x8010426D
OP = dict(reg_read=1, reg_write=2, phy_read=3, phy_write=4, xdata_read=5, xdata_write=6, flash_read=7,
          flash_arm=8, flash_erase=9, flash_prog=10, log=11, reboot=14, info=15, beacon=0x7E)
STATUS = {0: "ok", 1: "bad op", 2: "bad argument", 3: "denied", 4: "timeout"}
CHUNK = 200
FLASH_WRITABLE = ((0x0, 0x1FC000),)   # same as flash_allowed() in console.c


class Console:
    def __init__(self, iface):
        self._open_bpf(iface)
        self.seq = random.randrange(256)

    def _open_bpf(self, iface):
        for i in range(256):
            try:
                self.fd = os.open(f"/dev/bpf{i}", os.O_RDWR)
                break
            except OSError:
                continue
        else:
            sys.exit("no free /dev/bpf device (run with sudo)")
        fcntl.ioctl(self.fd, BIOCSETIF, struct.pack("16s16x", iface.encode()))
        fcntl.ioctl(self.fd, BIOCIMMEDIATE, struct.pack("I", 1))
        fcntl.ioctl(self.fd, BIOCSHDRCMPLT, struct.pack("I", 1))
        fcntl.ioctl(self.fd, BIOCSRTIMEOUT, struct.pack("qi4x", 0, 200000))
        self.blen = struct.unpack("I", fcntl.ioctl(self.fd, BIOCGBLEN, struct.pack("I", 0)))[0]
        # ifconfig can show a placeholder (02:00:00:00:00:00 on some USB adapters) while frames
        # leave with the hardware address, and replies to the placeholder are filtered out.
        out = subprocess.run(["networksetup", "-getmacaddress", iface], capture_output=True, text=True).stdout
        self.mac = bytes.fromhex(re.search(r"([0-9a-f]{2}(:[0-9a-f]{2}){5})", out).group(1).replace(":", ""))

    def _raw(self):
        """Received frames until the read times out."""
        buf = os.read(self.fd, self.blen)
        while buf:
            caplen, _, hdrlen = struct.unpack_from("<IIH", buf, 8)
            yield buf[hdrlen:hdrlen + caplen]
            buf = buf[(hdrlen + caplen + 3) & ~3:]

    def frames(self):
        for frame in self._raw():
            if len(frame) >= 17 and struct.unpack_from(">H", frame, 12)[0] == ETHERTYPE and frame[6:12] != self.mac:
                yield frame

    def send(self, frame):
        os.write(self.fd, frame.ljust(60, b"\0"))

    def call(self, op, args=b"", tries=3):
        self.seq = (self.seq + 1) & 0xFF
        frame = b"\xff" * 6 + self.mac + struct.pack(">HBB", ETHERTYPE, OP[op], self.seq) + args
        for _ in range(tries):
            self.send(frame)
            deadline = time.time() + 1
            while time.time() < deadline:
                for f in self.frames():
                    if f[14] == OP[op] | 0x80 and f[15] == self.seq:
                        if f[16]:
                            sys.exit(f"{op}: {STATUS.get(f[16], f[16])}")
                        return f[17:]
        sys.exit(f"{op}: no reply")


def info_str(d):
    ver, chip, up = struct.unpack_from(">HHH", d)
    ports = struct.unpack_from(">8H", d, 6)
    links = " ".join(f"p{i}:{'down' if not s & 0x10 else ('10M', '100M', '1G', '?')[s & 3]}" for i, s in enumerate(ports))
    return f"v{ver} chip={chip:#06x} up={up}s {links}"


def log_chunk(c, at):
    d = c.call("log", struct.pack(">H", at))
    return d[2:2 + struct.unpack_from(">H", d)[0]]


def read_flash(c, addr, size):
    out = bytearray()
    while len(out) < size:
        n = min(CHUNK, size - len(out))
        data = c.call("flash_read", (addr + len(out)).to_bytes(3, "big") + bytes([n]))
        if len(data) < n:
            sys.exit("short flash read")
        out += data[:n]
    return out


def num(s):
    return int(s, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("iface")
    ap.add_argument("cmd")
    ap.add_argument("args", nargs="*")
    ap.add_argument("-o", "--out")
    a = ap.parse_args()
    c = Console(a.iface)
    v = [num(x) for x in a.args] if a.cmd != "flash-write" else [num(a.args[0])]

    if a.cmd == "listen":
        log_at = 0
        while True:
            for f in c.frames():
                if f[14] == OP["beacon"]:
                    print(time.strftime("%H:%M:%S"), info_str(f[17:]))
                    log_len = struct.unpack_from(">H", f, 17 + 22)[0]
                    while log_at < log_len:
                        text = log_chunk(c, log_at)
                        print(text.decode(errors="replace"), end="")
                        log_at += len(text)
    elif a.cmd == "info":
        print(info_str(c.call("info")))
    elif a.cmd == "reg":
        d = c.call("reg_write", struct.pack(">HH", *v)) if len(v) == 2 else c.call("reg_read", struct.pack(">H", v[0]))
        print(f"{v[0]:#06x} = {struct.unpack('>H', d[:2])[0]:#06x}")
    elif a.cmd == "phy":
        d = c.call("phy_write", struct.pack(">BHH", *v)) if len(v) == 3 else c.call("phy_read", struct.pack(">BH", *v))
        print(f"phy {v[0]} {v[1]:#06x} = {struct.unpack('>H', d[:2])[0]:#06x}")
    elif a.cmd == "xdata":
        print(c.call("xdata_read", struct.pack(">HB", v[0], v[1]))[:v[1]].hex(" "))
    elif a.cmd in ("flash", "flash-write"):
        if a.cmd == "flash-write":
            with open(a.args[1], "rb") as source:
                data = source.read()
            addr = v[0]
            end = addr + (len(data) + 0xFFF) // 0x1000 * 0x1000   # erase covers whole sectors
            if addr % 0x1000 or not data or not any(lo <= addr and end <= hi for lo, hi in FLASH_WRITABLE):
                sys.exit(f"refusing: {addr:#x}..{end:#x} must be 4 KiB aligned and inside one of "
                         + ", ".join(f"{lo:#x}..{hi:#x}" for lo, hi in FLASH_WRITABLE))
            # Save the last sector's untouched bytes before any erase.
            data += read_flash(c, addr + len(data), end - addr - len(data))
            try:
                sr = struct.unpack(">H", c.call("flash_arm", struct.pack(">H", 0x5AFE))[:2])[0]
                if sr & 0x3C:
                    sys.exit(f"flash block protection set (status {sr:#04x})")
                for s in range(addr, end, 0x1000):
                    c.call("flash_erase", s.to_bytes(3, "big"))
                for o in range(0, len(data), 128):
                    part = data[o:o + 128]
                    c.call("flash_prog", (addr + o).to_bytes(3, "big") + bytes([len(part)]) + part)
            finally:
                c.call("flash_arm", struct.pack(">H", 0))
            v = [addr, len(data)]
        out = read_flash(c, v[0], v[1])
        if a.cmd == "flash-write":
            if out != data:
                sys.exit("verify FAILED")
            print("verify ok")
        elif a.out:
            open(a.out, "wb").write(out)
        else:
            print(out.hex(" "))
    elif a.cmd == "log":
        out, at = b"", 0
        while True:
            d = log_chunk(c, at)
            if not d:
                break
            out += d
            at += len(d)
        print(out.decode(errors="replace"), end="")
    elif a.cmd == "reboot":
        c.call("reboot")
        print("rebooting")
    else:
        ap.error(f"unknown command {a.cmd}")


if __name__ == "__main__":
    main()
