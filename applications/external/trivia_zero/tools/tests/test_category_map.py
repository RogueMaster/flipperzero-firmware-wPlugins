import pytest

from trivia_pack.category_map import (
    CATEGORY_MAP,
    SKIPPED_CATEGORIES,
    BucketId,
    is_skipped,
    map_opentdb_to_bucket,
)


def test_general_knowledge_maps_to_cultura_general() -> None:
    assert map_opentdb_to_bucket("General Knowledge") == BucketId.CULTURA_GENERAL


def test_geography_maps_to_geografia() -> None:
    assert map_opentdb_to_bucket("Geography") == BucketId.GEOGRAFIA


def test_sports_maps_to_deportes_y_ocio() -> None:
    assert map_opentdb_to_bucket("Sports") == BucketId.DEPORTES_Y_OCIO


def test_classic_entertainment_subcategories_map_to_entretenimiento() -> None:
    for sub in ("Entertainment: Film", "Entertainment: Music", "Entertainment: Television"):
        assert map_opentdb_to_bucket(sub) == BucketId.ENTRETENIMIENTO


def test_books_map_to_arte_y_literatura() -> None:
    assert map_opentdb_to_bucket("Entertainment: Books") == BucketId.ARTE_Y_LITERATURA


def test_science_subcategories_map_to_ciencia_y_naturaleza() -> None:
    for sub in (
        "Science & Nature",
        "Science: Computers",
        "Science: Mathematics",
        "Science: Gadgets",
        "Animals",
        "Vehicles",
    ):
        assert map_opentdb_to_bucket(sub) == BucketId.CIENCIA_Y_NATURALEZA


def test_history_and_politics_map_to_historia() -> None:
    assert map_opentdb_to_bucket("History") == BucketId.HISTORIA
    assert map_opentdb_to_bucket("Politics") == BucketId.HISTORIA


def test_mythology_and_art_map_to_arte_y_literatura() -> None:
    assert map_opentdb_to_bucket("Mythology") == BucketId.ARTE_Y_LITERATURA
    assert map_opentdb_to_bucket("Art") == BucketId.ARTE_Y_LITERATURA


def test_unknown_opentdb_category_raises() -> None:
    with pytest.raises(KeyError):
        map_opentdb_to_bucket("Some Future Category")


def test_niche_subcategories_are_skipped() -> None:
    # These categories exist in OpenTDB but are deliberately excluded from the
    # pack: their content does not match the "classic Trivial Pursuit" feel.
    for sub in (
        "Entertainment: Video Games",
        "Entertainment: Japanese Anime & Manga",
        "Entertainment: Comics",
        "Entertainment: Cartoon & Animations",
        "Entertainment: Board Games",
        "Entertainment: Musicals & Theatres",
        "Celebrities",
    ):
        assert is_skipped(sub), f"{sub} should be in SKIPPED_CATEGORIES"


def test_skipped_categories_are_disjoint_from_mapping() -> None:
    overlap = SKIPPED_CATEGORIES & set(CATEGORY_MAP.keys())
    assert overlap == set(), f"categories cannot be both mapped and skipped: {overlap}"


def test_skipped_categories_raise_on_map() -> None:
    # A skipped category should not be silently mapped — pipeline must check
    # is_skipped() first. If it forgets, mapping must fail loudly.
    for sub in SKIPPED_CATEGORIES:
        with pytest.raises(KeyError):
            map_opentdb_to_bucket(sub)


def test_full_table_covers_all_24_opentdb_categories() -> None:
    # Spec §4.4: 24 OpenTDB categories must each be either mapped or skipped.
    expected = {
        "General Knowledge",
        "Entertainment: Books",
        "Entertainment: Film",
        "Entertainment: Music",
        "Entertainment: Musicals & Theatres",
        "Entertainment: Television",
        "Entertainment: Video Games",
        "Entertainment: Board Games",
        "Entertainment: Comics",
        "Entertainment: Japanese Anime & Manga",
        "Entertainment: Cartoon & Animations",
        "Celebrities",
        "Science & Nature",
        "Science: Computers",
        "Science: Mathematics",
        "Science: Gadgets",
        "Animals",
        "Vehicles",
        "Geography",
        "History",
        "Politics",
        "Mythology",
        "Art",
        "Sports",
    }
    known = set(CATEGORY_MAP.keys()) | SKIPPED_CATEGORIES
    assert known == expected
