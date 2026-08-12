"""Add a caption bar under a recorded screen GIF.

    python tools/gif_captions.py in.gif out.gif "0:What the app is" "6.5:Scan the bus"

Each argument after the output is `seconds:text`, and the caption applies from
that time until the next one. Time is measured in the GIF's own timeline -- the
frame delays that were recorded -- so the captions land on the frames that were
on screen at that moment rather than on a frame index.

The bar is drawn below the screen, never over it: a 128x64 screen has no room
to spare, and text on top of a reading is text on top of the evidence.
"""
import sys
import pathlib

from PIL import Image, ImageDraw, ImageFont

BAR_H = 52
FONT_PATH = 'C:/Windows/Fonts/arialbd.ttf'
FONT_SIZE = 26


def load_frames(path):
    """[(RGB image, duration ms), ...] -- GIF frames are palettised and may be
    partial, so composite each onto the previous rather than trusting one."""
    im = Image.open(path)
    out = []
    for i in range(im.n_frames):
        im.seek(i)
        out.append((im.convert('RGB'), im.info.get('duration', 100)))
    return out


def caption_at(captions, t_ms):
    text = ''
    for at_ms, s in captions:
        if t_ms + 1 >= at_ms:
            text = s
        else:
            break
    return text


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    src, dst = sys.argv[1], sys.argv[2]
    captions = []
    for spec in sys.argv[3:]:
        at, text = spec.split(':', 1)
        captions.append((int(float(at) * 1000), text.strip()))
    captions.sort()

    frames = load_frames(src)
    w, h = frames[0][0].size
    font = ImageFont.truetype(FONT_PATH, FONT_SIZE)

    out, delays, t = [], [], 0
    for img, ms in frames:
        canvas = Image.new('RGB', (w, h + BAR_H), 'black')
        canvas.paste(img, (0, 0))
        text = caption_at(captions, t)
        if text:
            d = ImageDraw.Draw(canvas)
            tw = d.textbbox((0, 0), text, font=font)[2]
            if tw > w - 16:
                raise SystemExit(
                    'caption does not fit (%dpx > %dpx): %r' % (tw, w - 16, text))
            d.text(((w - tw) // 2, h + (BAR_H - FONT_SIZE) // 2 - 3),
                   text, font=font, fill='white')
        # Four colours: black, white, and whatever antialiasing the text needs.
        out.append(canvas.convert('P', palette=Image.ADAPTIVE, colors=4))
        delays.append(ms)
        t += ms

    out[0].save(dst, save_all=True, append_images=out[1:],
                duration=delays, loop=0, optimize=True, disposal=2)
    print('%s: %d frames, %.1fs, %.0f KB'
          % (dst, len(out), sum(delays) / 1000.0,
             pathlib.Path(dst).stat().st_size / 1024.0))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
