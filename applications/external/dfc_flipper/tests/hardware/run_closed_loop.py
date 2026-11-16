#!/usr/bin/env python3
"""Build, deploy, exercise, report, and clean a Flipper DESFire test run."""

import argparse
import hashlib
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tests.hardware.credential import load_read_fixture, load_read_fixtures
    from tests.hardware.flipper import Flipper
    from tests.hardware.pcsc import PcscCard, run_iso_2k3des_read, run_plain_reads
    from tests.hardware.report import metadata, print_case, write_reports
else:
    from .credential import load_read_fixture, load_read_fixtures
    from .flipper import Flipper
    from .pcsc import PcscCard, run_iso_2k3des_read, run_plain_reads
    from .report import metadata, print_case, write_reports


ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "tests/hardware/fixtures"


@dataclass(frozen=True)
class Case:
    name: str
    fixture: Path
    probe: str


SMOKE = Case("iso-2k3des-mac-read-1k", FIXTURES / "iso_2k3des_mac_1k.dfc", "iso")
MATRIX = (SMOKE, Case("ev3-plain-read-2k", FIXTURES / "ev3_plain_2k.dfc", "plain"))


def cases_for(suite: str) -> tuple[Case, ...]:
    return (SMOKE,) if suite == "smoke" else MATRIX


def compile_fixture(source: Path, destination: Path) -> None:
    subprocess.run(
        [
            sys.executable,
            str(ROOT / "tests/compile_dfcb.py"),
            str(source),
            str(destination),
        ],
        cwd=ROOT,
        check=True,
    )


def deploy_app() -> None:
    subprocess.run(["ufbt", "launch"], cwd=ROOT, check=True)


def execute_probe(case: Case, reader: str):
    if case.probe == "iso":
        fixture = load_read_fixture(case.fixture)
        with PcscCard(reader) as card:
            return run_iso_2k3des_read(card, fixture)
    return run_plain_reads(reader, load_read_fixtures(case.fixture))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", choices=("smoke", "matrix", "all"), default="smoke")
    parser.add_argument("--reader", default="ACR1552")
    parser.add_argument("--port", default="auto")
    parser.add_argument("--output", type=Path, default=ROOT / "artifacts/hardware")
    parser.add_argument(
        "--cycles", type=int, help="override 10 smoke or 3 matrix cycles"
    )
    parser.add_argument(
        "--skip-deploy", action="store_true", help="reuse the installed FAP"
    )
    args = parser.parse_args()
    if args.cycles is not None and args.cycles < 1:
        parser.error("--cycles must be positive")

    suite = "matrix" if args.suite == "all" else args.suite
    cycles = args.cycles or (10 if suite == "smoke" else 3)
    run = {
        **metadata(ROOT, args.suite, args.reader),
        "timing_policy": "record-only",
        "cases": [],
    }
    failure = False

    try:
        if not args.skip_deploy:
            deploy_app()

        with tempfile.TemporaryDirectory(prefix="dfc-hardware-") as temporary:
            compiled = Path(temporary)
            with Flipper(args.port) as flipper:
                flipper.close_app()
                flipper.prepare_directory()
                try:
                    for cycle in range(1, cycles + 1):
                        for case in cases_for(suite):
                            record = {
                                "suite": suite,
                                "name": case.name,
                                "cycle": cycle,
                                "status": "failed",
                                "error": "",
                            }
                            started = time.perf_counter()
                            remote = ""
                            memory = {"before": flipper.heap_snapshot()}
                            try:
                                binary = compiled / f"{case.name}-{cycle}.dfcb"
                                compile_fixture(case.fixture, binary)
                                remote = flipper.upload_verified(binary, binary.name)
                                launch_output = flipper.launch_fixture(remote)
                                result = execute_probe(case, args.reader)
                                logs = launch_output + flipper.drain_logs()
                                memory["during"] = flipper.heap_snapshot()
                                record.update(
                                    status="passed",
                                    fixture_sha256=hashlib.sha256(
                                        binary.read_bytes()
                                    ).hexdigest(),
                                    bytes_read=result.bytes_read,
                                    frames=result.frames,
                                    timings_ms=result.timings_ms,
                                    memory=memory,
                                    memory_log=flipper.memory_samples(logs),
                                )
                            except Exception as error:
                                failure = True
                                record["error"] = f"{type(error).__name__}: {error}"
                            finally:
                                record["duration_ms"] = (
                                    time.perf_counter() - started
                                ) * 1000
                                run["cases"].append(record)
                                print_case(record)
                                flipper.close_app()
                                if remote and record["status"] == "passed":
                                    record["hardware_profile"] = (
                                        flipper.read_hardware_profile()
                                    )
                                memory["after"] = flipper.heap_snapshot()
                                record["memory"] = memory
                                if remote:
                                    flipper.remove_fixture(remote)
                finally:
                    flipper.cleanup()
    except Exception as error:
        failure = True
        record = {
            "suite": suite,
            "name": "infrastructure",
            "cycle": 0,
            "status": "failed",
            "error": f"{type(error).__name__}: {error}",
            "duration_ms": 0.0,
        }
        run["cases"].append(record)
        print_case(record)
    finally:
        write_reports(args.output, run)
    print(f"JSON: {args.output / 'results.json'}")
    print(f"JUnit: {args.output / 'junit.xml'}")
    return 1 if failure else 0


if __name__ == "__main__":
    raise SystemExit(main())
