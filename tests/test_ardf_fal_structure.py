#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
types = (root / "src/firmware/plugins/ardf/mf_ardf_types.h").read_text()
api = (root / "src/firmware/plugins/ardf/mf_ardf_api.h").read_text()
host = (root / "src/firmware/morse_flipper_ardf_host.c").read_text()
hal = (root / "src/firmware/plugins/ardf/mf_ardf_hal.c").read_text()
runtime = (root / "src/firmware/morse_flipper_plugin_runtime.c").read_text()
scenes = (root / "src/firmware/morse_flipper_scenes.c").read_text()
fam = (root / "application.fam").read_text()
gpio = (root / "src/firmware/morse_flipper_gpio.c").read_text()
resident_core = (root / "src/firmware/morse_flipper_core.c").read_text()
radio_core = (root / "src/firmware/plugins/radio/mf_radio_core.c").read_text()
audio_route = (root / "src/firmware/morse_flipper_audio_route.c").read_text()

api_epochs = {
    "src/firmware/plugins/help_about/morse_flipper_help_about_api.h":
        "MORSE_FLIPPER_HELP_ABOUT_API_VERSION 4U",
    "src/firmware/plugins/icr/morse_flipper_icr_api.h": "MORSE_FLIPPER_ICR_API_VERSION 6U",
    "src/firmware/plugins/rx_practice/morse_flipper_rx_practice_api.h":
        "MORSE_FLIPPER_RX_PRACTICE_API_VERSION 12U",
    "src/firmware/plugins/passive_listening/mf_passive_api.h": "MF_PASSIVE_API_VERSION        8U",
    "src/firmware/plugins/settings/mf_settings_api.h": "MF_SETTINGS_API_VERSION 5U",
    "src/firmware/plugins/tx_groups/mf_tx_groups_api.h": "MF_TX_GROUPS_API_VERSION 2U",
    "src/firmware/plugins/radio/mf_radio_api.h": "MF_RADIO_API_VERSION 4U",
    "src/firmware/plugins/ardf/mf_ardf_api.h": "MF_ARDF_API_VERSION 2U",
}
for path, epoch in api_epochs.items():
    assert epoch in (root / path).read_text()

enter_args = types.split("} MfArdfEnterArgs;", 1)[0].rsplit("typedef struct {", 1)[1]
assert "(*" not in enter_args
assert all(field in enter_args for field in ("struct_size", "now_ms", "frequency_hz"))
assert "MorseFlipperCommandFalApi fal" in api
for command in ("HostActionInfo", "PopulateSettings", "TextResult", "HostActionResult", "ActivateRun"):
    assert f"MfArdfCommand{command}" in api
assert "morse_flipper_radio_host_close" in scenes
assert scenes.index("morse_flipper_radio_host_close") < scenes.index("morse_flipper_ardf_host_open")
assert "MorseFlipperPluginOwnerArdf" in host
assert "morse_flipper_ardf_host_apply_after_unlock" in host
assert "morse_flipper_plugin_runtime_call" in host
assert "MORSE_FLIPPER_MAPPED_INPUT" in host
assert "MfArdfCommandActivateRun" in host
assert "result.transition" in host
assert "furi_mutex_release" in host
assert 'appid="morse_flipper_ardf"' in fam
assert "ARDF Foxhunting" in scenes
plugin = (root / "src/firmware/plugins/ardf/mf_ardf_plugin.c").read_text()
for label in ("Start ARDF Fox", "Mode", "Modulation", "Message", "Custom", "Custom interval", "Light assistance", "Audio output", "WPM"):
    assert f'"{label}"' in plugin
assert plugin.count("variable_item_list_add") == 9
assert "variable_item_list_set_header" not in plugin
assert "assets_icons.h" not in (root / "src/firmware/plugins/ardf/mf_ardf_draw.c").read_text()
assert host.index("morse_flipper_plugin_runtime_apply_result_locked") < host.index("furi_mutex_release")
assert "morse_flipper_update_sidetone(app);" in host
assert "sequence_set_only_blue_255" in hal
assert "sequence_solid_yellow" not in hal
assert "sequence_display_backlight_on" in host
assert "sequence_display_backlight_off" in host
for forbidden in (
    "sequence_display_backlight_enforce_on",
    "sequence_display_backlight_enforce_auto",
    "furi_hal_light_set",
):
    assert forbidden not in host + hal
assert host.index("furi_mutex_release") < host.rindex("morse_flipper_ardf_host_apply_after_unlock")
assert "bool morse_flipper_plugin_runtime_call" in runtime
sync_ptt = gpio.split("void morse_flipper_sync_ptt", 1)[1].split("\n}", 1)[0]
sync_led = resident_core.split("void morse_flipper_sync_signal_led", 1)[1].split("\n}", 1)[0]
assert "ardf_gpio_owned" in sync_ptt
assert "ardf_gpio_owned" in sync_led
assert "mf_radio_tx_session_prepare" in radio_core
assert "mf_radio_tx_session_set_mark" in radio_core
assert "MorseFlipperSceneArdf" in scenes
assert "MorseFlipperSceneArdfTextInput" not in scenes
audio_pwm_scenes = audio_route.split("bool morse_flipper_scene_supports_audio_pwm", 1)[1].split(
    "bool morse_flipper_audio_path_is_sampled", 1
)[0]
assert "MorseFlipperSceneArdf" in audio_pwm_scenes
ardf_scene = scenes.split("static bool morse_flipper_scene_ardf_on_event", 1)[1].split(
    "static void morse_flipper_scene_streak_intro_start_training", 1
)[0]
load_failure = ardf_scene.split("if(!morse_flipper_ardf_host_open", 1)[1].split("return true;", 1)[0]
assert '"Plugin unavailable"' in load_failure
assert "morse_flipper_host_dialog" in load_failure
assert load_failure.index("morse_flipper_host_dialog") < load_failure.index(
    "scene_manager_search_and_switch_to_another_scene"
)
assert "MorseFlipperCustomArdfTextDone" in ardf_scene
assert "MorseFlipperCustomArdfTextCleanup" in ardf_scene
text_result_path = ardf_scene.split("MorseFlipperCustomArdfTextDone", 1)[1]
assert "scene_manager_previous_scene" not in text_result_path
for removed in ("ArdfLoading", "ArdfSettings", "ArdfClock", "ArdfRun"):
    assert f"MorseFlipperScene{removed}" not in scenes
print("ardf FAL structure: ok")
