"""Check the catalog-facing IR Builder files before a release."""
from pathlib import Path
import re

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
CATALOG_COLORS = {(254, 138, 44), (0, 0, 0)}


def main():
    forbidden = re.compile(
        r"(?m)^#{3,}|`|!\[|<[^>]+>|^\s*\||^\s*>|^---+$|&[A-Za-z]+;"
    )
    for name in ("description.md", "changelog.md"):
        text = (ROOT / "docs" / name).read_text(encoding="utf-8")
        assert not forbidden.search(text), f"Unsupported catalog Markdown in {name}"

    screenshots = sorted((ROOT / "screenshots").glob("*.png"))
    assert screenshots, "At least one catalog screenshot is required"
    for screenshot in screenshots:
        with Image.open(screenshot) as image:
            assert image.size == (512, 256), f"Wrong screenshot size: {screenshot}"
            colors = {color for _, color in image.convert("RGB").getcolors()}
            assert colors == CATALOG_COLORS, f"Wrong screenshot palette: {screenshot}"

    with Image.open(ROOT / "icons" / "ir_builder.png") as icon:
        assert icon.size == (10, 10), "Catalog icon must be 10 by 10 pixels"
        assert icon.mode == "1", "Catalog icon must be one-bit"

    fam = (ROOT / "application.fam").read_text(encoding="utf-8")
    expected = (
        'appid="ir_builder"',
        'fap_category="Infrared"',
        'fap_version=(3, 2)',
        'fap_author="@sanoobis"',
    )
    assert all(item in fam for item in expected), "Required FAM metadata is missing"
    print("PASS: catalog Markdown, screenshots, icon, and FAM metadata")


if __name__ == "__main__":
    main()
