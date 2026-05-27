"""Drop questions whose phrasing assumes a multiple-choice option list.

OpenTDB ships every item with four answer choices. When we collapse to a
single-answer trivia format, questions of the form "Which of the following
European languages..." or "Of these films, which..." lose the list of
candidates they were referencing and become unanswerable on their own.
The same goes for answers that point back at the (now hidden) list, like
"All of the above" or "None of these songs".

This filter is bilingual: it covers both English (the canonical OpenTDB
source) and Spanish (the translated side, plus ES-source questions). The
pipeline runs it pre-translation on the source language and again
post-translation, since translations occasionally introduce
"cual de los siguientes..." phrasing not present in the EN original.
"""

from __future__ import annotations

import re

_PATTERNS: tuple[str, ...] = (
    # --- English ---
    # "Which of the following X is Y?", "Which of these X..."
    r"\bwhich of (the following|these|those)\b",
    # "Which of the two following countries..." (an extra word between
    # "the" and "following" still signals a hidden option list).
    r"\bwhich of (the )?\w+ following\b",
    # "Which one of these X...", "Which one of the following..."
    r"\bone of (the following|these|those)\b",
    # "Of the following, which X...", "Of the following X is Y?"
    r"\bof the following\b",
    # "Which is the largest of these 4 islands?" / "...of those..."
    r"\bof these\b",
    r"\bof those\b",
    # Answers like "All of the above" / "None of the above" reference the
    # original choice list directly.
    r"\bof the above\b",
    # --- Spanish (with or without accent on "cual") ---
    # "Cual de los siguientes...", "Cual de las siguientes..."
    r"\bcu[aá]l de (los|las) siguientes\b",
    # "Cual de estos X...", "Cual de estas X..." (and esos/esas).
    r"\bcu[aá]l de (estos|estas|esos|esas)\b",
    # "Cual de los dos siguientes paises..." (extra word like in EN).
    r"\bcu[aá]l de (los|las) \w+ siguientes\b",
    # "De los siguientes...", "De las siguientes..." anywhere in text.
    r"\bde (los|las) siguientes\b",
    # Answers like "Todas las anteriores" / "Ninguna de las anteriores".
    r"\b(de )?(los|las) anteriores\b",
)

# Case-sensitive markers. OpenTDB writes negative-form MC questions with
# the negation word in ALL CAPS ("Which X is NOT...?", "Cual X NO es...?").
# These also implicitly require the hidden option list and must be dropped.
# Matching case-sensitively avoids false positives on:
#   - normal English text containing "not"
#   - Spanish short answers like "No"
_CASE_SENSITIVE_PATTERNS: tuple[str, ...] = (
    r"\bNOT\b",
    # NO followed by a Spanish verb — restricting by verb avoids catching
    # standalone "No" answers or sentence-initial "No" + noun phrases.
    r"\bNO\s+(?:es|fue|son|fueron|era|eran|estuvo|estaba|tiene|tuvo|"
    r"gan[oó]|cont[oó]|conto|presenta|present[oó]|aparece|aparec[ií]o|"
    r"incluye|incluy[oó]|figura|figur[oó]|forma|formaba|form[oó])\b",
)

_REGEX = re.compile("|".join(_PATTERNS), flags=re.IGNORECASE)
_REGEX_CASE_SENSITIVE = re.compile("|".join(_CASE_SENSITIVE_PATTERNS))


def is_multiple_choice_phrasing(question: str, answer: str) -> bool:
    """Return True iff the question or answer presupposes a list of options."""
    return bool(
        _REGEX.search(question)
        or _REGEX.search(answer)
        or _REGEX_CASE_SENSITIVE.search(question)
        or _REGEX_CASE_SENSITIVE.search(answer)
    )
