#!/usr/bin/env python3
"""Lock the Radio FAL ownership boundary throughout the extraction."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "src/firmware"
RADIO = FIRMWARE / "plugins/radio"
MANIFEST = (ROOT / "application.fam").read_text()
APP_HEADER = (FIRMWARE / "morse_flipper_app_i.h").read_text()
RUNTIME_HEADER = (FIRMWARE / "morse_flipper_plugin_runtime.h").read_text()
SCENES = (FIRMWARE / "morse_flipper_scenes.c").read_text()
LIVE_VIEW = (FIRMWARE / "morse_flipper_live_view.c").read_text()
RADIO_HAL = (RADIO / "mf_radio_hal.c").read_text()
RADIO_CORE = (RADIO / "mf_radio_core.c").read_text()
RADIO_API = (RADIO / "mf_radio_api.h").read_text()

LEGACY_MAIN_RF_SOURCES = {
    "src/firmware/morse_flipper_rf.c",
    "src/firmware/morse_flipper_rf_live.c",
    "src/firmware/morse_flipper_rf_timing.c",
    "src/firmware/morse_flipper_radio.c",
    "src/firmware/morse_flipper_live_view_rf.c",
}
FORBIDDEN_RADIO_SOURCES = {
    "src/firmware/morse_flipper_cw_decoder.c",
    "src/firmware/cw.c",
    "src/firmware/morse_flipper_cw_token.c",
    "src/firmware/morse_flipper_run_layout.c",
}


def app_block(appid: str) -> str:
    marker = f'appid="{appid}"'
    if marker not in MANIFEST:
        return ""
    start = MANIFEST.rfind("App(", 0, MANIFEST.index(marker))
    next_app = MANIFEST.find("\nApp(", MANIFEST.index(marker))
    return MANIFEST[start : len(MANIFEST) if next_app < 0 else next_app]

def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*\)\s*\{{", source)
    assert match is not None, name
    start = match.end() - 1
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function {name}")


def main() -> None:
    # The extraction reuses the one existing slot and its one app-lifetime mutex.
    assert APP_HEADER.count("MorseFlipperPluginSlot plugin_slot;") == 1
    assert RUNTIME_HEADER.count("FuriMutex* mutex;") == 1
    assert "radio_plugin_slot" not in APP_HEADER + RUNTIME_HEADER
    assert "radio_plugin_mutex" not in APP_HEADER + RUNTIME_HEADER

    radio_block = app_block("morse_flipper_radio")
    for source in FORBIDDEN_RADIO_SOURCES:
        assert source not in radio_block

    if RADIO.is_dir():
        radio_text = "\n".join(
            path.read_text() for path in RADIO.rglob("*.[ch]") if path.is_file()
        )
        assert "MorseFlipperApp" not in radio_text
        assert "start_async_rx" not in radio_text
        assert "stop_async_rx" not in radio_text
        assert not re.search(r"\b(scene_manager|morse_flipper_update_sidetone|"
                             r"morse_flipper_sync_ptt|morse_flipper_save_config)\b",
                             radio_text)
        for name in (
            "mf_radio_core_sync_tx",
            "mf_radio_core_tick",
            "mf_radio_core_input",
            "mf_radio_draw",
            "mf_radio_command_api",
            "mf_radio_tick_api",
            "mf_radio_input_api",
            "mf_radio_draw_api",
        ):
            body = function_body(radio_text, name)
            assert not re.search(r"\b(malloc|calloc|realloc|free)\s*\(", body), name

        tx_preset = RADIO_HAL.split("tx_ook_270khz_no_autocal_regs[] = {", 1)[1].split(
            "};", 1
        )[0]
        prepare_common = function_body(RADIO_HAL, "hal_prepare_common")
        prepare_tx = function_body(RADIO_HAL, "hal_prepare_tx")
        set_tx_level = function_body(RADIO_HAL, "hal_set_tx_level")
        assert "0x0D" in tx_preset
        assert "tx_ook_270khz_no_autocal_regs" in prepare_common
        assert "carrier_ook_650khz_no_autocal_regs" in prepare_common
        assert "subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs" in RADIO_HAL
        assert "furi_hal_subghz_start_async_tx" in RADIO_HAL
        assert "furi_hal_subghz_stop_async_tx" in RADIO_HAL
        assert "MF_RADIO_CWFM_DEVIATION_HZ 2380U" in radio_text
        assert "furi_hal_subghz_set_frequency_and_path(frequency_hz)" in prepare_tx
        assert "furi_hal_subghz_tx" not in prepare_tx
        assert "furi_hal_subghz_tx" in set_tx_level
        assert "furi_hal_subghz_reset" not in prepare_common + prepare_tx
        assert re.search(
            r"if\(hal->static_running\).*?furi_hal_subghz_idle\(\).*?"
            r"set_frequency_and_path\(hal->selected_frequency_hz\).*?"
            r"GpioModeInput.*?furi_hal_subghz_start_async_tx",
            set_tx_level,
            re.DOTALL,
        )
        assert re.search(
            r"furi_hal_subghz_stop_async_tx\(\).*?mf_radio_cwfm_static_config\(.*?"
            r"set_frequency_and_path\(static_config.frequency_hz\).*?"
            r"gpio_write\(data_gpio, static_config.data_level\).*?"
            r"furi_hal_subghz_tx\(\).*?static_running = true",
            set_tx_level,
            re.DOTALL,
        )
        assert "DEVIATN" not in RADIO_HAL
        assert "dev5_" not in RADIO_HAL
        assert "mf_radio_tx_session_stop" in function_body(RADIO_CORE, "mf_radio_quiesce")
        assert re.search(r"#define\s+MF_RADIO_API_VERSION\s+3U", RADIO_API)
        assert "MorseFlipperCommandFalApi fal" in RADIO_API

    # Legacy files are allowed only while the product still has no Radio FAL.
    main_block = app_block("morse_flipper")
    if "MORSE_FLIPPER_RADIO_FAL_CUTOVER=1" in main_block:
        for source in LEGACY_MAIN_RF_SOURCES:
            assert source not in main_block
            assert not (ROOT / source).exists()
        assert "furi_hal_subghz" not in main_block
        assert "MorseFlipperRf " not in APP_HEADER
        assert "MorseFlipperRadio " not in APP_HEADER
        assert "MfRadioState " not in APP_HEADER

    menu_enter = function_body(SCENES, "morse_flipper_scene_menu_rf_on_enter")
    menu_event = function_body(SCENES, "morse_flipper_scene_menu_rf_on_event")
    child_enter = function_body(SCENES, "morse_flipper_scene_radio_on_enter")
    child_exit = function_body(SCENES, "morse_flipper_scene_radio_on_exit")
    assert menu_enter.count("morse_flipper_radio_host_open") == 1
    assert menu_enter.count("submenu_add_item") == 4
    assert '"ARDF Foxhunting"' in menu_enter
    assert menu_event.count("morse_flipper_radio_host_close") == 1
    assert "morse_flipper_radio_host_close" not in child_enter + child_exit
    assert "morse_flipper_radio_host_set_page" in child_enter + child_exit

    trace_start = LIVE_VIEW.index("if(app->screen == MorseFlipperScreenTrace)")
    trace_end = LIVE_VIEW.index("\n    canvas_set_font(canvas, FontPrimary)", trace_start)
    trace_block = LIVE_VIEW[trace_start:trace_end]
    assert "MfRadioApi" not in trace_block
    assert "plugin_slot" not in trace_block

    print("radio FAL structure: ok")


if __name__ == "__main__":
    main()
