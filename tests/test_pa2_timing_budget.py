#!/usr/bin/env python3
"""Keep normal PA2 keying off the higher-rate voice transport."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
route = (ROOT / "src/firmware/morse_flipper_audio_route.c").read_text()
passive = (ROOT / "src/firmware/morse_flipper_passive_host.c").read_text()

assert "MORSE_FLIPPER_AUDIO_PWM_P2_TONE_SAMPLE_RATE_HZ" in route
assert "MORSE_FLIPPER_AUDIO_PWM_P2_SAMPLE_RATE_HZ" in passive
assert "MORSE_FLIPPER_AUDIO_PWM_P2_TONE_SAMPLE_RATE_HZ" not in passive

print("test_pa2_timing_budget: PASS")
