from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TYPES = (ROOT / "src/firmware/plugins/rx_practice/morse_flipper_rx_practice_types.h").read_text()
DRAW = (ROOT / "src/firmware/plugins/rx_practice/mf_rx_practice_draw.c").read_text()
HOST = (ROOT / "src/firmware/morse_flipper_rx_practice_host.c").read_text()


def main() -> None:
    assert "MfRxPracticeDrawServices" not in TYPES
    assert "draw_services" not in TYPES
    assert "draw_services" not in DRAW
    assert "morse_flipper_live_left_hint" not in HOST
    assert "state->button_paddle" in DRAW
    assert "state->answer_preview" in DRAW
    assert "MfRxPracticeState*)app->plugin_slot.state)->answer_preview" in HOST
    print("rx practice draw lock graph: ok")


if __name__ == "__main__":
    main()
