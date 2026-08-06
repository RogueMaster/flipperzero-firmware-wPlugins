"""Офлайн-разбор файлов с таймингами.

Два формата:

* `.sub` (Flipper SubGhz RAW) — строки `RAW_Data:` со знаковыми
  длительностями;
* дамп raw-режима команды `tpms_rx` — строки вида `+52 -49 +101 ...`.

Про `.sub`: штатная запись Sub-GHz -> Read RAW для Renault ненадёжна,
потому что RAW-декодер прошивки выбрасывает импульсы короче 50 мкс
(`lib/subghz/protocols/raw.c`, `te_short = 50`), а у этого протокола
длительность чипа как раз ~50 мкс. Разбор оставлен для файлов из других
источников и для отладки.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from .decoder import RenaultFrame, decode_timings


@dataclass
class TimingCapture:
    """Захват таймингов вместе с метаданными файла."""

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
    """Разобрать содержимое .sub файла."""
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
    """Разобрать вывод `tpms_rx <freq> raw`."""
    capture = TimingCapture()
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("{"):
            continue
        capture.timings.extend(_parse_ints(line))
    return capture


def load(path: str | Path) -> TimingCapture:
    """Прочитать файл, формат определяется по расширению и содержимому."""
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
