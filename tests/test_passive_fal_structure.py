#!/usr/bin/env python3
"""Guard the versioned Passive FAL boundary and resident host contract."""

import re
from pathlib import Path


root = Path(__file__).resolve().parents[1]
types = (root / "src/firmware/plugins/passive_listening/mf_passive_types.h").read_text()
api = (root / "src/firmware/plugins/passive_listening/mf_passive_api.h").read_text()
host = (root / "src/firmware/morse_flipper_passive_host.c").read_text()
playback = (root / "src/firmware/plugins/passive_listening/mf_passive_plugin.c").read_text()
settings = (
    root / "src/firmware/plugins/passive_listening/mf_passive_settings_plugin.c"
).read_text()
fam = (root / "application.fam").read_text()
rf_audio = (
    root / "src/firmware/plugins/passive_listening/mf_passive_rf_audio.c"
).read_text()
core = (root / "src/firmware/plugins/passive_listening/mf_passive_core.c").read_text()
draw = (root / "src/firmware/plugins/passive_listening/mf_passive_draw.c").read_text()
runtime = (root / "src/firmware/morse_flipper_plugin_runtime.c").read_text()

assert "MF_PASSIVE_API_VERSION        9U" in api
assert playback.count(".api_version = MF_PASSIVE_API_VERSION") == 1
assert playback.count(".ep_api_version = MF_PASSIVE_API_VERSION") == 1
assert settings.count(".api_version = MF_PASSIVE_API_VERSION") == 1
assert settings.count(".ep_api_version = MF_PASSIVE_API_VERSION") == 1
assert "MF_PASSIVE_API_VERSION," in host

playback_args = types.split("} MfPassivePlaybackArgs;", 1)[0].rsplit("typedef struct {", 1)[1]
for field in (
    "uint32_t now_ms",
    "uint32_t rng_seed",
    "uint32_t frequency_hz",
    "const MfPassiveHostServices* services",
):
    assert field in playback_args
assert ".frequency_hz = app->rf_frequency_hz" in host

host_commands = types.split("} MfPassiveHostCommand;", 1)[0].rsplit("typedef enum {", 1)[1]
assert host_commands.count("MfPassiveHostCommand") == 6
for command in ("Claim", "Silence", "Tone", "Voice", "Vibration", "Release"):
    assert f"MfPassiveHostCommand{command}" in host_commands
for forbidden in ("Rf", "Fm", "Transmit"):
    assert forbidden not in host_commands

api_tail = api.split("} MfPassiveApi;", 1)[0].rsplit("typedef struct {", 1)[1]
assert api_tail.count("(*") == 2
assert "MfPassiveEnterArgs" in api_tail
assert "MfPassiveResult" in api_tail

rf_source = '"src/firmware/plugins/passive_listening/mf_passive_rf_audio.c"'
assert fam.count(rf_source) == 1
playback_fal = fam.split('appid="morse_flipper_passive_listening"', 1)[1].split(
    'appid="morse_flipper_settings"', 1
)[0]
settings_fal = fam.split('appid="morse_flipper_passive_settings"', 1)[1]
resident_fap = fam.split('appid="morse_flipper"', 1)[1].split(
    'appid="morse_flipper_radio"', 1
)[0]
assert rf_source in playback_fal
assert rf_source not in settings_fal
assert rf_source not in resident_fap
assert "static MfPassiveRfAudio" not in rf_audio
assert "static const MfPassiveRfHardwareOps" in rf_audio
assert "static const uint8_t mf_passive_rf_preset[]" in rf_audio
assert "433160000" not in rf_audio
assert "Spike" not in rf_audio and "spike" not in rf_audio
assert not re.search(r"^static\s+(?!const).*mf_passive_rf_[^(\n]*;", rf_audio, re.MULTILINE)

phase_enum = types.split("} MfPassivePhase;", 1)[0].rsplit("typedef enum {", 1)[1]
for phase in (
    "InitialRfLock",
    "VoiceRfLock",
    "RepeatRfLock",
    "CueRfLock",
    "NextRfLock",
):
    assert phase_enum.count(f"MfPassivePhase{phase}") == 1
assert '"FM unavailable"' in draw
assert '"AUDIO ERR"' in draw
assert "DSP" not in draw
assert "mf_passive_rf_audio_set_voice_gain_pct" not in core
assert "mf_passive_rf_audio_set_dsp_enabled" not in core
assert "MF_PASSIVE_RF_DSP_GAIN_PCT       200L" in rf_audio

fail_body = core.split("static void mf_passive_fail(", 1)[1].split("\n}", 1)[0]
leave_body = core.split("void mf_passive_leave(", 1)[1].split("\n}", 1)[0]
for body in (fail_body, leave_body):
    assert body.index("mf_passive_rf_audio_release") < body.index("mf_passive_reset_pipe")
    assert body.index("mf_passive_reset_pipe") < body.index("mf_passive_voice_pack_close")
assert leave_body.index("mf_passive_voice_pack_close") < leave_body.index("memset")

resident_sources = host + runtime
for forbidden in (
    "mf_passive_rf_audio",
    "furi_hal_subghz",
    "MfPassiveRfAudio",
):
    assert forbidden not in resident_sources
playback_entry = host.split("entry_kind == MfPassiveEntryPlayback", 1)[1]
cleanup_calls = (
    "morse_flipper_drop_live_keying_for_playback",
    "morse_flipper_release_all_notes",
    "morse_flipper_reset_answer_decoder",
    "morse_flipper_sync_ptt",
)
positions = [playback_entry.index(call) for call in cleanup_calls]
assert positions == sorted(positions)
runtime_open = playback_entry.index("morse_flipper_plugin_runtime_open_mapped_locked")
assert all(position < runtime_open for position in positions)

print("passive FAL structure: ok")
