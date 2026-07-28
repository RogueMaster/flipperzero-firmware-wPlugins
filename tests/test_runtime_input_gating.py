#!/usr/bin/env python3
"""Keep global GPIO polling out of navigation and passive screens."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME = (ROOT / "src/firmware/morse_flipper_runtime.c").read_text(encoding="utf-8")


def function_body(name: str) -> str:
    start = RUNTIME.index(f"{name}(")
    brace = RUNTIME.index("{", start)
    depth = 0
    for index in range(brace, len(RUNTIME)):
        if RUNTIME[index] == "{":
            depth += 1
        elif RUNTIME[index] == "}":
            depth -= 1
            if depth == 0:
                return RUNTIME[brace + 1 : index]
    raise AssertionError(f"unterminated function: {name}")


def main() -> None:
    allowed = function_body("morse_flipper_gpio_keying_screen_allowed")
    expected = {
        "MorseFlipperScreenRun",
        "MorseFlipperScreenTrace",
        "MorseFlipperScreenSession",
        "MorseFlipperScreenRf",
        "MorseFlipperScreenStraight",
        "MorseFlipperScreenHamRun",
        "MorseFlipperScreenTxGroups",
        "MorseFlipperScreenRxPractice",
    }
    for screen in expected:
        assert f"case {screen}:" in allowed
    for screen in (
        "MorseFlipperScreenMenu",
        "MorseFlipperScreenHelp",
        "MorseFlipperScreenAbout",
        "MorseFlipperScreenPassive",
        "MorseFlipperScreenIcr",
        "MorseFlipperScreenRfRx",
        "MorseFlipperScreenRfFreq",
    ):
        assert f"case {screen}:" not in allowed

    release = function_body("morse_flipper_release_gpio_inputs")
    assert "MORSE_SOURCE_STRAIGHT_GPIO, false" in release
    assert "MORSE_PADDLE_SOURCE_GPIO_DIT, false" in release
    assert "MORSE_PADDLE_SOURCE_GPIO_DAH, false" in release

    sync = function_body("morse_flipper_sync_gpio_inputs")
    gate = sync.index("if(!morse_flipper_gpio_keying_screen_allowed(app))")
    pin_read = min(
        sync.index("morse_flipper_straight_down()"),
        sync.index("morse_flipper_logical_dit_down(app)"),
    )
    assert gate < pin_read
    assert "morse_flipper_release_gpio_inputs(app, now_ms);" in sync[gate:pin_read]

    print("runtime GPIO input gating: ok")


if __name__ == "__main__":
    main()
