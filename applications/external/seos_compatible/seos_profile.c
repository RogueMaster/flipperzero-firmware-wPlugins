#include "seos_profile.h"

#include "seos_common.h"
#include <gap.h>
#include <furi_ble/profile_interface.h>
#include "seos_service.h"
#include "seos_service_uuid.inc"
#include <ble/core/ble_defs.h>
#include <furi.h>

#define TAG "SeosProfile"

typedef struct {
    FuriHalBleProfileBase base;
    BleServiceSeos* seos_svc;
    FuriHalBleProfileParams params;
} BleProfileSeos;
_Static_assert(offsetof(BleProfileSeos, base) == 0, "Wrong layout");

static FuriHalBleProfileBase* ble_profile_seos_start(FuriHalBleProfileParams profile_params) {
    BleProfileSeos* profile = malloc(sizeof(BleProfileSeos));
    BleProfileParams* params = profile_params;

    profile->params = profile_params;
    profile->base.config = ble_profile_seos;

    profile->seos_svc = ble_svc_seos_start(params->mode);

    return &profile->base;
}

static void ble_profile_seos_stop(FuriHalBleProfileBase* profile) {
    furi_check(profile);
    furi_check(profile->config == ble_profile_seos);

    BleProfileSeos* seos_profile = (BleProfileSeos*)profile;
    ble_svc_seos_stop(seos_profile->seos_svc);
}

// AN5289: 4.7, in order to use flash controller interval must be at least 25ms + advertisement, which is 30 ms
// Since we don't use flash controller anymore interval can be lowered to 7.5ms
#define CONNECTION_INTERVAL_MIN (0x06)
// Up to 45 ms
#define CONNECTION_INTERVAL_MAX (0x24)

static GapConfig seos_reader_template_config = {
    .adv_service.UUID_Type = UUID_TYPE_128,
    .adv_service.Service_UUID_128 = BLE_SVC_SEOS_READER_UUID,
    .mfg_data =
        {0x2e,
         0x01,
         0x15,
         0x4b,
         0xe2,
         0xb6,
         0xb6,
         0xb6,
         0x2a,
         0x46,
         0x4c,
         0x30,
         0x4b,
         0x37,
         0x5a,
         0x30,
         0x31,
         0x55,
         0x31},
    .mfg_data_len = 19,
    .adv_name = "Seos",
    .bonding_mode = false,
    .pairing_method = GapPairingNone,
    .conn_param = {
        .conn_int_min = CONNECTION_INTERVAL_MIN,
        .conn_int_max = CONNECTION_INTERVAL_MAX,
        .slave_latency = 0,
        .supervisor_timeout = 0,
    }};

static GapConfig seos_cred_template_config = {
    .adv_service.UUID_Type = UUID_TYPE_128,
    .adv_service.Service_UUID_128 = BLE_SVC_SEOS_CRED_UUID,
    .adv_name = "Flp",
    .bonding_mode = false,
    .pairing_method = GapPairingNone,
    .conn_param = {
        .conn_int_min = CONNECTION_INTERVAL_MIN,
        .conn_int_max = CONNECTION_INTERVAL_MAX,
        .slave_latency = 0,
        .supervisor_timeout = 0,
    }};

static void
    ble_profile_seos_get_config(GapConfig* config, FuriHalBleProfileParams profile_params) {
    BleProfileParams* params = profile_params;

    FURI_LOG_D(TAG, "ble_profile_seos_get_config FlowMode %d", params->mode);
    furi_check(config);
    if(params->mode == FLOW_READER) {
        memcpy(config, &seos_reader_template_config, sizeof(GapConfig));
    } else if(params->mode == FLOW_CRED) {
        memcpy(config, &seos_cred_template_config, sizeof(GapConfig));
    }
    // Set mac address
    memcpy(config->mac_address, furi_hal_version_get_ble_mac(), sizeof(config->mac_address));
}

static const FuriHalBleProfileTemplate profile_callbacks = {
    .start = ble_profile_seos_start,
    .stop = ble_profile_seos_stop,
    .get_gap_config = ble_profile_seos_get_config,
};

const FuriHalBleProfileTemplate* ble_profile_seos = &profile_callbacks;

void ble_profile_seos_set_event_callback(
    FuriHalBleProfileBase* profile,
    uint16_t buff_size,
    FuriHalBtSeosCallback callback,
    void* context) {
    furi_check(profile && (profile->config == ble_profile_seos));

    BleProfileSeos* seos_profile = (BleProfileSeos*)profile;
    ble_svc_seos_set_callbacks(seos_profile->seos_svc, buff_size, callback, context);
}

bool ble_profile_seos_tx(FuriHalBleProfileBase* profile, uint8_t* data, uint16_t size) {
    furi_check(profile && (profile->config == ble_profile_seos));

    BleProfileSeos* seos_profile = (BleProfileSeos*)profile;

    if(size > BLE_PROFILE_SEOS_PACKET_SIZE_MAX) {
        return false;
    }

    uint16_t num_chunks = size / BLE_CHUNK_SIZE;
    if(size % BLE_CHUNK_SIZE) num_chunks++;

    uint8_t chunk[BLE_CHUNK_SIZE + 1];
    for(uint16_t i = 0; i < num_chunks; i++) {
        uint8_t flags = 0;
        if(i == 0) flags |= BLE_FLAG_SOM;
        if(i == num_chunks - 1) flags |= BLE_FLAG_EOM;
        // Add number of remaining chunks to lower nybble
        flags |= (num_chunks - 1 - i) & 0x0F;

        // Find number of bytes left to send
        uint8_t chunk_size = size - (i * BLE_CHUNK_SIZE);
        // Limit to maximum chunk size
        chunk_size = chunk_size > BLE_CHUNK_SIZE ? BLE_CHUNK_SIZE : chunk_size;

        // Combine and send
        chunk[0] = flags;
        memcpy(chunk + 1, &data[i * BLE_CHUNK_SIZE], chunk_size);
        if(!ble_svc_seos_update_tx(seos_profile->seos_svc, chunk, chunk_size + 1)) return false;
    }

    return true;
}
