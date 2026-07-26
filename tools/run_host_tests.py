#!/usr/bin/env python3
"""Build and run the Morse Flipper host test suite in a fresh directory."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "src" / "firmware"
SANITIZE = os.environ.get("MF_HOST_SANITIZE") == "1"


@dataclass(frozen=True)
class CTest:
    name: str
    sources: tuple[str, ...]
    defines: tuple[str, ...] = ()
    include_tests: bool = True


TESTS = (
    CTest("time", ("tests/test_time.c",)),
    CTest("training-timing", ("tests/test_training_timing.c", "src/firmware/morse_flipper_training_timing.c")),
    CTest("cw-decoder-preview", ("tests/test_cw_decoder_preview.c", "src/firmware/morse_flipper_cw_decoder.c", "src/firmware/morse_flipper_cw_token.c", "src/firmware/cw.c")),
    CTest("cw-markdown-widget", ("tests/test_cw_markdown_widget.c", "src/firmware/plugins/help_about/cw_markdown_widget.c"), ("CWMD_HOST_TEST",), False),
    CTest("rx-callsign-gen", ("tests/test_rx_callsign_gen.c", "src/firmware/plugins/common/mf_rx_rng.c", "src/firmware/plugins/common/mf_callsign_gen.c", "src/firmware/cw.c")),
    CTest("rx-callsign-controlled", ("tests/test_rx_callsign_controlled.c", "src/firmware/plugins/common/mf_callsign_gen.c", "src/firmware/cw.c")),
    CTest("rx-practice", ("tests/test_rx_practice.c", "src/firmware/plugins/rx_practice/mf_rx_practice_core.c", "src/firmware/plugins/common/mf_rx_rng.c", "src/firmware/plugins/common/mf_callsign_gen.c", "src/firmware/cw.c")),
    CTest("tx-groups", ("tests/test_tx_groups.c", "src/firmware/plugins/tx_groups/mf_tx_groups_core.c", "src/firmware/morse_flipper_cw_token.c", "src/firmware/cw.c")),
    CTest("passive-codec", ("tests/test_passive_codec.c", "src/firmware/plugins/passive_listening/mf_passive_codec.c")),
    CTest("passive-loading", ("tests/test_passive_loading.c", "src/firmware/plugins/passive_listening/mf_passive_loading.c")),
    CTest("passive-policy", ("tests/test_passive_policy.c", "src/firmware/plugins/passive_listening/mf_passive_policy.c", "src/firmware/plugins/passive_listening/mf_passive_voice_pack.c", "src/firmware/plugins/passive_listening/mf_passive_codec.c")),
    CTest("passive-voice-pack", ("tests/test_passive_voice_pack.c", "src/firmware/plugins/passive_listening/mf_passive_voice_pack.c", "src/firmware/plugins/passive_listening/mf_passive_codec.c")),
    CTest("passive-core", ("tests/test_passive_core.c", "src/firmware/plugins/passive_listening/mf_passive_core.c", "src/firmware/plugins/passive_listening/mf_passive_loading.c", "src/firmware/plugins/passive_listening/mf_passive_policy.c", "src/firmware/plugins/passive_listening/mf_passive_voice_pack.c", "src/firmware/plugins/passive_listening/mf_passive_codec.c", "src/firmware/plugins/common/mf_rx_rng.c", "src/firmware/plugins/common/mf_callsign_gen.c", "src/firmware/cw.c")),
    CTest("audio-pwm", ("tests/test_audio_pwm.c", "src/firmware/morse_flipper_audio_pwm.c")),
    CTest("settings-model", ("tests/test_settings_model.c", "src/firmware/plugins/settings/mf_settings_model.c", "src/firmware/trainer_lesson.c", "src/firmware/cw.c")),
    CTest("settings-plugin", ("tests/test_settings_plugin.c", "src/firmware/plugins/settings/mf_settings_model.c", "src/firmware/plugins/settings/mf_settings_plugin.c", "src/firmware/trainer_lesson.c", "src/firmware/pc_keys.c", "src/firmware/cw.c"), ("MF_SETTINGS_HOST_TEST",)),
    CTest("settings-host", ("tests/test_settings_host.c", "src/firmware/morse_flipper_settings_host.c"), ("MF_SETTINGS_HOST_TEST",)),
    CTest("config-compat", ("tests/test_config_compat.c", "src/firmware/morse_flipper_config.c"), ("MF_CONFIG_HOST_TEST",)),
    CTest("progress", ("tests/test_progress.c", "src/firmware/morse_flipper_progress.c", "src/firmware/trainer.c", "src/firmware/cw.c")),
    CTest("icr", ("tests/test_icr.c", "src/firmware/plugins/icr/morse_flipper_icr.c")),
    CTest("icr-runtime", ("tests/test_icr_runtime.c", "src/firmware/plugins/icr/morse_flipper_icr.c", "src/firmware/plugins/icr/morse_flipper_icr_runtime.c", "src/firmware/cw.c")),
)

PYTHON_TESTS = (
    "tests/test_passive_config_compat.py",
    "tests/test_passive_voice_pack_tool.py",
    "tests/test_scene_handlers.py",
    "tests/test_terminus24_asset.py",
    "tests/test_version_consistency.py",
)


def compiler() -> str:
    for candidate in ("cc", "gcc", "clang"):
        path = shutil.which(candidate)
        if path:
            return path
    raise SystemExit("no C compiler found (tried cc, gcc, clang)")


def include_paths(include_tests: bool) -> list[str]:
    paths = [FIRMWARE, *(path for path in (FIRMWARE / "plugins").iterdir() if path.is_dir())]
    if include_tests:
        paths.append(ROOT / "tests" / "include")
    return [f"-I{path}" for path in paths]


def run_c_test(test: CTest, build_dir: Path, cc: str) -> None:
    sources = [ROOT / source for source in test.sources]
    missing = [str(source) for source in sources if not source.is_file()]
    if missing:
        raise SystemExit(f"{test.name}: missing registered source: {', '.join(missing)}")
    output = build_dir / test.name
    command = [cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror"]
    if SANITIZE:
        command.extend(("-fsanitize=address,undefined", "-fno-omit-frame-pointer"))
    command.extend(f"-D{define}" for define in test.defines)
    command.extend(include_paths(test.include_tests))
    command.extend(str(source) for source in sources)
    command.extend(("-o", str(output)))
    print(f"[C] {test.name}")
    subprocess.run(command, check=True, cwd=ROOT)
    subprocess.run([str(output)], check=True, cwd=ROOT)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("tests", nargs="*", help="optional registered C test names")
    args = parser.parse_args()
    tests_by_name = {test.name: test for test in TESTS}
    unknown = [name for name in args.tests if name not in tests_by_name]
    if unknown:
        parser.error(f"unknown test name(s): {', '.join(unknown)}")
    selected = [tests_by_name[name] for name in args.tests] if args.tests else list(TESTS)
    cc = compiler()
    with tempfile.TemporaryDirectory(prefix="morse-flipper-host-tests-") as directory:
        build_dir = Path(directory)
        for test in selected:
            run_c_test(test, build_dir, cc)
    if not args.tests:
        for relative_path in PYTHON_TESTS:
            path = ROOT / relative_path
            if not path.is_file():
                raise SystemExit(f"missing registered Python test: {path}")
            print(f"[Python] {relative_path}")
            subprocess.run([sys.executable, str(path)], check=True, cwd=ROOT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
