"""Pure dataclasses passed between pipeline stages."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum


class Lang(IntEnum):
    ES = 0
    EN = 1


@dataclass(frozen=True)
class RawQuestion:
    """A question as it arrives from Open Trivia DB, before mapping/filtering."""

    source_lang: Lang
    opentdb_category: str
    question: str
    answer: str


@dataclass(frozen=True)
class MappedQuestion:
    """A RawQuestion after blacklist + category mapping survived. Still single-language."""

    source_lang: Lang
    bucket_id: int
    question: str
    answer: str


@dataclass
class BilingualQuestion:
    """A question with both ES and EN text, ready to be packed."""

    bucket_id: int
    question_es: str
    answer_es: str
    question_en: str
    answer_en: str
