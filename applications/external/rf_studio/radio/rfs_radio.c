#include "rfs_radio.h"

#include <furi_hal_power.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>
#include <lib/subghz/blocks/generic.h>
#include <lib/subghz/protocols/raw.h>
#include "../protocol/rfs_protocol.h"

#define RFS_EXT_DEVICE_NAME "cc1101_ext"

struct RfsRadio {
    RfsDiag* diag;
    RfsCapabilities caps;
    SubGhzSetting* setting;
    const SubGhzDevice* devices[RfsDeviceCount];
    FuriMutex* lock;
    bool busy;
    bool otg_enabled_by_us;
};

static const char* const device_labels[RfsDeviceCount] = {"Internal", "External"};

/* Built-in presets, used when the firmware settings do not define a name. */
typedef struct {
    const char* name;
    const char* file_value;
    const uint8_t* regs;
} RfsBuiltinPreset;

static const RfsBuiltinPreset builtin_presets[] = {
    {"AM270", "FuriHalSubGhzPresetOok270Async", subghz_device_cc1101_preset_ook_270khz_async_regs},
    {"AM650", "FuriHalSubGhzPresetOok650Async", subghz_device_cc1101_preset_ook_650khz_async_regs},
    {"FM238",
     "FuriHalSubGhzPreset2FSKDev238Async",
     subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs},
    {"FM476",
     "FuriHalSubGhzPreset2FSKDev476Async",
     subghz_device_cc1101_preset_2fsk_dev47_6khz_async_regs},
    {"MSK99",
     "FuriHalSubGhzPresetMSK99_97KbAsync",
     subghz_device_cc1101_preset_msk_99_97kb_async_regs},
    {"GFSK9",
     "FuriHalSubGhzPresetGFSK9_99KbAsync",
     subghz_device_cc1101_preset_gfsk_9_99kb_async_regs},
};

/* Preset register tables are address/value pairs ending with 0x00 0x00, followed by an
 * 8-byte PA table. */
static size_t rfs_regs_size(const uint8_t* regs) {
    size_t i = 0;
    while(!(regs[i] == 0 && regs[i + 1] == 0))
        i += 2;
    return i + 2 + 8;
}

static void rfs_radio_power_on(RfsRadio* radio) {
    if(!furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
        radio->otg_enabled_by_us = true;
        furi_delay_ms(50);
    }
}

static void rfs_radio_power_off(RfsRadio* radio) {
    if(radio->otg_enabled_by_us) {
        furi_hal_power_disable_otg();
        radio->otg_enabled_by_us = false;
    }
}

static bool rfs_radio_probe_external(RfsRadio* radio) {
    const SubGhzDevice* device = radio->devices[RfsDeviceExternal];
    if(!device) return false;
    rfs_radio_power_on(radio);
    bool connected = subghz_devices_is_connect(device);
    if(!connected) {
        subghz_devices_begin(device);
        connected = subghz_devices_is_connect(device);
        subghz_devices_end(device);
    }
    if(!connected) rfs_radio_power_off(radio);
    return connected;
}

RfsRadio* rfs_radio_alloc(RfsDiag* diag) {
    RfsRadio* radio = malloc(sizeof(RfsRadio));
    memset(radio, 0, sizeof(RfsRadio));
    radio->diag = diag;
    radio->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    subghz_devices_init();
    return radio;
}

void rfs_radio_free(RfsRadio* radio) {
    if(radio->setting) subghz_setting_free(radio->setting);
    subghz_devices_deinit();
    rfs_radio_power_off(radio);
    furi_mutex_free(radio->lock);
    free(radio);
}

void rfs_radio_refresh(RfsRadio* radio, bool probe_external) {
    RfsCapabilities* caps = &radio->caps;
    memset(caps, 0, sizeof(RfsCapabilities));
    rfs_firmware_detect(&caps->firmware);

    radio->devices[RfsDeviceInternal] = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    radio->devices[RfsDeviceExternal] = subghz_devices_get_by_name(RFS_EXT_DEVICE_NAME);

    caps->device_present[RfsDeviceInternal] = radio->devices[RfsDeviceInternal] != NULL;
    caps->device_connected[RfsDeviceInternal] = caps->device_present[RfsDeviceInternal];
    caps->device_present[RfsDeviceExternal] = radio->devices[RfsDeviceExternal] != NULL;
    if(probe_external && caps->device_present[RfsDeviceExternal]) {
        caps->device_connected[RfsDeviceExternal] = rfs_radio_probe_external(radio);
    }

    const SubGhzProtocol* raw = rfs_protocol_find(SUBGHZ_PROTOCOL_RAW_NAME);
    caps->raw_capture = raw && raw->decoder;
    caps->raw_transmit = rfs_protocol_can_send(raw);

    if(radio->setting) subghz_setting_free(radio->setting);
    radio->setting = subghz_setting_alloc();
    subghz_setting_load(radio->setting, RFS_SUBGHZ_SETTING_PATH);

    rfs_diag_info(
        radio->diag,
        "Capabilities: %s %s, ext %s, %u freqs, %u presets",
        caps->firmware.origin,
        caps->firmware.version,
        caps->device_connected[RfsDeviceExternal] ? "yes" : "no",
        (unsigned)subghz_setting_get_frequency_count(radio->setting),
        (unsigned)subghz_setting_get_preset_count(radio->setting));
}

const RfsCapabilities* rfs_radio_caps(RfsRadio* radio) {
    return &radio->caps;
}

size_t rfs_radio_freq_count(RfsRadio* radio, RfsDiscoverScope scope) {
    if(!radio->setting) return 0;
    if(scope == RfsDiscoverCommon) {
        size_t count = subghz_setting_get_hopper_frequency_count(radio->setting);
        if(count > 0) return count;
    }
    return subghz_setting_get_frequency_count(radio->setting);
}

uint32_t rfs_radio_freq_get(RfsRadio* radio, RfsDiscoverScope scope, size_t index) {
    if(!radio->setting) return 0;
    if(scope == RfsDiscoverCommon &&
       subghz_setting_get_hopper_frequency_count(radio->setting) > 0) {
        return subghz_setting_get_hopper_frequency(radio->setting, index);
    }
    return subghz_setting_get_frequency(radio->setting, index);
}

uint32_t rfs_radio_default_freq(RfsRadio* radio) {
    return radio->setting ? subghz_setting_get_default_frequency(radio->setting) : 433920000;
}

size_t rfs_radio_preset_count(RfsRadio* radio) {
    return radio->setting ? subghz_setting_get_preset_count(radio->setting) : 0;
}

const char* rfs_radio_preset_name(RfsRadio* radio, size_t index) {
    return radio->setting ? subghz_setting_get_preset_name(radio->setting, index) : "";
}

bool rfs_radio_preset_data(RfsRadio* radio, const char* name, const uint8_t** data, size_t* size) {
    if(radio->setting) {
        int idx = subghz_setting_get_inx_preset_by_name(radio->setting, name);
        if(idx >= 0) {
            *data = subghz_setting_get_preset_data(radio->setting, idx);
            *size = subghz_setting_get_preset_data_size(radio->setting, idx);
            if(*data && *size) return true;
        }
    }
    for(size_t i = 0; i < COUNT_OF(builtin_presets); i++) {
        if(strcmp(builtin_presets[i].name, name) == 0) {
            *data = builtin_presets[i].regs;
            *size = rfs_regs_size(builtin_presets[i].regs);
            return true;
        }
    }
    return false;
}

void rfs_radio_preset_to_file(const char* name, FuriString* out) {
    subghz_block_generic_get_preset_name(name, out);
}

bool rfs_radio_preset_from_file(const char* file_value, char* out, size_t out_size) {
    for(size_t i = 0; i < COUNT_OF(builtin_presets); i++) {
        if(strcmp(builtin_presets[i].file_value, file_value) == 0) {
            strlcpy(out, builtin_presets[i].name, out_size);
            return true;
        }
    }
    strlcpy(out, "Custom", out_size);
    return false;
}

const SubGhzDevice* rfs_radio_device(RfsRadio* radio, RfsDeviceId id) {
    return id < RfsDeviceCount ? radio->devices[id] : NULL;
}

const char* rfs_radio_device_label(RfsDeviceId id) {
    return id < RfsDeviceCount ? device_labels[id] : "?";
}

static RfsRadioResult rfs_radio_check_device(
    RfsRadio* radio,
    RfsDeviceId id,
    uint32_t frequency,
    bool transmit,
    RfsTxPermission* out_permission) {
    const SubGhzDevice* device = radio->devices[id];
    if(!device) return RfsRadioErrNoDevice;
    if(!radio->caps.device_connected[id]) return RfsRadioErrNotConnected;
    if(frequency == 0) return RfsRadioOk;
    if(!subghz_devices_is_frequency_valid(device, frequency)) return RfsRadioErrFrequency;
    if(transmit) {
        RfsTxPermission permission = rfs_firmware_check_tx(device, frequency);
        if(out_permission) *out_permission = permission;
        if(permission != RfsTxPermAllowed) return RfsRadioErrTxBlocked;
    }
    return RfsRadioOk;
}

RfsRadioResult rfs_radio_resolve(
    RfsRadio* radio,
    RfsRadioMode mode,
    uint32_t frequency,
    bool transmit,
    RfsDeviceId* out_id,
    RfsTxPermission* out_permission) {
    if(out_permission) *out_permission = RfsTxPermAllowed;
    if(mode == RfsRadioModeInternal || mode == RfsRadioModeExternal) {
        RfsDeviceId id = mode == RfsRadioModeInternal ? RfsDeviceInternal : RfsDeviceExternal;
        *out_id = id;
        return rfs_radio_check_device(radio, id, frequency, transmit, out_permission);
    }

    /* Auto: external first when connected and capable, otherwise internal. */
    RfsTxPermission ext_permission = RfsTxPermAllowed;
    if(rfs_radio_check_device(radio, RfsDeviceExternal, frequency, transmit, &ext_permission) ==
       RfsRadioOk) {
        *out_id = RfsDeviceExternal;
        return RfsRadioOk;
    }
    *out_id = RfsDeviceInternal;
    return rfs_radio_check_device(radio, RfsDeviceInternal, frequency, transmit, out_permission);
}

RfsRadioResult rfs_radio_acquire(RfsRadio* radio, RfsDeviceId id) {
    const SubGhzDevice* device = rfs_radio_device(radio, id);
    if(!device) return RfsRadioErrNoDevice;

    furi_mutex_acquire(radio->lock, FuriWaitForever);
    if(radio->busy) {
        furi_mutex_release(radio->lock);
        return RfsRadioErrBusy;
    }
    radio->busy = true;
    furi_mutex_release(radio->lock);

    if(id == RfsDeviceExternal) rfs_radio_power_on(radio);
    if(!subghz_devices_begin(device)) {
        /* The internal radio reports false when it was already started; treat a connected
         * device as usable and only fail when it is genuinely missing. */
        if(!subghz_devices_is_connect(device)) {
            subghz_devices_end(device);
            furi_mutex_acquire(radio->lock, FuriWaitForever);
            radio->busy = false;
            furi_mutex_release(radio->lock);
            rfs_diag_error(radio->diag, "Radio %s failed to start", device_labels[id]);
            return RfsRadioErrBegin;
        }
    }
    subghz_devices_reset(device);
    subghz_devices_idle(device);
    return RfsRadioOk;
}

void rfs_radio_release(RfsRadio* radio, RfsDeviceId id) {
    const SubGhzDevice* device = rfs_radio_device(radio, id);
    if(device) {
        subghz_devices_idle(device);
        subghz_devices_sleep(device);
        subghz_devices_end(device);
    }
    furi_mutex_acquire(radio->lock, FuriWaitForever);
    radio->busy = false;
    furi_mutex_release(radio->lock);
}

bool rfs_radio_is_busy(RfsRadio* radio) {
    furi_mutex_acquire(radio->lock, FuriWaitForever);
    bool busy = radio->busy;
    furi_mutex_release(radio->lock);
    return busy;
}

const char* rfs_radio_result_text(RfsRadioResult result) {
    switch(result) {
    case RfsRadioOk:
        return "OK";
    case RfsRadioErrNoDevice:
        return "Radio driver missing";
    case RfsRadioErrNotConnected:
        return "External radio not found";
    case RfsRadioErrFrequency:
        return "Frequency not supported";
    case RfsRadioErrTxBlocked:
        return "TX blocked by firmware";
    case RfsRadioErrBusy:
        return "Radio busy";
    default:
        return "Radio failed to start";
    }
}
