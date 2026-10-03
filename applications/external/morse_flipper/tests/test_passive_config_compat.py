#!/usr/bin/env python3
"""Guard the main settings record while passive settings live in passive.bin."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP_HEADER = (ROOT / "src/firmware/morse_flipper_app_i.h").read_text()
CONFIG = (ROOT / "src/firmware/morse_flipper_config.c").read_text()


def main() -> None:
    assert "#define MORSE_FLIPPER_SETTINGS_VERSION              1U" in APP_HEADER
    assert "} MorseFlipperConfig;" in CONFIG
    assert "sizeof(MorseFlipperConfig) == 632U" in CONFIG
    assert "MorseFlipperConfigV" not in CONFIG
    record = CONFIG.split("} MorseFlipperConfig;", 1)[0]
    assert "passive_" not in record
    print("test_passive_config_compat: passed")


if __name__ == "__main__":
    main()
