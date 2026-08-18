"""Exact canvas_string_width() for Flipper's FontSecondary, off the device.

Counting characters does not predict pixels in this font. "Temp/humidity sensor"
is 20 characters and overflows the narrowest slot by one pixel; "Magnetic angle
sensor" is 21 and fits with two to spare. So screen text gets measured, not
estimated -- and this is what measures it.

    python tools/screen_width.py "Jumper pin 15 to pin 16,"

How it is exact: canvas.c maps FontSecondary to u8g2_font_haxrcorp4089_tr and
canvas_string_width to u8g2_GetUTF8Width, so this reimplements u8g2_string_width
from the vendored lib/u8g2/u8g2_font.c. Flipper's copy predates
U8G2_BALANCED_STR_WIDTH_CALCULATION, so that branch is absent here too, and the
u8g2 issue #16/#46 tail adjustment is present.

The font bytes are read out of the SDK's own firmware.elf rather than kept in
this repo. That costs a dependency on ufbt being installed, and buys two things:
no third-party font data vendored here, and the numbers come from the firmware
the app is actually built against instead of a copy that can quietly drift from
it.

Deliberately not wired into CI. There is no invariant here to enforce -- the
character cap in gen_supported_chips.py is the tripwire, and this is the
instrument you reach for when the tripwire fires.
"""
import os
import struct
import sys
from pathlib import Path

FONT_SYMBOL = 'u8g2_font_haxrcorp4089_tr'


def sdk_firmware_elf():
    """Where ufbt keeps the firmware the app links against."""
    home = os.environ.get('UFBT_HOME')
    root = Path(home) if home else Path.home() / '.ufbt'
    elf = root / 'current' / 'firmware.elf'
    if not elf.is_file():
        raise SystemExit(
            'no firmware.elf at %s\n'
            'This reads the font out of the ufbt SDK. Run `ufbt update` first, '
            'or point UFBT_HOME at an existing one.' % elf)
    return elf


def elf_symbol_bytes(path, symbol):
    """The bytes of one symbol out of a 32-bit little-endian ELF.

    Hand-rolled rather than pyelftools: this reads one symbol from one file and
    a dependency nobody has installed is a tool nobody runs.
    """
    d = path.read_bytes()
    if d[:4] != b'\x7fELF':
        raise SystemExit('%s is not an ELF' % path)
    if d[4] != 1 or d[5] != 1:
        raise SystemExit('%s is not 32-bit little-endian; this parser only does f7' % path)

    e_shoff, = struct.unpack_from('<I', d, 0x20)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', d, 0x2E)
    fields = ('name', 'type', 'flags', 'addr', 'offset', 'size', 'link', 'info',
              'align', 'entsize')
    sections = [dict(zip(fields, struct.unpack_from('<10I', d, e_shoff + i * e_shentsize)))
                for i in range(e_shnum)]

    def name_at(strtab, off):
        end = d.index(b'\0', strtab['offset'] + off)
        return d[strtab['offset'] + off:end].decode('latin-1')

    shstr = sections[e_shstrndx]
    by_name = {name_at(shstr, s['name']): s for s in sections}
    symtab = by_name.get('.symtab')
    if not symtab:
        raise SystemExit('%s has no .symtab -- it has been stripped' % path)
    strtab = sections[symtab['link']]

    for i in range(symtab['size'] // symtab['entsize']):
        off = symtab['offset'] + i * symtab['entsize']
        st_name, st_value, st_size, _, _, st_shndx = struct.unpack_from('<IIIBBH', d, off)
        if name_at(strtab, st_name) != symbol:
            continue
        sec = sections[st_shndx]
        start = sec['offset'] + (st_value - sec['addr'])
        return d[start:start + st_size]
    raise SystemExit('%s holds no symbol %s' % (path, symbol))


class Bits:
    """u8g2_font_decode_get_unsigned_bits: LSB-first, spilling into the next byte."""

    def __init__(self, data, pos):
        self.d, self.p, self.b = data, pos, 0

    def u(self, count):
        v = 0
        for k in range(count):
            v |= ((self.d[self.p] >> self.b) & 1) << k
            self.b += 1
            if self.b == 8:
                self.b = 0
                self.p += 1
        return v

    def s(self, count):
        return self.u(count) - (1 << (count - 1))


class Font:
    """One u8g2 font, far enough decoded to measure with."""

    STRUCT_SIZE = 23  # U8G2_FONT_DATA_STRUCT_SIZE

    def __init__(self, data):
        self.d = data
        (self.glyph_cnt, self.bbx_mode, self.bits_0, self.bits_1,
         self.bits_w, self.bits_h, self.bits_x, self.bits_y,
         self.bits_dx, self.max_w, self.max_h) = data[0:11]
        self.ascent_A = data[13]
        self.descent_g = data[14] - 256 if data[14] > 127 else data[14]
        # The glyph table is linear: [encoding][jump][packed data], and a jump
        # of zero ends it.
        self.glyphs = {}
        p = self.STRUCT_SIZE
        while data[p + 1] != 0:
            self.glyphs[data[p]] = p + 2
            p += data[p + 1]

    def metrics(self, ch):
        """(delta_x, glyph_width, x_offset) -- what u8g2_GetGlyphWidth returns
        plus the two values it sets as side effects."""
        pos = self.glyphs.get(ord(ch))
        if pos is None:
            raise KeyError(
                '%r (0x%02X) is not in this font. The _tr variant carries the 95 '
                'printable ASCII glyphs and nothing else -- no accents, no arrows, '
                'no typographic dashes.' % (ch, ord(ch)))
        b = Bits(self.d, pos)
        glyph_w = b.u(self.bits_w)
        b.u(self.bits_h)
        x_offset = b.s(self.bits_x)
        b.s(self.bits_y)
        delta_x = b.s(self.bits_dx)
        return delta_x, glyph_w, x_offset

    def width(self, text):
        w = delta_x = glyph_w = x_offset = 0
        for ch in text:
            delta_x, glyph_w, x_offset = self.metrics(ch)
            w += delta_x
        if text and glyph_w != 0:  # u8g2 issue #16 / #46 tail adjustment
            w += glyph_w + x_offset - delta_x
        return w


# Where text actually lands, narrowest first. The numbers are what is left of
# the 128 after the x the string starts at, so a string fits its slot when its
# width is no greater than the budget.
#
# NOT YOURS is the one that binds, and it is worth knowing why: it prints an
# article in front of the kind, so "a magnetic angle sensor" is what has to fit,
# not "Magnetic angle sensor".
SLOTS = [
    ('not-yours', 28, 'NOT YOURS verdict -- article + kind'),
    ('scan-row', 26, 'a result row on the scan screen'),
    ('line', 2, 'an ordinary full-width line of body text'),
    ('centred', 0, 'anything drawn with canvas_draw_str_aligned centred'),
]

_font = None


def font():
    """Loaded once, and only if something actually asks to measure."""
    global _font
    if _font is None:
        _font = Font(elf_symbol_bytes(sdk_firmware_elf(), FONT_SYMBOL))
    return _font


def width(text):
    return font().width(text)


def main(argv):
    f = font()
    # Printed so the decode can be checked against canvas.c rather than trusted:
    # FontSecondary is declared there with ascent 7 and descent -2.
    print('%s: %d glyphs, ascent_A=%d descent_g=-%d'
          % (FONT_SYMBOL, len(f.glyphs), f.ascent_A, -f.descent_g))
    if not argv:
        print('\nusage: python tools/screen_width.py "some screen text" ...')
        return 0
    for text in argv:
        try:
            w = f.width(text)
        except KeyError as e:
            print('\n%r\n  %s' % (text, e.args[0]))
            continue
        print('\n%r is %dpx' % (text, w))
        for name, x, what in SLOTS:
            budget = 128 - x
            print('  %-10s %3d avail  %+4d  %s' % (name, budget, budget - w, what))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
