"""Parity test: every .sub in data/subfiles must classify to its golden label.

Locks in current Python behavior so accuracy changes are explicit. To accept a
new label, run `python3 tests/regen_golden.py` and commit the updated file.
"""
import json
import pathlib
import sys

import pytest

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))
from analyze import parse_sub_file, extract_features, classify

ROOT = pathlib.Path(__file__).parent.parent
SUB_DIR = ROOT / "data" / "subfiles"
GOLDEN = ROOT / "tests" / "fixtures" / "golden_labels.json"


def _load_golden():
    return json.loads(GOLDEN.read_text())["labels"]


def _sub_files():
    return sorted(SUB_DIR.glob("*.sub"))


@pytest.mark.parametrize("path", _sub_files(), ids=lambda p: p.name)
def test_parity(path):
    golden = _load_golden()
    expected = golden.get(path.name)
    assert expected is not None, (
        f"{path.name} missing from golden — run tests/regen_golden.py"
    )
    sub = parse_sub_file(str(path))
    fv = extract_features(sub)
    result = classify(fv)
    actual = [result.label, result.confidence]
    assert actual == expected, f"{path.name}: got {actual}, golden has {expected}"


def test_golden_covers_all_fixtures():
    golden = _load_golden()
    missing = {p.name for p in _sub_files()} - set(golden.keys())
    assert not missing, f"Missing golden labels: {sorted(missing)}"
