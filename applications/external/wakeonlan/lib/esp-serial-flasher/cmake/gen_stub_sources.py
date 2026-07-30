# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""
Generate per-chip stub source files and a lookup-table from esp-flasher-stub release JSON files.

Usage:
    gen_stub_sources.py <version> <base_url> <repo_root> [override_path]

Each chip gets its own C file in <repo_root>/src/stubs/.
A lookup-table file and the private header are also written/updated.

JSON format (espressif/esp-flasher-stub):
    { "entry": <uint32>, "text": "<base64>", "text_start": <uint32>,
      "data": "<base64>", "data_start": <uint32>, "bss_start": <uint32> }
"""

import base64
import json
import os
import sys
import urllib.request
from datetime import datetime
from string import Template


# ---------------------------------------------------------------------------
# Configuration — order MUST match target_chip_t enumeration in include/esp_loader.h
# Each name derives: enum = NAME.upper()+"_CHIP", json = name+".json", c_var = "esp_stub_"+name
# ---------------------------------------------------------------------------
def _chip(name: str):
    return (name.upper() + "_CHIP", f"{name}.json", f"esp_stub_{name}")


CHIPS = [
    _chip(n)
    for n in [
        "esp8266",
        "esp32",
        "esp32s2",
        "esp32c3",
        "esp32s3",
        "esp32c2",
        "esp32c5",
        "esp32h2",
        "esp32c6",
        "esp32p4",
        "esp32c61",
    ]
]

# Extra stubs: own source file, NOT in esp_stub[] table, referenced by extern name.
# Tuple: (json_filename, c_var_name)  — json names may differ from the standard pattern.
EXTRA_STUBS = [
    # ESP32-P4 ECO5-6 (chip revision v1.x / v2.x) — selected at runtime based on
    # eco_version from GET_SECURITY_INFO when loader->_target == ESP32P4_CHIP.
    ("esp32p4-rev1.json", "esp_stub_esp32p4rev1"),
]

LICENSE_HEADER = """\
// SPDX-FileCopyrightText: 2025-{year} Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0 OR MIT
"""


def hex_array(data: bytes, indent: int = 4) -> str:
    """Format raw bytes as a C hex initialiser list."""
    pad = " " * indent
    per_line = 16
    lines = []
    for i in range(0, len(data), per_line):
        chunk = data[i : i + per_line]
        lines.append(pad + ", ".join(f"0x{b:02x}" for b in chunk))
    return ",\n".join(lines)


def load_stub_json(file_obj):
    stub = json.load(file_obj)
    entry = stub["entry"]
    text = base64.b64decode(stub["text"])
    text_start = stub["text_start"]
    try:
        data = base64.b64decode(stub["data"])
        data_start = stub["data_start"]
    except KeyError:
        data = b""
        data_start = 0
    return entry, text, text_start, data, data_start


_CHIP_FILE_TEMPLATE = Template("""\
$license_header
// auto-generated from esp-flasher-stub v$version — $json_name
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v$version

#include "esp_stubs.h"

static const uint8_t ${c_var}_text[] = {
$text_hex
};

${data_array}\
const esp_stub_t $c_var = {
    .header = {
        .entrypoint = $entrypoint,
    },
    .segments = {
        {
            .addr = $text_start,
            .size = sizeof(${c_var}_text),
            .data = ${c_var}_text,
        },
        {
            .addr = $data_start,
            .size = $data_size,
            .data = $data_ptr,
        },
    },
};
""")


def write_chip_file(
    path: str,
    version: str,
    year: int,
    json_name: str,
    c_var: str,
    entry: int,
    text: bytes,
    text_start: int,
    data: bytes,
    data_start: int,
) -> None:
    data_array = (
        f"static const uint8_t {c_var}_data[] = {{\n{hex_array(data)}\n}};\n\n"
        if data
        else ""
    )
    content = _CHIP_FILE_TEMPLATE.substitute(
        license_header=LICENSE_HEADER.format(year=year),
        version=version,
        json_name=json_name,
        c_var=c_var,
        text_hex=hex_array(text) if text else "",
        data_array=data_array,
        entrypoint=f"0x{entry:08x}",
        text_start=f"0x{text_start:08x}",
        data_start=f"0x{data_start:08x}",
        data_size=len(data),
        data_ptr=f"{c_var}_data" if data else "NULL",
    )
    with open(path, "w", newline="\n") as f:
        f.write(content)


def write_table_file(path: str, version: str, year: int, chips) -> None:
    lines = [
        LICENSE_HEADER.format(year=year),
        f"// auto-generated from esp-flasher-stub v{version}",
        f"// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v{version}",
        "",
        '#include "esp_stubs.h"',
        "",
    ]

    # Static asserts to catch enum reordering
    lines.append("#if __STDC_VERSION__ >= 201112L")
    for idx, (enum_name, _json, _cvar) in enumerate(chips):
        lines.append(
            f'_Static_assert({enum_name} == {idx}, "Stub table order matches target_chip_t enumeration");'
        )
    lines.append(
        f'_Static_assert(ESP_MAX_CHIP == {len(chips)}, "Stub table order matches target_chip_t enumeration");'
    )
    lines.append("#endif")
    lines.append("")

    # Forward declarations
    for _enum, _json, c_var in chips:
        lines.append(f"extern const esp_stub_t {c_var};")
    lines.append("")

    # Lookup table
    lines.append("const esp_stub_t *const esp_stub[ESP_MAX_CHIP] = {")
    for enum_name, _json, c_var in chips:
        lines.append(f"    [{enum_name}] = &{c_var},")
    lines.append("};")
    lines.append("")

    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines))


def write_header_file(path: str, version: str, year: int, extra_stubs) -> None:
    extra_externs = "".join(
        f"\nextern const esp_stub_t {c_var};" for _json, c_var in extra_stubs
    )
    content = (
        LICENSE_HEADER.format(year=year)
        + f"""\
// auto-generated from esp-flasher-stub v{version}
// Source: https://github.com/espressif/esp-flasher-stub/releases/tag/v{version}

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {{
#endif

typedef struct {{
    esp_loader_bin_header_t header;
    esp_loader_bin_segment_t segments[2];
}} esp_stub_t;

extern const esp_stub_t *const esp_stub[ESP_MAX_CHIP];

// Extra stubs not in the lookup table — selected at runtime by application code.{extra_externs}

#ifdef __cplusplus
}}
#endif
"""
    )
    with open(path, "w", newline="\n") as f:
        f.write(content)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} <version> <base_url> <repo_root> [override_path]")
        sys.exit(1)

    version = sys.argv[1]
    base_url = sys.argv[2]
    repo_root = sys.argv[3]
    override_path = sys.argv[4] if len(sys.argv) >= 5 else None

    year = datetime.now().year
    stubs_src = os.path.join(repo_root, "src", "stubs")
    priv_inc = os.path.join(repo_root, "private_include")

    os.makedirs(stubs_src, exist_ok=True)

    def fetch_and_write(json_name, c_var):
        print(f"  Processing {json_name} → {c_var}.c ...")
        if override_path:
            json_path = os.path.join(override_path, json_name)
            with open(json_path) as fh:
                entry, text, text_start, data, data_start = load_stub_json(fh)
        else:
            url = f"{base_url}/v{version}/{json_name}"
            with urllib.request.urlopen(url) as resp:
                entry, text, text_start, data, data_start = load_stub_json(resp)
        out_path = os.path.join(stubs_src, f"{c_var}.c")
        write_chip_file(
            out_path,
            version,
            year,
            json_name,
            c_var,
            entry,
            text,
            text_start,
            data,
            data_start,
        )

    for _enum, json_name, c_var in CHIPS:
        fetch_and_write(json_name, c_var)

    for json_name, c_var in EXTRA_STUBS:
        fetch_and_write(json_name, c_var)

    table_path = os.path.join(stubs_src, "esp_stubs_table.c")
    print(f"  Writing {table_path} ...")
    write_table_file(table_path, version, year, CHIPS)

    header_path = os.path.join(priv_inc, "esp_stubs.h")
    print(f"  Writing {header_path} ...")
    write_header_file(header_path, version, year, EXTRA_STUBS)

    total = len(CHIPS) + len(EXTRA_STUBS)
    print(
        f"Done — {total} stub files generated from v{version} ({len(CHIPS)} in table, {len(EXTRA_STUBS)} extra)."
    )
