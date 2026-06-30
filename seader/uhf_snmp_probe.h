#pragma once

#include "snmp_ber_view.h"
#include "snmp_response_view.h"
#include "uhf_tag_config_view.h"

#define SEADER_UHF_SNMP_MAX_ID_LEN    32U
#define SEADER_UHF_SNMP_MAX_VALUE_LEN 32U

typedef enum {
    SeaderUhfSnmpProbeStageIdle = 0,
    SeaderUhfSnmpProbeStageDiscovery,
    SeaderUhfSnmpProbeStageReadIce,
    SeaderUhfSnmpProbeStageReadTagConfig,
    SeaderUhfSnmpProbeStageReadMonza4QtKey,
    SeaderUhfSnmpProbeStageReadHiggs3Key,
    SeaderUhfSnmpProbeStageDone,
    SeaderUhfSnmpProbeStageFailed,
} SeaderUhfSnmpProbeStage;

typedef struct {
    SeaderUhfSnmpProbeStage stage;
    uint32_t usm_engine_boots;
    uint32_t usm_engine_time;
    bool has_monza4qt;
    bool has_higgs3;
    bool monza4qt_key_present;
    bool higgs3_key_present;
    uint8_t usm_engine_id_storage[SEADER_UHF_SNMP_MAX_ID_LEN];
    size_t usm_engine_id_len;
    uint8_t usm_username_storage[SEADER_UHF_SNMP_MAX_ID_LEN];
    size_t usm_username_len;
    uint8_t ice_value_storage[SEADER_UHF_SNMP_MAX_VALUE_LEN];
    size_t ice_value_len;
} SeaderUhfSnmpProbe;

void seader_uhf_snmp_probe_init(SeaderUhfSnmpProbe* probe);

bool seader_uhf_snmp_probe_build_next_request(
    const SeaderUhfSnmpProbe* probe,
    uint8_t* scratch,
    size_t scratch_capacity,
    uint8_t* message,
    size_t message_capacity,
    size_t* message_len);

bool seader_uhf_snmp_probe_consume_response(
    SeaderUhfSnmpProbe* probe,
    const uint8_t* response,
    size_t response_len);

bool seader_uhf_snmp_probe_consume_error(
    SeaderUhfSnmpProbe* probe,
    uint32_t error_code,
    const uint8_t* data,
    size_t data_len);
