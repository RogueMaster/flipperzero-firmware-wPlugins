#!/usr/bin/env python3
"""Add a recovered Ultralight-C 3DES key to a Flipper NFC dump."""

import argparse
import re
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("key", help="16-byte recovered 2TDEA key (32 hex digits)")
    parser.add_argument(
        "--page40",
        help="optional original page 40 value (8 hex digits), e.g. 11EE0000",
    )
    parser.add_argument(
        "--page42",
        help="optional original page 42 value (8 hex digits), e.g. 04000000",
    )
    parser.add_argument(
        "--page43",
        help="optional original page 43 value (8 hex digits), e.g. 00000000",
    )
    args = parser.parse_args()

    key_hex = re.sub(r"[^0-9A-Fa-f]", "", args.key)
    if len(key_hex) != 32:
        raise SystemExit("key must contain exactly 32 hexadecimal digits")
    key = bytes.fromhex(key_hex)

    text = args.input.read_text(encoding="utf-8")
    required = (
        "Filetype: Flipper NFC device",
        "Device type: NTAG/Ultralight",
        "NTAG/Ultralight type: Mifare Ultralight C",
        "Pages total: 48",
    )
    for marker in required:
        if marker not in text:
            raise SystemExit(f"input is not the expected Ultralight-C dump: missing {marker!r}")

    text, count = re.subn(r"(?m)^Pages read: \d+$", "Pages read: 48", text)
    if count != 1:
        raise SystemExit("could not identify exactly one 'Pages read' field")

    key_lines = "\n".join(
        f"Page {44 + page}: "
        + " ".join(f"{byte:02X}" for byte in key[page * 4 : page * 4 + 4])
        for page in range(4)
    )
    text = re.sub(r"(?m)^Page (?:44|45|46|47): [0-9A-Fa-f ]+\n?", "", text)
    anchor = re.search(r"(?m)^Page 43: [0-9A-Fa-f ]+$", text)
    if not anchor:
        raise SystemExit("dump has no Page 43 anchor")
    insert_at = anchor.end()
    text = text[:insert_at] + "\n" + key_lines + text[insert_at:]

    for page, value in ((40, args.page40), (42, args.page42), (43, args.page43)):
        if value is None:
            continue
        page_hex = re.sub(r"[^0-9A-Fa-f]", "", value)
        if len(page_hex) != 8:
            raise SystemExit(f"page {page} must contain exactly 8 hexadecimal digits")
        page_data = " ".join(
            page_hex[index : index + 2].upper() for index in range(0, 8, 2)
        )
        text, count = re.subn(
            rf"(?m)^Page {page}: [0-9A-Fa-f ]+$", f"Page {page}: {page_data}", text
        )
        if count != 1:
            raise SystemExit(f"could not identify exactly one Page {page} field")

    uid_match = re.search(r"(?m)^UID: (.+)$", text)
    args.output.write_text(text, encoding="utf-8")
    print(f"Wrote {args.output}")
    print(f"UID: {uid_match.group(1) if uid_match else 'unknown'}")
    print(f"Recovered key: {key.hex().upper()}")


if __name__ == "__main__":
    main()
