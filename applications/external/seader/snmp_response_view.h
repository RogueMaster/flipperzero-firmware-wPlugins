#pragma once

#include "snmp_ber_view.h"

typedef struct {
    SeaderBytesView context_engine_id;
    SeaderBytesView usm_engine_id;
    SeaderBytesView usm_username;
    SeaderBytesView varbind_sequence;
    uint8_t pdu_tag;
    uint32_t usm_engine_boots;
    uint32_t usm_engine_time;
    uint32_t error_status;
    uint32_t error_index;
} SeaderSnmpResponseView;

bool seader_snmp_parse_response_view(
    const uint8_t* response,
    size_t response_len,
    SeaderSnmpResponseView* view);

bool seader_snmp_find_varbind_octet_value(
    SeaderBytesView varbind_sequence,
    SeaderBytesView expected_oid,
    SeaderBytesView* value);
