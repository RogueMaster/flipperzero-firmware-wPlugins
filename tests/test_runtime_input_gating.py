#!/usr/bin/env python3
"""Keep global GPIO polling out of navigation and passive screens."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME = (ROOT / "src/firmware/morse_flipper_runtime.c").read_text(encoding="utf-8")
KEYING = (ROOT / "src/firmware/morse_flipper_keying.c").read_text(encoding="utf-8")
SCREEN = (ROOT / "src/firmware/morse_flipper_screen.c").read_text(encoding="utf-8")
STATUS = (ROOT / "src/firmware/morse_flipper_status.c").read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    start = source.index(f"{name}(")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    raise AssertionError(f"unterminated function: {name}")


def compact(value: str) -> str:
    return " ".join(value.split())


def main() -> None:
    allowed = function_body(RUNTIME, "morse_flipper_gpio_keying_screen_allowed")
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

    release = function_body(RUNTIME, "morse_flipper_release_gpio_inputs")
    assert "MORSE_SOURCE_STRAIGHT_GPIO, false" in release
    assert "MORSE_PADDLE_SOURCE_GPIO_DIT, false" in release
    assert "MORSE_PADDLE_SOURCE_GPIO_DAH, false" in release
    assert "morse_keyer_reset(&app->keyer);" in release
    assert "morse_flipper_drain_keyer_events(app);" in release

    sync = function_body(RUNTIME, "morse_flipper_sync_gpio_inputs")
    gate = sync.index("if(!morse_flipper_gpio_keying_screen_allowed(app))")
    pin_read = min(
        sync.index("morse_flipper_straight_down()"),
        sync.index("morse_flipper_logical_dit_down(app)"),
    )
    assert gate < pin_read
    assert "morse_flipper_release_gpio_inputs(app, now_ms);" in sync[gate:pin_read]

    logical_dit = compact(function_body(RUNTIME, "morse_flipper_logical_dit_down"))
    logical_dah = compact(function_body(RUNTIME, "morse_flipper_logical_dah_down"))
    assert logical_dit.count("app->handedness") == 1
    assert "return morse_flipper_dah_down();" in logical_dit
    assert "return morse_flipper_dit_down();" in logical_dit
    assert logical_dah.count("app->handedness") == 1
    assert "return morse_flipper_dit_down();" in logical_dah
    assert "return morse_flipper_dah_down();" in logical_dah

    sync_compact = compact(sync)
    assert "dit_active = morse_flipper_logical_dit_down(app);" in sync_compact
    assert "dah_active = morse_flipper_logical_dah_down(app);" in sync_compact
    assert (
        "app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_GPIO_DIT, dit_active, now_ms"
        in sync_compact
    )
    assert (
        "app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_GPIO_DAH, dah_active, now_ms"
        in sync_compact
    )

    ok_map = compact(function_body(STATUS, "morse_flipper_ok_button_paddle"))
    back_map = compact(function_body(STATUS, "morse_flipper_back_button_paddle"))
    assert "? MorseKeyerPaddleDit : MorseKeyerPaddleDah" in ok_map
    assert "? MorseKeyerPaddleDah : MorseKeyerPaddleDit" in back_map

    button_sync = compact(function_body(KEYING, "morse_flipper_resync_button_paddles"))
    assert "morse_flipper_ok_button_paddle(app)" in button_sync
    assert "morse_flipper_back_button_paddle(app)" in button_sync
    assert button_sync.count("morse_flipper_set_paddle_source(") == 4
    assert "handedness" not in button_sync
    for expected in (
        "app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_OK, app->ok_down && ok_paddle == MorseKeyerPaddleDit, now_ms",
        "app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_OK, app->ok_down && ok_paddle == MorseKeyerPaddleDah, now_ms",
        "app, MorseKeyerPaddleDit, MORSE_PADDLE_SOURCE_BTN_BACK, app->back_down && back_paddle == MorseKeyerPaddleDit, now_ms",
        "app, MorseKeyerPaddleDah, MORSE_PADDLE_SOURCE_BTN_BACK, app->back_down && back_paddle == MorseKeyerPaddleDah, now_ms",
    ):
        assert expected in button_sync

    shared = compact(function_body(KEYING, "morse_flipper_set_paddle_source"))
    assert "morse_keyer_paddle_event(&app->keyer, paddle, after != 0U, now_ms);" in shared
    assert "handedness" not in shared

    transition = function_body(SCREEN, "morse_flipper_enter_screen")
    enter_reset = transition.index(
        "if(screen == MorseFlipperScreenSession && app->screen != MorseFlipperScreenSession"
    )
    reset = transition.index("morse_keyer_reset(&app->keyer);")
    drain = transition.index("morse_flipper_drain_keyer_events(app);", reset)
    drop = transition.index("morse_flipper_drop_live_keying_for_playback(app, now_ms);", drain)
    assign = transition.index("app->screen = screen;")
    assert reset < drain < drop < enter_reset < assign
    assert "morse_flipper_clear_button_keying(app, now_ms);" not in transition[:enter_reset]

    print("runtime GPIO input gating and paddle routing: ok")


if __name__ == "__main__":
    main()
