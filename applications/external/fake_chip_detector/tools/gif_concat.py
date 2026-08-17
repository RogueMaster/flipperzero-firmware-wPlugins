"""Join GIFs end to end, keeping every frame's own delay.

    python tools/gif_concat.py out.gif a.gif b.gif c.gif

ffmpeg's concat demuxer resamples to a constant frame rate, which throws away
the recorded timing -- the whole point of these captures is that the delays are
measured. This walks the frames instead.

All inputs must be the same size; a mismatch is an error rather than a silent
letterbox, because it means one of them was composed differently.
"""
import sys
import pathlib

from PIL import Image


def frames_of(path):
    im = Image.open(path)
    for i in range(im.n_frames):
        im.seek(i)
        yield im.convert('RGB'), im.info.get('duration', 100)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    dst, srcs = sys.argv[1], sys.argv[2:]

    images, delays, size = [], [], None
    for src in srcs:
        n = 0
        for img, ms in frames_of(src):
            if size is None:
                size = img.size
            elif img.size != size:
                raise SystemExit('%s is %dx%d, expected %dx%d'
                                 % ((src,) + img.size + size))
            images.append(img.convert('P', palette=Image.ADAPTIVE, colors=4))
            delays.append(ms)
            n += 1
        print('  %s: %d frames' % (pathlib.Path(src).name, n))

    images[0].save(dst, save_all=True, append_images=images[1:],
                   duration=delays, loop=0, optimize=True, disposal=2)
    print('%s: %d frames, %.1fs, %.0f KB'
          % (dst, len(images), sum(delays) / 1000.0,
             pathlib.Path(dst).stat().st_size / 1024.0))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
