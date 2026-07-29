#!/usr/bin/env python3
"""Keep scene callback tables complete and tied to named scene IDs."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/firmware/morse_flipper_app_i.h"
SCENES = ROOT / "src/firmware/morse_flipper_scenes.c"
APP = ROOT / "src/firmware/morse_flipper_app.c"
INPUT = ROOT / "src/firmware/morse_flipper_input.c"
FLOW = ROOT / "src/firmware/morse_flipper_flow.c"
SESSION = ROOT / "src/firmware/morse_flipper_session.c"
SETTINGS_MODEL = ROOT / "src/firmware/plugins/settings/mf_settings_model.c"
SETTINGS_PLUGIN = ROOT / "src/firmware/plugins/settings/mf_settings_plugin.c"
ICR_RUNTIME = ROOT / "src/firmware/plugins/icr/morse_flipper_icr_runtime.c"


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

    def test_settings_menu_routes_only_ordinary_pages_to_settings_fal(self) -> None:
        for table_name, expected in (
            (
                "morse_flipper_scene_on_enter_handlers",
                {
                    "MorseFlipperSceneHome": "morse_flipper_scene_home_on_enter",
                    "MorseFlipperSceneAudioCfg": "morse_flipper_scene_audio_cfg_on_enter",
                    "MorseFlipperSceneTrainer": "morse_flipper_scene_settings_listening_on_enter",
                    "MorseFlipperSceneStraightCfg": "morse_flipper_scene_settings_straight_on_enter",
                    "MorseFlipperSceneTxGroupsCfg": "morse_flipper_scene_settings_tx_groups_on_enter",
                    "MorseFlipperSceneRxCallsignsCfg": "morse_flipper_scene_settings_rx_callsigns_on_enter",
                    "MorseFlipperSceneGpio": "morse_flipper_scene_settings_gpio_on_enter",
                    "MorseFlipperScenePc": "morse_flipper_scene_pc_on_enter",
                    "MorseFlipperSceneIcr": "morse_flipper_scene_icr_on_enter",
                    "MorseFlipperScenePassive": "morse_flipper_scene_passive_on_enter",
                },
            ),
            (
                "morse_flipper_scene_on_event_handlers",
                {
                    "MorseFlipperSceneTrainer": "morse_flipper_scene_settings_on_event",
                    "MorseFlipperSceneStraightCfg": "morse_flipper_scene_settings_on_event",
                    "MorseFlipperSceneTxGroupsCfg": "morse_flipper_scene_settings_on_event",
                    "MorseFlipperSceneRxCallsignsCfg": "morse_flipper_scene_settings_on_event",
                    "MorseFlipperSceneGpio": "morse_flipper_scene_settings_on_event",
                    "MorseFlipperSceneIcr": "morse_flipper_scene_live_on_event",
                    "MorseFlipperScenePassive": "morse_flipper_scene_live_on_event",
                },
            ),
            (
                "morse_flipper_scene_on_exit_handlers",
                {
                    "MorseFlipperSceneTrainer": "morse_flipper_scene_settings_on_exit",
                    "MorseFlipperSceneStraightCfg": "morse_flipper_scene_settings_on_exit",
                    "MorseFlipperSceneTxGroupsCfg": "morse_flipper_scene_settings_on_exit",
                    "MorseFlipperSceneRxCallsignsCfg": "morse_flipper_scene_settings_on_exit",
                    "MorseFlipperSceneGpio": "morse_flipper_scene_settings_on_exit",
                    "MorseFlipperSceneIcr": "morse_flipper_scene_plugin_on_exit",
                    "MorseFlipperScenePassive": "morse_flipper_scene_plugin_on_exit",
                },
            ),
        ):
            actual = handler_table(table_name)
            for scene, handler in expected.items():
                with self.subTest(table=table_name, scene=scene):
                    self.assertEqual(actual[scene], handler)

    def test_every_settings_fal_scene_uses_settings_view(self) -> None:
        flow = FLOW.read_text(encoding="utf-8")
        view_switch = flow[
            flow.index("static uint8_t morse_flipper_scene_view") :
            flow.index("void morse_flipper_scene_enter_now")
        ]
        for scene in (
            "MorseFlipperSceneHome",
            "MorseFlipperSceneAudioCfg",
            "MorseFlipperSceneTrainer",
            "MorseFlipperSceneStraightCfg",
            "MorseFlipperSceneTxGroupsCfg",
            "MorseFlipperSceneRxCallsignsCfg",
            "MorseFlipperScenePc",
            "MorseFlipperSceneGpio",
        ):
            with self.subTest(scene=scene):
                self.assertIn(f"case {scene}:", view_switch)

    def test_passive_input_propagates_fal_redraw(self) -> None:
        input_source = INPUT.read_text(encoding="utf-8")
        passive_case = input_source[
            input_source.index("case MorseFlipperScreenPassive:") :
            input_source.index("case MorseFlipperScreenRfFreq:")
        ]
        input_call = passive_case.index(
            "result = api->input(app->plugin_slot.state, event, now_ms);"
        )
        apply_call = passive_case.index(
            "morse_flipper_plugin_runtime_apply_result_locked(app, result, now_ms);"
        )
        unlock = passive_case.index("furi_mutex_release(app->plugin_slot.mutex);")
        redraw = passive_case.index(
            "if(result.redraw) morse_flipper_view_dirty(app);"
        )
        self.assertLess(input_call, apply_call)
        self.assertLess(apply_call, unlock)
        self.assertLess(unlock, redraw)

    def test_startup_probe_is_an_overlay_not_the_scene_root(self) -> None:
        app = APP.read_text(encoding="utf-8")
        base = app.index(
            "app->onboarding_seen ? MorseFlipperSceneMenuMain : "
            "MorseFlipperSceneOnboarding"
        )
        probe = app.index(
            "scene_manager_next_scene(app->scene_manager, "
            "MorseFlipperSceneStartupProbe)",
            base,
        )
        self.assertLess(base, probe)

        scenes = SCENES.read_text(encoding="utf-8")
        handler = scenes[
            scenes.index("static bool morse_flipper_scene_startup_probe_on_event") :
            scenes.index("static bool morse_flipper_scene_onboarding_on_event")
        ]
        self.assertIn("scene_manager_previous_scene(app->scene_manager);", handler)
        self.assertNotIn("scene_manager_search_and_switch_to_another_scene", handler)

        input_source = INPUT.read_text(encoding="utf-8")
        handler = input_source[
            input_source.index("static bool morse_flipper_startup_probe_input") :
            input_source.index("static uint8_t morse_flipper_ham_dir_from_key")
        ]
        self.assertIn("scene_manager_previous_scene(app->scene_manager);", handler)
        self.assertNotIn("scene_manager_search_and_switch_to_another_scene", handler)

    def test_lesson_offer_uses_standard_back_and_ok_chrome(self) -> None:
        session = SESSION.read_text(encoding="utf-8")
        draw = session[
            session.index("static void morse_flipper_draw_lesson_advance") :
            session.index("void morse_flipper_draw_session_end")
        ]
        self.assertIn('"Would you like to try"', draw)
        self.assertIn('"the next lesson?"', draw)
        self.assertIn('elements_button_left(canvas, "No");', draw)
        self.assertIn('elements_button_center(canvas, "Yes");', draw)
        self.assertNotIn("elements_button_right", draw)

        input_source = INPUT.read_text(encoding="utf-8")
        offer = input_source[
            input_source.index("static bool morse_flipper_session_end_input") :
            input_source.index("static void morse_flipper_progress_load_recent")
        ]
        self.assertIn("if(event->key == InputKeyOk)", offer)
        self.assertIn("if(event->key == InputKeyBack)", offer)
        self.assertNotIn("InputKeyRight", offer)
        self.assertNotIn("InputKeyLeft", offer)

    def test_progress_history_cancels_pending_navigation_on_context_changes(self) -> None:
        input_source = INPUT.read_text(encoding="utf-8")
        open_result = input_source[
            input_source.index("static void morse_flipper_progress_open_history_result") :
            input_source.index("static void\n    morse_flipper_progress_enter_page")
        ]
        self.assertIn(
            "morse_flipper_progress_history_view_cancel(&app->progress_history);",
            open_result,
        )

        enter_page = input_source[
            input_source.index("static void\n    morse_flipper_progress_enter_page") :
            input_source.index("static bool morse_flipper_progress_history_input")
        ]
        self.assertIn(
            "morse_flipper_progress_history_view_cancel(&app->progress_history);",
            enter_page,
        )

        history = input_source[
            input_source.index("static bool morse_flipper_progress_history_input") :
            input_source.index("void morse_flipper_tick_progress_history_scroll")
        ]
        self.assertIn(
            "morse_flipper_progress_history_view_cancel(&app->progress_history);",
            history,
        )
        self.assertIn("app->progress_history.pending_dir != dir", history)

        tick = input_source[
            input_source.index("void morse_flipper_tick_progress_history_scroll") :
            input_source.index("static bool morse_flipper_progress_input")
        ]
        off_page = tick[
            tick.index("if(app->screen != MorseFlipperScreenProgress") :
            tick.index("if(app->progress_history.pending_dir != 0)")
        ]
        self.assertIn(
            "morse_flipper_progress_history_view_cancel(&app->progress_history);",
            off_page,
        )

        progress = input_source[
            input_source.index("static bool morse_flipper_progress_input") :
            input_source.index("static bool morse_flipper_streak_intro_input")
        ]
        back = progress.index("event->key == InputKeyBack")
        cancel = progress.index(
            "morse_flipper_progress_history_view_cancel(&app->progress_history);",
            back,
        )
        switch = progress.index(
            "scene_manager_search_and_switch_to_another_scene",
            back,
        )
        self.assertLess(cancel, switch)

        scenes = SCENES.read_text(encoding="utf-8")
        on_exit = scenes[
            scenes.index("static void morse_flipper_scene_progress_on_exit") :
            scenes.index("static void morse_flipper_scene_tx_groups_on_enter")
        ]
        cancel = on_exit.index(
            "morse_flipper_progress_history_view_cancel(&app->progress_history);"
        )
        preserve = on_exit.index("if(app->progress_debug_result) return;")
        self.assertLess(cancel, preserve)

    def test_gpio_is_a_root_settings_page_not_a_keying_submenu(self) -> None:
        scenes = SCENES.read_text(encoding="utf-8")
        settings = scenes[
            scenes.index("static void morse_flipper_scene_menu_settings_on_enter") :
            scenes.index("static bool morse_flipper_scene_menu_simple_on_event")
        ]
        gpio = settings.index('"GPIO", MorseFlipperSceneGpio')
        usb = settings.index('"USB", MorseFlipperScenePc')
        self.assertLess(gpio, usb)

        self.assertIn("case MfSettingsEntryKeying: return 4U;", SETTINGS_MODEL.read_text(encoding="utf-8"))
        plugin = SETTINGS_PLUGIN.read_text(encoding="utf-8")
        keying = plugin[
            plugin.index("if(state->args.entry == MfSettingsEntryKeying) {") :
            plugin.index("} else if(state->args.entry == MfSettingsEntryAudio)", plugin.index("if(state->args.entry == MfSettingsEntryKeying) {"))
        ]
        self.assertNotIn('"Audio output"', keying)
        self.assertNotIn('"GPIO"', keying)
        self.assertNotIn("MfSettingsNavigate", plugin)
        self.assertIn("mf_settings_enter_row, state", plugin)
        self.assertNotIn(
            "variable_item_list_set_enter_callback(args->list, NULL, NULL)",
            plugin,
        )

        transport = (ROOT / "src/firmware/morse_flipper_transport.c").read_text(encoding="utf-8")
        self.assertIn("morse_flipper_current_keyer_mode(app) == MorseKeyerModeStraight", transport)

    def test_shared_scene_handlers_derive_the_active_scene(self) -> None:
        scenes = SCENES.read_text(encoding="utf-8")
        enter = scenes[
            scenes.index("static void morse_flipper_scene_live_on_enter") :
            scenes.index("static void morse_flipper_scene_streak_intro_start_training")
        ]
        self.assertIn("scene_manager_get_current_scene(app->scene_manager)", enter)

        event = scenes[
            scenes.index("static bool morse_flipper_scene_menu_simple_on_event") :
            scenes.index("static void morse_flipper_scene_menu_help_on_enter")
        ]
        self.assertIn("scene_manager_get_current_scene(app->scene_manager)", event)
        self.assertIn(
            "scene == MorseFlipperSceneMenuHelp ? MorseFlipperSceneHelp : event.event",
            event,
        )

    def test_icr_reset_uses_standard_chrome_and_exits_settings_subflow(self) -> None:
        runtime = ICR_RUNTIME.read_text(encoding="utf-8")
        settings = runtime[
            runtime.index("static void morse_flipper_icr_settings_run") :
            runtime.index("bool morse_flipper_icr_runtime_enter")
        ]
        self.assertIn('dialog_message_set_buttons(message, "No", "Yes", NULL);', settings)
        self.assertIn('dialog_message_set_buttons(message, NULL, "OK", NULL);', settings)
        self.assertIn("dialog_message_show(dialogs, message)", settings)
        self.assertNotIn("elements_button_", settings)
        self.assertNotIn("variable_item_list_", runtime)

        scenes = SCENES.read_text(encoding="utf-8")
        enter = scenes[
            scenes.index("static void morse_flipper_scene_icr_on_enter") :
            scenes.index("static void morse_flipper_scene_rx_callsigns_on_enter")
        ]
        self.assertIn("if(morse_flipper_icr_host_enter(app, furi_get_tick()))", enter)
        host = (ROOT / "src/firmware/morse_flipper_icr_host.c").read_text(encoding="utf-8")
        host_enter = host[
            host.index("bool morse_flipper_icr_host_enter") :
            host.index("bool morse_flipper_icr_host_input")
        ]
        self.assertIn("if(initial.request_exit)", host_enter)
        self.assertIn("morse_flipper_scene_back(app);", host_enter)
        self.assertIn(
            "*initial = (MorseFlipperIcrResult){.handled = true, .request_exit = true};",
            runtime,
        )


if __name__ == "__main__":
    unittest.main()
