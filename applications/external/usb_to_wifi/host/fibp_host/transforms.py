from __future__ import annotations

import html
import json
import re
import unicodedata
from urllib.parse import urlsplit

from scripts.fibp_codec import RequestStart

NATIONAL_TODAY = "national_today"
RADIO_BROWSER = "radio_browser"


def transform_for(request: RequestStart) -> str | None:
    if request.method != 1:
        return None
    parts = urlsplit(request.url)
    host = (parts.hostname or "").lower()
    if host == "nationaltoday.com" and parts.path == "/today/" and not parts.query:
        return NATIONAL_TODAY
    if host == "all.api.radio-browser.info" and parts.path == "/json/stations/search":
        return RADIO_BROWSER
    return None


def maximum_source_bytes(kind: str) -> int:
    return 256 * 1024 if kind == NATIONAL_TODAY else 128 * 1024


def _ascii(value: str) -> str:
    replacements = {
        "–": "-",
        "—": " - ",
        "‘": "'",
        "’": "'",
        "“": '"',
        "”": '"',
        "\u00a0": " ",
    }
    for source, replacement in replacements.items():
        value = value.replace(source, replacement)
    return (
        unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode("ascii")
    )


def transform_national_today(data: bytes) -> bytes:
    text = data.decode("utf-8", errors="strict")
    marker = text.lower().find("single-date-header-content")
    if marker < 0:
        raise ValueError("National Today content marker was not found")
    match = re.search(
        r"<p(?:\s[^>]*)?>(.*?)</p>", text[marker:], re.IGNORECASE | re.DOTALL
    )
    if not match:
        raise ValueError("National Today paragraph was not found")
    fragment = re.sub(
        r"<(?:b|strong)(?:\s[^>]*)?>", "[[B]]", match.group(1), flags=re.IGNORECASE
    )
    fragment = re.sub(r"</(?:b|strong)>", "[[/B]]", fragment, flags=re.IGNORECASE)
    fragment = re.sub(r"<[^>]*>", "", fragment)
    plain = _ascii(html.unescape(fragment))
    plain = " ".join(plain.split())
    plain = plain.replace(" [[B]]", "[[B]]").replace("[[/B]] ", "[[/B]]")
    if not plain:
        raise ValueError("National Today paragraph was empty")
    return plain.encode("ascii")


def _field(value: object, fallback: str, limit: int) -> str:
    text = value if isinstance(value, str) and value else fallback
    return _ascii(text).replace("\t", " ").replace("\r", " ").replace("\n", " ")[:limit]


def transform_radio_browser(data: bytes) -> bytes:
    decoded = json.loads(data.decode("utf-8", errors="strict"))
    if not isinstance(decoded, list):
        raise ValueError("Radio Browser response is not an array")  # noqa: TRY004
    lines = ["FIBRADIO1"]
    for item in decoded[:5]:
        if not isinstance(item, dict):
            continue
        url = item.get("url_resolved")
        codec = item.get("codec")
        bitrate = item.get("bitrate", 0)
        if not isinstance(url, str) or urlsplit(url).scheme.lower() != "https":
            continue
        if not isinstance(codec, str) or codec.lower() != "mp3":
            continue
        if not isinstance(bitrate, (int, float)) or not 8 <= int(bitrate) <= 64:
            continue
        lines.append(
            "\t".join(
                (
                    _field(item.get("name"), "Unknown station", 42),
                    _field(item.get("country"), "Unknown", 24),
                    _field(item.get("state"), "", 24),
                    str(int(bitrate)),
                    url,
                )
            )
        )
    return ("\n".join(lines) + "\n").encode("utf-8")


def apply_transform(kind: str, data: bytes) -> bytes:
    if kind == NATIONAL_TODAY:
        return transform_national_today(data)
    if kind == RADIO_BROWSER:
        return transform_radio_browser(data)
    raise ValueError("unknown response transform")
