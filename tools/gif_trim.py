"""Cut a span out of a GIF without disturbing the remaining frame delays.

    python tools/gif_trim.py in.gif out.gif 2.0 13.0

Start and end are seconds in the GIF's own timeline. The frame that is on
screen at `start` is kept whole, with its delay shortened to the part that
falls inside the span -- so the cut lands where it was asked for rather than on
the nearest frame boundary.
"""
import sys
import pathlib

from PIL import Image


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    start = float(sys.argv[3]) * 1000
    end = float(sys.argv[4]) * 1000

    im = Image.open(src)
    images, delays, t = [], [], 0
    for i in range(im.n_frames):
        im.seek(i)
        ms = im.info.get('duration', 100)
        lo, hi = max(t, start), min(t + ms, end)
        if hi > lo:
            images.append(im.convert('RGB').convert(
                'P', palette=Image.ADAPTIVE, colors=4))
            delays.append(max(40, int(hi - lo)))
        t += ms

    if not images:
        raise SystemExit('nothing in %.1f..%.1fs (clip is %.1fs)'
                         % (start / 1000, end / 1000, t / 1000))
    images[0].save(dst, save_all=True, append_images=images[1:],
                   duration=delays, loop=0, optimize=True, disposal=2)
    print('%s: %d frames, %.1fs, %.0f KB'
          % (dst, len(images), sum(delays) / 1000.0,
             pathlib.Path(dst).stat().st_size / 1024.0))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
