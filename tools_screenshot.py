#!/usr/bin/env python3
"""Capture real Flipper Zero screens and recordings from Faraday, over RPC.

Every picture in this repository comes off the device. There is no mockup
renderer any more: a drawing of a screen is a second implementation of the UI
that can - and elsewhere in this family of apps, did - disagree with the
firmware while looking perfectly convincing. A capture cannot.

    python3 tools_screenshot.py --all        # drive Faraday, capture every screen
    python3 tools_screenshot.py --probe      # what is on the screen right now
    python3 tools_screenshot.py --shot NAME  # save the current screen as NAME
    python3 tools_screenshot.py --record NAME --seconds 6
    python3 tools_screenshot.py --splash     # the launch intro (GIF + still)
    python3 tools_screenshot.py --tour-gif   # one GIF of a scripted walk
    python3 tools_screenshot.py --sheet      # rebuild images/screens{,-dark}.png
    python3 tools_screenshot.py --catalog    # refresh screenshots/ssN.png aliases

The Flipper's CLI has no screenshot command on current firmware, but the
protobuf RPC session exposes the framebuffer and an input injector, so this
drives the app and grabs frames from the device itself. Only a handful of
message shapes are needed, so the protobuf is encoded by hand rather than
pulling in a generated stub. From flipper.proto:

    Main { command_id = 1, command_status = 2, has_next = 3, oneof content }
      16 = App.StartRequest { string name = 1, string args = 2 }
      19 = StopSession {}
      20 = Gui.StartScreenStreamRequest {}
      21 = Gui.StopScreenStreamRequest {}
      22 = Gui.ScreenFrame { bytes data = 1 }
      23 = Gui.SendInputEventRequest { key = 1, type = 2 }

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

FAP_PATH = "/ext/apps/Tools/faraday.fap"

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
    found = sorted(glob.glob("/dev/cu.usbmodemflip_*")) or sorted(glob.glob("/dev/ttyACM*"))
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
        time. Faraday's meter, hunt and splash views animate on a 100 ms tick,
        so a bounded drain on one of those returns with the backlog still
        queued - and the very next "capture" is a frame from before the key
        press, which looks exactly like the app ignoring the button.
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
        healthy and keeps "now" actually mean now.
        """
        end = time.time() + seconds
        while time.time() < end:
            data = self._await_frame(min(0.4, max(end - time.time(), 0.05)))
            if data is not None and on_frame is not None:
                on_frame(data)

    def frame(self, timeout=3.0, nudge=True):
        """The 1024-byte framebuffer from the next ScreenFrame message.

        The device only pushes a frame when the screen actually *redraws*.
        Faraday's meter, hunt and splash views animate on a 100 ms tick so they
        stream happily, but the main menu, Settings, Saved results and About are
        static and would otherwise time out with nothing sent. `nudge` walks the
        selection down and back up, which forces two repaints and lands on the
        row it started from - non-destructive on a submenu, a variable item list
        and a scrolling text box alike.
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
        catch the first frames an app paints - and Faraday's first frames are
        its intro, which plays once per launch and can never be navigated to.
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


def ink_fraction(img, top=0, bottom=64, left=0, right=128):
    px = img.load()
    rows, cols = max(bottom - top, 1), max(right - left, 1)
    n = sum(1 for y in range(top, bottom) for x in range(left, right) if px[x, y] == 0)
    return n / float(rows * cols)


def two_colours_only(path):
    """The branding law, checked rather than trusted."""
    cols = {c for _, c in Image.open(path).convert("RGB").getcolors(1 << 20)}
    return cols <= {LCD_LIT, LCD_INK}


# ------------------------------------------------------------ face reading ---
# Faraday's screens are told apart by where the ink is, not by guessing that a
# key press did what it was asked. Every one of these predicates was checked
# against a real capture before being relied on.
#
#   capture face  - a solid inverted phase pill fills rows 13..24 on the left
#   verdict face  - two thin comparison bar frames there instead, plus a grade
#                   badge (rows 34..51, x >= 96)
#   error face    - almost nothing above row 30 except the header rule
def is_capture_face(img):
    return ink_fraction(img, 13, 25, 2, 62) > 0.30


def is_verdict_face(img):
    """The verdict card, told apart from every other Faraday screen.

    Three tests, and each one exists because a looser version matched
    something it should not have:

      * the app's own near-solid action strip - without it this matched the
        Flipper DESKTOP, whose dolphin art has plenty of ink in the badge
        rectangle, and a watcher waiting for a verdict "captured" the launcher.
      * no phase pill - that rules out the two capture faces.
      * an EMPTY band where the third meter row would be. The Find-my-band
        screen also has a solid strip and no pill, so it passed the first two;
        but it draws four bar frames down the screen and the verdict draws
        exactly two, leaving rows 35..44 clear between the BAG bar and the
        action strip.
    """
    return (
        is_meter_screen(img)
        and not is_capture_face(img)
        and ink_fraction(img, 35, 44, 46, 92) < 0.02
        and ink_fraction(img, 15, 32, 30, 98) > 0.05
    )


def is_meter_screen(img):
    """Either face of the measurement screen (both own the bottom strip)."""
    return ink_fraction(img, 53, 64, 0, 128) > 0.55


def pill_signature(img):
    """The exact pixels of the phase pill row: the pill and its sub-label.

    Compared BETWEEN frames rather than measured against a threshold. An
    earlier version of this keyed on how much ink sat in one fixed rectangle
    below the bar - and then the firmware's layout changed, that rectangle
    gained a readout in both phases, and the predicate started answering
    "advanced" to everything. The tour dutifully saved a Baseline screen as
    shielded.png.

    A signature says only "these two frames differ here", which stays true
    however the pill is drawn: BASELINE/open air and SHIELDED/in pouch cannot
    render identically.
    """
    px = img.load()
    return bytes(
        bytearray(
            1 if px[x, y] == 0 else 0 for y in range(13, 25) for x in range(2, 127)
        )
    )


# ------------------------------------------------------------------- sheet ---
# The sheet ships in BOTH themes, served by <picture>. A single light sheet is
# a glaring white slab in the middle of a dark README - and a single dark one
# is the same problem inverted. The device captures inside it are untouched
# either way: they are already the only two colours they are allowed to be.
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


# -------------------------------------------------------------------- gifs ---
def _write_gif(frames, out, fps, scale, hold_cap_ms=2200):
    """Hold a static screen with per-frame DURATION, not with repeated frames.

    Repeating a frame does not survive encoding: Pillow's GIF optimiser
    collapses identical consecutive frames again on the way out, so a page with
    nothing moving on it flashes past in one tick however many copies were
    handed to it. Timing each unique frame by how long it was actually on
    screen is both what we mean and a smaller file.
    """
    runs = []
    for fr in frames:
        if runs and fr.tobytes() == runs[-1][0].tobytes():
            runs[-1][1] += 1
        else:
            runs.append([fr, 1])

    tick = 1000.0 / fps
    durations = [max(int(tick), min(int(count * tick), hold_cap_ms)) for _, count in runs]

    os.makedirs(IMAGES, exist_ok=True)
    big = [amber(fr, scale) for fr, _ in runs]
    big[0].save(out, save_all=True, append_images=big[1:], duration=durations, loop=0, optimize=True)
    total = sum(durations) / 1000.0
    print(
        f"  wrote {os.path.relpath(out, HERE)}  ({len(big)} unique frames, "
        f"~{total:.0f}s of playback, {os.path.getsize(out) // 1024} KB)"
    )
    return out


def record(f, name, seconds=6.0, fps=10, scale=3):
    """Grab a run of frames off the live screen and write an animated GIF.

    Real device frames, not a re-render: the sweeping scan marker, the live
    sparkline, the verdict reveal and the leak-hunt trace all animate on the
    Flipper's own 100 ms tick, so the GIF shows the actual instrument rather
    than an approximation of it.
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
    """Record the launch intro, from the launch request onwards.

    The intro is the one thing that cannot be captured by navigating to it: it
    plays once, on launch, before any key can be pressed. Launching over RPC
    rather than through the CLI is what makes it reachable at all - the screen
    stream stays up across the launch, so the very first frames the app paints
    come down the wire like any others.

    Frames are kept from the moment the request goes out, then the Flipper
    desktop's own frames are dropped by content: the desktop art is a large
    dark scene and every Faraday screen is mostly unlit.
    """
    for _ in range(7):
        f.press("back", settle=0.2)
    f.idle(0.5)
    f.flush()

    frames = []
    f.app_start(FAP_PATH)
    end = time.time() + 3.4
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
    # The still wants the COMPOSED intro - pouch, wordmark, tagline and a part
    # filled progress bar all present - not a mid-write first frame, so take
    # one from near the end but before the menu replaces it.
    save(frames[max(0, int(len(frames) * 0.80) - 1)], "splash")
    return _write_gif(frames, os.path.join(IMAGES, "splash.gif"), fps, scale, hold_cap_ms=900)


# ------------------------------------------------------------------- drive ---
def restart_app(f, fap=FAP_PATH, skip_splash=True):
    """Put the device in a known state: freshly launched Faraday, main menu.

    The tour navigates by relative key presses, so it only lines up if it
    starts from a known screen with a known menu cursor. Faraday's intro plays
    once per launch and has to be skipped before the menu exists at all.
    A fresh launch also resets the Start scene's saved selection, which is what
    lets Menu below start out certain it is sitting on row 0.
    """
    for _ in range(7):
        f.press("back", settle=0.2)
    f.idle(0.4)
    f.flush()
    f.app_start(fap)
    f.idle(1.0)
    if skip_splash:
        f.press("right", settle=0.6)  # any key but Back skips the intro
    f.idle(0.5)
    f.flush()


# Rows of the main submenu, in the order faraday_scene_start.c adds them.
ROW_SUBGHZ, ROW_NFC, ROW_BAND, ROW_HUNT, ROW_RESULTS, ROW_SETTINGS, ROW_ABOUT = range(7)
MENU_ROWS = 7


class Menu:
    """Exact cursor control for the main submenu.

    Two device facts make this arithmetic rather than hope, and the first one
    cost a whole bad capture run before it was noticed:

      * THE SUBMENU WRAPS. Pressing Up on the first row lands on the last. So
        the obvious "press Up plenty of times to get back to the top" does not
        park the cursor at the top at all - it walks it to (row - presses) mod
        6, which is an arbitrary row. On the first tour that silently opened
        Settings where NFC was wanted and About where Leak Hunt was wanted,
        and the only symptom was a static screen refusing to send a frame.

      * Backing out of a screen RESTORES the cursor to the row that was
        opened, because the Start scene re-applies its saved scene state on
        re-entry. So the position after a back() is known exactly.

    Between them the cursor is always known, and moving to a row is a single
    modulo - which the wrap now helps rather than hinders.
    """

    def __init__(self, f, at=ROW_SUBGHZ):
        self.f = f
        self.at = at

    def select(self, row):
        for _ in range((row - self.at) % MENU_ROWS):
            self.f.press("down", settle=0.28)
        self.at = row
        self.f.idle(0.25)

    def open(self, row, settle=1.2, confirm=True):
        """Select a row and open it, verifying the screen actually changed.

        An OK that does not land leaves the MENU on screen, and a capture taken
        straight afterwards saves a picture of the menu labelled as whatever
        was supposed to open - which is exactly how a shot of the submenu ended
        up captioned "About". Comparing frames costs one read and removes the
        assumption.
        """
        self.select(row)
        before = None
        if confirm:
            self.f.flush()
            before = self.f.frame(timeout=2.0)
        self.f.press("ok", settle=settle)
        if not confirm:
            return True
        for _ in range(3):
            after = self.f.frame(timeout=2.5)
            if after is not None and after != before:
                return True
            self.f.press("ok", settle=settle)  # the press did not land; retry
        print(f"  !! row {row} would not open")
        return False

    def back(self, settle=0.9):
        # Start re-applies its saved selection, which is the row just opened,
        # so self.at is still correct afterwards.
        self.f.press("back", settle=settle)


def settle_shot(f, name, wait=0.8, nudge=True):
    """Let a screen finish whatever it is doing, then capture it.

    Flush BEFORE the wait, never after: a static screen paints exactly one
    frame, and a flush placed after the wait throws away the only frame it
    will ever send.
    """
    f.idle(wait)
    f.flush()
    data = f.frame(nudge=nudge)
    if data is None:
        print(f"  !! no frame for {name}")
        return None
    img = to_image(data)
    save(img, name)
    return img


def strip_signature(img):
    """The pixels of the action strip's message area.

    Like pill_signature, this is compared BETWEEN frames rather than measured
    against an ink threshold. Thresholding the strip is what an earlier
    version did, on the assumption that the "signal is up" wording carries
    more ink than the "press your fob" wording - which was true of one pair of
    strings and stopped being true the moment the copy was reworded. A
    signature only ever claims "this differs from the frame I started with".
    """
    px = img.load()
    return bytes(
        bytearray(1 if px[x, y] == 0 else 0 for y in range(54, 63) for x in range(3, 88))
    )


def wait_for_signal(f, seconds=25.0, what="a carrier above the noise floor"):
    """Hold on the capture face until Faraday's own cue says it has something.

    The bottom strip is the app's signal cue: it prompts until a carrier has
    risen the required margin out of the tracked noise floor, and confirms
    once it has. Watching the strip rather than waiting a fixed number of
    seconds means the capture happens when the DEVICE agrees there is
    something to capture - and it is driven by the same expression the lock
    decision uses, so a screenshot cannot show a state the button would then
    refuse to act on.

    Returns the last frame seen, and whether the cue ever changed.
    """
    print(f"  waiting up to {seconds:.0f}s for {what} ...")
    f.flush()
    first = f.frame(nudge=False, timeout=3.0)
    base = strip_signature(to_image(first)) if first else None
    end = time.time() + seconds
    last = to_image(first) if first else None
    while time.time() < end:
        d = f._await_frame(0.6)
        if d is None:
            continue
        last = to_image(d)
        if base is not None and strip_signature(last) != base:
            print("  signal cue changed - the app has something to lock")
            return last, True
    print("  (cue never changed - carrying on with what is on screen)")
    return last, False


CATALOG = {
    # The catalog wants a handful of screens that show what the app does.
    # Order matters: ss0 is the one a browsing user sees first.
    "ss0": "verdict",
    "ss1": "baseline",
    "ss2": "shielded",
    "ss3": "hunt",
    "ss4": "menu",
    "ss5": "band",
    "ss6": "results",
}


def refresh_catalog():
    """Mirror the chosen screens to screenshots/ssN.png for the app catalog."""
    made = 0
    for alias, src in CATALOG.items():
        p = os.path.join(SHOTS, f"{src}.png")
        if not os.path.exists(p):
            print(f"  (skipping {alias} - {src}.png not captured)")
            continue
        dst = os.path.join(SHOTS, f"{alias}.png")
        shutil.copyfile(p, dst)
        ok = two_colours_only(dst)
        print(f"  {alias}.png <- {src}.png   two-colour: {'yes' if ok else 'NO'}")
        made += 1
    print(f"refreshed {made} catalog aliases")


def run_tour(f, fob_wait=25.0):
    """Drive Faraday end to end and capture every screen it can reach.

    The measurement flow is the part that cannot be faked: Baseline needs a
    real carrier, and Shielded needs a second one. The tour waits for the
    app's own signal cue at each step rather than pressing OK on a schedule
    and hoping - a press the app refuses looks identical to a press it
    ignored, and shipping a screenshot of a state the app would have rejected
    is exactly the kind of quiet lie a capture tool exists to prevent.
    """
    results = {}

    # -- intro + menu --
    print("\n[1/8] launch intro")
    splash_capture(f)
    f.idle(1.2)
    f.flush()
    # splash_capture relaunches the app, so the cursor is back on row 0.
    m = Menu(f, at=ROW_SUBGHZ)

    print("[2/8] main menu")
    results["menu"] = settle_shot(f, "menu")

    # -- Sub-GHz: baseline -> shielded -> verdict --
    print("[3/8] Sub-GHz test, Baseline face")
    m.open(ROW_SUBGHZ, settle=0.8)
    f.idle(4.2)  # let the opening card clear before anything is captured
    wait_for_signal(f, fob_wait, "a fob or ambient carrier at the tuned band")
    results["baseline"] = settle_shot(f, "baseline", wait=0.4, nudge=False)

    print("[4/8] locking the baseline")
    f.flush()
    before = f.frame(nudge=False, timeout=2.0)
    f.press("ok", settle=1.0)
    after = f.frame(nudge=False)
    advanced = (
        after is not None
        and before is not None
        and pill_signature(to_image(after)) != pill_signature(to_image(before))
    )
    print(f"  baseline {'locked' if advanced else 'REFUSED - no carrier above the floor'}")

    if advanced:
        print("[5/8] Shielded face")
        wait_for_signal(f, fob_wait, "the pouch-sealed carrier")
        results["shielded"] = settle_shot(f, "shielded", wait=0.4, nudge=False)

        print("  verdict")
        f.flush()
        f.press("ok", settle=0.2)
        # The reveal animation runs for ~6 ticks; record it, then hold the
        # settled card. The GIF is worth having on its own.
        frames = []
        end = time.time() + 2.6
        while time.time() < end:
            d = f._await_frame(0.4)
            if d is not None:
                frames.append(to_image(d))
        if frames:
            _write_gif(frames, os.path.join(IMAGES, "verdict.gif"), 10, 3, hold_cap_ms=1800)
            last = frames[-1]
            if is_verdict_face(last):
                save(last, "verdict")
                results["verdict"] = last
            else:
                print("  !! last frame is not the verdict face")
        f.idle(0.6)
    else:
        print("  [5/8] skipped: without a locked baseline there is no verdict to show")

    m.back()

    # -- NFC --
    print("[6/8] NFC test face")
    m.open(ROW_NFC)
    # 4 s: the opening "what goes in the bag" card holds for ~3.4 s, and a
    # shot taken under it shows the card rather than the screen.
    results["nfc"] = settle_shot(f, "nfc", wait=4.2, nudge=False)
    m.back()

    # -- find my band --
    print("[7/8] find my band")
    m.open(ROW_BAND, settle=0.5)
    f.idle(6.5)  # three sweeps of four bands, then the verdict
    f.flush()
    fr = f.frame(nudge=False, timeout=3.0)
    if fr is not None:
        results["band"] = to_image(fr)
        save(results["band"], "band")
    m.back()

    # -- Leak hunt --
    print("[8/8] leak hunt")
    m.open(ROW_HUNT)
    results["hunt"] = settle_shot(f, "hunt", wait=1.2, nudge=False)
    record(f, "hunt", seconds=5.0)
    m.back()

    # -- the static screens --
    print("[8/8] results, settings, about")
    for row, name in ((ROW_RESULTS, "results"), (ROW_SETTINGS, "settings"), (ROW_ABOUT, "about")):
        m.open(row, settle=1.0)
        results[name] = settle_shot(f, name, wait=0.8)
        m.back()

    have = [k for k, v in results.items() if v is not None]

    # Delete the file for any screen this run could NOT capture.
    #
    # Without this, a screen that fails today silently keeps yesterday's
    # picture: the contact sheet and the catalog aliases are built from
    # whatever is on disk, so a stale shot of an older build survives into
    # both while the run that produced it reported the screen as missing.
    # That is exactly how a screenshot of superseded UI nearly shipped.
    for name, _ in SHEET:
        if name in ("splash",) or name in have:
            continue
        stale = os.path.join(SHOTS, f"{name}.png")
        if os.path.exists(stale):
            os.remove(stale)
            print(f"  removed stale screenshots/{name}.png (not captured this run)")
        for alias, src in CATALOG.items():
            if src == name:
                a = os.path.join(SHOTS, f"{alias}.png")
                if os.path.exists(a):
                    os.remove(a)
    print(f"\ncaptured {len(have)} screens: {', '.join(sorted(have))}")
    missing = [k for k in ("baseline", "shielded", "verdict") if k not in have]
    if missing:
        print(
            "NOT captured: "
            + ", ".join(missing)
            + "\n  These need a real transmitter. Press a key fob (or hold a card to a\n"
            "  reader for the NFC flow) while the tour is on the capture face, or\n"
            "  re-run with --fob-wait 60 and press the fob when prompted."
        )
    return results


def run_fob(f, hold=45.0):
    """Capture the Shielded and Verdict screens from a REAL two-capture test.

    These two screens cannot be faked and cannot be reached without a
    transmitter: Faraday refuses to lock a baseline until a carrier has risen
    well clear of the tracked noise floor, which is the entire point of the
    refusal. So this walks a human through an actual measurement and captures
    what the device draws.

    Anything that genuinely blocks RF works for the shielded half - a
    signal-blocking pouch, or a couple of layers of kitchen foil wrapped right
    around the fob, which is a real Faraday shield and usually scores well.
    """
    print("\n=== guided capture: a real Sub-GHz shielding test ===")
    restart_app(f)
    m = Menu(f, at=ROW_SUBGHZ)
    m.open(ROW_SUBGHZ, settle=1.2)

    print("\n>>> STEP 1. Hold your key fob NEXT TO THE FLIPPER and press its")
    print(">>> button repeatedly. Keep pressing until this says it locked.")
    img, ok = wait_for_signal(f, hold, "your fob's carrier")
    if not ok:
        print("!! no carrier seen - is the fob on 433.92 MHz? Check Settings > Sub-GHz band.")
        return {}
    save(img, "baseline")

    f.flush()
    before = f.frame(nudge=False, timeout=2.0)
    f.press("ok", settle=1.2)
    after = f.frame(nudge=False)
    if not (after and before and pill_signature(to_image(after)) != pill_signature(to_image(before))):
        print("!! the baseline was refused - the carrier faded before OK landed. Try again.")
        return {}
    print("  baseline LOCKED")

    print("\n>>> STEP 2. Now SEAL THE FOB in the pouch (or wrap it in foil),")
    print(">>> hold it next to the Flipper and press its button again.")
    print(">>> A good pouch may block it completely - that is a valid result,")
    print(">>> so this step moves on by itself after the timeout.")
    img2, _ = wait_for_signal(f, hold, "the pouch-sealed carrier")
    shot = settle_shot(f, "shielded", wait=0.4, nudge=False)

    print("\n  taking the verdict ...")
    f.flush()
    f.press("ok", settle=0.2)
    frames = []
    end = time.time() + 3.0
    while time.time() < end:
        d = f._await_frame(0.4)
        if d is not None:
            frames.append(to_image(d))
    out = {"shielded": shot}
    if frames:
        _write_gif(frames, os.path.join(IMAGES, "verdict.gif"), 10, 3, hold_cap_ms=1800)
        last = frames[-1]
        if is_verdict_face(last):
            save(last, "verdict")
            out["verdict"] = last
            print("  verdict CAPTURED")
        else:
            print("  !! the last frame is not the verdict face - re-run --fob")
    return out


def run_session(f, seconds=300.0, out="session"):
    """Record every DISTINCT frame while a human drives the app.

    The screens where layout bugs actually live - the Shielded phase, the
    bag-it countdown, the verdict card - cannot be reached without a real
    transmitter, so no scripted tour can photograph them. This just watches:
    the operator walks the whole app end to end and every frame that differs
    from the one before it is kept, numbered in order.

    The result is a contact sheet of the real session to review in one go,
    rather than guessing which screen to try to catch next.
    """
    d = os.path.join(SHOTS, out)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d, exist_ok=True)

    print(f"recording distinct frames for {seconds:.0f}s - drive the app now")
    kept, prev, n = [], None, 0
    end = time.time() + seconds
    while time.time() < end:
        raw = f._await_frame(0.8)
        if raw is None:
            continue
        if raw == prev:
            continue
        prev = raw
        img = to_image(raw)
        n += 1
        amber(img, 4).save(os.path.join(d, f"{n:04d}.png"))
        kept.append(img)
        if n % 25 == 0:
            print(f"  {n} frames ...")
    print(f"kept {n} distinct frames in screenshots/{out}/")
    if kept:
        _write_gif(kept, os.path.join(IMAGES, f"{out}.gif"), 10, 3, hold_cap_ms=1500)
    return kept


def tour_gif(f, name="demo", fps=10, scale=3):
    """One continuous GIF of a scripted walk through the app.

    Frames are collected BETWEEN key presses rather than after them, so
    transitions and animations land in the recording - it is a screen capture
    of the device being used, not a slideshow of stills. That is also why
    every press here passes settle=0: a settling press drains the stream, and
    those are exactly the frames worth keeping.

    The walk never opens About. That is the one screen printing a version
    number, and a version number is what makes a demo GIF go stale the moment
    the next release ships.
    """
    frames = []

    def grab(seconds):
        end = time.time() + seconds
        while time.time() < end:
            d = f._await_frame(0.4)
            if d is not None:
                frames.append(to_image(d))

    # the intro, from the launch request onwards
    for _ in range(7):
        f.press("back", settle=0.2)
    f.idle(0.4)
    f.flush()
    f.app_start(FAP_PATH)
    grab(2.6)
    f.press("right", settle=0.0)  # skip to the menu
    grab(1.0)

    m = Menu(f, at=ROW_SUBGHZ)

    def step(row, dwell):
        for _ in range((row - m.at) % MENU_ROWS):
            f.press("down", settle=0.0)
            grab(0.35)
        m.at = row
        grab(0.4)
        f.press("ok", settle=0.0)
        grab(dwell)
        f.press("back", settle=0.0)
        grab(0.7)

    step(ROW_SUBGHZ, 3.4)  # the live meter, listening
    step(ROW_NFC, 3.0)  # the NFC field meter
    step(ROW_HUNT, 4.0)  # the leak sweep, warmest of the animated screens
    step(ROW_RESULTS, 2.6)  # the logbook

    if not frames:
        print("  !! nothing recorded for the tour")
        return None
    return _write_gif(frames, os.path.join(IMAGES, f"{name}.gif"), fps, scale)


SHEET = [
    ("splash", "Launch"),
    ("menu", "Main menu"),
    ("baseline", "Baseline - open air"),
    ("shielded", "Shielded - in pouch"),
    ("verdict", "Verdict and grade"),
    ("hunt", "Leak hunt"),
    ("band", "Find my fob's band"),
    ("nfc", "NFC field test"),
    ("results", "Saved results"),
    ("settings", "Settings"),
    ("about", "About"),
]


# -------------------------------------------------------------------- main ---
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port (default: the first Flipper found)")
    ap.add_argument("--all", action="store_true", help="drive the app and capture every screen")
    ap.add_argument("--probe", action="store_true", help="describe and save the current screen")
    ap.add_argument("--shot", metavar="NAME", help="save the current screen as NAME")
    ap.add_argument("--record", metavar="NAME", help="record the live screen to images/NAME.gif")
    ap.add_argument("--splash", action="store_true", help="capture the launch intro")
    ap.add_argument("--session", type=float, metavar="SECS",
                    help="record every distinct frame while you drive the app")
    ap.add_argument("--fob", action="store_true",
                    help="guided capture of a real shielding test (needs a key fob)")
    ap.add_argument("--tour-gif", action="store_true", help="one GIF of a scripted walk")
    ap.add_argument("--sheet", action="store_true", help="rebuild images/screens{,-dark}.png")
    ap.add_argument("--catalog", action="store_true", help="refresh screenshots/ssN.png aliases")
    ap.add_argument("--seconds", type=float, default=6.0, help="length for --record")
    ap.add_argument("--fps", type=int, default=10)
    ap.add_argument("--fob-wait", type=float, default=25.0, help="seconds to wait for a carrier")
    args = ap.parse_args()

    # Offline actions need no device.
    if args.sheet and not (args.all or args.shot or args.record or args.probe or args.splash):
        contact_sheet(SHEET)
        return
    if args.catalog and not (args.all or args.shot or args.record or args.probe or args.splash):
        refresh_catalog()
        return
    if not any((args.all, args.probe, args.shot, args.record, args.splash, args.tour_gif, args.fob, args.session)):
        ap.print_help()
        return

    f = Flipper(args.port)
    f.start_stream()
    try:
        if args.all:
            restart_app(f, skip_splash=False)
            run_tour(f, fob_wait=args.fob_wait)
            contact_sheet(SHEET)
            refresh_catalog()
        elif args.tour_gif:
            tour_gif(f)
        elif args.session:
            run_session(f, seconds=args.session)
        elif args.fob:
            run_fob(f, hold=max(args.fob_wait, 45.0))
            contact_sheet(SHEET)
            refresh_catalog()
        elif args.splash:
            splash_capture(f, fps=args.fps)
        elif args.probe:
            f.flush()
            data = f.frame()
            if data is None:
                print("no frame - is anything on screen redrawing?")
            else:
                img = to_image(data)
                save(img, "probe")
                print(
                    f"  ink {ink_fraction(img) * 100:.1f}%  "
                    f"meter={is_meter_screen(img)} capture={is_capture_face(img)} "
                    f"verdict={is_verdict_face(img)}"
                )
        elif args.shot:
            f.flush()
            data = f.frame()
            if data is None:
                print("no frame captured")
            else:
                save(to_image(data), args.shot)
        elif args.record:
            record(f, args.record, seconds=args.seconds, fps=args.fps)

        if args.sheet:
            contact_sheet(SHEET)
        if args.catalog:
            refresh_catalog()
    finally:
        f.close()


if __name__ == "__main__":
    main()
