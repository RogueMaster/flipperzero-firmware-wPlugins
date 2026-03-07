#!/usr/bin/env python3
"""
pcap_capture.py — Dedicated PCAP capture tool for WiFi Marauder.

Connects to the Marauder ESP32, starts the specified sniffer, and handles the
[BUF/BEGIN]..[BUF/CLOSE] binary framing that Marauder uses to multiplex PCAP
data and text output on a single UART channel.

Each completed PCAP frame is written to a timestamped .pcap file, ready for
import into Wireshark. Multiple captures in one session produce separate files.

Usage:
    python3 pcap_capture.py [OPTIONS]

    python3 pcap_capture.py --port /dev/ttyUSB0 --mode sniffraw --duration 60
    python3 pcap_capture.py --port COM3 --mode sniffpmkid --channel 6 --deauth
    python3 pcap_capture.py --port /dev/ttyUSB0 --mode sniffbeacon --duration 120 --out ./captures

Modes (maps to Marauder sniffer commands):
    sniffraw        Raw 802.11 frames (all types, all channels)
    sniffbeacon     Beacon frames only
    sniffdeauth     Deauthentication frames only
    sniffpmkid      PMKID/EAPOL handshake capture
    sniffprobe      Probe request frames

Requirements:
    pip install pyserial

For authorized security testing only.
"""

import sys
import os
import re
import time
import threading
import datetime
import argparse
import signal
import struct
import serial
import serial.tools.list_ports

# ── Constants ────────────────────────────────────────────────────────────────

BAUD_RATE   = 115200
TIMEOUT_SEC = 1.0

PCAP_BEGIN  = b"[BUF/BEGIN]"
PCAP_CLOSE  = b"[BUF/CLOSE]"

ANSI_RE = re.compile(rb'\x1b\[[0-9;]*[mGKHF]')

# libpcap global header (little-endian, 802.11 link type = 105)
PCAP_GLOBAL_HEADER = struct.pack(
    "<IHHiIII",
    0xA1B2C3D4,  # magic number
    2,           # major version
    4,           # minor version
    0,           # thiszone (UTC)
    0,           # sigfigs
    65535,       # snaplen
    105,         # network (LINKTYPE_IEEE802_11)
)

SNIFFER_COMMANDS = {
    "sniffraw":    "sniffraw",
    "sniffbeacon": "sniffbeacon",
    "sniffdeauth": "sniffdeauth",
    "sniffpmkid":  None,  # built dynamically
    "sniffprobe":  "sniffprobe",
}

FLIPPER_VIDS = {0x0483, 0x303A, 0x10C4, 0x1A86}

# ── Port Detection ───────────────────────────────────────────────────────────

def find_port():
    for p in serial.tools.list_ports.comports():
        if p.vid in FLIPPER_VIDS:
            return p.device
        if any(k in (p.description or "").lower()
               for k in ["flipper", "esp32", "cp210", "ch340", "ft232", "cdc"]):
            return p.device
    ports = list(serial.tools.list_ports.comports())
    return ports[0].device if ports else None

# ── Statistics ───────────────────────────────────────────────────────────────

class Stats:
    def __init__(self):
        self.frames_received = 0
        self.bytes_received  = 0
        self.files_written   = 0
        self.text_lines      = 0
        self.start_time      = time.time()

    def elapsed(self) -> str:
        e = int(time.time() - self.start_time)
        return f"{e//60:02d}:{e%60:02d}"

    def summary(self) -> str:
        return (
            f"Duration: {self.elapsed()}  "
            f"Frames: {self.frames_received}  "
            f"Bytes: {self.bytes_received}  "
            f"Files: {self.files_written}"
        )

# ── PCAP Capture Engine ──────────────────────────────────────────────────────

class PcapEngine:
    """Handles the [BUF/BEGIN]..[BUF/CLOSE] PCAP stream from Marauder."""

    def __init__(self, output_dir: str, stats: Stats, verbose: bool = False):
        os.makedirs(output_dir, exist_ok=True)
        self.output_dir  = output_dir
        self.stats       = stats
        self.verbose     = verbose
        self._buf        = bytearray()
        self._capturing  = False
        self._file_count = 0
        self._current_file = None
        self._current_path = None

    def feed(self, raw: bytes):
        """Process a chunk of raw serial data."""
        raw = ANSI_RE.sub(b"", raw)
        i = 0
        while i < len(raw):
            if not self._capturing:
                idx = raw.find(PCAP_BEGIN, i)
                if idx == -1:
                    # Pure text — print it
                    self._handle_text(raw[i:])
                    break
                self._handle_text(raw[i:idx])
                i = idx + len(PCAP_BEGIN)
                self._capturing = True
                self._open_new_file()
            else:
                idx = raw.find(PCAP_CLOSE, i)
                if idx == -1:
                    self._buf.extend(raw[i:])
                    break
                self._buf.extend(raw[i:idx])
                i = idx + len(PCAP_CLOSE)
                self._capturing = False
                self._close_file()

    def _open_new_file(self):
        self._file_count += 1
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        fname = f"capture_{ts}_{self._file_count:03d}.pcap"
        self._current_path = os.path.join(self.output_dir, fname)
        self._current_file = open(self._current_path, "wb")
        # Write a minimal libpcap global header so Wireshark can open it
        self._current_file.write(PCAP_GLOBAL_HEADER)
        self._buf = bytearray()
        if self.verbose:
            print(f"\n[pcap] Started capture → {fname}")

    def _close_file(self):
        if self._current_file:
            self._current_file.write(bytes(self._buf))
            self._current_file.close()
            self._current_file = None
            size = len(PCAP_GLOBAL_HEADER) + len(self._buf)
            self.stats.frames_received += 1
            self.stats.bytes_received  += size
            self.stats.files_written   += 1
            print(
                f"\n\033[32m[pcap]\033[0m Saved "
                f"\033[1m{os.path.basename(self._current_path)}\033[0m "
                f"({size:,} bytes)"
            )
            self._buf = bytearray()

    def flush(self):
        """Flush any in-progress capture on shutdown."""
        if self._capturing and self._current_file and self._buf:
            print("\n[pcap] Flushing partial capture...")
            self._close_file()

    def _handle_text(self, data: bytes):
        text = data.decode("utf-8", errors="replace")
        for line in text.splitlines():
            line = line.strip()
            if line:
                self.stats.text_lines += 1
                print(f"\033[90m  {line}\033[0m")

# ── Serial Reader Thread ──────────────────────────────────────────────────────

class ReaderThread(threading.Thread):
    def __init__(self, ser: serial.Serial, engine: PcapEngine):
        super().__init__(daemon=True)
        self.ser    = ser
        self.engine = engine
        self._stop  = threading.Event()

    def stop(self):
        self._stop.set()

    def run(self):
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(512)
            except serial.SerialException:
                break
            if chunk:
                self.engine.feed(chunk)

# ── Progress Printer ──────────────────────────────────────────────────────────

class ProgressThread(threading.Thread):
    def __init__(self, stats: Stats, duration: int | None):
        super().__init__(daemon=True)
        self.stats    = stats
        self.duration = duration
        self._stop    = threading.Event()

    def stop(self):
        self._stop.set()

    def run(self):
        while not self._stop.is_set():
            elapsed = int(time.time() - self.stats.start_time)
            if self.duration:
                remaining = max(0, self.duration - elapsed)
                bar_len = 20
                filled = int(bar_len * elapsed / self.duration)
                bar = "█" * filled + "░" * (bar_len - filled)
                sys.stdout.write(
                    f"\r\033[33m[{bar}]\033[0m "
                    f"{elapsed:3d}s / {self.duration}s  "
                    f"Frames: {self.stats.frames_received}  "
                    f"Files: {self.stats.files_written}"
                    "    "
                )
            else:
                sys.stdout.write(
                    f"\r\033[33m[ running ]\033[0m "
                    f"{elapsed:3d}s elapsed  "
                    f"Frames: {self.stats.frames_received}  "
                    f"Files: {self.stats.files_written}"
                    "    "
                )
            sys.stdout.flush()
            self._stop.wait(1.0)

# ── Main ─────────────────────────────────────────────────────────────────────

def build_sniffer_command(args) -> str:
    mode = args.mode
    if mode == "sniffpmkid":
        cmd = "sniffpmkid"
        if args.channel:
            cmd += f" -c {args.channel}"
        if args.deauth:
            cmd += " -d"
        cmd += " -l"
        return cmd
    base = SNIFFER_COMMANDS[mode]
    if args.channel and mode in ("sniffraw", "sniffbeacon", "sniffdeauth"):
        base += f" -c {args.channel}"
    return base

def main():
    parser = argparse.ArgumentParser(
        description="PCAP capture tool for WiFi Marauder"
    )
    parser.add_argument("--port",     "-p", help="Serial port (auto-detects if omitted)")
    parser.add_argument("--baud",     "-b", type=int, default=BAUD_RATE)
    parser.add_argument("--mode",     "-m", default="sniffraw",
                        choices=list(SNIFFER_COMMANDS.keys()),
                        help="Sniffer mode (default: sniffraw)")
    parser.add_argument("--duration", "-d", type=int, default=None,
                        help="Capture duration in seconds (default: run until Ctrl+C)")
    parser.add_argument("--channel",  "-c", type=int, default=None,
                        help="WiFi channel to capture on (default: all channels)")
    parser.add_argument("--deauth",         action="store_true",
                        help="Send deauth frames during PMKID capture (-d flag)")
    parser.add_argument("--out",      "-o", default="./pcap_captures",
                        help="Output directory for .pcap files (default: ./pcap_captures)")
    parser.add_argument("--verbose",  "-v", action="store_true")
    args = parser.parse_args()

    port = args.port or find_port()
    if not port:
        print("Error: No serial port found. Use --port to specify.")
        sys.exit(1)

    sniffer_cmd = build_sniffer_command(args)

    print(f"\033[1mMarauder PCAP Capture\033[0m  —  Authorized testing only")
    print(f"Port:     {port} @ {args.baud} baud")
    print(f"Mode:     {sniffer_cmd}")
    print(f"Duration: {'∞ (Ctrl+C to stop)' if not args.duration else f'{args.duration}s'}")
    print(f"Output:   {os.path.abspath(args.out)}")
    print()

    try:
        ser = serial.Serial(port, args.baud, timeout=TIMEOUT_SEC)
    except serial.SerialException as e:
        print(f"Failed to open {port}: {e}")
        sys.exit(1)

    stats    = Stats()
    engine   = PcapEngine(args.out, stats, verbose=args.verbose)
    reader   = ReaderThread(ser, engine)
    progress = ProgressThread(stats, args.duration)

    # Graceful shutdown on SIGINT
    def shutdown(sig, frame):
        print("\n\nStopping capture...")
        reader.stop()
        progress.stop()
        ser.write(b"stopscan\n")
        time.sleep(0.5)
        engine.flush()
        ser.close()
        print(f"\n\033[1mCapture complete.\033[0m")
        print(stats.summary())
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)

    reader.start()
    progress.start()

    # Send sniffer command
    time.sleep(0.5)  # let reader settle
    print(f"Starting: {sniffer_cmd}")
    ser.write((sniffer_cmd + "\n").encode())

    if args.duration:
        time.sleep(args.duration)
        shutdown(None, None)
    else:
        # Run until Ctrl+C
        while True:
            time.sleep(1)

if __name__ == "__main__":
    main()
