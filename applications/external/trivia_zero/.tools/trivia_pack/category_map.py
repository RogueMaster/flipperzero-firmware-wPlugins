"""OpenTDB (24 native categories) → 7-bucket Trivia Zero taxonomy.

Source of truth: docs/superpowers/specs/2026-04-28-trivia-zero-design.md §4.4.
The four ⚠️ rows in the spec are resolved here; if user review changes any of
those, update both this table AND the spec.

Some OpenTDB subcategories are skipped entirely (see SKIPPED_CATEGORIES) because
their content (Video Games, Anime, Comics, Cartoons, Board Games, Musicals,
Celebrities) is too niche or too anglo-centric for a "classic Trivial Pursuit"
feel. They are known categories — the difference vs. an unknown category is
that skipping is a deliberate exclusion, not a data-loss incident.
"""

from __future__ import annotations

from enum import IntEnum


class BucketId(IntEnum):
    GEOGRAFIA = 1
    ENTRETENIMIENTO = 2
    HISTORIA = 3
    ARTE_Y_LITERATURA = 4
    CIENCIA_Y_NATURALEZA = 5
    DEPORTES_Y_OCIO = 6
    CULTURA_GENERAL = 7


CATEGORY_MAP: dict[str, BucketId] = {
    # General
    "General Knowledge": BucketId.CULTURA_GENERAL,
    # Geography
    "Geography": BucketId.GEOGRAFIA,
    # History / Politics / Mythology (Mythology lives here per spec §4.4)
    "History": BucketId.HISTORIA,
    "Politics": BucketId.HISTORIA,
    "Mythology": BucketId.ARTE_Y_LITERATURA,
    # Arts & Literature
    "Entertainment: Books": BucketId.ARTE_Y_LITERATURA,
    "Art": BucketId.ARTE_Y_LITERATURA,
    # Entertainment (only the broad-audience subcategories — see SKIPPED below)
    "Entertainment: Film": BucketId.ENTRETENIMIENTO,
    "Entertainment: Music": BucketId.ENTRETENIMIENTO,
    "Entertainment: Television": BucketId.ENTRETENIMIENTO,
    # Science & Nature
    "Science & Nature": BucketId.CIENCIA_Y_NATURALEZA,
    "Science: Computers": BucketId.CIENCIA_Y_NATURALEZA,
    "Science: Mathematics": BucketId.CIENCIA_Y_NATURALEZA,
    "Science: Gadgets": BucketId.CIENCIA_Y_NATURALEZA,
    "Animals": BucketId.CIENCIA_Y_NATURALEZA,
    "Vehicles": BucketId.CIENCIA_Y_NATURALEZA,
    # Sports
    "Sports": BucketId.DEPORTES_Y_OCIO,
}


SKIPPED_CATEGORIES: frozenset[str] = frozenset(
    {
        "Entertainment: Video Games",
        "Entertainment: Japanese Anime & Manga",
        "Entertainment: Comics",
        "Entertainment: Cartoon & Animations",
        "Entertainment: Board Games",
        "Entertainment: Musicals & Theatres",
        "Celebrities",
    }
)


def is_skipped(opentdb_category: str) -> bool:
    return opentdb_category in SKIPPED_CATEGORIES


def map_opentdb_to_bucket(opentdb_category: str) -> BucketId:
    """Returns the 7-bucket id for an OpenTDB category string.

    Raises KeyError if the category is not in the table — the pipeline should
    surface unknown (or skipped) categories before calling this; reaching here
    with a non-mapped category is a programming error worth a hard failure.
    """
    return CATEGORY_MAP[opentdb_category]
