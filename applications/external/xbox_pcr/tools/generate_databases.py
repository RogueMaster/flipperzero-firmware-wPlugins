#!/usr/bin/env python3
"""Generate the Flipper and browser databases from the upstream CSV files."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "data"

CONSOLE_BITS = {
    "ALL": 0x1F,
    "XOP": 0x01,
    "XOS": 0x02,
    "XOX": 0x04,
    "XSS": 0x08,
    "XSX": 0x10,
}
TYPE_IDS = {"CPU": 1, "SP": 2, "SMC": 3, "OS": 4}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def parse_int(value: str | None, default: int = 0) -> int:
    value = (value or "").strip()
    return int(value, 0) if value else default


def console_mask(value: str) -> int:
    mask = 0
    for console in value.split(","):
        mask |= CONSOLE_BITS.get(console.strip().upper(), 0)
    return mask


def ascii_text(value: str | None) -> str:
    text = (value or "").strip()
    replacements = {
        "\u2018": "'",
        "\u2019": "'",
        "\u201c": '"',
        "\u201d": '"',
        "\u2013": "-",
        "\u2014": "-",
        "\u2192": "->",
        "\u00a0": " ",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    return text.encode("ascii", "replace").decode("ascii")


def c_string(value: str | None) -> str:
    text = ascii_text(value)
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", " ") + '"'


def parse_exxx_markdown(path: Path) -> list[dict[str, str]]:
    text = path.read_text(encoding="utf-8")
    matches = list(re.finditer(r"^###\s+(E\d{3})\s*:?\s*$", text, re.MULTILINE))
    records: list[dict[str, str]] = []
    for index, match in enumerate(matches):
        end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        body = text[match.end() : end].strip()
        body = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", body)
        body = re.sub(r"!\[[^\]]*\]\([^)]+\)", "", body)
        body = re.sub(r"^\s*[-*]\s*", "", body, flags=re.MULTILINE)
        body = re.sub(r"\s+", " ", body).strip()
        records.append(
            {
                "code": match.group(1),
                "name": match.group(1),
                "description": body,
                "source": "TorusHyperV/XboxOne-EXXX-err-Codes",
            }
        )
    return records


def source_hash(paths: list[Path]) -> str:
    digest = hashlib.sha256()
    for path in paths:
        digest.update(path.name.encode("utf-8"))
        digest.update(path.read_bytes())
    return digest.hexdigest()[:16]


def normalized_postcodes(rows: list[dict[str, str]]) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    for row in rows:
        records.append(
            {
                "console": row["Console"],
                "consoleMask": console_mask(row["Console"]),
                "type": row["Type"],
                "code": parse_int(row["Code"]),
                "bitmask": parse_int(row.get("Bitmask"), 0xFFFF),
                "isError": row.get("IsError", "0").strip() == "1",
                "name": ascii_text(row.get("Name")),
                "description": ascii_text(row.get("Description")),
            }
        )
    return records


def normalized_masks(rows: list[dict[str, str]]) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    for row in rows:
        records.append(
            {
                "console": row["Console"],
                "consoleMask": console_mask(row["Console"]),
                "type": row["Type"],
                "code": parse_int(row["Code"]),
                "bitmask": parse_int(row["Bitmask"]),
                "isError": True,
                "name": ascii_text(row.get("Name")),
                "description": ascii_text(row.get("Description")),
            }
        )
    return records


def generate_header(
    postcodes: list[dict[str, object]], masks: list[dict[str, object]]
) -> str:
    exact = sum(1 for record in postcodes if record["bitmask"] == 0xFFFF)
    masked = len(postcodes) - exact
    return f"""#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POSTCODE_DB_RECORD_COUNT {len(postcodes)}U
#define POSTCODE_DB_EXACT_COUNT  {exact}U
#define POSTCODE_DB_MASKED_COUNT {masked}U
#define POSTCODE_DB_FALLBACK_COUNT {len(masks)}U

typedef enum {{
    PostcodeConsoleXboxOnePhat = 0,
    PostcodeConsoleXboxOneS,
    PostcodeConsoleXboxOneX,
    PostcodeConsoleXboxSeriesS,
    PostcodeConsoleXboxSeriesX,
    PostcodeConsoleCount,
}} PostcodeConsole;

typedef struct {{
    uint16_t code;
    uint16_t mask;
    uint8_t consoles;
    uint8_t type;
    bool is_error;
    const char* name;
    const char* description;
}} PostcodeDbRecord;

const char* postcode_db_console_name(PostcodeConsole console);
const char* postcode_db_console_short(PostcodeConsole console);
const char* postcode_db_type_short(uint8_t type);
bool postcode_db_record_at(size_t index, PostcodeDbRecord* output);

/**
 * Look up an Xbox POST code using its bus segment and selected console.
 *
 * name_output is always populated. description points to static database
 * storage and may be an empty string. Returns true for a database match.
 */
bool postcode_db_format(
    uint16_t code,
    uint8_t segment,
    PostcodeConsole console,
    char* name_output,
    size_t name_output_size,
    const char** description,
    bool* is_error);
"""


def generate_c(
    postcodes: list[dict[str, object]], masks: list[dict[str, object]], digest: str
) -> str:
    rows = []
    for record in postcodes:
        rows.append(
            "    {"
            f"0x{record['code']:04X}, 0x{record['bitmask']:04X}, "
            f"0x{record['consoleMask']:02X}, {TYPE_IDS[record['type']]}, "
            f"{1 if record['isError'] else 0}, {c_string(record['name'])}, "
            f"{c_string(record['description'])}"
            "},"
        )
    fallback_rows = []
    for record in masks:
        fallback_rows.append(
            "    {"
            f"0x{record['code']:04X}, 0x{record['bitmask']:04X}, "
            f"0x{record['consoleMask']:02X}, {TYPE_IDS[record['type']]}, 1, "
            f"{c_string(record['name'])}, {c_string(record['description'])}"
            "},"
        )

    return f"""#include "postcode_db.h"

#include <stdio.h>
#include <string.h>

/* Generated by tools/generate_databases.py.
 * Source: XboxOneResearch/errorcodes, digest {digest}
 */

enum {{
    PostcodeTypeAny = 0,
    PostcodeTypeCpu = 1,
    PostcodeTypeSp = 2,
    PostcodeTypeSmc = 3,
    PostcodeTypeOs = 4,
}};

static const char* const console_names[] = {{
    "Xbox One Phat",
    "Xbox One S",
    "Xbox One X",
    "Xbox Series S",
    "Xbox Series X",
}};

static const char* const console_short_names[] = {{
    "ONE",
    "ONE S",
    "ONE X",
    "SERIES S",
    "SERIES X",
}};

static const PostcodeDbRecord postcode_records[] = {{
{chr(10).join(rows)}
}};

static const PostcodeDbRecord fallback_records[] = {{
{chr(10).join(fallback_rows)}
}};

const char* postcode_db_console_name(PostcodeConsole console) {{
    if(console >= PostcodeConsoleCount) return "Unknown Xbox";
    return console_names[console];
}}

const char* postcode_db_console_short(PostcodeConsole console) {{
    if(console >= PostcodeConsoleCount) return "UNKNOWN";
    return console_short_names[console];
}}

const char* postcode_db_type_short(uint8_t type) {{
    switch(type) {{
    case PostcodeTypeCpu:
        return "CPU";
    case PostcodeTypeSp:
        return "SP";
    case PostcodeTypeSmc:
        return "SMC";
    case PostcodeTypeOs:
        return "OS";
    default:
        return "ANY";
    }}
}}

bool postcode_db_record_at(size_t index, PostcodeDbRecord* output) {{
    if(!output || index >= POSTCODE_DB_RECORD_COUNT) return false;
    *output = postcode_records[index];
    return true;
}}

static uint8_t type_from_segment(uint8_t segment) {{
    switch(segment & 0xF0U) {{
    case 0x10U:
        return PostcodeTypeCpu;
    case 0x30U:
        return PostcodeTypeSp;
    case 0x70U:
        return PostcodeTypeSmc;
    case 0xF0U:
        return PostcodeTypeOs;
    default:
        return PostcodeTypeAny;
    }}
}}

static bool record_applies(
    const PostcodeDbRecord* record,
    uint16_t code,
    uint8_t type,
    uint8_t console_bit) {{
    if((record->consoles & console_bit) == 0U) return false;
    if(type != PostcodeTypeAny && record->type != type) return false;
    return (code & record->mask) == (record->code & record->mask);
}}

static const PostcodeDbRecord* find_best(
    const PostcodeDbRecord* records,
    size_t count,
    uint16_t code,
    uint16_t wanted_mask,
    uint8_t type,
    uint8_t console_bit) {{
    for(size_t i = 0; i < count; i++) {{
        const PostcodeDbRecord* record = &records[i];
        if(record->mask != wanted_mask) continue;
        if(record_applies(record, code, type, console_bit)) return record;
    }}
    return NULL;
}}

bool postcode_db_format(
    uint16_t code,
    uint8_t segment,
    PostcodeConsole console,
    char* name_output,
    size_t name_output_size,
    const char** description,
    bool* is_error) {{
    if(!name_output || name_output_size == 0U) return false;
    if(description) *description = "";
    if(is_error) *is_error = false;

    if(console >= PostcodeConsoleCount) console = PostcodeConsoleXboxOneS;
    const uint8_t console_bit = (uint8_t)(1U << console);
    const uint8_t type = type_from_segment(segment);

    const PostcodeDbRecord* exact = find_best(
        postcode_records,
        POSTCODE_DB_RECORD_COUNT,
        code,
        0xFFFFU,
        type,
        console_bit);
    if(!exact && type != PostcodeTypeAny) {{
        exact = find_best(
            postcode_records,
            POSTCODE_DB_RECORD_COUNT,
            code,
            0xFFFFU,
            PostcodeTypeAny,
            console_bit);
    }}
    if(exact) {{
        snprintf(name_output, name_output_size, "%s", exact->name);
        if(description) *description = exact->description;
        if(is_error) *is_error = exact->is_error != 0U;
        return true;
    }}

    const PostcodeDbRecord* high = find_best(
        postcode_records,
        POSTCODE_DB_RECORD_COUNT,
        code,
        0xFF00U,
        type,
        console_bit);
    if(!high && type != PostcodeTypeAny) {{
        high = find_best(
            postcode_records,
            POSTCODE_DB_RECORD_COUNT,
            code,
            0xFF00U,
            PostcodeTypeAny,
            console_bit);
    }}
    const PostcodeDbRecord* low = NULL;
    if(high) {{
        low = find_best(
            postcode_records,
            POSTCODE_DB_RECORD_COUNT,
            code,
            0x00FFU,
            type,
            console_bit);
    }}
    if(high && !low && type != PostcodeTypeAny) {{
        low = find_best(
            postcode_records,
            POSTCODE_DB_RECORD_COUNT,
            code,
            0x00FFU,
            PostcodeTypeAny,
            console_bit);
    }}
    if(high || low) {{
        if(high && low) {{
            snprintf(name_output, name_output_size, "%s / %s", high->name, low->name);
        }} else {{
            snprintf(name_output, name_output_size, "%s", high ? high->name : low->name);
        }}
        const PostcodeDbRecord* detail = high && high->description[0] ? high : low;
        if(description && detail) *description = detail->description;
        if(is_error) *is_error = high ? high->is_error != 0U : false;
        return true;
    }}

    for(size_t i = 0; i < POSTCODE_DB_FALLBACK_COUNT; i++) {{
        const PostcodeDbRecord* fallback = &fallback_records[i];
        if(record_applies(fallback, code, type, console_bit)) {{
            snprintf(name_output, name_output_size, "%s_UNKNOWN_%04X", fallback->name, code);
            if(description) *description = fallback->description;
            if(is_error) *is_error = true;
            return false;
        }}
    }}

    const char* type_name = "UNKNOWN";
    switch(type) {{
    case PostcodeTypeCpu:
        type_name = "CPU";
        break;
    case PostcodeTypeSp:
        type_name = "SP";
        break;
    case PostcodeTypeSmc:
        type_name = "SMC";
        break;
    case PostcodeTypeOs:
        type_name = "OS";
        break;
    default:
        break;
    }}
    snprintf(name_output, name_output_size, "%s_UNKNOWN_%04X", type_name, code);
    return false;
}}
"""


def generate_web_data(
    postcodes: list[dict[str, object]],
    masks: list[dict[str, object]],
    os_errors: list[dict[str, str]],
    e_codes: list[dict[str, str]],
    digest: str,
) -> str:
    payload = {
        "formatVersion": 3,
        "source": "XboxOneResearch/errorcodes",
        "sourceDigest": digest,
        "postcodes": postcodes,
        "errorMasks": masks,
        "osErrors": [
            {
                "console": row["Console"],
                "type": row["Type"],
                "code": row["Code"],
                "name": ascii_text(row.get("Name")),
                "description": ascii_text(row.get("Description")),
            }
            for row in os_errors
        ],
        "eCodes": e_codes,
    }
    return (
        "/* Generated by xbox_postcode_reader/tools/generate_databases.py. */\n"
        "window.XBOX_POSTCODE_DATA = "
        + json.dumps(payload, ensure_ascii=True, separators=(",", ":"))
        + ";\n"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--site-dir",
        type=Path,
        help="Optional WebXboxPOSTTool directory that receives postcode-data.js",
    )
    args = parser.parse_args()

    source_paths = [
        DATA_DIR / "postcodes.csv",
        DATA_DIR / "errormasks.csv",
        DATA_DIR / "oserrors.csv",
        DATA_DIR / "xbox-exxx-codes.md",
    ]
    digest = source_hash(source_paths)
    postcodes = normalized_postcodes(read_csv(source_paths[0]))
    masks = normalized_masks(read_csv(source_paths[1]))
    os_errors = read_csv(source_paths[2])
    e_codes = parse_exxx_markdown(source_paths[3])

    (ROOT / "postcode_db.h").write_text(
        generate_header(postcodes, masks), encoding="utf-8", newline="\n"
    )
    (ROOT / "postcode_db.c").write_text(
        generate_c(postcodes, masks, digest), encoding="utf-8", newline="\n"
    )

    if args.site_dir:
        args.site_dir.mkdir(parents=True, exist_ok=True)
        (args.site_dir / "postcode-data.js").write_text(
            generate_web_data(postcodes, masks, os_errors, e_codes, digest),
            encoding="utf-8",
            newline="\n",
        )

    print(
        f"Generated {len(postcodes)} POST records, {len(masks)} fallback masks, "
        f"{len(e_codes)} E-codes (digest {digest})."
    )


if __name__ == "__main__":
    main()
