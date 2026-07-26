#!/usr/bin/env python3
"""Generate language label packs from CLDR Windows keyboard layouts.

Source: unicode-org/cldr keyboards/windows (LDML keyboard XML, release-41).
Maps ISO key positions to our template fill ids (US QWERTY slot names).

Usage:
  python3 tools/generate_cldr_language_packs.py
  python3 tools/generate_cldr_language_packs.py --dry-run
"""

from __future__ import annotations

import argparse
import html
import json
import re
import shutil
import sys
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = REPO_ROOT / "android/app/src/main/assets/layouts/languages"
CACHE_DIR = REPO_ROOT / "tools/.cache/cldr-keyboards"
CLDR_TAG = "release-41"
ZIP_URL = (
    f"https://github.com/unicode-org/cldr/archive/refs/tags/{CLDR_TAG}.zip"
)
# ISO / PC 105 positions → our fill ids (US QWERTY labels as slot names).
ISO_TO_FILL = {
    "D01": "q",
    "D02": "w",
    "D03": "e",
    "D04": "r",
    "D05": "t",
    "D06": "y",
    "D07": "u",
    "D08": "i",
    "D09": "o",
    "D10": "p",
    "D11": "[",
    "D12": "]",
    "C01": "a",
    "C02": "s",
    "C03": "d",
    "C04": "f",
    "C05": "g",
    "C06": "h",
    "C07": "j",
    "C08": "k",
    "C09": "l",
    "C10": ";",
    "C11": "'",
    "B01": "z",
    "B02": "x",
    "B03": "c",
    "B04": "v",
    "B05": "b",
    "B06": "n",
    "B07": "m",
    "B08": ",",
    "B09": ".",
    "B10": "/",
}
CORE_ISO = list(ISO_TO_FILL.keys())
# Prefer these ranks when several Windows layouts share a pack id.
VARIANT_PENALTY = (
    ("dvorak", 1000),
    ("legacy", 80),
    ("phonetic", 70),
    ("extended", 40),
    ("var", 35),
    ("102key", 30),
    ("101key", 30),
    ("azerty", 25),
    ("qwerty", 5),  # slight penalty vs bare default
    ("el220", 20),
    ("el319", 20),
    ("lt1205", 20),
    ("lt1582", 20),
    ("patta", 20),
)
UNICODE_ESCAPE_RE = re.compile(r"\\u\{([0-9a-fA-F]+)\}")
# Skip non-desktop / IME shells that are not useful as letter labels.
SKIP_LANG_PREFIXES = {
    "zh",  # IME; Latin base is just en
    "ja",
    "ko",
}
# CLDR Windows names are sometimes country labels ("US"); prefer language titles.
TITLE_OVERRIDES = {
    "en": "English",
    "en-GB": "English (UK)",
    "en-IN": "English (India)",
    "es-419": "Spanish (Latin America)",
    "pt": "Portuguese (Brazil)",
    "pt-PT": "Portuguese (Portugal)",
    "fr-CA": "French (Canada)",
    "fr-BE": "French (Belgium)",
    "fr-CH": "French (Switzerland)",
    "de-CH": "German (Switzerland)",
    "de-BE": "German (Belgium)",
    "nl-BE": "Dutch (Belgium)",
    "az-Cyrl": "Azerbaijani (Cyrillic)",
    "bg-Latn": "Bulgarian (Latin)",
    "bs-Cyrl": "Bosnian (Cyrillic)",
    "el-Latn": "Greek (Latin)",
    "el-POLYTON": "Greek (Polytonic)",
    "sr-Latn": "Serbian (Latin)",
}


def decode_to(raw: str) -> str:
    """Decode CLDR `to=` value (entities + \\u{…})."""
    text = html.unescape(raw)

    def repl(match: re.Match[str]) -> str:
        return chr(int(match.group(1), 16))

    return UNICODE_ESCAPE_RE.sub(repl, text)


def pack_id_from_locale(locale: str) -> str:
    """
    Derive pack id from CLDR keyboard locale, e.g.
      ru-t-k0-windows → ru
      az-Cyrl-t-k0-windows → az-Cyrl
      en-GB-t-k0-windows → en-GB
      pt-PT-t-k0-windows → pt-PT
    """
    core = locale.split("-t-k0-", 1)[0]
    return core


def variant_score(filename: str) -> int:
    stem = filename.removesuffix(".xml").lower()
    score = 0
    for token, penalty in VARIANT_PENALTY:
        if f"-{token}" in stem or stem.endswith(token):
            score += penalty
    # Prefer the shortest default name for a language.
    score += len(stem)
    return score


def is_single_label(text: str) -> bool:
    if not text or text.isspace():
        return False
    # Allow one base char, optionally with combining marks.
    if len(text) == 1:
        return True
    # Reject long sequences / keycap macros.
    if len(text) > 4:
        return False
    # Common combining-mark clusters are ok (e.g. a + acute).
    return all(ord(ch) < 0x10000 for ch in text)


def parse_base_keymap(path: Path) -> tuple[str, str, dict[str, str]] | None:
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError as exc:
        print(f"warn: parse failed {path.name}: {exc}", file=sys.stderr)
        return None

    locale = root.attrib.get("locale", "")
    name = ""
    names = root.find("names")
    if names is not None:
        name_el = names.find("name")
        if name_el is not None:
            name = name_el.attrib.get("value", "").strip()

    base_map: dict[str, str] = {}
    for keymap in root.findall("keyMap"):
        if keymap.attrib.get("modifiers"):
            continue
        for m in keymap.findall("map"):
            iso = m.attrib.get("iso")
            if iso not in ISO_TO_FILL:
                continue
            to_raw = m.attrib.get("to")
            if to_raw is None:
                continue
            label = decode_to(to_raw)
            if is_single_label(label):
                base_map[iso] = label
        break  # first unmodified keyMap only

    if len(base_map) < 20:
        return None
    # Require most of the letter core (not just digits/punct).
    letter_iso = [k for k in CORE_ISO if k[0] in "DCB" and int(k[1:]) <= 10]
    letter_hits = sum(1 for k in letter_iso if k in base_map)
    if letter_hits < 24:
        return None

    return locale, name or pack_id_from_locale(locale), base_map


def ensure_cldr_windows(force: bool = False) -> Path:
    windows_dir = CACHE_DIR / f"cldr-{CLDR_TAG}" / "keyboards" / "windows"
    if windows_dir.is_dir() and not force and any(windows_dir.glob("*.xml")):
        return windows_dir

    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    zip_path = CACHE_DIR / f"cldr-{CLDR_TAG}.zip"
    if force or not zip_path.exists():
        print(f"Downloading {ZIP_URL} …")
        urllib.request.urlretrieve(ZIP_URL, zip_path)

    extract_root = CACHE_DIR / f"cldr-{CLDR_TAG}"
    if extract_root.exists() and force:
        shutil.rmtree(extract_root)

    print(f"Extracting keyboards/windows from {zip_path.name} …")
    with zipfile.ZipFile(zip_path) as zf:
        prefix = f"cldr-{CLDR_TAG}/keyboards/windows/"
        for info in zf.infolist():
            if not info.filename.startswith(prefix):
                continue
            if info.is_dir():
                continue
            rel = info.filename[len(f"cldr-{CLDR_TAG}/") :]
            dest = CACHE_DIR / f"cldr-{CLDR_TAG}" / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(info) as src, open(dest, "wb") as dst:
                shutil.copyfileobj(src, dst)
    return windows_dir


def select_layouts(windows_dir: Path) -> list[dict]:
    candidates: dict[str, list[tuple[int, Path, str, str, dict[str, str]]]] = defaultdict(
        list
    )
    for path in sorted(windows_dir.glob("*-t-k0-windows*.xml")):
        if path.name.startswith("_"):
            continue
        parsed = parse_base_keymap(path)
        if parsed is None:
            continue
        locale, name, base_map = parsed
        pid = pack_id_from_locale(locale)
        lang = pid.split("-", 1)[0].lower()
        if lang in SKIP_LANG_PREFIXES:
            continue
        # Drop pure Dvorak / left-right Dvorak always.
        lower = path.name.lower()
        if "dvorak" in lower:
            continue
        candidates[pid].append((variant_score(path.name), path, locale, name, base_map))

    selected: list[dict] = []
    for pid, items in sorted(candidates.items()):
        items.sort(key=lambda t: t[0])
        _score, path, locale, name, base_map = items[0]
        labels = {ISO_TO_FILL[iso]: ch for iso, ch in base_map.items()}
        labels["menu"] = "☰"
        # Locale tags for matching: language (+ script/region if present).
        locales = [pid.replace("_", "-")]
        display = TITLE_OVERRIDES.get(pid, name)
        selected.append(
            {
                "id": pid,
                "name": display,
                "locales": locales,
                "labels": labels,
                "source_file": path.name,
                "source_locale": locale,
            }
        )
    return selected


def write_packs(packs: list[dict], dry_run: bool) -> None:
    catalog = {"languages": []}
    if not dry_run:
        OUT_DIR.mkdir(parents=True, exist_ok=True)
        # Remove previously generated packs except keep nothing — regenerate all.
        for old in OUT_DIR.glob("*.json"):
            if old.name == "catalog.json":
                continue
            old.unlink()

    for pack in packs:
        body = {
            "id": pack["id"],
            "name": pack["name"],
            "locales": pack["locales"],
            "labels": pack["labels"],
            "source": {
                "cldr": CLDR_TAG,
                "file": pack["source_file"],
                "locale": pack["source_locale"],
            },
        }
        out_name = f"{pack['id']}.json"
        # Region/script ids use hyphens; keep filename safe.
        out_name = out_name.replace("/", "-")
        rel = f"layouts/languages/{out_name}"
        catalog["languages"].append(
            {
                "id": pack["id"],
                "title": pack["name"],
                "locales": pack["locales"],
                "file": rel,
            }
        )
        if dry_run:
            print(f"would write {out_name}  ({pack['source_file']})  {pack['name']}")
            continue
        (OUT_DIR / out_name).write_text(
            json.dumps(body, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    catalog["languages"].sort(key=lambda e: e["id"].lower())
    if dry_run:
        print(f"would write catalog.json with {len(catalog['languages'])} languages")
        return
    (OUT_DIR / "catalog.json").write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote {len(packs)} packs + catalog.json → {OUT_DIR}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--force-download",
        action="store_true",
        help="Re-download and re-extract CLDR zip",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and select packs without writing assets",
    )
    args = parser.parse_args()

    windows_dir = ensure_cldr_windows(force=args.force_download)
    packs = select_layouts(windows_dir)
    if not packs:
        print("error: no layouts selected", file=sys.stderr)
        return 1
    write_packs(packs, dry_run=args.dry_run)

    # Sanity: Russian ЙЦУКЕН and German QWERTZ.
    by_id = {p["id"]: p for p in packs}
    ru = by_id.get("ru", {}).get("labels", {})
    de = by_id.get("de", {}).get("labels", {})
    if ru.get("q") != "й" or ru.get("w") != "ц":
        print(f"warn: unexpected ru labels: q={ru.get('q')!r} w={ru.get('w')!r}", file=sys.stderr)
    if de.get("y") != "z" or de.get("z") != "y":
        print(f"warn: unexpected de QWERTZ: y={de.get('y')!r} z={de.get('z')!r}", file=sys.stderr)
    print(f"Selected {len(packs)} language packs from {windows_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
