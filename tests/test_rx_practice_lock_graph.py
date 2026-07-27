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
STATUS = (ROOT / "src/firmware/morse_flipper_status.c").read_text()
INPUT = (ROOT / "src/firmware/morse_flipper_input.c").read_text()


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
    assert "show_left_hint" not in TYPES + CORE + DRAW + HOST
    assert "state->draw_snapshot->answer_preview" in DRAW
    feed = HOST.index("bool morse_flipper_rx_practice_host_feed")
    clear = HOST.index("app->rx_draw_snapshot.answer_preview = '\\0';", feed)
    call = HOST.index("->feed_text(", clear)
    feed_unlock = HOST.index("furi_mutex_release", call)
    assert feed < clear < call < feed_unlock
    assert "MORSE_FLIPPER_RX_PRACTICE_API_VERSION 10U" in API
    assert "sync_draw_state" not in API + PLUGIN + HOST + RUNTIME
    tick = HOST.index("bool morse_flipper_rx_practice_host_tick")
    preview = HOST.index("preview = mf_rx_answer_preview(app);", tick)
    lock = HOST.index("furi_mutex_acquire", preview)
    cache = HOST.index("app->rx_draw_snapshot.answer_preview", lock)
    unlock = HOST.index("furi_mutex_release", cache)
    assert tick < preview < lock < cache < unlock
    assert "back_owned = app->plugin_slot.mode != 0U;" in HOST
    assert "back_owned = app->plugin_slot.mode != 0U &&" not in HOST
    assert "!result.handled && !back_owned && event->key == InputKeyBack" in HOST
    assert "snapshot.mode != 0U" in STATUS
    assert "g.btn = true;" in STATUS
    assert "g.btn_pad = true;" in STATUS
    assert "g.back_key = true;" in STATUS
    rx_back = INPUT.index("app->screen == MorseFlipperScreenRxPractice &&")
    live_reject = INPUT.index(
        "app->screen == MorseFlipperScreenRxPractice && !g.live", rx_back
    )
    assert "event->key == InputKeyBack && g.back_key" in INPUT[rx_back:live_reject]
    assert "MfRxPracticeCommandPaddleBackPress" not in TYPES + PLUGIN
    assert "button_paddle ? MfRxPracticeCommandNone" in PLUGIN
    assert "canvas_draw_line(canvas, 0, 34, 119, 34);" in DRAW
    assert "canvas_draw_box(canvas, 124, 34, 1, 1);" in DRAW
    rx_draw = LIVE.index("if(app->screen == MorseFlipperScreenRxPractice)")
    rx_draw_end = LIVE.index("return;", rx_draw)
    assert "morse_flipper_plugin_runtime_draw" in LIVE[rx_draw:rx_draw_end]
    assert "morse_flipper_live_left_hint" not in LIVE[rx_draw:rx_draw_end]
    print("rx practice draw lock graph: ok")


if __name__ == "__main__":
    main()
