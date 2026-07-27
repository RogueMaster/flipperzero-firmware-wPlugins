from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TYPES = (ROOT / "src/firmware/plugins/rx_practice/morse_flipper_rx_practice_types.h").read_text()
API = (ROOT / "src/firmware/plugins/rx_practice/morse_flipper_rx_practice_api.h").read_text()
CORE = (ROOT / "src/firmware/plugins/rx_practice/mf_rx_practice_core.h").read_text()
DRAW = (ROOT / "src/firmware/plugins/rx_practice/mf_rx_practice_draw.c").read_text()
PLUGIN = (ROOT / "src/firmware/plugins/rx_practice/mf_rx_practice_plugin.c").read_text()
HOST = (ROOT / "src/firmware/morse_flipper_rx_practice_host.c").read_text()
RUNTIME = (ROOT / "src/firmware/morse_flipper_plugin_runtime.c").read_text()
LIVE = (ROOT / "src/firmware/morse_flipper_live_view.c").read_text()


def main() -> None:
    assert "MfRxPracticeDrawServices" not in TYPES
    assert "(*" not in TYPES
    assert "(*" not in CORE
    assert "draw_services" not in TYPES
    assert "draw_services" not in DRAW
    assert "morse_flipper_live_left_hint" not in HOST
    assert "mf_rx_practice_core.h" not in HOST
    assert "MfRxPracticeState" not in HOST
    assert "const MfRxPracticeDrawSnapshot* draw_snapshot" in TYPES
    assert "const MfRxPracticeDrawSnapshot* draw_snapshot" in CORE
    assert "state->draw_snapshot->show_left_hint" in DRAW
    assert "state->draw_snapshot->answer_preview" in DRAW
    assert "MORSE_FLIPPER_RX_PRACTICE_API_VERSION 9U" in API
    assert "sync_draw_state" not in API + PLUGIN + HOST + RUNTIME
    tick = HOST.index("bool morse_flipper_rx_practice_host_tick")
    preview = HOST.index("preview = mf_rx_answer_preview(app);", tick)
    lock = HOST.index("furi_mutex_acquire", preview)
    cache = HOST.index("app->rx_draw_snapshot.answer_preview", lock)
    unlock = HOST.index("furi_mutex_release", cache)
    assert tick < preview < lock < cache < unlock
    assert "back_owned = app->plugin_slot.mode != 0U &&" in HOST
    assert "back_owned && app->plugin_slot.start_hold_mask == 0U" in HOST
    assert "!result.handled && !back_owned && event->key == InputKeyBack" in HOST
    rx_draw = LIVE.index("if(app->screen == MorseFlipperScreenRxPractice)")
    rx_draw_end = LIVE.index("return;", rx_draw)
    assert "morse_flipper_plugin_runtime_draw" in LIVE[rx_draw:rx_draw_end]
    assert "morse_flipper_live_left_hint" not in LIVE[rx_draw:rx_draw_end]
    print("rx practice draw lock graph: ok")


if __name__ == "__main__":
    main()
