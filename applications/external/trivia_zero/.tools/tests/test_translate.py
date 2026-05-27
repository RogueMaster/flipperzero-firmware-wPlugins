from __future__ import annotations

from pathlib import Path

import pytest

from trivia_pack.models import Lang
from trivia_pack.translate import StubTranslator, Translator


@pytest.fixture
def cache_path(tmp_path: Path) -> Path:
    return tmp_path / "translations.json"


def test_stub_translator_round_trip(cache_path: Path) -> None:
    t: Translator = StubTranslator(cache_path=cache_path)

    assert t.translate("Hello", source=Lang.EN, target=Lang.ES) == "[es] Hello"
    assert t.translate("Hola", source=Lang.ES, target=Lang.EN) == "[en] Hola"


def test_translator_caches_on_disk(cache_path: Path) -> None:
    t1 = StubTranslator(cache_path=cache_path)
    t1.translate("Hello", source=Lang.EN, target=Lang.ES)
    t1.flush()
    assert cache_path.exists()

    # New translator, same cache file → second call should not re-translate.
    t2 = StubTranslator(cache_path=cache_path)
    assert t2.translate("Hello", source=Lang.EN, target=Lang.ES) == "[es] Hello"
    assert t2.cache_hits == 1
    assert t2.cache_misses == 0


def test_same_input_distinct_target_caches_separately(cache_path: Path) -> None:
    t = StubTranslator(cache_path=cache_path)
    a = t.translate("Hello", source=Lang.EN, target=Lang.ES)
    b = t.translate("Hello", source=Lang.EN, target=Lang.EN)  # identity
    assert a == "[es] Hello"
    assert b == "Hello"  # translating to source language is identity


def test_empty_string_translates_to_empty(cache_path: Path) -> None:
    t = StubTranslator(cache_path=cache_path)
    assert t.translate("", source=Lang.EN, target=Lang.ES) == ""
