#include "uhf_snmp_probe.h"

#include "snmp_codec.h"
#include <string.h>

static const uint8_t oid_elite_ice[] = {0x03, 0x01, 0x07, 0x01, 0x38};
static const uint8_t oid_uhf_tags_config[] = {0x03, 0x01, 0x07, 0x03, 0x0B, 0x00};
static const uint8_t oid_monza4qt_access_key[] = {
    0x2B,
    0x06,
    0x01,
    0x04,
    0x01,
    0x81,
    0xE4,
    0x38,
    0x01,
    0x01,
    0x02,
    0x01,
    0x1E,
    0x01,
    0x01,
    0x01,
    0x01};
static const uint8_t oid_higgs3_access_key[] = {
    0x2B,
    0x06,
    0x01,
    0x04,
    0x01,
    0x81,
    0xE4,
    0x38,
    0x01,
    0x01,
    0x02,
    0x01,
    0x22,
    0x01,
    0x01,
    0x01,
    0x01};

static void seader_uhf_snmp_probe_advance_after_tag_config(SeaderUhfSnmpProbe* probe) {
    if(probe->has_monza4qt) {
        probe->stage = SeaderUhfSnmpProbeStageReadMonza4QtKey;
    } else if(probe->has_higgs3) {
        probe->stage = SeaderUhfSnmpProbeStageReadHiggs3Key;
    } else {
        probe->stage = SeaderUhfSnmpProbeStageDone;
    }
}

static void seader_uhf_snmp_probe_advance_after_monza(SeaderUhfSnmpProbe* probe) {
    if(probe->has_higgs3) {
        probe->stage = SeaderUhfSnmpProbeStageReadHiggs3Key;
    } else {
        probe->stage = SeaderUhfSnmpProbeStageDone;
    }
}

void seader_uhf_snmp_probe_init(SeaderUhfSnmpProbe* probe) {
    if(!probe) return;
    memset(probe, 0, sizeof(*probe));
    probe->stage = SeaderUhfSnmpProbeStageDiscovery;
}

bool seader_uhf_snmp_probe_build_next_request(
    const SeaderUhfSnmpProbe* probe,
    uint8_t* scratch,
    size_t scratch_capacity,
    uint8_t* message,
    size_t message_capacity,
    size_t* message_len) {
    if(!probe) return false;

    switch(probe->stage) {
    case SeaderUhfSnmpProbeStageDiscovery:
        return seader_snmp_build_discovery_request(
            scratch, scratch_capacity, message, message_capacity, message_len);
    case SeaderUhfSnmpProbeStageReadIce:
        return seader_snmp_build_get_data_request(
            probe->usm_engine_id_storage,
            probe->usm_engine_id_len,
            probe->usm_username_storage,
            probe->usm_username_len,
            probe->usm_engine_boots,
            probe->usm_engine_time,
            oid_elite_ice,
            sizeof(oid_elite_ice),
            scratch,
            scratch_capacity,
            message,
            message_capacity,
            message_len);
    case SeaderUhfSnmpProbeStageReadTagConfig:
        return seader_snmp_build_get_data_request(
            probe->usm_engine_id_storage,
            probe->usm_engine_id_len,
            probe->usm_username_storage,
            probe->usm_username_len,
            probe->usm_engine_boots,
            probe->usm_engine_time,
            oid_uhf_tags_config,
            sizeof(oid_uhf_tags_config),
            scratch,
            scratch_capacity,
            message,
            message_capacity,
            message_len);
    case SeaderUhfSnmpProbeStageReadMonza4QtKey:
        return seader_snmp_build_get_data_request(
            probe->usm_engine_id_storage,
            probe->usm_engine_id_len,
            probe->usm_username_storage,
            probe->usm_username_len,
            probe->usm_engine_boots,
            probe->usm_engine_time,
            oid_monza4qt_access_key,
            sizeof(oid_monza4qt_access_key),
            scratch,
            scratch_capacity,
            message,
            message_capacity,
            message_len);
    case SeaderUhfSnmpProbeStageReadHiggs3Key:
        return seader_snmp_build_get_data_request(
            probe->usm_engine_id_storage,
            probe->usm_engine_id_len,
            probe->usm_username_storage,
            probe->usm_username_len,
            probe->usm_engine_boots,
            probe->usm_engine_time,
            oid_higgs3_access_key,
            sizeof(oid_higgs3_access_key),
            scratch,
            scratch_capacity,
            message,
            message_capacity,
            message_len);
    default:
        return false;
    }
}

bool seader_uhf_snmp_probe_consume_response(
    SeaderUhfSnmpProbe* probe,
    const uint8_t* response,
    size_t response_len) {
    SeaderSnmpResponseView view = {0};
    SeaderBytesView value = {0};

    if(!probe || !seader_snmp_parse_response_view(response, response_len, &view)) {
        if(probe) probe->stage = SeaderUhfSnmpProbeStageFailed;
        return false;
    }
    if(view.error_status != 0U) {
        probe->stage = SeaderUhfSnmpProbeStageFailed;
        return false;
    }

    switch(probe->stage) {
    case SeaderUhfSnmpProbeStageDiscovery:
        probe->usm_engine_id_len = view.usm_engine_id.len;
        if(probe->usm_engine_id_len > sizeof(probe->usm_engine_id_storage)) {
            probe->stage = SeaderUhfSnmpProbeStageFailed;
            return false;
        }
        memcpy(probe->usm_engine_id_storage, view.usm_engine_id.ptr, probe->usm_engine_id_len);

        probe->usm_username_len = view.usm_username.len;
        if(probe->usm_username_len > sizeof(probe->usm_username_storage)) {
            probe->stage = SeaderUhfSnmpProbeStageFailed;
            return false;
        }
        memcpy(probe->usm_username_storage, view.usm_username.ptr, probe->usm_username_len);
        probe->usm_engine_boots = view.usm_engine_boots;
        probe->usm_engine_time = view.usm_engine_time;
        probe->stage = SeaderUhfSnmpProbeStageReadIce;
        return true;
    case SeaderUhfSnmpProbeStageReadIce:
        if(!seader_snmp_find_varbind_octet_value(
               view.varbind_sequence,
               (SeaderBytesView){oid_elite_ice, sizeof(oid_elite_ice)},
               &value)) {
            probe->stage = SeaderUhfSnmpProbeStageFailed;
            return false;
        }
        probe->ice_value_len = value.len;
        if(probe->ice_value_len > sizeof(probe->ice_value_storage)) {
            probe->stage = SeaderUhfSnmpProbeStageFailed;
            return false;
        }
        memcpy(probe->ice_value_storage, value.ptr, probe->ice_value_len);
        probe->stage = SeaderUhfSnmpProbeStageReadTagConfig;
        return true;
    case SeaderUhfSnmpProbeStageReadTagConfig:
        if(!seader_snmp_find_varbind_octet_value(
               view.varbind_sequence,
               (SeaderBytesView){oid_uhf_tags_config, sizeof(oid_uhf_tags_config)},
               &value)) {
            probe->stage = SeaderUhfSnmpProbeStageFailed;
            return false;
        }
        {
            SeaderUhfTagConfigView tag_config = {0};
            if(!seader_uhf_tag_config_parse(value, &tag_config)) {
                probe->stage = SeaderUhfSnmpProbeStageFailed;
                return false;
            }
            probe->has_monza4qt = tag_config.has_monza4qt;
            probe->has_higgs3 = tag_config.has_higgs3;
        }
        seader_uhf_snmp_probe_advance_after_tag_config(probe);
        return true;
    case SeaderUhfSnmpProbeStageReadMonza4QtKey:
        probe->monza4qt_key_present =
            seader_snmp_find_varbind_octet_value(
                view.varbind_sequence,
                (SeaderBytesView){oid_monza4qt_access_key, sizeof(oid_monza4qt_access_key)},
                &value) &&
            value.len > 0U;
        seader_uhf_snmp_probe_advance_after_monza(probe);
        return true;
    case SeaderUhfSnmpProbeStageReadHiggs3Key:
        probe->higgs3_key_present =
            seader_snmp_find_varbind_octet_value(
                view.varbind_sequence,
                (SeaderBytesView){oid_higgs3_access_key, sizeof(oid_higgs3_access_key)},
                &value) &&
            value.len > 0U;
        probe->stage = SeaderUhfSnmpProbeStageDone;
        return true;
    default:
        probe->stage = SeaderUhfSnmpProbeStageFailed;
        return false;
    }
}

bool seader_uhf_snmp_probe_consume_error(
    SeaderUhfSnmpProbe* probe,
    uint32_t error_code,
    const uint8_t* data,
    size_t data_len) {
    if(!probe) {
        return false;
    }

    if(error_code == 0x06U && data_len >= 2U && data[0] == 0x69U && data[1] == 0x82U) {
        if(probe->stage == SeaderUhfSnmpProbeStageReadMonza4QtKey) {
            probe->monza4qt_key_present = true;
            seader_uhf_snmp_probe_advance_after_monza(probe);
            return true;
        } else if(probe->stage == SeaderUhfSnmpProbeStageReadHiggs3Key) {
            probe->higgs3_key_present = true;
            probe->stage = SeaderUhfSnmpProbeStageDone;
            return true;
        }
    }

    if(error_code == 0x11U && data_len >= 2U &&
       ((data[0] == 0x2EU && data[1] == 0x00U) || (data[0] == 0x39U && data[1] == 0x00U))) {
        if(probe->stage == SeaderUhfSnmpProbeStageReadMonza4QtKey) {
            probe->monza4qt_key_present = false;
            seader_uhf_snmp_probe_advance_after_monza(probe);
            return true;
        } else if(probe->stage == SeaderUhfSnmpProbeStageReadHiggs3Key) {
            probe->higgs3_key_present = false;
            probe->stage = SeaderUhfSnmpProbeStageDone;
            return true;
        }
    }

    probe->stage = SeaderUhfSnmpProbeStageFailed;
    return false;
}
