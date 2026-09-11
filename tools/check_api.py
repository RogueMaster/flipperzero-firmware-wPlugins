#!/usr/bin/env python3
"""Check every symbol this app needs against the firmware's export table.

A .fap is a relocatable ELF whose undefined symbols the firmware resolves
when it loads. If one is not in the firmware's public API table, the app
does not fail to build - it builds, installs, and then refuses to start
on the device, which is the worst place to find out.

Reads symbol names on stdin, one per line (arm-none-eabi-nm -u).
"""
import csv
import sys
import urllib.request

URL = ("https://raw.githubusercontent.com/flipperdevices/"
       "flipperzero-firmware/dev/targets/f7/api_symbols.csv")


def main():
    need = sorted({line.strip() for line in sys.stdin if line.strip()})
    if not need:
        print("no symbols on stdin", file=sys.stderr)
        return 1
    try:
        with urllib.request.urlopen(URL, timeout=60) as fh:
            rows = list(csv.reader(fh.read().decode("utf-8", "replace").splitlines()))
    except Exception as exc:                                   # noqa: BLE001
        print(f"could not fetch the API table: {exc}", file=sys.stderr)
        return 2

    version = "?"
    have = {}
    for row in rows:
        if len(row) >= 3 and row[0] in ("Function", "Variable"):
            have[row[2]] = row[1]
        elif len(row) >= 3 and row[0] == "Version":
            version = row[2]

    missing = [n for n in need if have.get(n) != "+"]
    if missing:
        print(f"firmware API {version} does not export:")
        for n in missing:
            print(f"    {n}{'' if n in have else '   (not in the table at all)'}")
        return 1
    print(f"all {len(need)} symbols exported by firmware API {version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
