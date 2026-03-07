#!/usr/bin/env python3
"""
marauder_serial.py — Interactive CLI wrapper for WiFi Marauder over serial.

Connects to the Flipper Zero's ESP32 WiFi Dev Board via the Flipper's USB serial
port (or directly to the ESP32's USB-C port) and provides:
  - Line-editing interactive shell with history
  - Automatic PCAP framing detection: saves [BUF/BEGIN]..[BUF/CLOSE] blocks
    to timestamped .pcap files automatically
  - Clean console output (strips ANSI/binary noise)
  - Command macros for common attack chains

Usage:
    python3 marauder_serial.py [PORT] [BAUD]
    python3 marauder_serial.py /dev/ttyUSB0 115200
    python3 marauder_serial.py COM3 115200          # Windows

    # If PORT is omitted, auto-detects the first Flipper/ESP32 serial port.

Requirements:
    pip install pyserial

For authorized security testing only.
"""

import sys
import os
import re
import time
import threading
import readline
import glob
import struct
import datetime
import argparse
import serial
import serial.tools.list_ports

# ── Configuration ────────────────────────────────────────────────────────────

BAUD_RATE    = 115200
PCAP_DIR     = "./pcap_captures"
HISTORY_FILE = os.path.expanduser("~/.marauder_history")
TIMEOUT      = 1.0

# PCAP frame markers (11 bytes each, from WiFi Marauder source)
PCAP_BEGIN   = b"[BUF/BEGIN]"
PCAP_CLOSE   = b"[BUF/CLOSE]"

# ANSI escape sequence pattern
ANSI_RE = re.compile(rb'\x1b\[[0-9;]*[mGKHF]')

# Known VID/PID for Flipper Zero USB and ESP32 boards
FLIPPER_VIDS = {0x0483, 0x303A, 0x10C4, 0x1A86}

# Macro definitions: name → list of (command, delay_seconds)
MACROS = {
    "quick_scan": [
        ("scanap", 8),
        ("scansta", 5),
        ("listap", 0),
    ],
    "deauth_flood": [
        ("scanap", 8),
        ("select -a 0", 0),
        ("attack -t deauth", 0),
    ],
    "pmkid_force": [
        ("scanap", 8),
        ("sniffpmkid -c 6 -d -l", 60),
        ("stopscan", 0),
    ],
    "beacon_spam_random": [
        ("attack -t beacon -r", 0),
    ],
    "wardrive": [
        ("wardrive", 0),
    ],
    "ble_full": [
        ("sniffbt", 10),
        ("sourapple", 20),
        ("swiftpair", 20),
        ("samsungblespam", 20),
        ("spoofat", 20),
        ("stopscan", 0),
    ],
    "recon": [
        ("scanap", 10),
        ("sniffprobe", 30),
        ("sniffbeacon", 10),
        ("stopscan", 0),
        ("listap", 0),
    ],
}

# ── Port Detection ───────────────────────────────────────────────────────────

def find_marauder_port():
    """Auto-detect the most likely Flipper/ESP32 serial port."""
    ports = list(serial.tools.list_ports.comports())
    candidates = []

    for p in ports:
        if p.vid in FLIPPER_VIDS:
            candidates.append((0, p.device))  # high priority
        elif any(kw in (p.description or "").lower()
                 for kw in ["flipper", "esp32", "cp210", "ch340", "ft232", "cdc"]):
            candidates.append((1, p.device))
        elif p.device:
            candidates.append((2, p.device))

    if not candidates:
        return None

    candidates.sort()
    return candidates[0][1]

# ── PCAP Handler ─────────────────────────────────────────────────────────────

class PcapCapture:
    """Buffers raw PCAP data between [BUF/BEGIN] and [BUF/CLOSE] markers
    and saves completed captures to timestamped .pcap files."""

    def __init__(self, output_dir: str):
        os.makedirs(output_dir, exist_ok=True)
        self.output_dir = output_dir
        self._buf = bytearray()
        self._capturing = False
        self._count = 0

    def feed(self, data: bytes) -> bytes:
        """
        Feed raw bytes. Returns the non-PCAP portion (text output) for display.
        """
        text_out = bytearray()
        i = 0
        while i < len(data):
            if not self._capturing:
                # Look for BEGIN marker
                idx = data.find(PCAP_BEGIN, i)
                if idx == -1:
                    text_out.extend(data[i:])
                    break
                text_out.extend(data[i:idx])  # text before marker
                i = idx + len(PCAP_BEGIN)
                self._capturing = True
                self._buf = bytearray()
            else:
                # Look for CLOSE marker
                idx = data.find(PCAP_CLOSE, i)
                if idx == -1:
                    self._buf.extend(data[i:])
                    break
                self._buf.extend(data[i:idx])
                i = idx + len(PCAP_CLOSE)
                self._capturing = False
                self._save()

        return bytes(text_out)

    def _save(self):
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        self._count += 1
        fname = os.path.join(self.output_dir, f"capture_{ts}_{self._count}.pcap")
        with open(fname, "wb") as f:
            f.write(bytes(self._buf))
        print(f"\n\033[32m[pcap]\033[0m Saved {len(self._buf)} bytes → {fname}")
        self._buf = bytearray()

# ── Reader Thread ─────────────────────────────────────────────────────────────

class SerialReader(threading.Thread):
    """Reads from serial port in background, handles PCAP framing, prints output."""

    def __init__(self, ser: serial.Serial, pcap: PcapCapture):
        super().__init__(daemon=True)
        self.ser = ser
        self.pcap = pcap
        self._stop = threading.Event()

    def stop(self):
        self._stop.set()

    def run(self):
        line_buf = bytearray()
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(256)
            except serial.SerialException:
                break
            if not chunk:
                continue

            # Strip ANSI codes before PCAP processing
            chunk = ANSI_RE.sub(b"", chunk)

            # Route through PCAP handler
            text = self.pcap.feed(chunk)

            # Print text output
            for byte in text:
                if byte == ord('\n'):
                    line = line_buf.decode("utf-8", errors="replace").rstrip()
                    if line:
                        # Print on its own line (clear current input line first)
                        sys.stdout.write(f"\r\033[K\033[90m> {line}\033[0m\n")
                        sys.stdout.flush()
                    line_buf.clear()
                elif byte == ord('\r'):
                    pass
                else:
                    line_buf.append(byte)

# ── Command Dispatch ──────────────────────────────────────────────────────────

def send_command(ser: serial.Serial, cmd: str):
    """Send a single command to Marauder."""
    payload = (cmd.strip() + "\n").encode("utf-8")
    ser.write(payload)
    ser.flush()

def run_macro(ser: serial.Serial, name: str):
    """Execute a named macro sequence."""
    if name not in MACROS:
        print(f"\033[31mUnknown macro: {name}\033[0m")
        print(f"Available: {', '.join(MACROS.keys())}")
        return
    steps = MACROS[name]
    print(f"\033[33m[macro]\033[0m Running '{name}' ({len(steps)} steps)...")
    for cmd, delay in steps:
        print(f"\033[33m[macro]\033[0m  → {cmd}" + (f"  (wait {delay}s)" if delay else ""))
        send_command(ser, cmd)
        if delay:
            time.sleep(delay)
    print(f"\033[33m[macro]\033[0m '{name}' complete.")

def print_help():
    print("""
\033[1mMarauder Serial Shell\033[0m

Built-in commands:
  !macro <name>    Run a command macro sequence
  !macros          List available macros
  !pcap            Show PCAP capture directory
  !help            Show this help
  exit / quit / q  Disconnect and exit

Any other input is sent as-is to WiFi Marauder.

Marauder quick reference:
  scanap                          Scan for access points
  scansta                         Scan for stations
  listap                          List discovered APs
  attack -t deauth                Deauth flood
  attack -t beacon -r             Random beacon spam
  sniffpmkid -d -l                PMKID + EAPOL capture (deauth + log)
  sniffprobe                      Probe request capture
  sniffraw                        Raw 802.11 frame capture
  evilportal -c start -w file.html  Start evil portal
  karma                           Karma attack
  stopscan                        Stop active scan/attack
  btspamall                       BLE spam (all types)
  sourapple                       iOS BLE popup spam
  swiftpair                       Windows SwiftPair spam
  info                            Device information
  reboot                          Reboot ESP32

Macros:
""")
    for name, steps in MACROS.items():
        cmds = " → ".join(s[0] for s in steps)
        print(f"  {name:<22} {cmds}")
    print()

# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Interactive CLI for WiFi Marauder over serial"
    )
    parser.add_argument("port", nargs="?", help="Serial port (auto-detects if omitted)")
    parser.add_argument("baud", nargs="?", type=int, default=BAUD_RATE, help="Baud rate (default: 115200)")
    parser.add_argument("--pcap-dir", default=PCAP_DIR, help="PCAP output directory")
    args = parser.parse_args()

    port = args.port or find_marauder_port()
    if not port:
        print("\033[31mError: No serial port found. Specify port as argument.\033[0m")
        print("Example: python3 marauder_serial.py /dev/ttyUSB0")
        sys.exit(1)

    print(f"\033[1mMarauder Serial Shell\033[0m  —  Authorized testing only")
    print(f"Connecting to {port} @ {args.baud} baud...")

    try:
        ser = serial.Serial(port, args.baud, timeout=TIMEOUT)
    except serial.SerialException as e:
        print(f"\033[31mFailed to open port: {e}\033[0m")
        sys.exit(1)

    print(f"\033[32mConnected.\033[0m  Type !help for built-in commands.\n")

    pcap = PcapCapture(args.pcap_dir)
    reader = SerialReader(ser, pcap)
    reader.start()

    # Readline history
    if os.path.exists(HISTORY_FILE):
        readline.read_history_file(HISTORY_FILE)
    readline.set_history_length(500)

    try:
        while True:
            try:
                line = input("\033[36mmarauder>\033[0m ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if not line:
                continue

            readline.write_history_file(HISTORY_FILE)

            if line.lower() in ("exit", "quit", "q"):
                break
            elif line == "!help":
                print_help()
            elif line == "!macros":
                for name in MACROS:
                    print(f"  {name}")
            elif line == "!pcap":
                print(f"PCAP directory: {os.path.abspath(args.pcap_dir)}")
                files = sorted(glob.glob(os.path.join(args.pcap_dir, "*.pcap")))
                if files:
                    for f in files[-10:]:
                        size = os.path.getsize(f)
                        print(f"  {os.path.basename(f)}  ({size} bytes)")
                else:
                    print("  (no captures yet)")
            elif line.startswith("!macro "):
                run_macro(ser, line[7:].strip())
            else:
                send_command(ser, line)
                time.sleep(0.05)

    finally:
        reader.stop()
        ser.close()
        print("Disconnected.")

if __name__ == "__main__":
    main()
