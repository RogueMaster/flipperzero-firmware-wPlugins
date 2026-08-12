"""Record the Flipper's screen over RPC and write an animated GIF.

    python tools/capture_gif.py COM3 out.gif 20            # record 20 seconds
    python tools/capture_gif.py COM3 out.gif 20 --scale 4  # bigger pixels

Every frame in the output is a frame the device actually drew: the stream is
started once and read continuously, and identical consecutive frames are
collapsed into one longer-held frame rather than being invented or dropped.
The GIF's timing is the timing that was measured, not a constant guess.

flipper_rpc.py's screenshot() restarts the stream for each capture to be sure
it has the current screen, which costs about a second per frame. That is right
for stills and useless for an animation, so this reads the stream straight.
"""
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from PIL import Image  # noqa: E402  (flipper_rpc needs it too)

import flipper_rpc as F  # noqa: E402


def record(port_name, seconds, scale):
    """Return [(PIL image, milliseconds it was on screen), ...]."""
    rpc = F.FlipperRPC(port_name)
    frames = []
    try:
        rpc.start_stream()
        rpc.port.reset_input_buffer()
        deadline = time.time() + seconds
        last_bytes = None
        last_at = None
        while time.time() < deadline:
            try:
                msg = F.read_message(rpc.port, timeout_s=2.0)
            except (TimeoutError, ValueError):
                continue
            try:
                data = F.find_screen_frame(msg)
            except ValueError:
                continue
            if not data or len(data) < F.WIDTH * F.HEIGHT // 8:
                continue
            now = time.time()
            if data == last_bytes:
                continue  # same picture; it just stays up longer
            if last_at is not None:
                frames.append((last_bytes, int((now - last_at) * 1000)))
            last_bytes, last_at = data, now
        if last_bytes is not None:
            frames.append((last_bytes, 100))
    finally:
        rpc.close()
    return [(F.framebuffer_to_image(b, scale), ms) for b, ms in frames]


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    port_name, out, seconds = sys.argv[1], sys.argv[2], float(sys.argv[3])
    scale = 4
    if '--scale' in sys.argv:
        scale = int(sys.argv[sys.argv.index('--scale') + 1])

    frames = record(port_name, seconds, scale)
    if not frames:
        print('no frames captured -- is the app on screen?')
        return 1

    # GIF stores delays in hundredths of a second and most viewers refuse to go
    # below 20ms, so clamp rather than let a burst of fast frames play at a
    # speed no one will see.
    images = [im.convert('P', palette=Image.ADAPTIVE, colors=2) for im, _ in frames]
    delays = [max(40, min(6000, ms)) for _, ms in frames]

    images[0].save(
        out, save_all=True, append_images=images[1:],
        duration=delays, loop=0, optimize=True, disposal=2)
    total = sum(delays) / 1000.0
    print('%s: %d frames, %.1fs, %d bytes'
          % (out, len(images), total, pathlib.Path(out).stat().st_size))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
