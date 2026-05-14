#!/usr/bin/env python3
"""Regenerate tests/fixtures/golden_labels.json from current analyze.py output."""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))
from analyze import parse_sub_file, extract_features, classify

ROOT = pathlib.Path(__file__).parent.parent
SUB_DIR = ROOT / "data" / "subfiles"
OUT = ROOT / "tests" / "fixtures" / "golden_labels.json"


def main() -> None:
    labels = {}
    for path in sorted(SUB_DIR.glob("*.sub")):
        sub = parse_sub_file(str(path))
        fv = extract_features(sub)
        result = classify(fv)
        labels[path.name] = [result.label, result.confidence]

    payload = {
        "_comment": "Baseline labels for data/subfiles/. Regenerate with tests/regen_golden.py after intentional classifier changes.",
        "labels": labels,
    }
    OUT.write_text(json.dumps(payload, indent=2) + "\n")
    print(f"Wrote {len(labels)} labels to {OUT}")


if __name__ == "__main__":
    main()
