"""Drive the app over RPC and record the screen to a GIF in one pass.

    python tools/demo_gif.py COM3 out.gif "0.8:down, 1.6:ok, 5:ok, 8:ok" 22

One serial port means one process: the presses and the frame capture have to
share a connection, so this interleaves them instead of running two scripts.
The third argument is a timeline of `seconds:key` pairs measured from the
start of the recording; the fourth is how long to record for.

Every frame written is a frame the device drew. Identical consecutive frames
collapse into one held longer, and the delays are the measured ones -- nothing
is invented and nothing is resampled to a pretty constant rate.
"""
import sys
import time
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from PIL import Image  # noqa: E402

import flipper_rpc as F  # noqa: E402


def read_frame_if_any(port, idle_s=0.05):
    """One RPC message, or None if the port is quiet.

    read_message() aborts a half-read body when its deadline passes, which
    desynchronises the stream. Only start reading once bytes are actually
    waiting, then let it finish.
    """
    if not port.in_waiting:
        time.sleep(idle_s)
        return None
    try:
        return F.read_message(port, timeout_s=2.0)
    except (TimeoutError, ValueError):
        return None


def run(port_name, timeline, seconds, scale):
    rpc = F.FlipperRPC(port_name)
    frames = []
    try:
        rpc.start_stream()
        rpc.port.reset_input_buffer()
        t0 = time.time()
        pending = sorted(timeline)
        last_bytes, last_at = None, None

        while True:
            now = time.time()
            elapsed = now - t0
            if elapsed >= seconds:
                break
            if pending and elapsed >= pending[0][0]:
                _, key = pending.pop(0)
                print('  t=%.1f  %s' % (elapsed, key))
                rpc.press(key)
                continue
            msg = read_frame_if_any(rpc.port)
            if msg is None:
                continue
            try:
                data = F.find_screen_frame(msg)
            except ValueError:
                continue
            if not data or len(data) < F.WIDTH * F.HEIGHT // 8:
                continue
            if data == last_bytes:
                continue
            if last_at is not None:
                frames.append((last_bytes, int((now - last_at) * 1000)))
            last_bytes, last_at = data, now
        if last_bytes is not None:
            # However long the last screen was actually up for. A fixed tail
            # would cut a final verdict short precisely when the recording was
            # extended to let someone read it.
            frames.append((last_bytes, int((time.time() - last_at) * 1000)))
    finally:
        rpc.close()
    return [(F.framebuffer_to_image(b, scale), ms) for b, ms in frames]


def save(frames, out):
    # The framebuffer arrives as mode "1", which quantize() refuses; go through
    # "L" so the palette conversion has something to count.
    images = [im.convert('L').convert('P', palette=Image.ADAPTIVE, colors=2)
              for im, _ in frames]
    # Most viewers will not honour a delay under 20ms, so there is a floor. The
    # ceiling is generous on purpose: a verdict that stayed on screen for four
    # seconds should stay on screen for four seconds. Clamping it to two was
    # squashing exactly the screens a viewer needs time to read.
    delays = [max(40, min(6000, ms)) for _, ms in frames]
    images[0].save(
        out, save_all=True, append_images=images[1:],
        duration=delays, loop=0, optimize=True, disposal=2)
    print('%s: %d frames, %.1fs, %.0f KB'
          % (out, len(images), sum(delays) / 1000.0,
             pathlib.Path(out).stat().st_size / 1024.0))


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    port_name, out, spec, seconds = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4])
    scale = int(sys.argv[5]) if len(sys.argv) > 5 else 4

    timeline = []
    for part in spec.split(','):
        part = part.strip()
        if not part:
            continue
        at, key = part.split(':')
        timeline.append((float(at), key.strip()))

    frames = run(port_name, timeline, seconds, scale)
    if not frames:
        print('no frames captured')
        return 1
    save(frames, out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
