#!/usr/bin/env python3
"""Lock the hidden Trace screen's navigation and button-paddle contract."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CONTENT = (ROOT / "src/firmware/morse_flipper_content_host.c").read_text(encoding="utf-8")
INPUT = (ROOT / "src/firmware/morse_flipper_input.c").read_text(encoding="utf-8")
STATUS = (ROOT / "src/firmware/morse_flipper_status.c").read_text(encoding="utf-8")
VIEW = (ROOT / "src/firmware/morse_flipper_live_view.c").read_text(encoding="utf-8")


def function_body(source: str, name: str, signature: str = "") -> str:
    start = source.index(f"{signature}{name}(")
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


def main() -> None:
    actions = function_body(CONTENT, "morse_flipper_content_host_apply_action")
    trace = actions[actions.index("MorseFlipperContentActionOpenTrace") :]
    unload = trace.index("morse_flipper_plugin_runtime_unload_current(app)")
    main_scene = trace.index(
        "scene_manager_search_and_switch_to_another_scene(\n"
        "            app->scene_manager, MorseFlipperSceneMenuMain)"
    )
    open_trace = trace.index(
        "morse_flipper_scene_open(app, MorseFlipperSceneTrace)"
    )
    assert unload < main_scene < open_trace

    gate = function_body(STATUS, "morse_flipper_input_gate")
    assert (
        "app->screen == MorseFlipperScreenRun || "
        "app->screen == MorseFlipperScreenTrace"
    ) in gate
    assert "g.back_key = g.btn_pad;" in gate
    assert "g.left_hint = g.back_key;" in gate

    route = function_body(INPUT, "morse_flipper_run_trace_home_input")
    trace_route = route.index("app->screen == MorseFlipperScreenTrace")
    assert "morse_flipper_handle_active_keying_event(app, event);" in route[trace_route:]

    keying = function_body(
        INPUT, "morse_flipper_handle_active_keying_event", "void "
    )
    assert "event->key == InputKeyLeft" in keying
    assert "event->type == InputTypeLong" in keying
    assert "morse_flipper_leave_live_screen(app, now_ms);" in keying
    assert "event->key == InputKeyBack" in keying
    assert "btn_pad" in keying

    trace_view = VIEW[
        VIEW.index("if(app->screen == MorseFlipperScreenTrace)") :
        VIEW.index("\n    canvas_set_font(canvas, FontPrimary)")
    ]
    assert "morse_flipper_live_left_hint(app)" in trace_view
    assert "morse_flipper_draw_left_exit_hint(canvas)" in trace_view

    print("trace input and navigation contract: ok")


if __name__ == "__main__":
    main()
