#!/usr/bin/env python3
"""
Scans the Helldivers 2 fan wiki Stratagems page, diffs the result against
stratagems.c, and for any stratagems missing from the repo:

  - downloads the stratagem's icon SVG (or reuses an existing icon if the
    wiki entry shares its icon with a stratagem already in the repo, e.g.
    the various "Drill" objectives)
  - rasterizes it to a 512x512 PNG in images/stratagems/, named
    stratagem_<name>.png
  - appends a Stratagem record for it to the end of stratagems.c, referencing
    &I_stratagem_<name>

Usage:
    scripts/scan_stratagems.py [--dry-run] [--html PATH]

    --dry-run   Only report what would change; don't touch images/ or stratagems.c
    --html PATH Parse a locally saved copy of the wiki page instead of fetching it
"""
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path
from typing import TypedDict


class StratagemRow(TypedDict):
    category: str | None
    subheader: str | None
    type: str | None
    raw_title: str
    title: str
    code: str
    cooldown: int
    level: int
    svg_src: str | None
    svg_filename: str | None


# (title, icon identifier or None) for a stratagem already in stratagems.c
ExistingEntry = tuple[str, str | None]

WIKI_URL = "https://helldivers.wiki.gg/wiki/Stratagems"
WIKI_BASE = "https://helldivers.wiki.gg"
USER_AGENT = "stratahero-stratagem-scanner/1.0 (+https://github.com/maximkulkin/flipper-zero-stratagem-hero)"

REPO_ROOT = Path(__file__).resolve().parent.parent
STRATAGEMS_C = REPO_ROOT / "stratagems.c"
IMAGES_DIR = REPO_ROOT / "images" / "stratagems"
ICON_PREFIX = "stratagem_"

ARROW_MAP = {
    "Stratagem Arrow Up.svg": "U",
    "Stratagem Arrow Down.svg": "D",
    "Stratagem Arrow Left.svg": "L",
    "Stratagem Arrow Right.svg": "R",
}

# Top-level wiki category -> StratagemType. Categories not listed here
# ("Mission Stratagems") are resolved via MISSION_SUBTYPE instead, based on
# the "Ship" / "Objective" / "Unavailable" sub-heading inside that section.
CATEGORY_TYPE = {
    "Orbital Strikes": "StratagemType_OrbitalStrike",
    "Eagle Strikes": "StratagemType_EagleStrike",
    "Support Weapons": "StratagemType_SupportWeapon",
    "Backpacks": "StratagemType_Backpack",
    "Vehicles": "StratagemType_Vehicle",
    "Sentries": "StratagemType_Sentry",
    "Emplacements": "StratagemType_Emplacement",
}

MISSION_SUBTYPE = {
    "Ship": "StratagemType_Ship",
    "Objective": "StratagemType_Objective",
    "Unavailable": None,  # not currently obtainable in-game; never auto-added
}


def fetch(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.read().decode("utf-8")


def normalize_name(title: str) -> str:
    """Matches this repo's existing filename/title convention: strip periods
    and apostrophes, collapse whitespace/dashes to underscores, lowercase."""
    s = title.replace("'", "").replace(".", "")
    s = re.sub(r"[\s\-]+", "_", s.strip())
    s = s.lower()
    s = re.sub(r"[^a-z0-9_]", "", s)
    return s


def strip_code_prefix(title: str) -> str:
    """Wiki titles are prefixed with an item code, e.g. "MG-43 Machine Gun"
    or "B/MD C4 Pack". This repo's titles drop that prefix ("Machine Gun",
    "C4 Pack"). A prefix is recognized as a first word containing a digit or
    a slash."""
    parts = title.split(" ", 1)
    if len(parts) == 2:
        first, rest = parts
        if re.search(r"[0-9/]", first):
            return rest
    return title


def parse_wiki(html: str) -> list[StratagemRow]:
    start = html.index('id="List_of_Stratagems"')
    end = html.index('id="Gallery"')
    section = html[start:end]

    # Walk the section in document order, tracking the nearest preceding
    # <h3> (top permit heading), <summary> (category), and <big><b> (mission
    # sub-type) so each table can be tagged with its full context.
    marker_re = re.compile(
        r'<h3><span class="mw-headline" id="[^"]*">([^<]*)</span></h3>'
        r"|<summary>([^<]+)</summary>"
        r"|<big><b>([^<]+)</b></big>"
        r'|(<table class="wikitable sortable".*?</table>)',
        re.S,
    )

    category: str | None = None
    subheader: str | None = None
    rows_out: list[StratagemRow] = []
    for m in marker_re.finditer(section):
        h3_text, summary_text, big_text, table_html = m.groups()
        if h3_text is not None:
            subheader = None
        elif summary_text is not None:
            category = summary_text.strip()
            subheader = None
        elif big_text is not None:
            subheader = big_text.strip()
        elif table_html is not None:
            rows_out.extend(parse_table(table_html, category, subheader))
    return rows_out


def parse_table(
    table_html: str, category: str | None, subheader: str | None
) -> list[StratagemRow]:
    rows = re.findall(r"<tr>(.*?)</tr>", table_html, re.S)
    out: list[StratagemRow] = []
    for r in rows:
        if "<th>" in r:
            continue
        cells = re.findall(r"<td>(.*?)</td>", r, re.S)
        if len(cells) < 4:
            continue
        icon_cell, name_cell, code_cell, cooldown_cell = cells[:4]
        level_cell: str | None = cells[5] if len(cells) >= 6 else None

        m_src = re.search(r'<img[^>]+src="([^"?]+\.svg)', icon_cell)
        svg_src = m_src.group(1) if m_src else None
        svg_filename = svg_src.rsplit("/", 1)[-1] if svg_src else None

        m_name = re.search(r'title="([^"]+)">[^<]+</a>', name_cell)
        raw_title = (
            m_name.group(1) if m_name else re.sub("<[^>]+>", "", name_cell).strip()
        )

        arrows = re.findall(r'alt="([^"]+)"', code_cell)
        code = "".join(ARROW_MAP.get(a, "") for a in arrows)

        cooldown_text = re.sub("<[^>]+>", "", cooldown_cell)
        m_cd = re.search(r"(\d+)", cooldown_text)
        cooldown = int(m_cd.group(1)) if m_cd else 0

        level = 0
        if level_cell:
            m_lv = re.search(r"(\d+)", re.sub("<[^>]+>", "", level_cell))
            if m_lv:
                level = int(m_lv.group(1))

        clean_title = strip_code_prefix(raw_title)

        strat_type: str | None = None
        if category == "Mission Stratagems":
            strat_type = MISSION_SUBTYPE.get(subheader) if subheader else None
        elif category:
            strat_type = CATEGORY_TYPE.get(category)

        out.append(
            {
                "category": category,
                "subheader": subheader,
                "type": strat_type,
                "raw_title": raw_title,
                "title": clean_title,
                "code": code,
                "cooldown": cooldown,
                "level": level,
                "svg_src": svg_src,
                "svg_filename": svg_filename,
            }
        )
    return out


def load_existing_stratagems() -> list[ExistingEntry]:
    """Parse stratagems.c to learn each existing stratagem's title and its
    bare icon identifier, i.e. with any "stratagem_" prefix stripped (e.g.
    "prospecting_drill" from &I_stratagem_prospecting_drill). Bare names are
    used internally so they line up with normalize_name()/svg_base_name()
    output; the prefix is re-added wherever a filename or C symbol is built."""
    blocks = re.findall(
        r"Stratagem STRATAGEM_\w+ = \{(.*?)\};", STRATAGEMS_C.read_text(), re.S
    )
    entries: list[ExistingEntry] = []
    for b in blocks:
        m_title = re.search(r'\.title = "([^"]+)"', b)
        m_icon = re.search(rf"\.icon = &I_(?:{ICON_PREFIX})?(\w+)", b)
        if m_title:
            entries.append((m_title.group(1), m_icon.group(1) if m_icon else None))
    return entries


def svg_base_name(svg_filename: str | None) -> str | None:
    """Normalize a wiki icon filename to the same key space as our icon
    identifiers, e.g. "Fast_Recon_Vehicle_Stratagem_Icon_Background.svg" ->
    "fast_recon_vehicle". Used to catch stratagems that were renamed on the
    wiki but still use their original (unrenamed) icon asset."""
    if not svg_filename:
        return None
    base = re.sub(r"_?Stratagem_Icon_Background\.svg$", "", svg_filename, flags=re.I)
    base = re.sub(r"\.svg$", "", base, flags=re.I)
    return normalize_name(base.replace("_", " "))


def check_tool(name: str) -> bool:
    return shutil.which(name) is not None


def svg_to_png_512(svg_bytes: bytes, out_path: Path) -> None:
    with tempfile.NamedTemporaryFile(suffix=".svg") as tmp_svg:
        tmp_svg.write(svg_bytes)
        tmp_svg.flush()
        if check_tool("rsvg-convert"):
            subprocess.run(
                [
                    "rsvg-convert",
                    "-w",
                    "512",
                    "-h",
                    "512",
                    "-o",
                    str(out_path),
                    tmp_svg.name,
                ],
                check=True,
            )
        elif check_tool("magick"):
            subprocess.run(
                [
                    "magick",
                    "-background",
                    "none",
                    "-density",
                    "384",
                    tmp_svg.name,
                    "-resize",
                    "512x512",
                    str(out_path),
                ],
                check=True,
            )
        else:
            raise RuntimeError(
                "Neither 'rsvg-convert' nor 'magick' (ImageMagick) found on PATH; "
                "install librsvg (brew install librsvg) to convert SVG icons."
            )

    from PIL import Image

    im = Image.open(out_path).convert("RGBA")
    if im.size != (512, 512):
        resample = getattr(getattr(Image, "Resampling", Image), "LANCZOS")
        im = im.resize((512, 512), resample)
    im.save(out_path)


def build_c_record(row: StratagemRow, icon_symbol: str) -> tuple[str, str]:
    """icon_symbol is the full identifier after "I_", e.g.
    "stratagem_meltagun" or "no_icon_stratagem"."""
    enum_name = f"STRATAGEM_{normalize_name(row['title']).upper()}"
    lines = [f"Stratagem {enum_name} = {{"]
    lines.append(f"    .type = {row['type']},")
    lines.append(f"    .title = \"{row['title']}\",")
    lines.append(f"    .code = \"{row['code']}\",")
    lines.append(f"    .icon = &I_{icon_symbol},")
    lines.append(f"    .cooldown = {row['cooldown']},")
    if row["level"]:
        lines.append(f"    .level = {row['level']},")
    lines.append("};")
    return enum_name, "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="report only; don't write images or stratagems.c",
    )
    parser.add_argument(
        "--html",
        type=Path,
        default=None,
        help="parse a locally saved copy of the wiki page instead of fetching it",
    )
    args = parser.parse_args()

    print(f"Fetching {WIKI_URL} ..." if not args.html else f"Reading {args.html} ...")
    html = args.html.read_text(encoding="utf-8") if args.html else fetch(WIKI_URL)

    rows = parse_wiki(html)
    print(f"Parsed {len(rows)} stratagem entries from the wiki.")

    existing_entries = load_existing_stratagems()
    existing_titles_norm = {normalize_name(t) for t, _ in existing_entries}
    existing_icon_names = {icon for _, icon in existing_entries if icon}
    # A wiki row counts as "already present" if its title OR its icon's base
    # name matches either an existing title or an existing icon identifier.
    # The icon-identifier side of this catches two real cases found in this
    # repo: stratagems.c has a couple of misspelled titles ("Oribtal ..."
    # instead of "Orbital ...") whose *icon* identifiers are spelled
    # correctly, and stratagems the wiki has since renamed (e.g. "Fast Recon
    # Vehicle" -> "M-102 Gunner FRV") but which still use their original,
    # unrenamed icon asset.
    existing_keys = existing_titles_norm | existing_icon_names

    def is_new(row: StratagemRow) -> bool:
        keys = {normalize_name(row["title"])}
        base = svg_base_name(row["svg_filename"])
        if base:
            keys.add(base)
        return not (keys & existing_keys)

    # Map wiki icon SVG filename -> repo icon identifier, learned from
    # stratagems that are already in the repo. This lets a genuinely new
    # wiki row that shares an icon with an existing stratagem (e.g. all the
    # "* Drill" objectives share one generic icon) reuse that icon instead
    # of downloading a duplicate.
    title_to_icon = {normalize_name(t): icon for t, icon in existing_entries}
    svg_to_icon: dict[str, str] = {}
    for row in rows:
        icon = title_to_icon.get(normalize_name(row["title"]))
        if icon and row["svg_filename"]:
            svg_to_icon.setdefault(row["svg_filename"], icon)

    new_rows = [r for r in rows if is_new(r)]

    if not new_rows:
        print("No new stratagems found. stratagems.c is up to date.")
        return

    print(f"\nFound {len(new_rows)} stratagem(s) not present in stratagems.c:\n")

    to_add: list[StratagemRow] = []
    skipped: list[StratagemRow] = []
    for row in new_rows:
        label = f"  - {row['title']!r} ({row['category']}"
        label += f" / {row['subheader']}" if row["subheader"] else ""
        label += ")"
        if row["type"] is None:
            print(label + "  [SKIPPED: unavailable on wiki, not adding]")
            skipped.append(row)
            continue
        print(label)
        to_add.append(row)

    if not to_add:
        print("\nNothing to add (all new entries were unavailable/unreleased).")
        return

    if args.dry_run:
        print("\n--dry-run set: not downloading icons or modifying stratagems.c.")
        return

    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    new_blocks: list[str] = []
    for row in to_add:
        icon_symbol: str
        if row["svg_filename"] in svg_to_icon:
            icon_name = svg_to_icon[row["svg_filename"]]
            icon_symbol = f"{ICON_PREFIX}{icon_name}"
            print(
                f"\n{row['title']}: reusing existing icon '{icon_symbol}' "
                f"(shares wiki icon '{row['svg_filename']}')"
            )
        elif row["svg_src"]:
            icon_name = normalize_name(row["title"])
            icon_symbol = f"{ICON_PREFIX}{icon_name}"
            out_path = IMAGES_DIR / f"{icon_symbol}.png"
            print(f"\n{row['title']}: downloading icon {WIKI_BASE + row['svg_src']}")
            svg_bytes = fetch_bytes(WIKI_BASE + row["svg_src"])
            svg_to_png_512(svg_bytes, out_path)
            print(f"  -> saved {out_path.relative_to(REPO_ROOT)} (512x512)")
        else:
            icon_symbol = "no_icon_stratagem"
            print(
                f"\n{row['title']}: no icon available on wiki (placeholder); "
                f"using &I_{icon_symbol}"
            )

        enum_name, block = build_c_record(row, icon_symbol)
        print(f"  -> {enum_name}")
        new_blocks.append(block)

    with STRATAGEMS_C.open("a", encoding="utf-8") as f:
        f.write(
            "\n\n/* --- New stratagems found by scripts/scan_stratagems.py --- */\n"
        )
        f.write("\n".join(new_blocks))
        f.write("\n")

    print(
        f"\nAppended {len(new_blocks)} new record(s) to {STRATAGEMS_C.relative_to(REPO_ROOT)}."
    )
    print(
        "Move each record to its proper section and add a pointer to the "
        "`stratagems[]` array to finish wiring it up."
    )

    if skipped:
        print(
            f"\n{len(skipped)} entr{'y' if len(skipped)==1 else 'ies'} skipped "
            f"(unavailable on wiki): " + ", ".join(r["title"] for r in skipped)
        )


def fetch_bytes(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.read()


if __name__ == "__main__":
    sys.exit(main())
