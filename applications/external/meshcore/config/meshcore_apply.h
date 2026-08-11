/*
 * Turning a preset into commands, and checking the node actually took them.
 *
 * Split out from the scene on purpose: building the payloads and judging the
 * read-back are pure functions over meshcore_c, which has no I/O, so the host
 * tests can cover the part that is easy to get wrong. The scene is left with
 * nothing but "send this, wait for that, draw a tick".
 *
 * Applying is deliberately several commands rather than one:
 *
 *   SET_RADIO_PARAMS     frequency, bandwidth, spreading factor, coding rate
 *   SET_PATH_HASH_MODE   its own command -- not part of set-radio
 *   SET_ADVERT_NAME      only when the preset carries a node name
 *
 * meshcore_c has no builder for SET_PATH_HASH_MODE, so this file provides one.
 * That is the right place for it: the vendored library stays byte-identical to
 * upstream and anything it lacks lives in our layer.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/meshcore_c/meshcore_companion.h"
#include "meshcore_preset.h"

/* Not in meshcore_c's command enum; confirmed against MyMesh.cpp. */
#define MESHCORE_CMD_SET_PATH_HASH_MODE 61

typedef enum {
    MeshCoreApplyRadio,
    MeshCoreApplyPathHash,
    MeshCoreApplyName,
    MeshCoreApplyTxPower,

    MeshCoreApplyStepCount,
} MeshCoreApplyStep;

/** Short label for the step, for the result checklist. */
const char* meshcore_apply_step_name(MeshCoreApplyStep step);

/** Does this preset have anything to say about this step? A preset with no
 *  node name should not send SET_ADVERT_NAME at all. */
bool meshcore_apply_step_applies(const MeshCorePreset* preset, MeshCoreApplyStep step);

/** Build the payload for one step. Returns its length, or 0 if the step does
 *  not apply or would not fit. */
size_t meshcore_apply_build(
    const MeshCorePreset* preset,
    MeshCoreApplyStep step,
    uint8_t* out,
    size_t cap);

/** Did the node end up where the preset asked? Judged against a re-read
 *  SELF_INFO, which is what makes Apply's ticks mean something. */
bool meshcore_apply_verify_radio(const MeshCorePreset* preset, const mc_self_info_t* info);
bool meshcore_apply_verify_name(const MeshCorePreset* preset, const mc_self_info_t* info);
bool meshcore_apply_verify_tx(const MeshCorePreset* preset, const mc_self_info_t* info);

/** Path hash comes back in DEVICE_INFO, and only on fw_ver >= 10. On older
 *  firmware there is nothing to check against, so `checkable` says whether the
 *  result means anything. */
bool meshcore_apply_verify_path_hash(
    const MeshCorePreset* preset,
    const mc_device_info_t* info,
    bool* checkable);
