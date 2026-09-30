#!/usr/bin/env python3
"""Capture real Flipper Zero screens and recordings over the serial RPC session.

Every picture in this repository comes from the device. There is no mockup
renderer any more: the drawings it produced were a second implementation of the
UI that could - and did - disagree with the firmware while looking perfectly
convincing (see docs/changelog.md, 3.0.1). A capture cannot.

    python3 tools_screenshot.py --all          # drive Specter, capture every screen
    python3 tools_screenshot.py --shot NAME    # grab whatever is on screen now
    python3 tools_screenshot.py --record NAME  # animated GIF of the live screen
    python3 tools_screenshot.py --tour-gif     # one GIF of a scripted walk
    python3 tools_screenshot.py --sheet        # rebuild images/screens.png
    python3 tools_screenshot.py --catalog      # refresh screenshots/ssN.png aliases

The Flipper's CLI has no screenshot command on current firmware, but the
protobuf RPC session exposes the framebuffer and an input injector, so this
drives the app and grabs frames from the device itself.

Only four message shapes are needed, so the protobuf is encoded by hand rather
than pulling in a generated stub. From flipper.proto:

    Main { command_id = 1, command_status = 2, has_next = 3, oneof content }
      16 = App.StartRequest { string name = 1, string args = 2 }
      19 = StopSession {}
      20 = Gui.StartScreenStreamRequest {}
      21 = Gui.StopScreenStreamRequest {}
      22 = Gui.ScreenFrame { bytes data = 1 }
      23 = Gui.SendInputEventRequest { key = 1, type = 2 }

Screens are written at 4x (512x256) in the device's own two colours, which is
both what the Flipper app catalog accepts and what the branding rules require -
tools_gen_banner.py refuses to draw an orange it did not sample from one of
these files.

Requires: python3 -m pip install pyserial pillow
"""
import argparse
import glob
import os
import shutil
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required:  python3 -m pip install pyserial")
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, "screenshots")
IMAGES = os.path.join(HERE, "images")

# The device's two colours, and the only two allowed in screenshots/. The
# banner samples its orange out of these files rather than naming one, so a
# third colour here would quietly break the branding asserts.
LCD_LIT = (254, 138, 44)  # backlight through an unset pixel
LCD_INK = (0, 0, 0)  # a set pixel

FAP_PATH = "/ext/apps/NFC/specter.fap"

KEY = {"up": 0, "down": 1, "right": 2, "left": 3, "ok": 4, "back": 5}
TYPE = {"press": 0, "release": 1, "short": 2, "long": 3, "repeat": 4}

F_APP_START = 16
F_STOP_SESSION = 19
F_START_STREAM = 20
F_STOP_STREAM = 21
F_SCREEN_FRAME = 22
F_INPUT = 23


# ---------------------------------------------------------------- protobuf ---
def varint(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            return bytes(out)


def read_varint(buf, i):
    shift = val = 0
    while True:
        if i >= len(buf):
            return None, i
        b = buf[i]
        i += 1
        val |= (b & 0x7F) << shift
        if not b & 0x80:
            return val, i
        shift += 7


def tag(field, wire=2):
    return varint((field << 3) | wire)


# ------------------------------------------------------------------ device ---
def find_port(explicit=None):
    if explicit:
        return explicit
    found = sorted(glob.glob("/dev/cu.usbmodemflip_*")) or sorted(
        glob.glob("/dev/ttyACM*")
    )
    if not found:
        sys.exit(
            "No Flipper found on USB.\n"
            "Plug it in and make sure nothing else holds the port "
            "(qFlipper and the Flipper mobile app both do)."
        )
    return found[0]


class Flipper:
    def __init__(self, port=None):
        self.s = serial.Serial(find_port(port), timeout=1.0)
        self.buf = b""
        self.cmd = 0
        self._to_cli()
        # Terminate with a bare CR and consume the echo *exactly*. Sending
        # "\r\n" leaves a stray newline in the device's input buffer, and the
        # RPC decoder reads it as the start of a frame (0x0a = "a 10-byte
        # message follows"), swallowing the first real request and leaving the
        # session permanently desynced - every reply comes back ERROR_DECODE.
        self.s.write(b"start_rpc_session\r")
        self.s.read_until(b"start_rpc_session\r\n")

    # -- session plumbing --
    def _drain(self, seconds=0.5):
        out = b""
        t0 = time.time()
        while time.time() - t0 < seconds:
            if self.s.in_waiting:
                out += self.s.read(self.s.in_waiting)
            else:
                time.sleep(0.03)
        return out

    def _to_cli(self):
        """Get back to a CLI prompt from any state.

        A session left open by an earlier run swallows plain text, so the first
        move is always to close one that may or may not exist.
        """
        for _ in range(4):
            self.send(F_STOP_SESSION)
            self._drain(0.4)
            self.s.write(b"\r")
            if b">:" in self._drain(0.6):
                self._drain(0.2)
                return
        sys.exit("Could not get the Flipper back to a CLI prompt - try replugging it.")

    def send(self, field, body=b""):
        self.cmd += 1
        msg = tag(1, 0) + varint(self.cmd) + tag(field) + varint(len(body)) + body
        self.s.write(varint(len(msg)) + msg)

    def _next_message(self, timeout=2.0):
        t0 = time.time()
        while time.time() - t0 < timeout:
            ln, i = read_varint(self.buf, 0)
            if ln is not None and len(self.buf) >= i + ln:
                msg = self.buf[i : i + ln]
                self.buf = self.buf[i + ln :]
                return msg
            if self.s.in_waiting:
                self.buf += self.s.read(self.s.in_waiting)
            else:
                time.sleep(0.02)
        return None

    # -- screen --
    def start_stream(self):
        self.send(F_START_STREAM)
        time.sleep(0.3)

    def stop_stream(self):
        self.send(F_STOP_STREAM)

    def flush(self):
        """Throw away frames already in flight.

        The device pushes a frame on every redraw, so by the time a key press
        has been delivered there are several older frames queued. Without this a
        capture returns the screen as it was one or two steps ago.

        Drain whole MESSAGES, never raw bytes: clearing the byte buffer
        mid-message throws away a length prefix, everything after it is parsed
        at the wrong offset, and the stream never resynchronises.

        Drain until nothing is actually pending rather than for a fixed slice of
        time. An animating screen pushes a frame every ~100 ms, so a bounded
        drain on one of those returns with the backlog still queued - and the
        very next "capture" is then a frame from before the key press. That is
        not theoretical: it is what made a Site Survey verdict shot come back as
        a fresh running screen, twice, and it looked exactly like an app bug.
        """
        deadline = time.time() + 5.0
        while time.time() < deadline:
            if self.s.in_waiting or self.buf:
                if self._await_frame(0.05) is None and not self.s.in_waiting:
                    return
                continue
            return

    def idle(self, seconds, on_frame=None):
        """Let time pass WITHOUT letting the stream back up.

        Never plain-sleep while the screen stream is running. The device pushes
        a ~1 KB frame on every redraw, so a 12-second wait on an animating
        screen queues well over a hundred kilobytes - far past the host's serial
        buffer, which then drops bytes and desynchronises the protocol for the
        rest of the session. Reading and discarding as we wait keeps the stream
        healthy and keeps "now" actually meaning now.
        """
        end = time.time() + seconds
        while time.time() < end:
            data = self._await_frame(min(0.4, max(end - time.time(), 0.05)))
            if data is not None and on_frame is not None:
                on_frame(data)

    def frame(self, timeout=3.0, nudge=True):
        """The 1024-byte framebuffer from the next ScreenFrame message.

        The device only pushes a frame when the screen actually *redraws*.
        Specter's four measurement views animate on a 100 ms tick so they stream
        happily, but the menus, Settings and the logbook are static and would
        otherwise time out with nothing sent. `nudge` walks the selection down
        and back up, which forces two repaints and lands on the row it started
        from - non-destructive on a submenu, a variable item list and a
        scrolling text box alike.
        """
        data = self._await_frame(timeout)
        if data is None and nudge:
            # Press with settle=0 and read the redraw straight back. A settling
            # press now DRAINS the stream, so nudging with one would swallow the
            # very repaint it exists to provoke - which reads as "this screen
            # never sends a frame" on exactly the static screens that need it.
            self.press("down", settle=0.0)
            self._await_frame(1.5)
            self.press("up", settle=0.0)
            data = self._await_frame(timeout)
        if data is None:
            return None
        # Keep draining briefly and return the NEWEST frame. The first frame to
        # arrive is often mid-transition - after a nudge it is the screen with
        # the selection moved down, before the matching "up" has been drawn.
        deadline = time.time() + 0.45
        while time.time() < deadline:
            newer = self._await_frame(0.25)
            if newer is None:
                break
            data = newer
        return data

    def _await_frame(self, timeout):
        t0 = time.time()
        while time.time() - t0 < timeout:
            msg = self._next_message(1.2)
            if msg is None:
                continue
            i = 0
            while i < len(msg):
                fw, i = read_varint(msg, i)
                if fw is None:
                    break
                fnum, wire = fw >> 3, fw & 7
                if wire == 0:
                    v, i = read_varint(msg, i)
                    if v is None:
                        break
                elif wire == 2:
                    ln, i = read_varint(msg, i)
                    if ln is None:
                        break
                    body, i = msg[i : i + ln], i + ln
                    if fnum == F_SCREEN_FRAME:
                        j = 0
                        while j < len(body):
                            fw2, j = read_varint(body, j)
                            if fw2 is None:
                                break
                            f2, w2 = fw2 >> 3, fw2 & 7
                            if w2 == 2:
                                l2, j = read_varint(body, j)
                                if l2 is None:
                                    break
                                data, j = body[j : j + l2], j + l2
                                if f2 == 1:
                                    return data
                            elif w2 == 0:
                                _, j = read_varint(body, j)
                            else:
                                break
                else:
                    break
        return None

    # -- input --
    def app_start(self, path, args_str=""):
        """App.StartRequest { string name = 1; string args = 2 }.

        Launching this way rather than through `loader open` on the CLI means
        the screen stream stays up across the launch, which is the only way to
        catch the first frames an app paints.
        """
        name = path.encode()
        body = tag(1) + varint(len(name)) + name
        if args_str:
            a = args_str.encode()
            body += tag(2) + varint(len(a)) + a
        self.send(F_APP_START, body)
        time.sleep(0.15)

    def _key(self, name, kind):
        body = tag(1, 0) + varint(KEY[name]) + tag(2, 0) + varint(TYPE[kind])
        self.send(F_INPUT, body)

    def press(self, name, long=False, settle=0.45):
        """A real press is press -> short|long -> release, same as the hardware."""
        self._key(name, "press")
        time.sleep(0.4 if long else 0.03)
        self._key(name, "long" if long else "short")
        time.sleep(0.03)
        self._key(name, "release")
        # Settle by DRAINING, not by sleeping: a press on an animating screen
        # is exactly where a backlog would start. Callers that want the frame
        # this press paints pass settle=0 and read it themselves.
        self.idle(settle)

    def close(self):
        try:
            self.stop_stream()
            time.sleep(0.2)
            self.send(F_STOP_SESSION)
            time.sleep(0.2)
        finally:
            self.s.close()


# ------------------------------------------------------------------- image ---
def to_image(data):
    """1024 bytes: 8 pages of 128 columns, LSB = topmost pixel of the page."""
    img = Image.new("1", (128, 64), 1)
    px = img.load()
    for i, byte in enumerate(data[:1024]):
        x, page = i % 128, i // 128
        for bit in range(8):
            if byte & (1 << bit):
                px[x, page * 8 + bit] = 0
    return img


def amber(img, scale=1):
    """Render a 1-bit frame in the device's own two colours.

    A plain black-on-white capture does not look like the thing in your hand:
    the panel is a monochrome LCD behind an orange backlight, so an unset pixel
    is lit and a set pixel is dark.
    """
    out = Image.new("RGB", (128, 64), LCD_LIT)
    px, src = out.load(), img.convert("1").load()
    for y in range(64):
        for x in range(128):
            if src[x, y] == 0:
                px[x, y] = LCD_INK
    if scale > 1:
        out = out.resize((128 * scale, 64 * scale), Image.NEAREST)
    return out


def save(img, name, scale=4):
    """Write screenshots/<name>.png at 4x in the device's two colours."""
    os.makedirs(SHOTS, exist_ok=True)
    path = os.path.join(SHOTS, f"{name}.png")
    amber(img, scale).save(path)
    print(f"  saved screenshots/{name}.png  ({128 * scale}x{64 * scale})")
    return path


def ink_fraction(img, top=0, bottom=64):
    px = img.load()
    rows = max(bottom - top, 1)
    return sum(1 for y in range(top, bottom) for x in range(128) if px[x, y] == 0) / (
        128.0 * rows
    )


# ----------------------------------------------------------------- carrier ---
def read_carrier(img):
    """Recover the raw carrier from a Fingerprint frame. Returns (bits, hi, lo).

    The same trick tools_brand_data.py uses on a saved file, done in memory so a
    take can be SCORED while it is still being recorded. fingerprint_view.c's
    draw_trace() puts a dot per column at the carrier's level and a full
    HI..LO vertical on every level CHANGE, so the middle row is inked by edges
    and by nothing else: level is an exact XOR chain along it.

    Sampling the HI row directly does not work - falling edges cross it too.
    Returns (None, 0, 0) if this is not a Fingerprint screen.
    """
    px = img.load()

    def ink(x, y):
        return px[x, y] == 0

    cnt = {y: sum(ink(x, y) for x in range(128)) for y in range(40, 64)}
    full_rows = [y for y in cnt if cnt[y] == 128]
    if not full_rows:
        return None, 0, 0
    div = max(full_rows)
    band = [y for y in range(div + 1, 64) if cnt[y] > 0]
    if not band:
        return None, 0, 0
    hi, lo = min(band), max(band)
    if lo - hi < 4:
        return None, 0, 0
    mid = (hi + lo) // 2

    level = [ink(0, hi)]
    for i in range(1, 128):
        level.append(level[-1] ^ ink(i, mid))
    return [1 if v else 0 for v in level], hi, lo


def carrier_quality(img):
    """How much cadence a Fingerprint frame actually carries.

    Score is the edge count: a frame where the reader polled steadily for the
    whole ~1 s trace window has many edges, while one caught during a gap is
    mostly a flat line however good the rest of the screen looks. This is the
    difference between a capture that can serve as the brand's measured
    waveform and one that cannot - and it is not something you can judge by
    eye at 128x64.
    """
    bits, _hi, _lo = read_carrier(img)
    if bits is None:
        return -1, 0.0
    edges = sum(1 for i in range(1, len(bits)) if bits[i] != bits[i - 1])
    duty = 100.0 * sum(bits) / len(bits)
    return edges, duty


# ------------------------------------------------------------------ verify ---
def two_colours_only(path):
    """The branding law, checked rather than trusted."""
    cols = {c for _, c in Image.open(path).convert("RGB").getcolors(1 << 20)}
    return cols <= {LCD_LIT, LCD_INK}


# ------------------------------------------------------------------- sheet ---
# The sheet ships in BOTH themes, served by <picture>, exactly as the banner
# does. A single light sheet is a glaring white slab in the middle of a dark
# README - and a single dark one is the same problem inverted. The device
# captures inside it are untouched either way: they are already the only two
# colours they are allowed to be.
PAPER = (243, 241, 236)
INK = (22, 19, 13)
MUTED = (87, 82, 74)
RULE = (217, 212, 200)

BLACK = (0, 0, 0)
ASH = (169, 162, 154)
SCORE = (42, 39, 36)

SHEET_THEMES = {
    "light": dict(ground=PAPER, label=MUTED, rule=RULE, suffix=""),
    "dark": dict(ground=BLACK, label=ASH, rule=SCORE, suffix="-dark"),
}


def _sheet_font(size, mono=True):
    names = (
        ["/System/Library/Fonts/Supplemental/Andale Mono.ttf"]
        if mono
        else ["/System/Library/Fonts/Supplemental/Futura.ttc"]
    )
    for n in names:
        try:
            return ImageFont.truetype(n, size)
        except OSError:
            continue
    return ImageFont.load_default()


def contact_sheet(names, cols=3, scale=2):
    """Compose captured shots into images/screens{,-dark}.png for the README."""
    tiles = []
    for name, label in names:
        p = os.path.join(SHOTS, f"{name}.png")
        if not os.path.exists(p):
            print(f"  (skipping {name} - not captured)")
            continue
        tiles.append((Image.open(p).convert("RGB"), label))
    if not tiles:
        sys.exit("No screenshots to compose - run --all first.")

    tw, th = 128 * scale, 64 * scale
    pad, cap, gut = 22, 30, 22
    rows = (len(tiles) + cols - 1) // cols
    W = pad * 2 + cols * tw + (cols - 1) * gut
    H = pad * 2 + rows * (th + cap) + (rows - 1) * gut

    os.makedirs(IMAGES, exist_ok=True)
    f = _sheet_font(13)

    for theme in SHEET_THEMES.values():
        sheet = Image.new("RGB", (W, H), theme["ground"])
        d = ImageDraw.Draw(sheet)
        for i, (tile, label) in enumerate(tiles):
            r, c = divmod(i, cols)
            x = pad + c * (tw + gut)
            y = pad + r * (th + cap + gut)
            d.rectangle([x - 1, y - 1, x + tw, y + th], outline=theme["rule"])
            sheet.paste(tile.resize((tw, th), Image.NEAREST), (x, y))
            d.text((x, y + th + 9), label, font=f, fill=theme["label"])
        out = os.path.join(IMAGES, f"screens{theme['suffix']}.png")
        sheet.save(out)
        print(f"wrote {os.path.relpath(out, HERE)}  ({W}x{H}, {len(tiles)} screens)")


# ------------------------------------------------------------------- drive ---
def restart_app(f, fap=FAP_PATH):
    """Put the device in a known state: a freshly launched Specter, main menu.

    The tour navigates by relative key presses, so it only lines up if it starts
    from a known screen with a known menu cursor.
    """
    for _ in range(7):
        f.press("back", settle=0.2)
    f.idle(0.5)
    f.flush()
    f.app_start(fap)
    # The launch request returns before the app paints, so the first frames off
    # the wire are still the Flipper desktop. Counting frames is unreliable -
    # how many arrive depends on how busy the device is - so discard by content
    # instead: the desktop's dolphin art is a large dark scene, while every
    # Specter screen is mostly unlit.
    for _ in range(25):
        d = f._await_frame(0.4)
        if d is not None and ink_fraction(to_image(d)) < 0.40:
            break
    # Then sit out the boot intro. Content alone cannot tell the intro from the
    # menu - both are mostly unlit - and the intro even goes fully INVERTED for
    # its contact beat, which an ink test would read as "still the desktop".
    # SPLASH_DONE_TICKS is 22 at a 100 ms tick, so 2.2 s plus a margin.
    f.idle(2.8)
    f.flush()


def alarm_strip_lit(img):
    """True when the sweep's bottom strip is inverted, i.e. a reader is locked on.

    sweep_view.c fills rows 53..63 solid and knocks the text out in white, so a
    majority-ink bottom band is a reliable signal that the alarm state is on
    screen - no need to guess at timing. Watch mode's ACTIVE READER band sits
    higher up, so it gets its own test.
    """
    return ink_fraction(img, STRIP_TOP, 64) > 0.55


STRIP_TOP = 53


def watch_band_lit(img):
    """Watch mode's inverted READER band lives just under the header."""
    return ink_fraction(img, 14, 28) > 0.55


def wait_for_reader(f, test=alarm_strip_lit, seconds=45, what="a reader"):
    """Sit on the current screen until the device actually senses a carrier.

    Waiting for the real thing beats staging one: the strip only inverts when
    the detector says present, so whatever this catches is a genuine detection.
    """
    print(f"  >>> hold the Flipper's NFC side against {what} (up to {seconds}s) ...")
    deadline = time.time() + seconds
    tick = time.time() + 2.0
    while time.time() < deadline:
        data = f.frame(timeout=1.5, nudge=False)
        if data is None:
            continue
        img = to_image(data)
        if test(img):
            print("      got it")
            return img
        if time.time() >= tick:
            print(
                "      nothing yet - back of the Flipper flat on the reader", flush=True
            )
            tick = time.time() + 2.0
    print("  !! no detection seen")
    return None


def capture_survey_verdict(f, walk_seconds=13.0):
    """Run a Site Survey to a real verdict and capture the card.

    Two things make this screen awkward, and both are worth stating because
    they are easy to rediscover as "bugs in the app":

    * A survey shorter than SPECTER_SURVEY_MIN_CLEAN_MS is graded TOO SHORT
      rather than CLEAN, deliberately - so the walk has to actually last.
    * The verdict card is completely static. It is painted once, by the OK
      press that ends the survey, and never redrawn. So the backlog has to be
      cleared BEFORE the press and the frame caught immediately after it;
      flushing afterwards throws away the only frame there will ever be, and
      frame()'s nudge cannot help because the card ignores UP and DOWN.
    """
    print(f"  walking the survey for {walk_seconds:.0f}s ...")
    f.idle(walk_seconds)
    f.flush()
    f.press("ok", settle=0.0)
    data = f._await_frame(3.0)
    if data is None:
        print("  !! no verdict frame")
        return None
    img = to_image(data)
    save(img, "survey_done")
    return img


def settle_shot(f, name, wait=0.7):
    f.idle(wait)
    f.flush()
    data = f.frame()
    if data is None:
        print(f"  !! no frame for {name}")
        return None
    img = to_image(data)
    save(img, name)
    return img


# Each step: (name, caption, keys pressed to get there from the previous shot).
# The start scene remembers the row you activated, so BACK returns the cursor to
# it and exactly one DOWN reaches the next entry - not a running count.
TOUR = [
    ("menu", "Main menu", []),
    ("sweep_idle", "Sweep - listening", [("ok", False)]),
    ("sweep_reader", "Sweep - reader found", None),  # None = wait for a real detection
    (
        "fingerprint",
        "Fingerprint - cadence",
        [("back", False), ("down", False), ("ok", False)],
    ),
    (
        "survey_run",
        "Site Survey - walking",
        [("back", False), ("down", False), ("ok", False)],
    ),
    ("survey_done", "Site Survey - verdict", None),
    (
        "watch_quiet",
        "Watch Mode - standing guard",
        [("back", False), ("down", False), ("ok", False)],
    ),
    (
        "logbook",
        "Logbook - findings",
        [("back", False), ("down", False), ("ok", False), ("ok", False)],
    ),
    (
        "settings",
        "Settings",
        [("back", False), ("back", False), ("down", False), ("ok", False)],
    ),
    ("about", "Help & About", [("back", False), ("down", False), ("ok", False)]),
]

# What goes on the README contact sheet, in reading order.
# The COMPLEMENT of the per-mode stills in the README, deliberately. The four
# mode sections each already carry their own hero capture in context, and a
# sheet that repeats them is just the same screens twice on one page. These are
# the ones no mode section owns: the quiet states, the intro, and the key map.
SHEET = [
    ("splash", "Boot intro"),
    ("menu", "Main menu"),
    ("sweep_idle", "Sweep - listening"),
    ("fingerprint", "Fingerprint - nothing there"),
    ("survey_run", "Site Survey - walking"),
    ("watch_quiet", "Watch - a contact logged"),
    ("logbook", "Logbook"),
    ("settings", "Settings"),
    # Help & About is deliberately absent: it prints the version number, which
    # makes it the one capture guaranteed to be wrong by the next release. Same
    # reason the tour GIF does not visit it. The key map it shows is written out
    # in the README's Controls section, where it does not go stale.
]

# The six the Flipper Apps Catalog manifest points at, by the names it uses.
# Keeping the aliases here means the catalog never has to be re-pointed when a
# capture is retaken - only refreshed.
#
# ss2 must be the Fingerprint screen WITH a reader on it, not the idle one.
# Two reasons, and the second one bites silently: an idle card is a poor shop
# window, and tools_brand_data.carrier_from_capture() reads the banner's entire
# signature waveform back out of ss2.png - point it at a screen with no carrier
# and the brand's one measured element becomes a flat line.
CATALOG = {
    "ss0": "sweep_reader",
    "ss1": "watch_reader",
    "ss2": "fingerprint_reader",
    "ss0_2": "sweep_idle",
    "ss1_2": "watch_quiet",
    "ss2_2": "survey_done",
}


def refresh_catalog():
    missing = []
    for alias, src in CATALOG.items():
        p = os.path.join(SHOTS, f"{src}.png")
        if not os.path.exists(p):
            missing.append(src)
            continue
        shutil.copyfile(p, os.path.join(SHOTS, f"{alias}.png"))
        print(f"  screenshots/{alias}.png  <- {src}.png")
    if missing:
        print(f"  !! not captured yet: {', '.join(missing)}")


def run_tour(f, passive=False):
    """Walk every screen, capturing each one.

    `passive` skips the steps that need a live reader held against the device,
    so the screens that stand on their own can be refreshed without anyone
    standing at a payment terminal.
    """
    for name, _caption, keys in TOUR:
        if keys is None:
            continue  # the live-detection shots are handled by their owners
        for key, is_long in keys:
            f.press(key, long=is_long)
        settle_shot(f, name)

        if name == "sweep_idle" and not passive:
            img = wait_for_reader(
                f, alarm_strip_lit, 45, "a reader (a payment terminal, a door pad)"
            )
            if img is not None:
                save(img, "sweep_reader")
        elif name == "survey_run":
            capture_survey_verdict(f)
        elif name == "watch_quiet" and not passive:
            img = wait_for_reader(
                f, watch_band_lit, 30, "a reader, to log a Watch contact"
            )
            if img is not None:
                save(img, "watch_reader")


def run_reader_shots(f):
    """Just the screens that only exist while a carrier is actually present.

    Split out from the full tour because these are the ones that cost somebody
    their time: they need a live reader - a payment terminal, a door pad, or a
    phone with its wallet open - held against the back of the Flipper for as
    long as it takes. Everything else can be refreshed with --passive on a desk.
    """
    restart_app(f)

    f.press("ok")  # menu -> Sweep
    img = wait_for_reader(f, alarm_strip_lit, 60, "a live reader")
    if img is not None:
        save(img, "sweep_reader")

    f.press("back")
    f.press("down")
    f.press("ok")  # -> Fingerprint

    # Keep the BEST take, not the last one. The trace window is only about a
    # second wide, and a real reader polls in bursts, so which frame you happen
    # to grab decides whether the carrier is a rich square wave or a flat line
    # with two blips. The banner reads its entire signature waveform out of this
    # file, so "whatever was on screen when the timer went off" is not good
    # enough. Score every frame by how many polls it actually caught.
    print("  measuring the cadence - keep it on the reader for ~15s ...")
    best, best_edges, best_duty = None, -1, 0.0
    end = time.time() + 15.0
    tick = time.time() + 3.0
    while time.time() < end:
        d = f._await_frame(0.6)
        if d is None:
            continue
        img = to_image(d)
        edges, duty = carrier_quality(img)
        if edges > best_edges:
            best, best_edges, best_duty = img, edges, duty
        if time.time() >= tick:
            print(
                f"      best so far: {best_edges} edges, {best_duty:.0f}% duty",
                flush=True,
            )
            tick = time.time() + 3.0
    if best is not None and best_edges > 0:
        save(best, "fingerprint_reader")
        print(
            f"  kept the richest take: {best_edges} edges, {best_duty:.1f}% trace duty"
        )
    else:
        print("  !! no usable Fingerprint frame - was a reader present?")

    f.press("back")
    f.press("down")
    f.press("down")  # skip Site Survey
    f.press("ok")  # -> Watch
    img = wait_for_reader(
        f, watch_band_lit, 60, "the reader again, to log a Watch contact"
    )
    if img is not None:
        save(img, "watch_reader")

    f.press("back")
    f.press("down")
    f.press("ok")  # -> Logbook filter
    f.press("ok")  # -> Logbook, now with entries in it
    settle_shot(f, "logbook", wait=1.0)


# -------------------------------------------------------------------- gifs ---
def _write_gif(frames, out, fps, scale, hold_cap_ms=2200):
    """Hold a static screen with per-frame DURATION, not with repeated frames.

    Repeating a frame does not survive encoding: Pillow's GIF optimiser
    collapses identical consecutive frames again on the way out, so a page with
    nothing moving on it flashes past in one tick however many copies were handed
    to it. Timing each unique frame by how long it was actually on screen is both
    what we mean and a smaller file.
    """
    runs = []
    for fr in frames:
        if runs and fr.tobytes() == runs[-1][0].tobytes():
            runs[-1][1] += 1
        else:
            runs.append([fr, 1])

    tick = 1000.0 / fps
    durations = [
        max(int(tick), min(int(count * tick), hold_cap_ms)) for _, count in runs
    ]

    os.makedirs(IMAGES, exist_ok=True)
    big = [amber(fr, scale) for fr, _ in runs]
    big[0].save(
        out,
        save_all=True,
        append_images=big[1:],
        duration=durations,
        loop=0,
        optimize=True,
    )
    total = sum(durations) / 1000.0
    print(
        f"  wrote {os.path.relpath(out, HERE)}  ({len(big)} unique frames, "
        f"~{total:.0f}s of playback, {os.path.getsize(out) // 1024} KB)"
    )
    return out


def record(f, name, seconds=6.0, fps=10, scale=3):
    """Grab a run of frames off the live screen and write an animated GIF.

    Real device frames, not a re-render: the needle, the scanner bug, the throb
    ring and the alarm strip animate on the Flipper's own 100 ms tick, so the
    GIF shows the actual instrument rather than an approximation of it.
    """
    frames = []
    want = int(seconds * fps)
    print(f"  recording ~{seconds:.0f}s at {fps}fps ...")
    while len(frames) < want:
        data = f._await_frame(1.5)
        if data is None:
            break
        frames.append(to_image(data))
    if not frames:
        print("  !! no frames captured")
        return None
    return _write_gif(frames, os.path.join(IMAGES, f"{name}.gif"), fps, scale)


def splash_capture(f, fps=10, scale=3):
    """Record the boot intro, from the launch request onwards.

    The intro is the one thing that cannot be captured by navigating to it: it
    plays once, on launch, before any key can be pressed. Launching over RPC
    rather than through the CLI is what makes it reachable at all - the screen
    stream stays up across the launch, so the very first frames the app paints
    come down the wire like any others.

    Frames are kept from the moment the request goes out, then the Flipper
    desktop's own frames are dropped by content: the desktop art is a large dark
    scene and every Specter screen is mostly unlit.
    """
    for _ in range(7):
        f.press("back", settle=0.2)
    f.idle(0.5)
    f.flush()

    frames = []
    f.app_start(FAP_PATH)
    end = time.time() + 3.2
    while time.time() < end:
        d = f._await_frame(0.5)
        if d is None:
            continue
        img = to_image(d)
        if not frames and ink_fraction(img) >= 0.40:
            continue  # still the desktop
        frames.append(img)

    if not frames:
        print("  !! nothing recorded for the intro")
        return None
    # The still wants the COMPOSED intro - nameplate, rule, band and tagline all
    # present - not a mid-write frame, so take one from near the end but before
    # the menu replaces it.
    save(frames[max(0, int(len(frames) * 0.90) - 1)], "splash")
    return _write_gif(
        frames, os.path.join(IMAGES, "splash.gif"), fps, scale, hold_cap_ms=900
    )


# How long the Site Survey verdict card stays on screen in the tour GIF. It is
# one static frame that six seconds of walking exist to produce, so it needs
# noticeably longer than any animated stretch - and _write_gif's per-frame hold
# cap has to be raised to match, or it silently clips it back.
VERDICT_HOLD_MS = 3400


def tour_gif(f, name="demo", fps=10, scale=3):
    """One continuous GIF of a scripted walk through the app.

    Three deliberate choices.

    Frames are collected *between* key presses rather than after them, so
    transitions and animations end up in the recording - it is a screen capture
    of the device being used, not a slideshow of stills. That is also why every
    press here passes settle=0: a settling press drains the stream, and those
    are exactly the frames worth keeping.

    THE WALK NEVER OPENS HELP & ABOUT. That is the one screen that prints the
    version number, and a version number is the one thing that makes a demo
    stale. Leaving it out means this GIF stays accurate across releases instead
    of needing a re-shoot every time the version moves. Everything else - every
    mode the app has - is in here.

    And it ends back on the main menu, so the loop closes where it opened.
    """
    frames = []

    def grab(seconds, cue=None, watch=None):
        """Collect frames for a while.

        `watch` echoes whether the device is currently detecting, so whoever is
        holding the reader can correct their aim mid-take instead of finding out
        afterwards that the whole segment is empty.
        """
        if cue:
            print(f"    [{cue}]", flush=True)
        end = time.time() + seconds
        next_tick = time.time() + 1.5
        hit = False
        while time.time() < end:
            d = f._await_frame(0.6)
            if d is None:
                continue
            img = to_image(d)
            frames.append(img)
            if watch is not None:
                if watch(img):
                    hit = True
                if time.time() >= next_tick:
                    print(
                        "      "
                        + (
                            "DETECTING"
                            if hit
                            else "nothing yet - hold it on the reader"
                        ),
                        flush=True,
                    )
                    hit = False
                    next_tick = time.time() + 1.5

    def step(*keys):
        """Walk the menu without losing the transition frames."""
        for k in keys:
            f.press(k, settle=0.0)
            grab(0.45)

    # Open on the boot intro rather than waiting it out. The GIF is meant to be
    # the app being used, and the first thing using it does is play that - so
    # starting after it is over throws away the one screen written specifically
    # to be watched. Captured the only way it can be: over RPC, where the screen
    # stream stays up across the launch.
    for _ in range(7):
        f.press("back", settle=0.2)
    f.idle(0.5)
    f.flush()
    f.app_start(FAP_PATH)
    print("    [boot intro]", flush=True)
    end = time.time() + 2.9
    seen_app = False
    while time.time() < end:
        d = f._await_frame(0.5)
        if d is None:
            continue
        img = to_image(d)
        # Drop the Flipper desktop's own frames by content: its dolphin art is a
        # large dark scene. Only until the app has painted once - the intro
        # itself goes briefly dark in the carrier band, and an ink test applied
        # throughout would cut exactly the frames worth keeping.
        if not seen_app:
            if ink_fraction(img) >= 0.40:
                continue
            seen_app = True
        frames.append(img)
    f.idle(0.4)

    grab(2.0, "main menu")

    # -- Sweep: the hunt ----------------------------------------------------
    step("ok")
    grab(1.2, ">>> SWEEP: put the BACK of the Flipper flat on the reader <<<")
    got, deadline = 0, time.time() + 45
    tick = time.time() + 1.5
    while time.time() < deadline and got < 32:
        d = f._await_frame(0.6)
        if d is None:
            continue
        img = to_image(d)
        frames.append(img)
        if alarm_strip_lit(img):
            got += 1
        if time.time() >= tick:
            print(
                f"      {'DETECTING - hold it there' if got else 'nothing yet - flat on the reader'}"
                f"  ({got}/32 frames)",
                flush=True,
            )
            tick = time.time() + 1.5
    print(f"    [sweep captured {got} detected frames]", flush=True)
    grab(1.0)

    # UP and DOWN change sensitivity without leaving the hunt - worth showing,
    # because it is the one binding nothing on screen can teach you.
    f.press("down", settle=0.0)
    grab(1.6, "sensitivity down - keep it on the reader", watch=alarm_strip_lit)
    f.press("up", settle=0.0)
    grab(1.6, "sensitivity back up", watch=alarm_strip_lit)

    # -- Fingerprint: what kind of thing is it? -----------------------------
    step("back", "down", "ok")
    grab(5.0, "fingerprint - keep holding it on the reader", watch=None)

    # -- Site Survey: is the room clean? ------------------------------------
    step("back", "down", "ok")
    grab(6.0, "site survey - walk the room")
    # The verdict card is painted once by this press and never redrawn, so the
    # press must not settle and the frame must be read straight back.
    #
    # It is also the single hardest frame in the GIF to read: the whole survey
    # exists to produce it, and it arrives as one static frame after six
    # seconds of motion. Held for VERDICT_HOLD_MS - which is why _write_gif is
    # given a matching cap below, since its default would clip it.
    f.press("ok", settle=0.0)
    d = f._await_frame(3.0)
    if d is not None:
        frames.extend([to_image(d)] * int(VERDICT_HOLD_MS / (1000.0 / fps)))
    else:
        print("    !! no survey verdict frame")

    # -- Watch: leave it standing guard -------------------------------------
    # The last mode in the walk, and the one that needs the longest look: its
    # whole point is what it does over time, so a glance at it says nothing.
    step("back", "down", "ok")
    grab(8.0, "watch mode - standing guard", watch=watch_band_lit)

    # The Logbook is deliberately NOT in the walk. It is a wall of timestamped
    # text that needs reading rather than watching, so on a looping GIF it
    # either flashes past unread or holds long enough to stall the loop - and
    # it is the one screen whose content is different on every device. It gets
    # a still in the README instead.

    # -- home. Help & About is deliberately NOT visited: version number. ----
    step("back")
    grab(2.5, "back to the menu - done")

    if not frames:
        print("  !! nothing recorded")
        return None
    return _write_gif(
        frames,
        os.path.join(IMAGES, f"{name}.gif"),
        fps,
        scale,
        hold_cap_ms=VERDICT_HOLD_MS,
    )


# -------------------------------------------------------------------- main ---
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--port", help="serial port (default: autodetect)")
    ap.add_argument(
        "--all", action="store_true", help="drive Specter and capture every screen"
    )
    ap.add_argument(
        "--shot", metavar="NAME", help="capture whatever is on screen right now"
    )
    ap.add_argument(
        "--record", metavar="NAME", help="record an animated GIF of the live screen"
    )
    ap.add_argument(
        "--tour-gif", action="store_true", help="record a GIF of a scripted walk"
    )
    ap.add_argument(
        "--splash", action="store_true", help="record the boot intro (launch onwards)"
    )
    ap.add_argument(
        "--reader",
        action="store_true",
        help="capture only the screens that need a live reader held against the device",
    )
    ap.add_argument("--sheet", action="store_true", help="rebuild images/screens.png")
    ap.add_argument(
        "--catalog", action="store_true", help="refresh screenshots/ssN.png aliases"
    )
    ap.add_argument(
        "--verify", action="store_true", help="check every screenshot is two colours"
    )
    ap.add_argument(
        "--seconds", type=float, default=6.0, help="recording length (default 6)"
    )
    ap.add_argument(
        "--fps", type=int, default=10, help="recording frame rate (default 10)"
    )
    ap.add_argument("--launch", metavar="FAP", help="start this app over RPC first")
    ap.add_argument(
        "--no-launch", action="store_true", help="capture without restarting the app"
    )
    ap.add_argument(
        "--passive",
        action="store_true",
        help="skip the shots that need a live reader held against the device",
    )
    args = ap.parse_args()

    # Offline jobs first - none of these need the device.
    offline = False
    if args.verify:
        offline = True
        bad = [
            p
            for p in sorted(glob.glob(os.path.join(SHOTS, "*.png")))
            if not two_colours_only(p)
        ]
        for p in bad:
            print(f"  !! {os.path.relpath(p, HERE)} is not two colours")
        print(
            "all screenshots are the device's two colours"
            if not bad
            else f"{len(bad)} bad"
        )
    if args.catalog:
        offline = True
        refresh_catalog()
    if args.sheet and not (args.all or args.shot):
        offline = True
        contact_sheet(SHEET)
    live = (
        args.all
        or args.shot
        or args.record
        or args.tour_gif
        or args.reader
        or args.splash
    )
    if offline and not live:
        return

    if not live:
        ap.print_help()
        return

    f = Flipper(args.port)
    try:
        f.start_stream()
        if args.splash:
            splash_capture(f)
        elif args.tour_gif:
            tour_gif(f)
        elif args.reader:
            run_reader_shots(f)
            refresh_catalog()
            contact_sheet(SHEET)
        elif args.all:
            if not args.no_launch:
                restart_app(f, args.launch or FAP_PATH)
            run_tour(f, passive=args.passive)
            refresh_catalog()
            contact_sheet(SHEET)
        elif args.record:
            if args.launch:
                f.app_start(args.launch)
                f.idle(1.5)
                f.flush()
            record(f, args.record, args.seconds, args.fps)
        elif args.shot:
            f.idle(0.4)
            f.flush()
            data = f.frame()
            if data is None:
                sys.exit("No frame received. Is the screen updating?")
            save(to_image(data), args.shot)
            if args.sheet:
                contact_sheet(SHEET)
    finally:
        f.close()


if __name__ == "__main__":
    main()
