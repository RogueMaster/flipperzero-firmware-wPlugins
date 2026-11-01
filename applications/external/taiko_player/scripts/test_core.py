#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 ZeroNinx

"""Build and run portable C tests, including local songs when present."""

import os
from pathlib import Path
import shlex
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    build = root / "build"
    build.mkdir(exist_ok=True)
    executable = build / "test_core"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Isrc",
            "src/tja_parser.c",
            "src/wav_reader.c",
            "tests/test_core.c",
            "-lm",
            "-o",
            str(executable),
        ],
        cwd=root,
        check=True,
    )
    songs = sorted((root / "songs").rglob("*.tja"))
    subprocess.run([str(executable), *map(str, songs)], cwd=root, check=True)
    gpio_test = build / "test_gpio"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Itests/stubs",
            "-Isrc",
            "src/taiko_gpio.c",
            "src/taiko_gpio_config.c",
            "tests/test_gpio.c",
            "-o",
            str(gpio_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(gpio_test)], cwd=root, check=True)
    lane_test = build / "test_lane"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Isrc",
            "src/taiko_lane.c",
            "src/taiko_hits.c",
            "tests/test_lane.c",
            "-lm",
            "-o",
            str(lane_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(lane_test)], cwd=root, check=True)
    bars_test = build / "test_bars"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Isrc",
            "src/tja_parser.c",
            "src/taiko_lane.c",
            "src/taiko_hits.c",
            "tests/test_bars.c",
            "-lm",
            "-o",
            str(bars_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(bars_test)], cwd=root, check=True)
    menu_test = build / "test_menu"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Isrc",
            "src/taiko_menu.c",
            "src/taiko_gpio_config.c",
            "tests/test_menu.c",
            "-o",
            str(menu_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(menu_test)], cwd=root, check=True)
    settings_test = build / "test_settings"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Isrc",
            "src/taiko_menu.c",
            "src/taiko_gpio_config.c",
            "src/taiko_settings.c",
            "tests/test_settings.c",
            "-o",
            str(settings_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(settings_test)], cwd=root, check=True)
    mixer_test = build / "test_mixer"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Isrc",
            "src/taiko_mixer.c",
            "src/taiko_hits.c",
            "tests/test_mixer.c",
            "-lm",
            "-o",
            str(mixer_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(mixer_test)], cwd=root, check=True)
    rolls_test = build / "test_rolls"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-Isrc",
            "src/tja_parser.c",
            "src/taiko_hits.c",
            "src/taiko_lane.c",
            "src/taiko_mixer.c",
            "tests/test_rolls.c",
            "-lm",
            "-o",
            str(rolls_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(rolls_test)], cwd=root, check=True)
    memory_test = build / "test_memory"
    subprocess.run(
        [
            *shlex.split(os.environ.get("CC", "cc")),
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-g",
            "-fsanitize=address,undefined",
            "-DFAP_VERSION=1",
            "-Itests/stubs",
            "-Isrc",
            "src/tja_parser.c",
            "tests/test_memory.c",
            "-lm",
            "-o",
            str(memory_test),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(memory_test)], cwd=root, check=True)


if __name__ == "__main__":
    main()
