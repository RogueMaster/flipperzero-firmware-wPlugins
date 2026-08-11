#include "meshcore_apply.h"

#include <string.h>

const char* meshcore_apply_step_name(MeshCoreApplyStep step) {
    switch(step) {
    case MeshCoreApplyRadio:
        return "Radio";
    case MeshCoreApplyPathHash:
        return "Path hash";
    case MeshCoreApplyName:
        return "Name";
    case MeshCoreApplyTxPower:
        return "TX power";
    default:
        return "?";
    }
}

bool meshcore_apply_step_applies(const MeshCorePreset* preset, MeshCoreApplyStep step) {
    switch(step) {
    case MeshCoreApplyRadio:
    case MeshCoreApplyPathHash:
        return true;
    case MeshCoreApplyName:
        /* Sending an empty name would wipe the node's own, which is not what
         * "this preset says nothing about names" means. */
        return preset->has_node_name && preset->node_name[0] != '\0';
    case MeshCoreApplyTxPower:
        return preset->has_tx_power;
    default:
        return false;
    }
}

size_t meshcore_apply_build(
    const MeshCorePreset* preset,
    MeshCoreApplyStep step,
    uint8_t* out,
    size_t cap) {
    if(!meshcore_apply_step_applies(preset, step)) return 0;

    switch(step) {
    case MeshCoreApplyRadio:
        /* freq in kHz, bw in Hz -- the preset already holds wire units, which
         * is the whole reason it stores them that way. */
        return mc_cmd_set_radio_params(out, cap, preset->freq_khz, preset->bw_hz, preset->sf, preset->cr);

    case MeshCoreApplyPathHash: {
        /* [61][0][mode]. The zero is mandatory: the firmware checks
         * cmd_frame[1] == 0 before looking at the mode. */
        if(cap < 3) return 0;
        out[0] = MESHCORE_CMD_SET_PATH_HASH_MODE;
        out[1] = 0;
        out[2] = meshcore_preset_path_hash_mode(preset);
        return 3;
    }

    case MeshCoreApplyName:
        return mc_cmd_set_advert_name(out, cap, preset->node_name);

    case MeshCoreApplyTxPower:
        return mc_cmd_set_tx_power(out, cap, preset->tx_power);

    default:
        return 0;
    }
}

bool meshcore_apply_verify_radio(const MeshCorePreset* preset, const mc_self_info_t* info) {
    return info->radio_freq == preset->freq_khz && info->radio_bw == preset->bw_hz &&
           info->radio_sf == preset->sf && info->radio_cr == preset->cr;
}

bool meshcore_apply_verify_name(const MeshCorePreset* preset, const mc_self_info_t* info) {
    if(!preset->has_node_name) return true; /* nothing was asked for */
    return strncmp(info->name, preset->node_name, sizeof(preset->node_name)) == 0;
}

bool meshcore_apply_verify_tx(const MeshCorePreset* preset, const mc_self_info_t* info) {
    if(!preset->has_tx_power) return true; /* nothing was asked for */
    return info->tx_power == preset->tx_power;
}

bool meshcore_apply_verify_path_hash(
    const MeshCorePreset* preset,
    const mc_device_info_t* info,
    bool* checkable) {
    /* Older firmware simply does not report it, so an unchecked step must be
     * shown as unknown rather than as a failure. */
    if(checkable) *checkable = info->have_path_hash != 0;
    if(!info->have_path_hash) return false;

    return info->path_hash_mode == meshcore_preset_path_hash_mode(preset);
}
