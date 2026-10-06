#!/usr/bin/env python3
"""Encode finished monochrome PNGs as raw native XBM bytes; never alter artwork.
The checked-in outputs let FBT build the application without a Pillow dependency.
"""
from pathlib import Path
import argparse, hashlib, io, json, re
from PIL import Image, ImageOps

root = Path(__file__).resolve().parents[1]
sources = [
    ("original", "original_128x64.png"),
    ("gta", "dnd_gta_loading_128x64.png"),
    ("gta_v2", "dnd_gta_loading_v2_128x64.png"),
    ("rogue", "dnd_gta_loading_rogue_128x64.png"),
    ("wizard", "dnd_gta_loading_wizard_128x64.png"),
    ("ranger", "dnd_gta_loading_ranger_128x64.png"),
    ("paladin", "dnd_gta_loading_paladin_128x64.png"),
    ("cleric", "dnd_gta_loading_cleric_128x64.png"),
    ("bard", "dnd_gta_loading_bard_128x64.png"),
    ("artificer", "dnd_gta_loading_artificer_128x64.png"),
    ("druid", "dnd_gta_loading_druid_128x64.png"),
    ("monk", "dnd_gta_loading_monk_128x64.png"),
    ("sorcerer", "dnd_gta_loading_sorcerer_128x64.png"),
    ("warlock", "dnd_gta_loading_warlock_128x64.png"),
    ("barbarian", "dnd_gta_loading_barbarian_128x64.png"),
]


def bitmap_bytes(path):
    with Image.open(path) as image:
        assert image.mode == "1" and image.size == (128, 64), path
        pixels = image.load()
        data = bytearray(1024)
        for y in range(64):
            for x in range(128):
                if pixels[x, y] == 0:
                    data[y * 16 + x // 8] |= 1 << (x % 8)
        # Match the supplied firmware's PNG -> inverted XBM convention exactly.
        stream = io.BytesIO()
        ImageOps.invert(image).save(stream, format="XBM")
        expected = bytes(
            int(value, 16)
            for value in re.findall(rb"0x([0-9a-fA-F]{2})", stream.getvalue())
        )
        assert bytes(data) == expected, path
        return bytes(data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    entries = []
    for choice, (name, filename) in enumerate(sources):
        source = root / "splash_sources" / filename
        data = bitmap_bytes(source)
        targets = [
            root / "character_assets/loading" / (name + ".bin"),
            root / "dnd_loading_assets/splashes" / (name + ".bin"),
        ]
        for target in targets:
            if args.check:
                assert target.read_bytes() == data, target
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
        entries.append(
            {
                "choice": choice,
                "name": name,
                "source": source.relative_to(root).as_posix(),
                "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                "bitmap_bytes": len(data),
                "bitmap_sha256": hashlib.sha256(data).hexdigest(),
                "outputs": [target.relative_to(root).as_posix() for target in targets],
            }
        )
    declared_count = int(
        re.search(
            r"DND_SPLASH_IMAGE_COUNT\s+(\d+)U",
            (root / "dnd_splash_image.h").read_text(),
        ).group(1)
    )
    assert declared_count == len(entries)
    manifest = {
        "format": "Raw row-major XBM, 128x64, 16 bytes per row, least-significant bit first, 1=black, no header.",
        "choices": len(entries),
        "resident_bitmap_bytes": 1024,
        "images": entries,
    }
    encoded = json.dumps(manifest, indent=2) + "\n"
    manifest_path = root / "splash_sources/ASSETS.json"
    if args.check:
        assert manifest_path.read_text() == encoded
    else:
        manifest_path.write_text(encoded)
    print(
        "PASS: fifteen exact-pixel splash bitmaps, both file-asset bundles and source hashes match; each bitmap is 1024 bytes."
    )


if __name__ == "__main__":
    main()
