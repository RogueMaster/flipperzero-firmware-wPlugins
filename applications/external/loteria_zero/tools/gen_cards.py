#!/usr/bin/env python3
"""Generate static C catalog from cards.json (single source of truth)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


REQUIRED_FIELDS = ("id", "name", "image", "rhyme")


def validate(catalog: dict, images_dir: Path | None) -> list[str]:
    errors: list[str] = []
    cards = catalog.get("cards")
    if not isinstance(cards, list):
        return ["'cards' must be a list"]

    total = catalog.get("total_cards")
    if total != 54:
        errors.append(f"total_cards must be 54, got {total!r}")
    if len(cards) != 54:
        errors.append(f"expected 54 cards, found {len(cards)}")

    seen_ids: set[int] = set()
    seen_images: set[str] = set()
    for i, card in enumerate(cards):
        prefix = f"cards[{i}]"
        if not isinstance(card, dict):
            errors.append(f"{prefix} is not an object")
            continue
        for field in REQUIRED_FIELDS:
            if field not in card or card[field] in (None, ""):
                errors.append(f"{prefix} missing required field '{field}'")
        cid = card.get("id")
        if not isinstance(cid, int) or cid < 1 or cid > 54:
            errors.append(f"{prefix}.id must be int 1..54, got {cid!r}")
        elif cid in seen_ids:
            errors.append(f"duplicate id {cid}")
        else:
            seen_ids.add(cid)

        image = card.get("image")
        if isinstance(image, str):
            if image in seen_images:
                errors.append(f"duplicate image key '{image}'")
            seen_images.add(image)
            if images_dir is not None:
                png = images_dir / f"{image}.png"
                if not png.is_file():
                    errors.append(f"missing illustration: {png.name}")

        rhyme = card.get("rhyme")
        if isinstance(rhyme, str) and len(rhyme.strip()) < 8:
            errors.append(f"{prefix}.rhyme too short")

    expected_ids = set(range(1, 55))
    if seen_ids and seen_ids != expected_ids:
        missing = sorted(expected_ids - seen_ids)
        extra = sorted(seen_ids - expected_ids)
        if missing:
            errors.append(f"missing ids: {missing}")
        if extra:
            errors.append(f"unexpected ids: {extra}")

    return errors


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


# Flipper fonts are ASCII-oriented; fold display strings for the device.
_ASCII_MAP = str.maketrans(
    {
        "á": "a",
        "Á": "A",
        "é": "e",
        "É": "E",
        "í": "i",
        "Í": "I",
        "ó": "o",
        "Ó": "O",
        "ú": "u",
        "Ú": "U",
        "ü": "u",
        "Ü": "U",
        "ñ": "n",
        "Ñ": "N",
        "¡": "",
        "¿": "",
        "—": "-",
        "–": "-",
        "…": "...",
        "·": "-",
        "“": '"',
        "”": '"',
        "‘": "'",
        "’": "'",
    }
)


def to_ascii(s: str) -> str:
    folded = s.translate(_ASCII_MAP)
    return folded.encode("ascii", "ignore").decode("ascii")


def generate(catalog: dict, out_h: Path, out_c: Path) -> None:
    cards = sorted(catalog["cards"], key=lambda c: c["id"])
    out_h.write_text(
        """\
#pragma once

#include <stdint.h>
#include <gui/icon.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LOTERIA_CARD_COUNT 54

typedef struct {
    uint8_t id;
    const char* name;
    const char* rhyme;
    const Icon* icon;
} LoteriaCard;

extern const LoteriaCard loteria_cards[LOTERIA_CARD_COUNT];

#ifdef __cplusplus
}
#endif
""",
        encoding="utf-8",
    )

    lines = [
        '#include "cards_data.h"',
        '#include "loteria_zero_icons.h"',
        "",
        "/* Display strings are ASCII-folded for Flipper fonts.",
        " * Canonical Unicode text remains in cards.json. */",
        "const LoteriaCard loteria_cards[LOTERIA_CARD_COUNT] = {",
    ]
    for card in cards:
        name = to_ascii(card["name"])
        rhyme = to_ascii(card["rhyme"])
        # Multi-line initializers stay within clang-format column limits.
        lines.append(f'    {{{card["id"]},')
        lines.append(f'     "{c_escape(name)}",')
        lines.append(f'     "{c_escape(rhyme)}",')
        lines.append(f'     &I_{card["image"]}}},')
    lines.append("};")
    lines.append("")
    out_c.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json", type=Path, required=True)
    parser.add_argument("--out-h", type=Path, required=True)
    parser.add_argument("--out-c", type=Path, required=True)
    parser.add_argument("--images", type=Path, default=None)
    parser.add_argument("--skip-images", action="store_true")
    args = parser.parse_args()

    catalog = json.loads(args.json.read_text(encoding="utf-8"))
    images_dir = None if args.skip_images else args.images
    errors = validate(catalog, images_dir)
    if errors:
        print("cards.json validation failed:", file=sys.stderr)
        for err in errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    args.out_h.parent.mkdir(parents=True, exist_ok=True)
    generate(catalog, args.out_h, args.out_c)
    print(f"Generated {args.out_h} and {args.out_c}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
