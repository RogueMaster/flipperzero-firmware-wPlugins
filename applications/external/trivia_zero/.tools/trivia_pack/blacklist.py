"""Keyword blacklist for filtering culturally-anglo questions in both languages."""

from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Blacklist:
    keywords: tuple[str, ...]
    _regex: re.Pattern[str]

    @classmethod
    def from_file(cls, path: Path) -> Blacklist:
        keywords: list[str] = []
        for raw in path.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            keywords.append(line)

        if not keywords:
            # An impossible-to-match regex so .search always returns None.
            pattern = re.compile(r"(?!x)x")
        else:
            # Word-boundary, case-insensitive alternation.
            escaped = [re.escape(k) for k in keywords]
            pattern = re.compile(
                r"(?<!\w)(?:" + "|".join(escaped) + r")(?!\w)",
                flags=re.IGNORECASE,
            )

        return cls(keywords=tuple(keywords), _regex=pattern)

    @property
    def size(self) -> int:
        return len(self.keywords)

    def is_blacklisted(self, question: str, answer: str) -> bool:
        return bool(self._regex.search(question) or self._regex.search(answer))
