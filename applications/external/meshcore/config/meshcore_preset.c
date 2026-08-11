#include "meshcore_preset.h"

#include <stdio.h>
#include <string.h>

#include "meshcore_json.h"

/* Shipped in the binary so the Presets screen is never empty, and so the
 * everyday network is one click away on a node that has just been flashed.
 *
 * 868.731018 MHz becomes 868731 kHz: the wire carries whole kHz and the
 * reference client truncates the same way, so this loses nothing that could
 * have been transmitted anyway. */
const MeshCorePreset MESHCORE_BUILTIN_PRESETS[] = {
    {
        .name = "City/daily",
        .freq_khz = 868731,
        .bw_hz = 62500,
        .sf = 7,
        .cr = 7,
        /* 1-byte path hash (mode 0). Firmware v1.13.0 and older DROP packets
         * with multi-byte path hashes, so the everyday one-click preset stays
         * on the legacy 1-byte form that every node on a mixed-version mesh can
         * relay. Card presets can opt into 2-3 bytes where the whole mesh is new. */
        .path_hash_bytes = 1,
        .has_node_name = false,
        .node_name = "",
        .has_role = false,
        .role = "",
        .built_in = true,
    },
};

const size_t MESHCORE_BUILTIN_PRESET_COUNT =
    sizeof(MESHCORE_BUILTIN_PRESETS) / sizeof(MESHCORE_BUILTIN_PRESETS[0]);

uint8_t meshcore_preset_path_hash_mode(const MeshCorePreset* preset) {
    /* MyMesh.cpp sends with _prefs.path_hash_mode + 1 hash bytes, so the mode
     * is one less than the byte count. */
    return (uint8_t)(preset->path_hash_bytes > 0 ? preset->path_hash_bytes - 1 : 0);
}

bool meshcore_preset_validate(const MeshCorePreset* preset, const char** why) {
    const char* reason = NULL;

    if(preset->name[0] == '\0') {
        reason = "preset has no name";
    } else if(preset->freq_khz < 100000u || preset->freq_khz > 1000000u) {
        /* Sub-GHz LoRa lives between roughly 137 MHz and 960 MHz; anything
         * outside this is a typo, most likely MHz written where kHz was meant. */
        reason = "frequency out of range";
    } else if(preset->bw_hz < 7000u || preset->bw_hz > 500000u) {
        reason = "bandwidth out of range";
    } else if(preset->sf < 5 || preset->sf > 12) {
        reason = "spreading factor must be 5..12";
    } else if(preset->cr < 5 || preset->cr > 8) {
        reason = "coding rate must be 5..8";
    } else if(preset->path_hash_bytes < 1 || preset->path_hash_bytes > 3) {
        /* The firmware rejects a mode of 3 or more, and sendFlood refuses a
         * hash size of 0 or above 3. */
        reason = "path hash bytes must be 1..3";
    }

    if(why) *why = reason;
    return reason == NULL;
}

bool meshcore_preset_from_json(const char* json, MeshCorePreset* out, const char** why) {
    const char* reason = NULL;
    memset(out, 0, sizeof(*out));

    if(!json) {
        if(why) *why = "empty file";
        return false;
    }

    if(!meshcore_json_get(json, "name", out->name, sizeof(out->name))) {
        reason = "missing \"name\"";
    } else if(!meshcore_json_get_scaled(json, "freq_mhz", 1000, &out->freq_khz)) {
        reason = "missing or bad \"freq_mhz\"";
    } else if(!meshcore_json_get_scaled(json, "bw_khz", 1000, &out->bw_hz)) {
        reason = "missing or bad \"bw_khz\"";
    } else {
        uint32_t value = 0;
        if(!meshcore_json_get_uint(json, "sf", &value) || value > 255) {
            reason = "missing or bad \"sf\"";
        } else {
            out->sf = (uint8_t)value;
            if(!meshcore_json_get_uint(json, "cr", &value) || value > 255) {
                reason = "missing or bad \"cr\"";
            } else {
                out->cr = (uint8_t)value;
                /* Optional: a preset written before path hash was understood
                 * simply keeps the node's current setting. 1 byte is the
                 * firmware default (path_hash_mode 0). */
                if(meshcore_json_get_uint(json, "path_hash_bytes", &value) && value <= 255) {
                    out->path_hash_bytes = (uint8_t)value;
                } else {
                    out->path_hash_bytes = 1;
                }

                /* Optional: TX power in dBm. Absent means "leave the node's
                 * power alone" (has_tx_power stays false). */
                uint32_t tx = 0;
                if(meshcore_json_get_uint(json, "tx_dbm", &tx) && tx >= 1 && tx <= 30) {
                    out->has_tx_power = true;
                    out->tx_power = (uint8_t)tx;
                }

                out->has_node_name = meshcore_json_get(
                    json, "node_name", out->node_name, sizeof(out->node_name));
                out->has_role = meshcore_json_get(json, "role", out->role, sizeof(out->role));
            }
        }
    }

    if(reason) {
        if(why) *why = reason;
        return false;
    }

    return meshcore_preset_validate(out, why);
}

void meshcore_preset_format_freq(uint32_t freq_khz, char* out, size_t cap) {
    snprintf(
        out,
        cap,
        "%lu.%03lu MHz",
        (unsigned long)(freq_khz / 1000u),
        (unsigned long)(freq_khz % 1000u));
}

void meshcore_preset_format_bw(uint32_t bw_hz, char* out, size_t cap) {
    /* One decimal is enough for every bandwidth the radio supports — 62.5 and
     * 41.7 are the only ones that need it at all. */
    snprintf(
        out,
        cap,
        "%lu.%01lu kHz",
        (unsigned long)(bw_hz / 1000u),
        (unsigned long)((bw_hz % 1000u) / 100u));
}
