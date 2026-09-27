#!/usr/bin/env python3
"""Compile every source with warnings the firmware build does not turn on.

ufbt builds with -Wall -Wextra -Werror, which is a good floor and misses things
that matter on a device with 256 KB of RAM and a GUI thread that must never
block. This adds the ones that have actually caught bugs in this family of
apps, and reports per-function stack frames so a worker thread's real usage can
be checked against the size it was allocated.

It found a live bug the first time it ran here: three scene handlers were each
putting a 2.1 KB TrackerRecord snapshot array on the GUI thread's stack.

    python3 tools_strict_build.py              # report, exit 1 on any warning
    python3 tools_strict_build.py --frames 400 # also flag big stack frames

Needs the ufbt SDK unpacked (~/.ufbt/current). CI installs ufbt and runs a
build first, which puts the generated icons header where this expects it.
"""
import argparse
import glob
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
UFBT = os.path.expanduser("~/.ufbt")
SDK_ROOT = os.path.join(UFBT, "current", "sdk_headers")

# Warnings beyond the SDK's own set. Each of these has caught something real
# somewhere in this family of FAPs - see the notes in the repo's other tools.
EXTRA = [
    "-Wall",
    "-Wextra",
    "-Wshadow",
    "-Wpointer-arith",
    "-Wcast-align",
    "-Wformat=2",
    "-Wnull-dereference",
    "-Wdouble-promotion",
    "-Wlogical-op",
    "-Wduplicated-cond",
    "-Wduplicated-branches",
    "-Wvla",
    "-Wredundant-decls",
    "-Wundef",
]


def toolchain_gcc():
    hits = sorted(glob.glob(os.path.join(UFBT, "toolchain", "*", "bin", "arm-none-eabi-gcc")))
    if not hits:
        sys.exit(
            "arm-none-eabi-gcc not found under ~/.ufbt/toolchain.\n"
            "Run `ufbt` once so it downloads the toolchain."
        )
    return hits[0]


def sdk_args():
    opts_path = os.path.join(SDK_ROOT, "sdk.opts")
    if not os.path.exists(opts_path):
        sys.exit(f"{opts_path} not found. Run `ufbt` once to unpack the SDK.")
    opts = json.load(open(opts_path))
    args = shlex.split(opts["cc_args"].replace("SDK_ROOT_DIR", SDK_ROOT))
    # Keep the includes, the defines and the machine flags; supply our own
    # warning and optimisation set.
    keep = [a for a in args if a.startswith(("-I", "-D", "-m")) or a == "-nostdlib"]

    # The icons header is generated into the build directory by ufbt.
    gen = os.path.join(UFBT, "build", "ghosttag")
    if not os.path.exists(os.path.join(gen, "ghosttag_icons.h")):
        sys.exit(
            "ghosttag_icons.h not found in ~/.ufbt/build/ghosttag.\n"
            "Run `ufbt` once before this so the icons are generated."
        )
    keep += ["-I" + gen, "-I" + HERE]
    return keep


def sources():
    out = []
    for pat in ("*.c", "helpers/*.c", "views/*.c", "scenes/*.c"):
        out += sorted(glob.glob(os.path.join(HERE, pat)))
    return out


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument(
        "--frames",
        type=int,
        default=512,
        help="flag any function whose stack frame exceeds this many bytes (default 512)",
    )
    args = ap.parse_args()

    gcc = toolchain_gcc()
    base = sdk_args() + EXTRA + [
        "-fsingle-precision-constant",
        "-std=gnu2x",
        "-Os",
        "-fstack-usage",
        f"-Wstack-usage={args.frames * 2}",
    ]

    work = tempfile.mkdtemp(prefix="ghosttag-strict-")
    rsp = os.path.join(work, "flags.rsp")
    # A response file, because the include list is long enough that passing it
    # inline trips the argument limit ("File name too long").
    with open(rsp, "w") as fh:
        fh.write("\n".join(base) + "\n")

    problems = 0
    srcs = sources()
    try:
        for src in srcs:
            rel = os.path.relpath(src, HERE)
            obj = os.path.join(work, os.path.basename(src) + ".o")
            proc = subprocess.run(
                [gcc, "-c", "@" + rsp, src, "-o", obj],
                cwd=work,
                capture_output=True,
                text=True,
            )
            out = (proc.stdout + proc.stderr).strip()
            if out:
                problems += 1
                print(f"--- {rel}")
                print("\n".join("    " + ln for ln in out.splitlines()[:20]))

        frames = []
        for su in glob.glob(os.path.join(work, "*.su")):
            for line in open(su):
                parts = line.rstrip("\n").split("\t")
                if len(parts) < 2:
                    continue
                try:
                    size = int(parts[1])
                except ValueError:
                    continue
                if size > args.frames:
                    where = parts[0].replace(HERE + "/", "")
                    frames.append((size, where))

        print(f"\ncompiled {len(srcs)} sources with {len(EXTRA)} extra warnings")
        if frames:
            print(f"\nstack frames over {args.frames} bytes:")
            for size, where in sorted(frames, reverse=True):
                print(f"  {size:>6}  {where}")
            problems += len(frames)
        else:
            print(f"no stack frame exceeds {args.frames} bytes")

        if problems:
            print(f"\n{problems} problem(s)")
            return 1
        print("strict build clean")
        return 0
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
