#!/usr/bin/env python3
"""Size the successor room that lets the editor load into the unlock plugin's blocks.

The unlock plugin (flippass_open_host) is the only large module mapped through an
open, so the heap blocks holding its .text and .rodata survive the open intact while
everything around them fragments. Padding both sections ("successor room") makes the
blocks they free on unload big enough for the editor plugin's whole load sequence,
so the editor no longer has to stay resident through the open to remain available.

The firmware loader (lib/flipper_application/elf/elf_file.c) allocates each SHF_ALLOC
section and each .fast.rel.* section in section-table order with
aligned_malloc(size, align) = malloc(size + align - 1 + 4), after checking that the
largest free block is at least size + 1024; .fast.rel data is freed only after
relocation. The heap (memmgr_heap.c) adds an 8-byte header, rounds to 8 bytes and
may hand out up to 16 extra bytes instead of splitting. The proof below admits
arbitrary placement in both freed blocks, charges worst-case physical requests,
retains SMALL_MARGIN of bookkeeping per block, and checks a merged-block bound.
Useful-code surplus in .text can therefore hold later sections instead of
reserving every later allocation in .rodata alone.

It separately writes the aggregate successor reserve: max(0, E_max - H_min),
rounded up to 64 bytes. E_max bounds the editor's resident allocations; H_min is
the minimum returned by ALL resident open-host sections. The open keeps only that
positive delta out of its model budget. This does not change the contiguous lane
checks, firmware loader guard, stack reserve or the /ext consent policy.

    ufbt build && python extras/tools/flippass_plugin_room.py write && ufbt build
    python extras/tools/flippass_plugin_room.py check   # after a build; exits 1 if violated
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

REPO = Path(__file__).resolve().parents[2]
HEADER = REPO / "plugins" / "flippass_open_host_room.h"
HEAP_HEADER = 8
HEAP_ALIGN = 8
NO_SPLIT_SLACK = 16
LOADER_CHECK_SLACK = 1024
SMALL_MARGIN = 1024
ROOM_GRANULE = 4
# ELFFile, FlipperApplication, manifest, section dictionary and names kept per plugin.
RESIDENT_BOOKKEEPING = 512
RESERVE_GRANULE = 64


def ufbt_home() -> Path:
    if os.environ.get("UFBT_HOME"):
        return Path(os.environ["UFBT_HOME"])
    for candidate in (Path.home() / ".ufbt", REPO / ".ufbt"):
        if (candidate / "build").is_dir():
            return candidate
    return Path.home() / ".ufbt"


def find_tool(home: Path, name: str) -> Path:
    for base in [
        home / "toolchain" / "current" / "bin",
        *sorted((home / "toolchain").glob("*/bin")),
    ]:
        for candidate in (base / f"{name}.exe", base / name):
            if candidate.exists():
                return candidate
    sys.exit(f"{name} not found under {home / 'toolchain'}; run `ufbt update` first.")


def sections(readelf: Path, fal: Path) -> list[dict]:
    out = subprocess.run(
        [str(readelf), "-S", "-W", str(fal)], check=True, capture_output=True, text=True
    ).stdout
    rows = []
    pattern = re.compile(
        r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+\w+\s+\w+\s+(\w+)\s+\w+\s+(\S*)\s+\d+\s+\d+\s+(\d+)"
    )
    for line in out.splitlines():
        match = pattern.match(line)
        if match:
            index, name, kind, size, flags, align = match.groups()
            rows.append(
                {
                    "index": int(index),
                    "name": name,
                    "type": kind,
                    "size": int(size, 16),
                    "flags": flags,
                    "align": max(int(align), 1),
                }
            )
    return rows


def loaded(rows: list[dict]) -> list[dict]:
    """Sections the loader allocates, in load order (see elf_preload_section)."""
    result = []
    for row in rows:
        name = row["name"]
        if name.startswith((".ARM.", ".rel.ARM.", ".fast.rel.ARM.")):
            continue
        if "A" in row["flags"] or name.startswith(".fast.rel"):
            if row["size"]:
                result.append(row)
    return result


def request(row: dict) -> int:
    return row["size"] + row["align"] - 1 + 4


def block(nbytes: int) -> int:
    """Heap bytes consumed by malloc(nbytes), worst case."""
    return -(-(nbytes + HEAP_HEADER) // HEAP_ALIGN) * HEAP_ALIGN + NO_SPLIT_SLACK


def guaranteed_block(nbytes: int) -> int:
    """Smallest free block left behind when an allocation of nbytes is freed."""
    return -(-(nbytes + HEAP_HEADER) // HEAP_ALIGN) * HEAP_ALIGN


def editor_needs(seq: list[dict]) -> tuple[int, int]:
    if (
        not seq
        or seq[0]["name"] != ".text"
        or any(r["size"] > seq[0]["size"] for r in seq)
    ):
        sys.exit("the editor's .text must be its first and largest loaded section")
    first = seq[0]
    text_need = SMALL_MARGIN + max(
        first["size"] + LOADER_CHECK_SLACK, block(request(first))
    )
    rest_need, used = SMALL_MARGIN, 0
    for row in seq[1:]:
        rest_need = max(
            rest_need,
            SMALL_MARGIN
            + used
            + max(row["size"] + LOADER_CHECK_SLACK, block(request(row))),
        )
        used += block(request(row))
    return text_need, rest_need


def successor_load_proof(seq: list[dict], text_bytes: int, rodata_bytes: int) -> dict:
    """Conservative loader proof with surplus shared between both freed blocks.

    Reserve SMALL_MARGIN independently in each block. For every section, model
    arbitrary placement in either block or elsewhere, charging the worst
    physical size even when that choice is infeasible. Never prune by first-fit
    eligibility: doing so would make extra real capacity change the proof's
    allowed choices. A separate total-consumption bound covers merged blocks.
    Bookkeeping must remain within the same per-lane bounds as editor_needs().
    """
    states = {(max(0, text_bytes - SMALL_MARGIN), max(0, rodata_bytes - SMALL_MARGIN))}
    merged = max(0, text_bytes + rodata_bytes - 2 * SMALL_MARGIN)
    steps = []
    for row in seq:
        guard = row["size"] + LOADER_CHECK_SLACK
        minimum = min(max(state) for state in states)
        charge = block(request(row))
        required = max(guard, charge)
        step = {
            "section": row["name"],
            "guard": guard,
            "worst_physical_charge": charge,
            "required": required,
            "minimum_known_largest": minimum,
            "merged_lower_bound": merged,
            "placement_states": len(states),
        }
        steps.append(step)
        if minimum < required or merged < required:
            return {"valid": False, "steps": steps}
        following = set(states)  # A preceding external hole takes this section.
        for left, right in states:
            following.add((max(0, left - charge), right))
            following.add((left, max(0, right - charge)))
        states = following
        merged = max(0, merged - charge)
    return {"valid": True, "steps": steps}


def rodata_room_for(seq: list[dict], text_bytes: int, base: dict, old_room: int) -> int:
    # A linear search avoids assuming a monotonic first-fit placement function.
    # The proof admits every placement and keeps the original two 1 KiB margins.
    _, independent_need = editor_needs(seq)
    for padding in range(ROOM_GRANULE, independent_need + ROOM_GRANULE, ROOM_GRANULE):
        projected = dict(base, size=base["size"] - old_room + padding)
        if successor_load_proof(seq, text_bytes, guaranteed_block(request(projected)))[
            "valid"
        ]:
            return padding + 4 * ROOM_GRANULE  # Link-order/alignment settling slack.
    sys.exit("no successor rodata room satisfies the complete editor load proof")


def current_room() -> tuple[int, int, int]:
    text = rodata = reserve = 0
    if HEADER.exists():
        content = HEADER.read_text(encoding="utf-8")
        text = int(re.search(r"TEXT_ROOM_BYTES\s+(\d+)", content).group(1))
        rodata = int(re.search(r"RODATA_ROOM_BYTES\s+(\d+)", content).group(1))
        found = re.search(r"SUCCESSOR_RESERVE_BYTES\s+(\d+)", content)
        reserve = int(found.group(1)) if found else 0
    return text, rodata, reserve


def resident_footprint(seq: list[dict]) -> int:
    kept = [r for r in seq if not r["name"].startswith(".fast.rel")]
    return sum(block(request(r)) for r in kept) + RESIDENT_BOOKKEEPING


def reclaimable_footprint(seq: list[dict]) -> int:
    """Lower bound returned by unmapping all resident open-host sections.

    Omit loader bookkeeping and no-split slack, both of which can only add to
    the reclaimed bytes. Geometry is checked separately by editor_needs().
    """
    return sum(
        guaranteed_block(request(row))
        for row in seq
        if not row["name"].startswith(".fast.rel")
    )


def successor_reserve(footprint: int, reclaimed: int) -> int:
    # F_after >= F_before + H_min - E_max. Count the entire unmapped owner,
    # rather than just its padding, or the future editor is budgeted twice.
    reserve = max(0, footprint - reclaimed)
    return -(-reserve // RESERVE_GRANULE) * RESERVE_GRANULE


def room_for(need: int, have: int, old_room: int, base_size: int, align: int) -> int:
    """Keep a room that already fits with little to spare: the padding before and
    after the aligned room array shifts by a few bytes between builds, so resizing
    for the last byte would never settle. Otherwise resize with ROOM_GRANULE slack."""
    if old_room and need <= have < need + 4 * ROOM_GRANULE:
        return old_room
    room = need - HEAP_HEADER - (align - 1 + 4) - base_size + ROOM_GRANULE
    room = max(ROOM_GRANULE, room)
    return -(-room // ROOM_GRANULE) * ROOM_GRANULE


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("action", choices=("write", "check"))
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="uFBT build directory (default: $UFBT_HOME/build)",
    )
    args = parser.parse_args()

    home = ufbt_home()
    build = args.build_dir or home / "build"
    readelf = find_tool(home, "arm-none-eabi-readelf")
    editor_fal, host_fal = (
        build / "flippass_editor.fal",
        build / "flippass_open_host.fal",
    )
    if not editor_fal.exists() or not host_fal.exists():
        sys.exit(
            f"flippass_editor.fal / flippass_open_host.fal missing in {build}; run `ufbt build`."
        )

    editor_seq = loaded(sections(readelf, editor_fal))
    text_need, rest_need = editor_needs(editor_seq)
    footprint = resident_footprint(editor_seq)
    host = {row["name"]: row for row in loaded(sections(readelf, host_fal))}
    text, rodata = host[".text"], host[".rodata"]
    text_block = guaranteed_block(request(text))
    rodata_block = guaranteed_block(request(rodata))
    old_text_room, old_rodata_room, old_reserve = current_room()
    peak = sum(block(request(r)) for r in host.values())

    print(
        f"independent editor lanes need {text_need} B (.text) and {rest_need} B (rest); "
        f"it keeps about {footprint} B once loaded"
    )
    print(
        f"open_host frees {text_block} B (.text {text['size']} B, room {old_text_room}) and "
        f"{rodata_block} B (.rodata {rodata['size']} B, room {old_rodata_room}); "
        f"its own load peaks at about {peak} B"
    )

    if args.action == "write":
        new_text = room_for(
            text_need,
            text_block,
            old_text_room,
            text["size"] - old_text_room,
            text["align"],
        )
        projected_text = dict(text, size=text["size"] + new_text - old_text_room)
        new_rodata = rodata_room_for(
            editor_seq,
            guaranteed_block(request(projected_text)),
            rodata,
            old_rodata_room,
        )
        projected = [dict(row) for row in host.values()]
        for row in projected:
            if row["name"] == ".text":
                row["size"] += new_text - old_text_room
            elif row["name"] == ".rodata":
                row["size"] += new_rodata - old_rodata_room
        reclaimed = reclaimable_footprint(projected)
        new_reserve = successor_reserve(footprint, reclaimed)
        macros = {
            "FLIPPASS_OPEN_HOST_TEXT_ROOM_BYTES": new_text,
            "FLIPPASS_OPEN_HOST_RODATA_ROOM_BYTES": new_rodata,
            "FLIPPASS_OPEN_SUCCESSOR_RESERVE_BYTES": new_reserve,
        }
        width = max(
            map(len, macros)
        )  # aligned the way clang-format (setup.sh) leaves them
        HEADER.write_text(
            "// Generated by extras/tools/flippass_plugin_room.py from the built plugins. Do not edit by hand.\n"
            "// Complete editor load checked over both freed blocks, arbitrary placement,\n"
            "// 1024 B bookkeeping per block and the unchanged 1024 B firmware guard.\n"
            f"// Resident editor bound {footprint} B; open_host returns at least {reclaimed} B; reserve only a positive delta.\n"
            "#pragma once\n"
            + "".join(
                f"#define {name:<{width}} {value}\n" for name, value in macros.items()
            ),
            encoding="utf-8",
            newline="\n",
        )
        print(
            f"{HEADER.name}: text room {new_text} B, rodata room {new_rodata} B, "
            f"successor reserve {new_reserve} B (rebuild to apply)"
        )
        return 0

    problems = []
    if text_block < text_need:
        problems.append(
            f".text block {text_block} B < {text_need} B: run `write` and rebuild"
        )
    proof = successor_load_proof(editor_seq, text_block, rodata_block)
    if not proof["valid"]:
        problems.append(
            f"complete two-block editor load fails at {proof['steps'][-1]['section']}: run `write` and rebuild"
        )
    expected_reserve = successor_reserve(
        footprint, reclaimable_footprint(list(host.values()))
    )
    if old_reserve != expected_reserve:
        problems.append(
            f"successor reserve {old_reserve} B != {expected_reserve} B: run `write` and rebuild"
        )
    for problem in problems:
        print(f"error: {problem}", file=sys.stderr)
    if not problems:
        print(
            "complete editor load holds under arbitrary two-block placement and merged-block bounds"
        )
        for step in proof["steps"]:
            print(
                f"  {step['section']}: guard {step['guard']}, worst largest "
                f"{step['minimum_known_largest']}, merged floor {step['merged_lower_bound']}"
            )
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
