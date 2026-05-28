#!/usr/bin/env python3
"""
MR60FDA UART sniffer — runs on PC while Flipper is in UART Bridge mode.
Usage:  python3 sniffer.py [/dev/ttyACM0] [--raw]
"""
import sys, serial, time, struct, argparse

HDR = b'\x53\x59'
TAIL = b'\x54\x43'

MOTION_STR = {0: 'none', 1: 'static', 2: 'active'}

class Parser:
    def __init__(self):
        self.reset()

    def reset(self):
        self.state = 'HDR0'
        self.ctrl = self.cmd = 0
        self.length = self.idx = 0
        self.data = bytearray()
        self.acc = 0

    def feed(self, b):
        s = self.state
        if s == 'HDR0':
            if b == 0x53:
                self.acc = b; self.state = 'HDR1'
        elif s == 'HDR1':
            if b == 0x59:
                self.acc += b; self.state = 'CTRL'
            else:
                self.reset(); self.feed(b)
        elif s == 'CTRL':
            self.ctrl = b; self.acc += b; self.state = 'CMD'
        elif s == 'CMD':
            self.cmd = b; self.acc += b; self.state = 'LENH'
        elif s == 'LENH':
            self.length = b << 8; self.acc += b; self.state = 'LENL'
        elif s == 'LENL':
            self.length |= b; self.acc += b
            self.data = bytearray(); self.idx = 0
            if self.length > 64:
                self.reset()
            elif self.length == 0:
                self.state = 'CHK'
            else:
                self.state = 'DATA'
        elif s == 'DATA':
            self.data.append(b); self.acc += b; self.idx += 1
            if self.idx >= self.length:
                self.state = 'CHK'
        elif s == 'CHK':
            if b == (self.acc & 0xFF):
                self.state = 'TAIL0'
            else:
                self.reset(); self.feed(b)
        elif s == 'TAIL0':
            if b == 0x54:
                self.state = 'TAIL1'
            else:
                self.reset(); self.feed(b)
        elif s == 'TAIL1':
            ok = (b == 0x43)
            ctrl, cmd, data = self.ctrl, self.cmd, bytes(self.data)
            self.reset()
            if ok:
                return ctrl, cmd, data
        return None


def decode(ctrl, cmd, data):
    if ctrl == 0x80:
        if cmd == 0x01 and len(data) >= 1:
            return f"Presence : {'YES' if data[0] else 'no'}"
        if cmd == 0x02 and len(data) >= 1:
            return f"Motion   : {MOTION_STR.get(data[0], data[0])}"
        if cmd == 0x03 and len(data) >= 1:
            return f"BSign    : {data[0]}"
        if cmd == 0x0E and len(data) >= 6:
            total = (data[0] << 8) | data[1]
            zones = data[2:6]
            bars = ''.join('█' if z > 128 else ('▄' if z > 0 else '░') for z in zones)
            return f"Height   : tot={total} [{bars}] 0▲200cm"
    elif ctrl == 0x81:
        if cmd == 0x02 and len(data) >= 1:
            return f"BreathRate: {data[0]} /min"
    elif ctrl == 0x83:
        if cmd == 0x01 and len(data) >= 1:
            return f"Fall     : {'*** ALARM ***' if data[0] else 'no'}"
        if cmd == 0x04 and len(data) >= 4:
            secs = struct.unpack('>I', data[:4])[0]
            return f"Residence: {secs}s"
    elif ctrl == 0x85:
        if cmd == 0x02 and len(data) >= 1:
            return f"HeartRate: {data[0]} bpm"
    elif ctrl == 0x01:
        if cmd == 0x01:
            return f"SysHbeat : status=0x{data[0]:02X}" if data else "SysHbeat"
    return f"ctrl=0x{ctrl:02X} cmd=0x{cmd:02X} data={data.hex()}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('port', nargs='?', default='/dev/ttyACM0')
    ap.add_argument('--raw', action='store_true', help='also print raw hex of each frame')
    args = ap.parse_args()

    print(f"Opening {args.port} at 115200 … (Ctrl+C to stop)")
    try:
        ser = serial.Serial(args.port, 115200, timeout=0.1)
    except serial.SerialException as e:
        print(f"ERROR: {e}"); sys.exit(1)

    p = Parser()
    total = bad = 0
    t0 = time.time()
    try:
        while True:
            chunk = ser.read(64)
            for b in chunk:
                result = p.feed(b)
                if result:
                    ctrl, cmd, data = result
                    total += 1
                    ts = time.time() - t0
                    line = f"[{ts:7.2f}s #{total:04d}] {decode(ctrl, cmd, data)}"
                    if args.raw:
                        raw = bytes([0x53,0x59,ctrl,cmd,
                                     len(data)>>8, len(data)&0xFF])
                        raw += data
                        line += f"  | {raw.hex(' ')}"
                    print(line, flush=True)
    except KeyboardInterrupt:
        elapsed = time.time() - t0
        print(f"\n--- {total} frames in {elapsed:.1f}s ---")
        ser.close()

if __name__ == '__main__':
    main()
