#!/usr/bin/env python3
# /// script
# requires-python = ">=3.8"
# dependencies = ["pyserial"]
# ///
"""Measures a Flipper Zero's crystal oscillator against internet time (NTP).

The script saves the result as the calibration in the RFID detector app (1.5 or
later). This is the command-line version of
https://mfcarroll.github.io/rfid-test-tools/flipper/crystal/

    uv run flipper_crystal.py              # or: pipx run flipper_crystal.py
    python3 flipper_crystal.py             # after: pip install pyserial
    python3 flipper_crystal.py COM5 --minutes 15

Plug your Flipper in to USB and close qFlipper. Exit any app that's open on the
Flipper itself. The script finds the Flipper by USB ID. Specify the port if more than
one Flipper is plugged in. The script opens the app on the "Computer calibration"
screen, and shows progress on the Flipper's screen during the run.

When the run finishes, the script prints the result in ppm (+ means the crystal runs
fast). The script also saves the result in the app if the two halves of the run are
consistent, the Flipper's temperature changed by 3 °C or less, and the result is
within ±100 ppm.

Method: the app's `rfid_detector_cal` command reads a 64 MHz timer that runs from the
crystal. The script reads that timer every 2 s, and notes this computer's monotonic
clock immediately before and after each read. NTP gives the monotonic clock's own rate
error over the same time. The script removes that error from the result. The script
queries the NTP server every 16 s and uses the fastest half of the queries based on
round-trip timing to fit the result.
"""

import argparse
import json
import math
import os
import re
import socket
import struct
import sys
import time

import serial
from serial.tools import list_ports

NOMINAL = 64e6  # the app's timer: 64 MHz from the crystal's PLL
COMMAND = b"rfid_detector_cal\r"
# RM installs the detector under RFID; the NFC menu alias uses the same source.
APP_PATH = "/ext/apps/RFID/nfc_rfid_detector.fap"
READ_INTERVAL, NTP_INTERVAL = 2.0, 16.0
SETTLED_SIGMA = 5.0  # ppm, the running estimate is shown once the ± is less than this
NTP_EPOCH = 2208988800  # 1900 -> 1970
CAL_LIMIT = 100.0  # ppm, same limit as the app's manual calibration screen
# The crystal shifts by about this much per °C (measured on one Flipper, with ice packs)
PPM_PER_DEGREE = 0.25
# The result isn't saved if the temperature changed by more than this during a run (about 0.75 ppm)
TEMPERATURE_LIMIT = 3.0  # °C

# now() reads perf_counter (mach_absolute_time on macOS, QueryPerformanceCounter on
# Windows), not time.time(), because the system's time sync can step the wall clock
# mid-run. _BASE shifts the counter onto Unix time, so an NTP offset is milliseconds,
# not the whole date. The NTP fit measures the counter's rate error.
_BASE = time.time() - time.perf_counter()


def now():
    return _BASE + time.perf_counter()


# ---------------------------------------------------------------- Flipper

FLIPPER_USB = (0x0483, 0x5740)  # the Flipper's USB serial (CDC) ID


def find_port():
    ports = [
        port for port in list_ports.comports() if (port.vid, port.pid) == FLIPPER_USB
    ]
    if len(ports) == 1:
        return ports[0].device
    if not ports:
        raise SystemExit(
            "no Flipper found on USB. Plug one in and close qFlipper, or give its port."
        )
    names = "\n  ".join(f"{port.device}  {port.description}" for port in ports)
    raise SystemExit(
        f"more than one Flipper is connected. Give the port of one:\n  {names}"
    )


class Flipper:
    """A session of the app's rfid_detector_cal command. README.md has the protocol.

    The 32-bit count wraps every 67 s, and read() adds the wraps back.
    """

    def __init__(self, port):
        try:
            # exclusive: on macOS and Linux the port isn't locked otherwise
            self.ser = serial.Serial(
                port, 230400, timeout=2, exclusive=True if os.name == "posix" else None
            )
        except serial.SerialException as error:
            raise SystemExit(
                f"couldn't open {port}: {error.strerror or error}\n"
                "Something else has the Flipper's port open. Close qFlipper, any "
                "browser tab connected to the Flipper and any other script, then run "
                "again."
            ) from None
        time.sleep(0.4)
        self.ser.reset_input_buffer()
        self.last, self.hi, self.open = None, 0, False

    def _line(self):
        line = self.ser.readline().decode("utf-8", "replace").strip()
        if not line:
            raise TimeoutError("no reply from the Flipper")
        if line == "STOPPED":  # Cancel on the Flipper, or it left Config
            self.open = False
            raise SystemExit("cancelled on the Flipper. The calibration is unchanged.")
        return line

    def cli(self, cmd, timeout=5):
        """One CLI command: its output up to the ">: " prompt, without the echo."""
        self.ser.reset_input_buffer()
        self.ser.write(cmd.encode() + b"\r")
        out, end = b"", time.monotonic() + timeout
        while b">: " not in out and time.monotonic() < end:
            out += self.ser.read(max(1, self.ser.in_waiting))
        text = out.decode("utf-8", "replace").split(">: ")[0]
        return "\n".join(
            line.strip()
            for line in text.splitlines()
            if line.strip() and line.strip() != cmd
        )

    def open_app(self):
        """Open the RFID detector app on its Computer calibration screen."""
        out = self.cli(f"loader open {APP_PATH} cal")
        if out:
            raise SystemExit(
                f"Flipper: couldn't open the RFID detector app: {out}\n"
                "Exit any app that's open on the Flipper, or open the RFID detector "
                "yourself. Press Left for Config, then choose Calibrate with a "
                "computer."
            )
        time.sleep(1.5)

    def running_app(self):
        """The name of the app open on the Flipper, or None."""
        match = re.search(r'Application "(.*)" is running', self.cli("loader info"))
        return match.group(1) if match else None

    def start(self):
        for attempt in range(2):
            # a CR first ends anything half-typed at the shell prompt
            self.ser.write(b"\r")
            time.sleep(0.3)
            self.ser.reset_input_buffer()
            self.ser.write(COMMAND)
            end = time.monotonic() + 10
            while time.monotonic() < end:
                line = self.ser.readline().decode("utf-8", "replace").strip()
                if line.startswith("ERR"):
                    raise SystemExit(f"Flipper: {line[4:]}")
                if "could not find command" in line:
                    if attempt:
                        raise SystemExit(
                            "Flipper: the RFID detector app is older than 1.5. "
                            "Update it."
                        )
                    # If the RFID detector app is closed, open it and send the
                    # command again.
                    if "detector" not in (self.running_app() or "").lower():
                        self.open_app()
                    break
                if line.startswith("OK lfcal"):
                    self.open = True
                    return int(line.split()[3]) / 10
            else:
                raise TimeoutError("the Flipper didn't start the session")
        raise SystemExit("Flipper: couldn't start the calibration session")

    def read(self):
        before = now()
        self.ser.write(b"r")
        line = self._line()
        after = now()
        try:
            raw, pll = (int(field) for field in line.split())
        except ValueError:
            raise SystemExit(
                f"Flipper: unexpected reply {line!r} (Config screen closed?)"
            ) from None
        if self.last is not None and raw < self.last:
            self.hi += 1
        self.last = raw
        return {
            "t": (before + after) / 2,
            "ticks": (self.hi << 32) | raw,
            "pll": bool(pll),
        }

    def temperature(self):
        """The Flipper's temperature in °C, from its battery gauge."""
        self.ser.write(b"t")
        line = self._line()
        if not line.startswith("T "):
            raise SystemExit(f"Flipper: unexpected reply {line!r} to t")
        return int(line[2:]) / 10

    def progress(self, elapsed, total, ppm=None, sigma=None):
        message = f"p{int(elapsed)} {int(total)}"
        if ppm is not None:
            message += f" {round(ppm * 10)} {max(1, round(sigma * 10))}"
        self.ser.write((message + "\r").encode())

    def send(self, ppm, sigma):
        value = round(ppm * 10)
        self.ser.write(f"s{value} {max(1, round(sigma * 10))}\r".encode())
        self.ser.timeout = 12  # the app replies after saving, within 10 s
        line = self._line()
        self.ser.timeout = 2
        if line.startswith("ERR"):
            raise SystemExit(f"Flipper didn't save the calibration: {line[4:]}")
        if line != f"SET {value}":
            raise RuntimeError(f"Flipper didn't take the value: {line!r}")

    def close(self):
        try:
            if self.open:
                self.ser.write(b"q")
        except serial.SerialException:
            pass  # the Flipper is gone, and its session ended with it
        self.open = False
        self.ser.close()


# ---------------------------------------------------------------- NTP


def ntp_query(address, timeout=2.0):
    """One SNTP query (RFC 4330, with its timestamps T1 to T4).

    Returns (local mid time, server - local offset, round trip), all in seconds.
    """
    family = socket.AF_INET6 if ":" in address[0] else socket.AF_INET
    with socket.socket(family, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        t1 = now()
        sock.sendto(b"\x23" + 47 * b"\0", address)  # LI 0, version 4, mode 3 (client)
        packet, _ = sock.recvfrom(512)
        t4 = now()
    if len(packet) < 48 or packet[1] == 0:  # stratum 0: e.g. rate limited
        raise RuntimeError(f"server refused ({packet[12:16]!r})")

    def timestamp(field_offset):
        seconds, fraction = struct.unpack(
            "!II", packet[field_offset : field_offset + 8]
        )
        return seconds - NTP_EPOCH + fraction / 2**32

    t2, t3 = timestamp(32), timestamp(40)
    return (t1 + t4) / 2, ((t2 - t1) + (t3 - t4)) / 2, (t4 - t1) - (t3 - t2)


# ---------------------------------------------------------------- fits


def linfit(x, y):
    """Least squares y = slope x + intercept.

    Returns (slope, intercept, residuals, sigma of the slope).
    """
    count = len(x)
    mean_x, mean_y = sum(x) / count, sum(y) / count
    sxx = sum((xi - mean_x) ** 2 for xi in x)
    slope = sum((xi - mean_x) * (yi - mean_y) for xi, yi in zip(x, y)) / sxx
    intercept = mean_y - slope * mean_x
    residuals = [yi - (slope * xi + intercept) for xi, yi in zip(x, y)]
    if count > 2:
        slope_sigma = math.sqrt(
            sum(residual**2 for residual in residuals) / (count - 2) / sxx
        )
    else:
        slope_sigma = float("nan")
    return slope, intercept, residuals, slope_sigma


def median(values):
    ordered = sorted(values)
    middle = len(ordered) // 2
    return (
        ordered[middle]
        if len(ordered) % 2
        else (ordered[middle - 1] + ordered[middle]) / 2
    )


def robust_fit(x, y):
    """Least squares, refit once without points more than 5 MADs off the line.

    Returns (slope, sigma of the slope, points kept, points).
    """
    slope, _, residuals, slope_sigma = linfit(x, y)
    center = median(residuals)
    mad = median([abs(residual - center) for residual in residuals]) or 1e-12
    keep = [abs(residual - center) <= 5 * 1.4826 * mad for residual in residuals]
    if len(x) >= 4 and 3 <= sum(keep) < len(x):
        kept_x = [xi for xi, kept in zip(x, keep) if kept]
        kept_y = [yi for yi, kept in zip(y, keep) if kept]
        slope, _, residuals, slope_sigma = linfit(kept_x, kept_y)
        return slope, slope_sigma, len(kept_x), len(x)
    return slope, slope_sigma, len(x), len(x)


# A network path's delay drifts by a few tenths of a millisecond over minutes. The drift
# tilts the NTP fit but barely adds to the scatter, so the fit ± misses the drift.
# with_path_floor adds PATH_FLOOR at each end of the fitted span, as a root-sum-square
# with the fit's own ±. Without the floor, ordinary drift fails the halves check.
PATH_FLOOR = 0.3e-3


def with_path_floor(slope_sigma, times):
    """The NTP fit's slope sigma, with PATH_FLOOR added for the span of times."""
    return math.hypot(
        slope_sigma, math.sqrt(2) * PATH_FLOOR / (max(times) - min(times))
    )


def fastest_half(ntp_samples):
    """The NTP samples with the shorter half of the round trips."""
    cut = sorted(round_trip for _, _, round_trip in ntp_samples)[len(ntp_samples) // 2]
    return [sample for sample in ntp_samples if sample[2] <= cut]


def crystal_ppm(tick_rate, tick_rate_sigma, clock_drift, clock_drift_sigma):
    """(ppm, sigma) of the crystal.

    From the Flipper's ticks per local second and the local clock's drift.
    """
    ppm = (tick_rate / (1 + clock_drift) / NOMINAL - 1) * 1e6
    return ppm, math.hypot(tick_rate_sigma / NOMINAL, clock_drift_sigma) * 1e6


def quote(value, sigma):
    """ "+16.2 ± 0.7 ppm". Both numbers go to the ±'s first significant digit, or to its
    first two when that digit is 1, and never finer than 0.1 ppm (the app's resolution).
    """
    decimals = 1
    if sigma > 0:
        exponent = math.floor(math.log10(sigma))
        decimals = -exponent + (1 if sigma / 10**exponent < 2 else 0)
        decimals = max(0, min(1, decimals))
    return f"{value:+.{decimals}f} ± {sigma:.{decimals}f} ppm"


def fit_run(reads, ntp_samples):
    """The crystal from the reads and NTP samples so far, or None if there are too few.

    Returns a dict with:
    - ppm and sigma
    - tick_rate: Flipper ticks per local second
    - rms_us: the reads' scatter, which shows USB stalls and laptop sleep
    - clock_drift: the local clock against NTP, fitted through the fastest half of the
      queries (+ means the local clock runs slow)
    - kept, query_times and query_offsets: that fit's points
    - min_round_trip
    """
    if len(reads) < 10 or len(ntp_samples) < 4:
        return None
    elapsed = [read_time - reads[0][0] for read_time, _ in reads]
    ticks = [read_ticks - reads[0][1] for _, read_ticks in reads]
    tick_rate, tick_rate_sigma, _, _ = robust_fit(elapsed, ticks)
    residuals = linfit(elapsed, ticks)[2]
    rms_ticks = math.sqrt(sum(residual**2 for residual in residuals) / len(elapsed))
    fast = fastest_half(ntp_samples)
    if len(fast) < 3:
        return None
    query_times = [mid - ntp_samples[0][0] for mid, _, _ in fast]
    query_offsets = [offset for _, offset, _ in fast]
    clock_drift, clock_drift_sigma, kept, _ = robust_fit(query_times, query_offsets)
    clock_drift_sigma = with_path_floor(clock_drift_sigma, query_times)
    ppm, sigma = crystal_ppm(tick_rate, tick_rate_sigma, clock_drift, clock_drift_sigma)
    return {
        "ppm": ppm,
        "sigma": sigma,
        "tick_rate": tick_rate,
        "rms_us": rms_ticks / tick_rate * 1e6,
        "clock_drift": clock_drift,
        "kept": kept,
        "query_times": query_times,
        "query_offsets": query_offsets,
        "min_round_trip": min(round_trip for _, _, round_trip in ntp_samples),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument(
        "port",
        nargs="?",
        help="the Flipper's serial port (default: find it by its USB ID)",
    )
    parser.add_argument(
        "--minutes",
        type=float,
        default=30,
        help="how long to measure. 30 by default (15 gives about ±0.5 ppm)",
    )
    parser.add_argument(
        "--server", default="time.cloudflare.com", help="the NTP server, looked up once"
    )
    parser.add_argument("--log", help="write every sample to this JSON-lines file")
    parser.add_argument(
        "--no-send",
        action="store_true",
        help="print the result without saving it on the Flipper",
    )
    args = parser.parse_args()

    try:
        address = socket.getaddrinfo(args.server, 123, type=socket.SOCK_DGRAM)[0][4][:2]
    except OSError as error:
        raise SystemExit(f"couldn't look up {args.server}: {error}") from None
    port = args.port or find_port()
    flipper = Flipper(port)
    log = None
    if args.log:
        # Line-buffered, which writes each sample as it comes. The log then holds every
        # sample if the run stops early. The finally below closes it.
        log = open(args.log, "w", buffering=1)  # noqa: SIM115
    reads, ntp_samples, temperatures = [], [], []
    try:
        saved = flipper.start()
        print(
            f"Flipper on {port}, calibration now {saved:+.1f} ppm, "
            f"NTP {args.server} ({address[0]}), {args.minutes:g} min"
        )
        reading = flipper.read()
        start = reading["t"]
        end = start + args.minutes * 60
        next_ntp = start
        while True:
            loop_start = now()
            if loop_start >= next_ntp:
                try:
                    sample = ntp_query(address)
                    ntp_samples.append(sample)
                    if log:
                        log.write(json.dumps({"ntp": list(sample)}) + "\n")
                except (OSError, RuntimeError) as error:
                    print(f"\n  NTP query failed: {error}")
                temperatures.append((now(), flipper.temperature()))
                if log:
                    log.write(json.dumps({"temp": list(temperatures[-1])}) + "\n")
                next_ntp += NTP_INTERVAL
            if not reading["pll"]:
                raise SystemExit(
                    "the Flipper's clock left the crystal during the run. Run again."
                )
            reads.append((reading["t"], reading["ticks"]))
            if log:
                log.write(
                    json.dumps({"flipper": [reading["t"], reading["ticks"]]}) + "\n"
                )
            if reading["t"] >= end:
                break
            fit = fit_run(reads, ntp_samples)
            settled = fit is not None and fit["sigma"] < SETTLED_SIGMA
            if settled:
                flipper.progress(
                    reading["t"] - start, args.minutes * 60, fit["ppm"], fit["sigma"]
                )
            else:
                flipper.progress(reading["t"] - start, args.minutes * 60)
            elapsed = int(reading["t"] - start)
            status = (
                f"\r  {elapsed // 60:2d}:{elapsed % 60:02d} / {args.minutes:g} min  "
            )
            status += quote(fit["ppm"], fit["sigma"]) if settled else "settling"
            if fit:
                status += (
                    f"  computer clock {-fit['clock_drift'] * 1e6:+.1f} ppm"
                    f"  reads {fit['rms_us'] / 1e3:.1f} ms rms"
                    f"  NTP round trip {fit['min_round_trip'] * 1e3:.0f} ms"
                )
            status += f"  {temperatures[-1][1]:.1f} °C"
            print(f"{status:<110}", end="", flush=True)
            time.sleep(max(0.0, READ_INTERVAL - (now() - loop_start)))
            reading = flipper.read()
        print()
        fit = fit_run(reads, ntp_samples)
        if fit is None or len(ntp_samples) < 12:
            raise SystemExit("too short to fit")
        if fit["rms_us"] > 20000:
            raise SystemExit(
                f"reads scatter {fit['rms_us'] / 1e3:.0f} ms rms. "
                "The computer probably slept. Run again."
            )

        # The halves check fits each half of the run on its own and compares the two
        # crystal figures. The local clock's rate can change between the halves. If that
        # happens, the Flipper's rate against the local clock moves the opposite way.
        query_times, query_offsets = fit["query_times"], fit["query_offsets"]
        half = len(query_times) // 2
        split = ntp_samples[0][0] + query_times[half]
        halves = []
        for times, offsets, in_half in (
            (query_times[: half + 1], query_offsets[: half + 1], lambda t: t <= split),
            (query_times[half:], query_offsets[half:], lambda t: t >= split),
        ):
            drift, drift_sigma, _, _ = robust_fit(times, offsets)
            drift_sigma = with_path_floor(drift_sigma, times)
            half_reads = [read for read in reads if in_half(read[0])]
            rate, rate_sigma, _, _ = robust_fit(
                [read_time - half_reads[0][0] for read_time, _ in half_reads],
                [read_ticks - half_reads[0][1] for _, read_ticks in half_reads],
            )
            halves.append((*crystal_ppm(rate, rate_sigma, drift, drift_sigma), drift))
        (
            (first_ppm, first_sigma, first_drift),
            (second_ppm, second_sigma, second_drift),
        ) = halves

        ppm, sigma = fit["ppm"], fit["sigma"]
        print(f"crystal          {quote(ppm, sigma)}   (+ = fast)")
        print(
            f"  uncorrected    {(fit['tick_rate'] / NOMINAL - 1) * 1e6:+.1f} ppm "
            "vs this computer's clock"
        )
        print(
            f"  computer clock {-fit['clock_drift'] * 1e6:+.1f} ppm vs NTP (fastest "
            f"{fit['kept']} of {len(ntp_samples)} queries, min round trip "
            f"{fit['min_round_trip'] * 1e3:.1f} ms)"
        )
        halves_agree = abs(second_ppm - first_ppm) <= 3 * math.hypot(
            first_sigma, second_sigma
        )
        print(
            f"  halves         {first_ppm:+.1f} then {second_ppm:+.1f} ppm "
            f"(computer clock {-first_drift * 1e6:+.1f} then "
            f"{-second_drift * 1e6:+.1f}): "
            + (
                "ok"
                if halves_agree
                else "DISAGREE (path to the server changed?): re-run"
            )
        )
        print(f"  Flipper reads  {len(reads)}, {fit['rms_us']:.0f} us rms")
        celsius = [reading for _, reading in temperatures]
        change = max(celsius) - min(celsius)
        print(
            f"  temperature    {celsius[0]:.1f} -> {celsius[-1]:.1f} °C "
            f"(range {min(celsius):.1f}-{max(celsius):.1f})"
        )
        if change > TEMPERATURE_LIMIT:
            print(
                f"  ⚠ the Flipper's temperature changed by {change:.1f} °C during the "
                f"run, which can move the result by about "
                f"{change * PPM_PER_DEGREE:.1f} ppm"
            )

        if abs(ppm) > CAL_LIMIT:
            print(f"  {ppm:+.1f} ppm is beyond the app's ±{CAL_LIMIT:g}: not saved")
            return 1
        elif not halves_agree:
            print("  not saved on the Flipper: the halves check failed. Run again")
            return 1
        elif change > TEMPERATURE_LIMIT:
            print(
                "  not saved on the Flipper: its temperature changed. Run again once "
                "its temperature is steady"
            )
            return 1
        elif not args.no_send:
            flipper.send(ppm, sigma)
            print(
                f"  saved on the Flipper: {quote(ppm, sigma)} (it was {saved:+.1f} ppm)"
            )
    except KeyboardInterrupt:
        print("\n  stopped")
        return 1
    except TimeoutError as error:
        raise SystemExit(f"\n{error}. Run again.") from None
    except serial.SerialException:
        raise SystemExit("\nlost the connection to the Flipper. Run again.") from None
    finally:
        flipper.close()
        if log:
            log.close()


if __name__ == "__main__":
    sys.exit(main())
