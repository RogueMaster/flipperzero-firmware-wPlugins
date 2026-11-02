#!/usr/bin/env python3
"""Compile-time bundle of the original .kl maps; no runtime FAP /assets extraction needed."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
files = sorted((root / "keyboard_layouts").glob("*.kl"))
if not files:
    raise SystemExit("No .kl files found")

out = [
    "// Auto-generated; edit keyboard_layouts/*.kl and rerun tools/generate_embedded_layouts.py.",
    "// Binary tables are embedded in the FAP, independent of /assets extraction.",
    "#pragma once",
    "#include <stdint.h>",
    "#include <stddef.h>",
    "typedef struct {",
    "    const char* filename;",
    "    uint8_t data[256];",
    "} BadUsbBundledLayout;",
    "static const BadUsbBundledLayout bad_usb_bundled_layouts[] = {",
]
for f in files:
    data = f.read_bytes()
    if len(data) != 256:
        raise SystemExit(f"Invalid layout size {f.name}: {len(data)}")
    if not all(ch.isascii() and (ch.isalnum() or ch in ".-_") for ch in f.name):
        raise SystemExit(f"Invalid layout filename: {f.name}")
    out.append('    {"' + f.name + '", {')
    for i in range(0, 256, 16):
        out.append("        " + ", ".join("0x%02X" % b for b in data[i : i + 16]) + ",")
    out.append("    }},")
out.extend(
    [
        "};",
        "#define BAD_USB_BUNDLED_LAYOUT_COUNT (sizeof(bad_usb_bundled_layouts) / sizeof(bad_usb_bundled_layouts[0]))",
        "",
    ]
)
(root / "helpers" / "bundled_keyboard_layouts.h").write_text(
    "\n".join(out), encoding="ascii"
)
print(f"Embedded {len(files)} keyboard layouts into helpers/bundled_keyboard_layouts.h")
