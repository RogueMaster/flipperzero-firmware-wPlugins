#!/usr/bin/env python3
"""Build a binary deck for Hanzi Cards from a TSV word list.

    python tools/build_deck.py decks/hsk1.tsv assets/hsk1.deck [--preview out.png]

Characters are rendered from Noto Sans CJK (SC for simplified, TC for
traditional), expected in fonts/. Get them from
https://github.com/notofonts/noto-cjk/tree/main/Sans/OTF or pass --font and
--trad-font.

Deck layout (little-endian):
    header  "HZD2", u16 cards, u16 glyphs, u8 glyph size, 3 pad bytes
    cards   u16 simplified[4], u16 traditional[4] (0xFFFF = unused),
            char pinyin[24], char english[24]
    glyphs  size*size/8 bytes each, rows top to bottom, bit 0 = leftmost pixel
"""
import argparse
import os
import re
import struct

from PIL import Image, ImageDraw, ImageFont

GLYPH = 32
MAX_HANZI = 4
TEXT_LEN = 24
MAX_ENGLISH = 22
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_FONT = os.path.join(ROOT, "fonts", "NotoSansCJKsc-Regular.otf")
DEFAULT_TRAD_FONT = os.path.join(ROOT, "fonts", "NotoSansCJKtc-Regular.otf")
SYLLABLE = re.compile(r"^[a-z]+[1-4]?$")


def read_words(path):
    words = []
    seen = set()
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            where = f"{path}:{n}"
            hanzi, trad, pinyin, english = line.split("\t")
            if trad == "=":
                trad = hanzi
            assert hanzi not in seen, f"{where}: duplicate {hanzi}"
            seen.add(hanzi)
            assert 1 <= len(hanzi) <= MAX_HANZI, f"{where}: 1-{MAX_HANZI} characters"
            assert len(trad) == len(hanzi), f"{where}: traditional length differs"
            syllables = pinyin.split(" ")
            assert all(SYLLABLE.match(s) for s in syllables), f"{where}: bad pinyin"
            assert len(pinyin) < TEXT_LEN, f"{where}: pinyin too long"
            assert (
                len(english) <= MAX_ENGLISH and english.isascii()
            ), f"{where}: english"
            words.append((hanzi, trad, pinyin, english))
    return words


def render(font, char):
    """Return a GLYPH x GLYPH 1-bit image, or None if the glyph is clipped."""
    big = Image.new("1", (GLYPH * 2, GLYPH * 2), 0)
    draw = ImageDraw.Draw(big)
    draw.fontmode = "1"
    draw.text((GLYPH, GLYPH), char, font=font, fill=1, anchor="mm")
    box = big.getbbox()
    assert box, f"font has no glyph for {char}"
    off = GLYPH // 2
    if box[0] < off or box[1] < off or box[2] > off + GLYPH or box[3] > off + GLYPH:
        return None
    return big.crop((off, off, off + GLYPH, off + GLYPH))


def pack(img):
    out = bytearray()
    px = img.load()
    for y in range(GLYPH):
        for bx in range(GLYPH // 8):
            byte = 0
            for bit in range(8):
                if px[bx * 8 + bit, y]:
                    byte |= 1 << bit
            out.append(byte)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tsv")
    ap.add_argument("deck")
    ap.add_argument(
        "--font", default=DEFAULT_FONT, help="font for simplified characters"
    )
    ap.add_argument(
        "--trad-font",
        default=DEFAULT_TRAD_FONT,
        help="font for traditional-only characters",
    )
    ap.add_argument("--preview", help="write a PNG sheet of all glyphs")
    args = ap.parse_args()

    words = read_words(args.tsv)
    # Simplified characters first, then the ones only the traditional forms use
    chars = []
    for column in (0, 1):
        for word in words:
            for c in word[column]:
                if c not in chars:
                    chars.append(c)
    simplified = {c for word in words for c in word[0]}

    # Largest size at which every glyph fits the cell
    for size in range(GLYPH, GLYPH - 8, -1):
        fonts = {
            True: ImageFont.truetype(args.font, size),
            False: ImageFont.truetype(args.trad_font, size),
        }
        images = [render(fonts[c in simplified], c) for c in chars]
        if all(images):
            break
    else:
        raise SystemExit("glyphs do not fit")

    def indices(text):
        return [chars.index(c) for c in text] + [0xFFFF] * (MAX_HANZI - len(text))

    with open(args.deck, "wb") as f:
        f.write(struct.pack("<4sHHB3x", b"HZD2", len(words), len(chars), GLYPH))
        for hanzi, trad, pinyin, english in words:
            f.write(
                struct.pack(
                    f"<{MAX_HANZI * 2}H{TEXT_LEN}s{TEXT_LEN}s",
                    *indices(hanzi),
                    *indices(trad),
                    pinyin.encode(),
                    english.encode(),
                )
            )
        for img in images:
            f.write(pack(img))

    if args.preview:
        cols = 16
        rows = (len(chars) + cols - 1) // cols
        sheet = Image.new("1", (cols * (GLYPH + 2), rows * (GLYPH + 2)), 0)
        for i, img in enumerate(images):
            sheet.paste(
                img, ((i % cols) * (GLYPH + 2) + 1, (i // cols) * (GLYPH + 2) + 1)
            )
        sheet.resize((sheet.width * 2, sheet.height * 2)).save(args.preview)

    print(
        f"{len(words)} words, {len(chars)} characters, font size {size} -> {args.deck}"
    )


if __name__ == "__main__":
    main()
