#pragma once

#include "../rfs_types.h"
#include "../diagnostics/rfs_diag.h"
#include "rfs_firmware.h"
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/subghz_setting.h>

/*
 * RF Capability Manager and Radio Device Manager (Specification Sections 5 and 4.1).
 * All radio access goes through this module. It discovers what the firmware and hardware offer
 * and never adds application-level frequency restrictions (RF-010).
 */

typedef enum {
    RfsDeviceInternal,
    RfsDeviceExternal,
    RfsDeviceCount,
} RfsDeviceId;

typedef enum {
    RfsRadioOk,
    RfsRadioErrNoDevice, /* driver not present in firmware */
    RfsRadioErrNotConnected, /* external module not detected */
    RfsRadioErrFrequency, /* frequency not supported by the radio */
    RfsRadioErrTxBlocked, /* firmware refuses transmission on this frequency */
    RfsRadioErrBusy, /* another operation owns the radio */
    RfsRadioErrBegin, /* radio failed to start */
} RfsRadioResult;

typedef struct {
    RfsFirmwareInfo firmware;
    bool device_present[RfsDeviceCount]; /* driver available in firmware */
    bool device_connected[RfsDeviceCount]; /* hardware detected */
    bool raw_capture; /* RAW decoder available */
    bool raw_transmit; /* RAW encoder available */
} RfsCapabilities;

typedef struct RfsRadio RfsRadio;

RfsRadio* rfs_radio_alloc(RfsDiag* diag);
void rfs_radio_free(RfsRadio* radio);

/** Rediscover firmware, radios and frequency settings (RF-013). Must not be called while a
 *  capture or transmission is running. probe_external powers the GPIO 5 V rail briefly. */
void rfs_radio_refresh(RfsRadio* radio, bool probe_external);

const RfsCapabilities* rfs_radio_caps(RfsRadio* radio);

/* Frequencies from the firmware settings file (RF-003, CAP-ADD-002). */
size_t rfs_radio_freq_count(RfsRadio* radio, RfsDiscoverScope scope);
uint32_t rfs_radio_freq_get(RfsRadio* radio, RfsDiscoverScope scope, size_t index);
uint32_t rfs_radio_default_freq(RfsRadio* radio);

/* Modulation presets from the firmware settings file (RF-005). */
size_t rfs_radio_preset_count(RfsRadio* radio);
const char* rfs_radio_preset_name(RfsRadio* radio, size_t index);

/** Register data for a named preset. Falls back to the built-in CC1101 tables. */
bool rfs_radio_preset_data(RfsRadio* radio, const char* name, const uint8_t** data, size_t* size);

/** Convert a preset name such as "AM650" to the .sub file form. */
void rfs_radio_preset_to_file(const char* name, FuriString* out);

/** Convert a .sub "Preset" value to a preset name. Returns false for custom presets. */
bool rfs_radio_preset_from_file(const char* file_value, char* out, size_t out_size);

/** Device handle, or NULL if the firmware lacks the driver. */
const SubGhzDevice* rfs_radio_device(RfsRadio* radio, RfsDeviceId id);
const char* rfs_radio_device_label(RfsDeviceId id);

/** Choose a radio for an operation (Section 5.6). Auto mode prefers a connected external radio
 *  that supports the frequency, then the internal radio. frequency 0 skips frequency checks. */
RfsRadioResult rfs_radio_resolve(
    RfsRadio* radio,
    RfsRadioMode mode,
    uint32_t frequency,
    bool transmit,
    RfsDeviceId* out_id,
    RfsTxPermission* out_permission);

/** Take exclusive ownership of a radio and start it (TX-013). */
RfsRadioResult rfs_radio_acquire(RfsRadio* radio, RfsDeviceId id);

/** Return the radio to sleep and release ownership (TX-011). */
void rfs_radio_release(RfsRadio* radio, RfsDeviceId id);

bool rfs_radio_is_busy(RfsRadio* radio);

const char* rfs_radio_result_text(RfsRadioResult result);
