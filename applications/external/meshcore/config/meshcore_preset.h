/*
 * Radio presets — a named set of settings you can push onto a node.
 *
 * The point is cloning: set one node up the way a network needs, save the
 * preset, then apply it to every other node without retyping five fields on a
 * 128x64 screen.
 *
 * Values are held in **wire units**, not display units: frequency in kHz and
 * bandwidth in Hz, because that is what SET_RADIO_PARAMS carries and the two
 * genuinely differ. Converting once at load time means the apply path never
 * has to think about it.
 *
 * Every field here is settable at runtime — including path_hash_bytes, which
 * was expected to be fixed at flash time and is not. See "Settings layers" in
 * AGENTS.md for the trace.
 *
 * Free of furi so the host tests can cover parsing and validation, which is
 * where a hand-edited file on an SD card will go wrong.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MESHCORE_PRESET_NAME_LEN 24
#define MESHCORE_PRESET_NODE_NAME_LEN 32
#define MESHCORE_PRESET_ROLE_LEN 16

/* Long enough for "868.731 MHz" / "62.5 kHz". */
#define MESHCORE_PRESET_FIELD_LEN 16

typedef struct {
    char name[MESHCORE_PRESET_NAME_LEN];

    uint32_t freq_khz; /* wire units: 868731 == 868.731 MHz */
    uint32_t bw_hz; /* wire units: 62500 == 62.5 kHz     */
    uint8_t sf; /* spreading factor, 5..12           */
    uint8_t cr; /* coding rate, 5..8                 */
    uint8_t path_hash_bytes; /* 1..3; the wire wants bytes - 1    */

    /* TX power in dBm. Optional: a preset that says nothing about it leaves the
     * node's power alone. Applied with SET_TX_POWER and confirmed against
     * SELF_INFO.tx_power, the same read-back discipline as the radio params. */
    bool has_tx_power;
    uint8_t tx_power;

    bool has_node_name;
    char node_name[MESHCORE_PRESET_NODE_NAME_LEN];

    /* Parsed and shown, but not applied: whether a node accepts a runtime role
     * change is still unconfirmed. See the Role caveat in AGENTS.md. */
    bool has_role;
    char role[MESHCORE_PRESET_ROLE_LEN];

    bool built_in;
} MeshCorePreset;

/* Compiled in, so there is always at least one preset even with no SD card. */
extern const MeshCorePreset MESHCORE_BUILTIN_PRESETS[];
extern const size_t MESHCORE_BUILTIN_PRESET_COUNT;

/** Parse one preset file. `why` is set to a short reason on failure. */
bool meshcore_preset_from_json(const char* json, MeshCorePreset* out, const char** why);

/** Range-check a preset. `why` is set to a short reason on failure. */
bool meshcore_preset_validate(const MeshCorePreset* preset, const char** why);

/** What SET_PATH_HASH_MODE wants: bytes - 1. */
uint8_t meshcore_preset_path_hash_mode(const MeshCorePreset* preset);

/** "868.731 MHz" */
void meshcore_preset_format_freq(uint32_t freq_khz, char* out, size_t cap);
/** "62.5 kHz" */
void meshcore_preset_format_bw(uint32_t bw_hz, char* out, size_t cap);
