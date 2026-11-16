"""Console, JSON, and JUnit reporting for physical hardware runs."""

import json
import platform
import subprocess
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path


def metadata(root: Path, suite: str, reader: str) -> dict:
    return {
        "schema": 1,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "suite": suite,
        "reader": reader,
        "host": platform.platform(),
        "git_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
    }


def write_reports(output: Path, run: dict) -> None:
    output.mkdir(parents=True, exist_ok=True)
    (output / "results.json").write_text(
        json.dumps(run, indent=2, sort_keys=True) + "\n"
    )

    cases = run["cases"]
    failures = sum(case["status"] != "passed" for case in cases)
    suite = ET.Element(
        "testsuite",
        name="dfc-flipper-hardware",
        tests=str(len(cases)),
        failures=str(failures),
        time=f"{sum(case['duration_ms'] for case in cases) / 1000:.3f}",
    )
    for case in cases:
        node = ET.SubElement(
            suite,
            "testcase",
            classname=case["suite"],
            name=f"{case['name']}[{case['cycle']}]",
            time=f"{case['duration_ms'] / 1000:.3f}",
        )
        if case["status"] != "passed":
            ET.SubElement(node, "failure", message=case["error"]).text = case["error"]
    ET.ElementTree(suite).write(
        output / "junit.xml", encoding="utf-8", xml_declaration=True
    )


def print_case(case: dict) -> None:
    marker = "PASS" if case["status"] == "passed" else "FAIL"
    print(
        f"{marker} {case['name']} cycle {case['cycle']}: {case['duration_ms']:.1f} ms"
    )
    if case.get("error"):
        print(f"  {case['error']}")
