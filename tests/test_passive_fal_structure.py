#!/usr/bin/env python3
"""Guard the versioned Passive FAL boundary and resident host contract."""

from pathlib import Path


root = Path(__file__).resolve().parents[1]
types = (root / "src/firmware/plugins/passive_listening/mf_passive_types.h").read_text()
api = (root / "src/firmware/plugins/passive_listening/mf_passive_api.h").read_text()
host = (root / "src/firmware/morse_flipper_passive_host.c").read_text()
playback = (root / "src/firmware/plugins/passive_listening/mf_passive_plugin.c").read_text()
settings = (
    root / "src/firmware/plugins/passive_listening/mf_passive_settings_plugin.c"
).read_text()

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

print("passive FAL structure: ok")
