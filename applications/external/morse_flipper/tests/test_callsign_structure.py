#!/usr/bin/env python3
"""Keep callsign coverage explicit without regressing to per-entity code."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/firmware/plugins/common/mf_callsign_gen.h"
SOURCE = ROOT / "src/firmware/plugins/common/mf_callsign_gen.c"

EXPECTED_ENTITIES = [
    "MfCallsignEntityUs",
    "MfCallsignEntityGermany",
    "MfCallsignEntityItaly",
    "MfCallsignEntityCanada",
    "MfCallsignEntityRomania",
    "MfCallsignEntityEuropeanRussia",
    "MfCallsignEntitySpain",
    "MfCallsignEntityJapan",
    "MfCallsignEntityPoland",
    "MfCallsignEntityEngland",
    "MfCallsignEntityBrazil",
    "MfCallsignEntityFinland",
    "MfCallsignEntityFrance",
    "MfCallsignEntityCzechRepublic",
    "MfCallsignEntityUkraine",
    "MfCallsignEntityNetherlands",
    "MfCallsignEntityAsiaticRussia",
    "MfCallsignEntityArgentina",
    "MfCallsignEntitySlovenia",
    "MfCallsignEntityHungary",
]


def main() -> None:
    header = HEADER.read_text(encoding="ascii")
    source = SOURCE.read_text(encoding="ascii")
    enum = re.search(r"typedef enum \{(.*?)\} MfCallsignEntity;", header, re.DOTALL)
    assert enum is not None
    names = re.findall(r"\bMfCallsignEntity[A-Za-z]+\b", enum.group(1))
    assert names == [*EXPECTED_ENTITIES, "MfCallsignEntityCount"]
    assert "Indonesia" not in header + source
    assert "China" not in header + source
    assert "mf_make_" not in source
    assert not re.search(r"\b(?:malloc|calloc|realloc|free)\s*\(", source)
    assert source.count("RULE(") < 180
    print("test_callsign_structure: passed")


if __name__ == "__main__":
    main()
