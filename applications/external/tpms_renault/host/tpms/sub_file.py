"""Offline parsing of timing files.

Two formats:

* `.sub` (Flipper SubGhz RAW) — `RAW_Data:` lines with signed durations;
* a dump of the `tpms_rx` raw mode — lines like `+52 -49 +101 ...`.

About `.sub`: the stock Sub-GHz -> Read RAW capture is unreliable for
Renault, because the firmware RAW decoder throws away pulses shorter than
50 us (`lib/subghz/protocols/raw.c`, `te_short = 50`), and this protocol
has a chip duration of about exactly 50 us. Parsing is kept for files from
other sources and for debugging.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from .decoder import RenaultFrame, decode_timings


@dataclass
class TimingCapture:
    """A timing capture together with the file metadata."""

    timings: list[int] = field(default_factory=list)
    meta: dict[str, str] = field(default_factory=dict)

    @property
    def frequency(self) -> int | None:
        value = self.meta.get("Frequency")
        return int(value) if value and value.isdigit() else None

    @property
    def preset(self) -> str | None:
        return self.meta.get("Preset")

    def decode(self) -> list[RenaultFrame]:
        return decode_timings(self.timings)


def parse_sub(text: str) -> TimingCapture:
    """Parse the contents of a .sub file."""
    capture = TimingCapture()

    for line in text.splitlines():
        line = line.strip()
        if not line or ":" not in line:
            continue

        key, _, value = line.partition(":")
        key = key.strip()
        value = value.strip()

        if key == "RAW_Data":
            capture.timings.extend(_parse_ints(value))
        elif key not in capture.meta:
            capture.meta[key] = value

    return capture


def parse_raw_dump(text: str) -> TimingCapture:
    """Parse the output of `tpms_rx <freq> raw`."""
    capture = TimingCapture()
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("{"):
            continue
        capture.timings.extend(_parse_ints(line))
    return capture


def load(path: str | Path) -> TimingCapture:
    """Read a file; the format is detected by extension and content."""
    path = Path(path)
    text = path.read_text(encoding="utf-8", errors="replace")

    if path.suffix.lower() == ".sub" or "RAW_Data" in text:
        return parse_sub(text)
    return parse_raw_dump(text)


def _parse_ints(value: str) -> list[int]:
    numbers: list[int] = []
    for token in value.replace(",", " ").split():
        try:
            numbers.append(int(token))
        except ValueError:
            continue
    return numbers
