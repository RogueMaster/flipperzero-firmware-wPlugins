#!/usr/bin/env python3
"""Generate binary Flipper BadUSB ASCII HID maps from xkeyboard-config/XKB.
These are NOT Windows tables; do not call them Windows Legacy.
Source: /usr/share/X11/xkb/symbols, libxkbcommon 1.x, model pc105.
"""
from pathlib import Path
import ctypes as C
import struct
import sys


class XkbNames(C.Structure):
    _fields_ = [
        ("rules", C.c_char_p),
        ("model", C.c_char_p),
        ("layout", C.c_char_p),
        ("variant", C.c_char_p),
        ("options", C.c_char_p),
    ]


so = C.CDLL("libxkbcommon.so.0")
for n, ret, args in [
    ("xkb_context_new", C.c_void_p, [C.c_int]),
    ("xkb_context_unref", None, [C.c_void_p]),
    (
        "xkb_keymap_new_from_names",
        C.c_void_p,
        [C.c_void_p, C.POINTER(XkbNames), C.c_int],
    ),
    ("xkb_keymap_unref", None, [C.c_void_p]),
    ("xkb_keymap_min_keycode", C.c_uint32, [C.c_void_p]),
    ("xkb_keymap_max_keycode", C.c_uint32, [C.c_void_p]),
    ("xkb_keymap_key_get_name", C.c_char_p, [C.c_void_p, C.c_uint32]),
    (
        "xkb_keymap_key_get_syms_by_level",
        C.c_int,
        [
            C.c_void_p,
            C.c_uint32,
            C.c_uint32,
            C.c_uint32,
            C.POINTER(C.POINTER(C.c_uint32)),
        ],
    ),
    ("xkb_keysym_to_utf32", C.c_uint32, [C.c_uint32]),
]:
    f = getattr(so, n)
    f.restype = ret
    f.argtypes = args

keys = {
    "TLDE": 0x35,
    "BKSL": 0x31,
    "LSGT": 0x64,
    "SPCE": 0x2C,
    "TAB": 0x2B,
    "RTRN": 0x28,
    "BKSP": 0x2A,
}
for i, key in enumerate(
    ["AE01", "AE02", "AE03", "AE04", "AE05", "AE06", "AE07", "AE08", "AE09", "AE10"]
):
    keys[key] = 0x1E + i
keys.update(AE11=0x2D, AE12=0x2E)
for key, val in zip(
    [
        "AD01",
        "AD02",
        "AD03",
        "AD04",
        "AD05",
        "AD06",
        "AD07",
        "AD08",
        "AD09",
        "AD10",
        "AD11",
        "AD12",
    ],
    [0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C, 0x12, 0x13, 0x2F, 0x30],
):
    keys[key] = val
for key, val in zip(
    [
        "AC01",
        "AC02",
        "AC03",
        "AC04",
        "AC05",
        "AC06",
        "AC07",
        "AC08",
        "AC09",
        "AC10",
        "AC11",
    ],
    [0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F, 0x33, 0x34],
):
    keys[key] = val
for key, val in zip(
    ["AB01", "AB02", "AB03", "AB04", "AB05", "AB06", "AB07", "AB08", "AB09", "AB10"],
    [0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36, 0x37, 0x38],
):
    keys[key] = val

# Only variants whose XKB layouts are clear/different. Use names that signal
# they are XKB-derived and not necessarily identical to Windows presets.
variants = [
    ("en-US-Colemak", "us", "colemak"),
    ("en-US-ColemakDH", "us", "colemak_dh"),
    ("en-US-Workman", "us", "workman"),
    ("en-US-DvorakLeft", "us", "dvorak-l"),
    ("en-UK-Colemak", "gb", "colemak"),
    ("en-UK-Dvorak", "gb", "dvorak"),
    ("fr-FR-Bepo", "fr", "bepo"),
    ("de-DE-Dvorak", "de", "dvorak"),
    ("de-DE-Qwerty", "de", "qwerty"),
    ("es-ES-Dvorak", "es", "dvorak"),
    ("es-ES-NoDead", "es", "nodeadkeys"),
    ("it-IT-Dvorak", "it", "dvorak"),
    ("it-IT-NoDead", "it", "nodeadkeys"),
    ("pt-PT-Colemak", "pt", "colemak"),
    ("pt-BR-Dvorak", "br", "dvorak"),
]


def generate(ctx, layout, variant):
    kn = XkbNames(None, b"pc105", layout.encode(), variant.encode(), None)
    km = so.xkb_keymap_new_from_names(ctx, C.byref(kn), 0)
    if not km:
        raise RuntimeError(f"Cannot load {layout}({variant})")
    keynames = {
        so.xkb_keymap_key_get_name(km, k): k
        for k in range(so.xkb_keymap_min_keycode(km), so.xkb_keymap_max_keycode(km) + 1)
    }
    result = [0] * 128
    result[8] = 0x2A
    result[9] = 0x2B
    result[10] = 0x28
    result[13] = 0x28
    result[32] = 0x2C
    entries = []
    # level 0=plain, 1=shift, 2=AltGr, 3=AltGr+Shift (pc105 XKB conventions)
    for lv, mod in enumerate((0, 0x02, 0x40, 0x42)):
        for name, hid in keys.items():
            kc = keynames.get(name.encode())
            if kc is None:
                continue
            syms = C.POINTER(C.c_uint32)()
            n = so.xkb_keymap_key_get_syms_by_level(km, kc, 0, lv, C.byref(syms))
            if n != 1:
                continue  # composed key sequence impossible in stock kl
            u = so.xkb_keysym_to_utf32(syms[0])
            if 33 <= u <= 126 and not result[u]:
                result[u] = (mod << 8) | hid
                entries.append((chr(u), name, lv, hid, mod))
    so.xkb_keymap_unref(km)
    return result, entries


ctx = so.xkb_context_new(0)
base = Path(__file__).resolve().parent.parent / "keyboard_layouts"
summary = []
out = Path(sys.argv[1]) if len(sys.argv) > 1 else base
out.mkdir(exist_ok=True, parents=True)
for name, layout, variant in variants:
    m, e = generate(ctx, layout, variant)
    have = sum(bool(m[ch]) for ch in range(32, 127))
    file = out / (name + ".kl")
    blob = struct.pack("<128H", *m)
    assert len(blob) == 256
    file.write_bytes(blob)
    ref = base / (name[:5] + ".kl")
    if name.startswith("en-UK"):
        ref = base / "en-UK.kl"
    elif name.startswith("fr-CA"):
        ref = base / "fr-CA.kl"
    elif name.startswith("pt-BR"):
        ref = base / "pt-BR.kl"
    elif name.startswith("en-US"):
        ref = base / "en-US.kl"
    diff = (
        sum(a != b for a, b in zip(blob, ref.read_bytes())) // 2 if ref.exists() else -1
    )
    missing = "".join(chr(ch) for ch in range(32, 127) if m[ch] == 0)
    summary.append((name, have, diff, missing, layout + "(" + variant + ")"))
so.xkb_context_unref(ctx)
for name, have, diff, missing, source in summary:
    print(f"{name:23s} {have:2d}/95, diff={diff:3d}, missing={missing!r}, {source}")
