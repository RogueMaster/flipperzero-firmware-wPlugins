#!/usr/bin/env python3
"""
Verify the built site against the promises it makes.

The site's whole job is to open on a phone with no signal, so "no external
requests" is not a style preference — one stray CDN link and the page is blank
in a field. This checks that mechanically instead of by eye.

    python tools/docs-build/verify.py
"""

from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOCS = os.path.join(ROOT, "docs")
SRC = os.path.join(ROOT, "docs-src")

# The source-to-output counts below have to compare like with like, so the
# sources are read through the same include expansion the builder uses rather
# than a second copy of that logic that could drift away from it.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build import expand_includes  # noqa: E402

PAGES = ["index.html", "app-guide.html", "guide-sergey.html", "guide-mark.html"]

failures: list[str] = []
checks = 0


def check(condition: bool, description: str, detail: str = "") -> None:
    global checks
    checks += 1
    mark = "ok  " if condition else "FAIL"
    if not condition:
        failures.append(description)
    print(f"  {mark} {description}{(' -> ' + detail) if detail else ''}")


def read(name: str) -> str:
    with open(os.path.join(DOCS, name), encoding="utf-8") as handle:
        return handle.read()


def main() -> int:
    # The pages are Russian and full of emoji; on a Windows console the default
    # code page cannot encode them and the run dies on a *passing* check.
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except AttributeError:  # pragma: no cover - very old Python
        pass

    print("docs site verification\n")

    for name in PAGES:
        check(os.path.exists(os.path.join(DOCS, name)), f"{name} exists")

    for name in ["sw.js", "manifest.webmanifest", "icon.svg", "meshlog.py", ".nojekyll"]:
        check(os.path.exists(os.path.join(DOCS, name)), f"{name} exists")

    print()

    # --- the offline promise: nothing may be *loaded* from outside.
    # Links a reader can tap are fine; resources the page fetches are not.
    resource_attrs = re.compile(
        r'<(?:script|img|iframe|source|video|audio)\b[^>]*\bsrc\s*=\s*["\']([^"\']+)',
        re.I,
    )
    stylesheet = re.compile(
        r'<link\b[^>]*\brel\s*=\s*["\'](?:stylesheet|preload|prefetch)["\'][^>]*\bhref\s*=\s*["\']([^"\']+)',
        re.I,
    )
    css_import = re.compile(r"@import\b|url\(\s*['\"]?https?:", re.I)

    for name in PAGES:
        text = read(name)
        external = [u for u in resource_attrs.findall(text) if u.startswith(("http://", "https://", "//"))]
        external += [u for u in stylesheet.findall(text) if u.startswith(("http://", "https://", "//"))]
        check(not external, f"{name}: no external resources", str(external[:3]) if external else "")
        check(not css_import.search(text), f"{name}: no @import or remote url() in CSS")
        # Everything must be inlined, so there should be no local resource
        # references either apart from the manifest and the download link.
        check("<style>" in text, f"{name}: CSS is inlined")
        check("<script>" in text, f"{name}: JS is inlined")

    print()

    # --- copy button on every code block
    for name in PAGES:
        text = read(name)
        blocks = len(re.findall(r"<pre>", text))
        has_js = "document.querySelectorAll('pre')" in text
        check(has_js, f"{name}: copy-button script present", f"{blocks} code blocks")

    print()

    # --- service worker caches what the site offers
    sw = read("sw.js")
    for path in ["./index.html", "./app-guide.html", "./guide-sergey.html", "./guide-mark.html", "./meshlog.py"]:
        check(f'"{path}"' in sw, f"service worker caches {path}")
    check('scope: \'./\'' in read("index.html"), "service worker registered with a relative scope")
    check('"scope": "./"' in read("manifest.webmanifest"), "manifest scope is relative")
    check('"start_url": "./index.html"' in read("manifest.webmanifest"), "manifest start_url is relative")

    print()

    # --- the hub reaches everything
    index = read("index.html")
    for target in ["app-guide.html", "guide-sergey.html", "guide-mark.html", "meshlog.py"]:
        check(f'href="{target}"' in index, f"hub links {target}")

    # --- no broken internal links
    internal = set()
    for name in PAGES:
        for href in re.findall(r'href="([^"#][^"]*)"', read(name)):
            if href.startswith(("http://", "https://", "mailto:")):
                continue
            internal.add(href.split("#")[0])
    missing = [h for h in sorted(internal) if h and not os.path.exists(os.path.join(DOCS, h))]
    check(not missing, "no broken internal links", str(missing) if missing else "")

    print()

    # --- Mark's guide: exactly two edits, and no more
    mark = read("guide-mark.html")
    check("meshcore-node-fw" in mark, "Mark's guide points at our flasher")
    check("flasher.meshcore.io" not in mark, "and no longer at the official one")
    check('href="meshlog.py"' in mark, "Mark's guide offers meshlog.py for download")
    check("Ты ничего не паяешь" in mark, "Mark's guide carries the 'you solder nothing' note")
    check("это сторона Сергея" in mark, "wiring shown for reference only")
    # His methodology must survive untouched: he is on the phone, over TCP and BLE.
    check("Termux" in mark, "Mark keeps Termux")
    check("Trace Path" in mark, "Mark keeps Trace Path")

    sergey = read("guide-sergey.html")
    check("meshcore-node-fw" in sergey, "Sergey's guide points at our flasher")
    check("Trace Path" in sergey, "Sergey's guide keeps the honest Trace Path gap")
    # The word survives in the sentence explaining what was removed; what must
    # be gone is the procedure — no termux- commands, no package juggling.
    for gone in ["termux-wake-lock", "termux-location", "termux-setup-storage", "pkg upgrade"]:
        check(gone not in sergey, f"Sergey's guide has no {gone}")

    # --- the corrected pinout, by header position rather than GPIO name alone
    for label, needle in [
        ("T114 GND", "P1·4"),
        ("T114 detect", "P1·8"),
        ("T114 RX", "P1·12"),
        ("T114 TX", "P1·13"),
        ("V4 GND", "J2·1"),
        ("V4 detect", "J2·12"),
        ("V4 RX", "J2·13"),
        ("V4 TX", "J2·14"),
    ]:
        check(needle in sergey, f"Sergey's guide gives {label} by header position", needle)

    check("J3" not in sergey, "the old J3 header is gone from Sergey's guide")

    wiring = ""
    with open(os.path.join(ROOT, "docs-src", "svg", "wiring.svg"), encoding="utf-8") as handle:
        wiring = handle.read()
    for needle in ["P1·4", "P1·8", "P1·12", "P1·13", "J2·1", "J2·12", "J2·13", "J2·14"]:
        check(needle in wiring, f"wiring diagram shows {needle}")
    check("J3" not in wiring, "wiring diagram no longer mentions J3")

    print()

    # --- the field part is shared, and "shared" has to mean identical.
    # The two of them run these scenarios together and compare the numbers
    # afterwards, so a sentence that drifted in one guide is two people
    # running different experiments without noticing.
    with open(os.path.join(SRC, "field-common.md"), encoding="utf-8") as handle:
        field = handle.read()

    for heading in re.findall(r"^#{2,3} (.+)$", field, re.M):
        # `code` and **bold** become tags, so a heading containing them is not
        # one contiguous string in the output. Compare the plain runs instead.
        fragments = [f.strip() for f in re.split(r"`|\*\*", heading) if len(f.strip()) > 4]
        in_both = all(f in sergey and f in mark for f in fragments)
        check(in_both, f"field section in both guides: {heading[:44]}")

    for phrase in [
        "Вниз по течению не вернуться",
        "забыт навсегда",
        "Сеть под тестом — не средство связи",
        "два судна расходятся по воде",
        "прямой плёс",
        "и километры по воде, и прямое расстояние",
        "замер → вмешательство → повторный замер",
        "Сверьте часы перед первым замером",
    ]:
        check(phrase in sergey and phrase in mark, f"both guides carry: {phrase[:44]}")

    # The old flat-site walk must be gone, or a reader following the table of
    # contents will still find "walk the perimeter" and do that instead.
    for stale in ["по периметру площадки", "Фаза 1. Параллельный обход", "Прогон по площадке"]:
        check(
            stale not in sergey and stale not in mark,
            f"the old flat-site walk is gone: {stale[:40]}",
        )

    check("{{include:" not in sergey and "{{include:" not in mark, "no unexpanded includes")

    print()

    # --- content survived the rendering, counted against the source rather
    # than against a guessed floor: a section quietly lost in conversion is
    # exactly the failure this is here to catch.
    src_dir = os.path.join(ROOT, "docs-src")
    for page_name, source_name in [
        # Mark's page renders from the transformed markdown, which the builder
        # writes out so the two allowed edits stay reviewable.
        ("guide-mark.html", "guide-mark.effective.md"),
        ("guide-sergey.html", "guide-sergey.md"),
        ("app-guide.html", "app-guide.md"),
    ]:
        with open(os.path.join(src_dir, source_name), encoding="utf-8") as handle:
            source = expand_includes(handle.read())
        text = read(page_name)

        src_tables = len(re.findall(r"^[ \t]*\|[ :|\-]+\|[ \t]*$", source, re.M))
        out_tables = len(re.findall(r"<table>", text))
        check(
            out_tables == src_tables,
            f"{page_name}: every table survived",
            f"{out_tables}/{src_tables}",
        )

        src_headings = len(re.findall(r"^#{1,4} ", source, re.M))
        out_headings = len(re.findall(r"<h[1-4]\b", text))
        check(
            out_headings == src_headings,
            f"{page_name}: every heading survived",
            f"{out_headings}/{src_headings}",
        )

        # Fences also occur inside blockquotes, where the marker is preceded by
        # "> " -- counting only column-zero fences undercounts and makes a
        # correct render look broken.
        src_code = len(re.findall(r"^>?[ \t]*```", source, re.M)) // 2
        out_code = len(re.findall(r"<pre>", text))
        check(
            out_code == src_code,
            f"{page_name}: every code block survived",
            f"{out_code}/{src_code}",
        )

    print(f"\n{checks} checks, {len(failures)} failures")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
