#!/usr/bin/env python3
"""Lock clean-install WPM defaults without adding a persistence migration."""

from pathlib import Path
import re


root = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (root / path).read_text(encoding="utf-8")


def has_define(text: str, name: str, value: str) -> bool:
    return re.search(rf"^#define\s+{name}\s+{re.escape(value)}(?:\s|$)", text, re.MULTILINE) is not None


app_h = read("src/firmware/morse_flipper_app_i.h")
app = read("src/firmware/morse_flipper_app.c")
config = read("src/firmware/morse_flipper_config.c")
trainer = read("src/firmware/trainer.c")
rx = read("src/firmware/morse_flipper_rx_settings.c")
passive = read("src/firmware/plugins/passive_listening/mf_passive_policy.c")
radio = read("src/firmware/plugins/radio/mf_radio_core.h")
ardf = read("src/firmware/plugins/ardf/mf_ardf_settings.c")
icr = read("src/firmware/plugins/icr/morse_flipper_icr_runtime.c")
settings_api = read("src/firmware/plugins/settings/mf_settings_api.h")

assert has_define(app_h, "MORSE_FLIPPER_DEFAULT_DIT_MS", "48U")
assert has_define(app_h, "MORSE_FLIPPER_DEFAULT_FARNSWORTH_WPM", "12U")
assert has_define(app_h, "MORSE_FLIPPER_STRAIGHT_DEFAULT_DIT_MS", "80U")
assert ".straight_dit_ms = MORSE_FLIPPER_STRAIGHT_DEFAULT_DIT_MS" in app
assert ".farnsworth_wpm = MORSE_FLIPPER_DEFAULT_FARNSWORTH_WPM" in app
assert "trainer->local_dit_ms = 48U" in trainer
assert ".wpm = 25U" in rx and ".farnsworth_wpm = 12U" in rx
assert ".dit_ms = 48U" in passive and ".farnsworth_wpm = 12U" in passive
assert has_define(radio, "MF_RADIO_RX_DEFAULT_WPM", "15U")
assert ".wpm = 10U" in ardf
assert has_define(icr, "MORSE_FLIPPER_ICR_DIT_MS", "48U")

assert has_define(app_h, "MORSE_FLIPPER_SETTINGS_VERSION", "1U")
assert has_define(settings_api, "MF_SETTINGS_API_VERSION", "5U")
assert "keying_dit_ms" not in app_h
assert "keying_wpm" not in config

print("default WPM policy: ok")
