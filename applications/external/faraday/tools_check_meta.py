#!/usr/bin/env python3
"""Repository facts that must agree with each other, checked rather than trusted.

Every check here exists because the corresponding thing was actually wrong in
this repo at some point:

  * application.fam shipped fap_version="1.1" while the firmware compiled
    FARADAY_VERSION "1.2" and the git tag said v1.2. The number a user sees in
    the app catalog came from the first of those, and nothing compared them.
  * The screenshots under screenshots/ are what goes to the Flipper app
    catalog, which wants the device's own two colours. A file with a third
    colour in it is a re-render, not a capture.
  * The app icon must be 10x10 and 1-bit or the build embeds something the
    launcher cannot draw.

    python3 tools_check_meta.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

LCD_LIT = (254, 138, 44)
LCD_INK = (0, 0, 0)


def read(*parts):
    with open(os.path.join(HERE, *parts)) as fh:
        return fh.read()


def check_version(problems):
    header = re.search(r'#define\s+FARADAY_VERSION\s+"([^"]+)"', read("faraday_i.h"))
    fam = re.search(r'fap_version\s*=\s*"([^"]+)"', read("application.fam"))
    if not header or not fam:
        problems.append("could not find FARADAY_VERSION or fap_version")
        return None
    hv, fv = header.group(1), fam.group(1)
    if hv != fv:
        problems.append(
            f"version mismatch: faraday_i.h says {hv}, application.fam says {fv}. "
            "The catalog shows the .fam one and the About screen shows the header one."
        )
    else:
        print(f"  version    {hv} (header and manifest agree)")

    changelog = read("CHANGELOG.md")
    if hv not in changelog:
        problems.append(f"CHANGELOG.md has no entry for {hv}")
    else:
        print(f"  changelog  has an entry for {hv}")
    return hv


def check_icons(problems):
    try:
        from PIL import Image
    except ImportError:
        print("  icons      skipped (Pillow not installed)")
        return
    icons = os.path.join(HERE, "icons")
    for name in sorted(os.listdir(icons)):
        if not name.endswith(".png"):
            continue
        im = Image.open(os.path.join(icons, name))
        ok = im.size == (10, 10) and im.mode == "1"
        if not ok:
            problems.append(
                f"icons/{name} is {im.size[0]}x{im.size[1]} mode={im.mode}, want 10x10 mode=1"
            )
    print(
        f"  icons      {len([n for n in os.listdir(icons) if n.endswith('.png')])} checked"
    )


def check_screenshots(problems):
    try:
        from PIL import Image
    except ImportError:
        print("  shots      skipped (Pillow not installed)")
        return
    shots = os.path.join(HERE, "screenshots")
    if not os.path.isdir(shots):
        problems.append("screenshots/ does not exist - run tools_screenshot.py --all")
        return
    catalog = sorted(n for n in os.listdir(shots) if re.fullmatch(r"ss\d+\.png", n))
    if not catalog:
        problems.append(
            "no screenshots/ssN.png catalog aliases - run tools_screenshot.py --catalog"
        )
        return
    for name in catalog:
        path = os.path.join(shots, name)
        im = Image.open(path).convert("RGB")
        cols = {c for _, c in im.getcolors(1 << 20)}
        if not cols <= {LCD_LIT, LCD_INK}:
            problems.append(
                f"screenshots/{name} has {len(cols)} colours - a device capture has exactly two. "
                "A third means it is a re-render rather than a capture."
            )
        w, h = im.size
        if w % 128 or h % 64 or w // 128 != h // 64:
            problems.append(
                f"screenshots/{name} is {w}x{h}, not a whole multiple of 128x64"
            )
    print(
        f"  shots      {len(catalog)} catalog images, two-colour and correctly scaled"
    )


def main():
    problems = []
    print("checking repository metadata")
    check_version(problems)
    check_icons(problems)
    check_screenshots(problems)
    print()
    if problems:
        for p in problems:
            print(f"  FAIL  {p}")
        print(f"\n{len(problems)} problem(s)")
        return 1
    print("all consistent")
    return 0


if __name__ == "__main__":
    sys.exit(main())
