#!/usr/bin/env python3
"""Capture real GhostTag screens and recordings off a Flipper Zero over serial RPC.

Every picture in this repository comes from the device. Mockup renderers are a
second implementation of the UI that can - and elsewhere in this family of apps,
did - disagree with the firmware while looking perfectly convincing. A capture
cannot.

    python3 tools_screenshot.py --all        # drive GhostTag, capture every screen
    python3 tools_screenshot.py --shot NAME  # grab whatever is on screen now
    python3 tools_screenshot.py --record N   # animated GIF of the live screen
    python3 tools_screenshot.py --tour-gif   # one GIF of a scripted walk
    python3 tools_screenshot.py --splash     # the boot intro, from launch onwards
    python3 tools_screenshot.py --sheet      # rebuild images/screens{,-dark}.png
    python3 tools_screenshot.py --catalog    # refresh screenshots/ssN.png aliases
    python3 tools_screenshot.py --verify     # assert every shot is two colours

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

GhostTag needs no ESP32 board to be photographed: the app ships a Demo mode that
synthesises a scripted stalking scenario on-device, and every screen it paints
is stamped DEMO. The tour drives that, so these captures are of real firmware
drawing real frames - the tracker data behind them is simulated, and the screens
say so themselves.

Screens are written at 4x (512x256) in the device's own two colours, which is
both what the Flipper Apps Catalog accepts and what the branding rules require.

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

FAP_PATH = "/ext/apps/Bluetooth/ghosttag.fap"

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
    return sum(1 for y in range(top, bottom) for x in range(128) if px[x, y] == 0) / (128.0 * rows)


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


# -------------------------------------------------------------------- gifs ---
def _write_gif(frames, out, fps, scale, hold_cap_ms=2600):
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


# ------------------------------------------------------------------- drive ---
def alert_banner_lit(img):
    """True on an alert screen while its banner is inverted.

    alert_view.c fills rows 0..13 solid and knocks the title out in white, so a
    majority-ink top band means an alert is up. The banner strobes at 2.5 Hz,
    so a poll only has to be faster than that to land on a lit frame.
    """
    return ink_fraction(img, 0, 14) > 0.5


def restart_app(f, fap=FAP_PATH):
    """Put the device in a known state: freshly launched GhostTag, main menu.

    The tour navigates by relative key presses, so it only lines up if it
    starts from a known screen with a known menu cursor - and the submenu WRAPS,
    so "press Up plenty of times to get to the top" walks the cursor to an
    arbitrary row instead. A fresh launch is the only reliable anchor.
    """
    for _ in range(8):
        f.press("back", settle=0.2)
    f.idle(0.5)
    f.flush()
    f.app_start(fap)
    # The launch request returns before the app paints, so the first frames off
    # the wire are still the Flipper desktop. Counting frames is unreliable, so
    # discard by CONTENT: the desktop's dolphin art is a large dark scene while
    # every GhostTag screen is mostly unlit.
    for _ in range(25):
        d = f._await_frame(0.4)
        if d is not None and ink_fraction(to_image(d)) < 0.40:
            break
    # Then sit out the boot intro by DURATION. Content cannot tell the intro
    # from the menu - both are mostly unlit. SPLASH_FRAMES is 24 at a 100 ms
    # tick, so 2.4 s plus a margin.
    f.idle(3.0)
    f.flush()


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


def wait_for_alert(f, seconds=60):
    """Sit on the demo radar until the scenario actually trips an alert.

    Waiting for the real thing beats timing it: the banner only inverts when
    the follow heuristic has genuinely promoted a tracker, so whatever this
    catches is the app's own verdict rather than a guess about when it is due.
    """
    print(f"  >>> waiting for the demo scenario to trip an alert (up to {seconds}s) ...")
    deadline = time.time() + seconds
    tick = time.time() + 5.0
    while time.time() < deadline:
        data = f.frame(timeout=1.5, nudge=False)
        if data is None:
            continue
        img = to_image(data)
        if alert_banner_lit(img):
            print("      alert is up")
            return img
        if time.time() >= tick:
            print("      not yet - the AirTag needs ~25s of dwell", flush=True)
            tick = time.time() + 5.0
    print("  !! no alert seen")
    return None


# Main menu rows, in order. The submenu remembers the row you activated, so
# BACK returns the cursor to it - the tour tracks that rather than counting.
ROW_HUNT, ROW_AIR, ROW_DEMO, ROW_DETECTIONS, ROW_SETTINGS, ROW_ABOUT = range(6)


def step_to(f, frm, to):
    """Walk the menu cursor without relying on wrap-around."""
    for _ in range(abs(to - frm)):
        f.press("down" if to > frm else "up")
    return to


def run_tour(f, fap=FAP_PATH, relaunch=True):
    """Walk every screen, capturing each one."""
    if relaunch:
        restart_app(f, fap)
    cursor = ROW_HUNT

    settle_shot(f, "menu")

    # -- Hunt with no board attached. This is what most people see first, and
    #    it is the screen the app was previously worst at explaining.
    f.press("ok")
    settle_shot(f, "radar_noboard", wait=1.2)
    f.press("back")

    # -- Air Check: the onboard radio, measuring band energy. Give it a few
    #    seconds so the bars are built on real samples rather than zeros.
    cursor = step_to(f, cursor, ROW_AIR)
    f.press("ok")
    f.idle(7.0)
    settle_shot(f, "air", wait=0.6)
    f.press("back")

    # -- Demo: the scripted scenario, through the real UI.
    cursor = step_to(f, cursor, ROW_DEMO)
    f.press("ok")
    f.idle(9.0)  # let the cast arrive and the dial fill
    settle_shot(f, "radar_demo", wait=0.6)

    img = wait_for_alert(f)
    if img is not None:
        save(img, "alert")
        # OK on the alert pops it and opens the detail screen.
        f.press("ok")
        settle_shot(f, "detail", wait=0.8)
        f.press("back")  # detail -> radar

    # -- Detections list, live, mid-demo.
    f.press("ok")
    settle_shot(f, "list", wait=0.8)
    f.press("back")  # list -> radar
    f.press("back")  # radar -> menu (cursor back on Demo)
    cursor = ROW_DEMO

    # -- Settings.
    cursor = step_to(f, cursor, ROW_SETTINGS)
    f.press("ok")
    settle_shot(f, "settings")
    f.press("back")

    # -- Help & About.
    cursor = step_to(f, cursor, ROW_ABOUT)
    f.press("ok")
    settle_shot(f, "about")
    f.press("back")


def splash_capture(f, fps=10, scale=3, fap=FAP_PATH):
    """Record the boot intro, from the launch request onwards.

    Launching over RPC rather than with `loader open` is the whole trick: the
    screen stream stays up across the launch, which is the only way to catch
    the first frames an app ever paints.
    """
    for _ in range(8):
        f.press("back", settle=0.2)
    f.idle(0.5)
    f.flush()
    f.app_start(fap)

    frames = []
    started = False
    end = time.time() + 6.0
    while time.time() < end:
        d = f._await_frame(0.5)
        if d is None:
            continue
        img = to_image(d)
        if not started:
            # Skip the desktop's dolphin art by ink, then keep everything.
            if ink_fraction(img) >= 0.40:
                continue
            started = True
        frames.append(img)
        if len(frames) >= int(3.4 * fps):
            break
    if not frames:
        print("  !! no intro frames")
        return None
    save(frames[len(frames) // 2], "splash")
    return _write_gif(frames, os.path.join(IMAGES, "splash.gif"), fps, scale)


def tour_gif(f, name="demo", fps=10, scale=3):
    """One GIF of a scripted walk: menu -> demo -> alert -> detail -> list."""
    frames = []

    def grab(seconds, note, watch=None):
        print(f"    {note}")
        f.idle(seconds, on_frame=lambda d: frames.append(to_image(d)))
        if watch is not None and frames and not watch(frames[-1]):
            pass

    restart_app(f)
    grab(1.8, "main menu")

    f.press("down", settle=0.0)
    grab(1.0, "cursor to Demo")
    f.press("ok", settle=0.0)
    grab(10.0, "demo radar - the cast arrives")

    # The alert is the point of the whole walk, so wait for the real one
    # rather than hoping it lands inside a fixed window.
    print("    waiting for the alert ...")
    deadline = time.time() + 45
    got = False
    while time.time() < deadline:
        d = f._await_frame(1.0)
        if d is None:
            continue
        img = to_image(d)
        frames.append(img)
        if alert_banner_lit(img):
            got = True
            break
    if got:
        grab(3.0, "alert strobing")
        f.press("ok", settle=0.0)
        grab(3.5, "detail for the follower")
        f.press("back", settle=0.0)
        grab(1.0, "back to the dial")
    else:
        print("    !! no alert during the walk")

    f.press("ok", settle=0.0)
    grab(3.5, "detections list")
    f.press("back", settle=0.0)
    f.press("back", settle=0.0)
    grab(1.8, "home - done")

    if not frames:
        print("  !! nothing recorded")
        return None
    return _write_gif(frames, os.path.join(IMAGES, f"{name}.gif"), fps, scale)


# What goes on the README contact sheet, in reading order.
SHEET = [
    ("menu", "Main menu"),
    ("radar_demo", "Radar - demo hunt"),
    ("alert", "Tracker alert"),
    ("detail", "Per-device detail"),
    ("list", "Detections"),
    ("radar_noboard", "Hunt with no board"),
    ("air", "Air Check - onboard radio"),
    ("settings", "Settings"),
    ("splash", "Boot intro"),
    # Help & About is deliberately absent: it prints the version number, which
    # makes it the one capture guaranteed to be wrong by the next release.
]

# The aliases the Flipper Apps Catalog manifest points at. Keeping them here
# means the catalog never has to be re-pointed when a capture is retaken.
CATALOG = {
    "ss0": "radar_demo",
    "ss1": "alert",
    "ss2": "list",
    "ss3": "detail",
    "ss4": "air",
    "ss5": "radar_noboard",
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


# -------------------------------------------------------------------- main ---
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--port", help="serial port (default: autodetect)")
    ap.add_argument("--all", action="store_true", help="drive GhostTag and capture every screen")
    ap.add_argument("--shot", metavar="NAME", help="capture whatever is on screen right now")
    ap.add_argument("--record", metavar="NAME", help="record an animated GIF of the live screen")
    ap.add_argument("--tour-gif", action="store_true", help="record a GIF of a scripted walk")
    ap.add_argument("--splash", action="store_true", help="record the boot intro")
    ap.add_argument("--sheet", action="store_true", help="rebuild images/screens{,-dark}.png")
    ap.add_argument("--catalog", action="store_true", help="refresh screenshots/ssN.png aliases")
    ap.add_argument("--verify", action="store_true", help="check every screenshot is two colours")
    ap.add_argument("--seconds", type=float, default=6.0, help="recording length (default 6)")
    ap.add_argument("--fps", type=int, default=10, help="recording frame rate (default 10)")
    ap.add_argument("--launch", metavar="FAP", help="start this app over RPC first")
    ap.add_argument("--no-launch", action="store_true", help="capture without restarting the app")
    args = ap.parse_args()

    offline = False
    if args.verify:
        offline = True
        bad = [p for p in sorted(glob.glob(os.path.join(SHOTS, "*.png"))) if not two_colours_only(p)]
        for p in bad:
            print(f"  !! {os.path.relpath(p, HERE)} is not two colours")
        print("all screenshots are the device's two colours" if not bad else f"{len(bad)} bad")
    if args.catalog:
        offline = True
        refresh_catalog()
    if args.sheet and not (args.all or args.shot):
        offline = True
        contact_sheet(SHEET)

    live = args.all or args.shot or args.record or args.tour_gif or args.splash
    if offline and not live:
        return
    if not live:
        ap.print_help()
        return

    f = Flipper(args.port)
    try:
        f.start_stream()
        if args.splash:
            splash_capture(f, fap=args.launch or FAP_PATH)
        elif args.tour_gif:
            tour_gif(f)
        elif args.all:
            # run_tour restarts the app itself - the tour navigates by relative
            # presses and a fresh launch is its only reliable anchor.
            run_tour(f, fap=args.launch or FAP_PATH, relaunch=not args.no_launch)
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
