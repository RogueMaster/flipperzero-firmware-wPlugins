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

    # Legacy files are allowed only while the product still has no Radio FAL.
    main_block = app_block("morse_flipper")
    if radio_block:
        for source in LEGACY_MAIN_RF_SOURCES:
            assert source not in main_block

    print("radio FAL structure: ok")


if __name__ == "__main__":
    main()
