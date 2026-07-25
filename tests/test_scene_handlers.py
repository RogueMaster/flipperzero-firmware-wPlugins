#!/usr/bin/env python3
"""Keep scene callback tables complete and tied to named scene IDs."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/firmware/morse_flipper_app_i.h"
SCENES = ROOT / "src/firmware/morse_flipper_scenes.c"


def scene_names() -> set[str]:
    text = HEADER.read_text(encoding="utf-8")
    body = re.search(
        r"typedef enum \{(?P<body>.*?)MorseFlipperSceneNum,\s*\}\s*MorseFlipperScene;",
        text,
        re.DOTALL,
    )
    if body is None:
        raise AssertionError("MorseFlipperScene enum not found")
    return set(re.findall(r"\b(MorseFlipperScene[A-Za-z0-9_]+)\b", body.group("body")))


def handler_table(name: str) -> dict[str, str]:
    text = SCENES.read_text(encoding="utf-8")
    body = re.search(
        rf"\b{name}\[MorseFlipperSceneNum\]\s*=\s*\{{(?P<body>.*?)\n\}};",
        text,
        re.DOTALL,
    )
    if body is None:
        raise AssertionError(f"{name} not found")
    entries = re.findall(
        r"\[(MorseFlipperScene[A-Za-z0-9_]+)\]\s*=\s*"
        r"(morse_flipper_scene_[A-Za-z0-9_]+)",
        body.group("body"),
    )
    mapped = dict(entries)
    if len(mapped) != len(entries):
        raise AssertionError(f"{name} contains duplicate scene designators")
    return mapped


class SceneHandlerTableTest(unittest.TestCase):
    def test_every_scene_has_named_handlers(self) -> None:
        expected = scene_names()
        for table_name in (
            "morse_flipper_scene_on_enter_handlers",
            "morse_flipper_scene_on_event_handlers",
            "morse_flipper_scene_on_exit_handlers",
        ):
            with self.subTest(table=table_name):
                self.assertEqual(set(handler_table(table_name)), expected)

    def test_regression_sensitive_event_handlers(self) -> None:
        events = handler_table("morse_flipper_scene_on_event_handlers")
        self.assertEqual(
            events["MorseFlipperSceneSessionEnd"],
            "morse_flipper_scene_live_on_event",
        )
        self.assertEqual(
            events["MorseFlipperSceneProgress"],
            "morse_flipper_scene_progress_on_event",
        )
        self.assertEqual(
            events["MorseFlipperScenePassive"],
            "morse_flipper_scene_live_on_event",
        )


if __name__ == "__main__":
    unittest.main()
