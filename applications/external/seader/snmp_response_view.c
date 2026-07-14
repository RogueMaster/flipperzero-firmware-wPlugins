#include "snmp_response_view.h"

#include <string.h>

bool seader_snmp_parse_response_view(
    const uint8_t* response,
    size_t response_len,
    SeaderSnmpResponseView* view) {
    SeaderBerCursor root_cursor = {0};
    SeaderBerTlvView root = {0};
    SeaderBerTlvView tlv = {0};

    if(!response || response_len == 0U || !view) return false;
    memset(view, 0, sizeof(*view));

    seader_ber_cursor_init(&root_cursor, response, response_len);
    if(!seader_ber_next_tlv(&root_cursor, &root) || root.tag != 0x30U) return false;

    SeaderBerCursor message_cursor = {0};
    seader_ber_cursor_init(&message_cursor, root.value.ptr, root.value.len);

    if(!seader_ber_next_tlv(&message_cursor, &tlv) || tlv.tag != 0x02U) return false;
    if(!seader_ber_next_tlv(&message_cursor, &tlv) || tlv.tag != 0x30U) return false;

    SeaderBerTlvView security_params_octet = {0};
    if(!seader_ber_next_tlv(&message_cursor, &security_params_octet) ||
       security_params_octet.tag != 0x04U) {
        return false;
    }
    if(!seader_ber_next_tlv(&message_cursor, &tlv) || tlv.tag != 0x30U) return false;

    SeaderBerCursor sec_octet_cursor = {0};
    SeaderBerTlvView sec_sequence = {0};
    seader_ber_cursor_init(
        &sec_octet_cursor, security_params_octet.value.ptr, security_params_octet.value.len);
    if(!seader_ber_next_tlv(&sec_octet_cursor, &sec_sequence) || sec_sequence.tag != 0x30U)
        return false;

    SeaderBerCursor sec_cursor = {0};
    SeaderBerTlvView usm_engine_id = {0};
    SeaderBerTlvView usm_engine_boots = {0};
    SeaderBerTlvView usm_engine_time = {0};
    SeaderBerTlvView usm_username = {0};
    SeaderBerTlvView ignore = {0};
    seader_ber_cursor_init(&sec_cursor, sec_sequence.value.ptr, sec_sequence.value.len);

    if(!seader_ber_next_tlv(&sec_cursor, &usm_engine_id) || usm_engine_id.tag != 0x04U)
        return false;
    if(!seader_ber_next_tlv(&sec_cursor, &usm_engine_boots) || usm_engine_boots.tag != 0x02U)
        return false;
    if(!seader_ber_next_tlv(&sec_cursor, &usm_engine_time) || usm_engine_time.tag != 0x02U)
        return false;
    if(!seader_ber_next_tlv(&sec_cursor, &usm_username) || usm_username.tag != 0x04U) return false;
    if(!seader_ber_next_tlv(&sec_cursor, &ignore) || ignore.tag != 0x04U) return false;
    if(!seader_ber_next_tlv(&sec_cursor, &ignore) || ignore.tag != 0x04U) return false;

    SeaderBerCursor scoped_cursor = {0};
    SeaderBerTlvView context_engine = {0};
    SeaderBerTlvView context_name = {0};
    SeaderBerTlvView pdu = {0};
    seader_ber_cursor_init(&scoped_cursor, tlv.value.ptr, tlv.value.len);
    if(!seader_ber_next_tlv(&scoped_cursor, &context_engine) || context_engine.tag != 0x04U)
        return false;
    if(!seader_ber_next_tlv(&scoped_cursor, &context_name) || context_name.tag != 0x04U)
        return false;
    if(!seader_ber_next_tlv(&scoped_cursor, &pdu)) return false;

    SeaderBerCursor pdu_cursor = {0};
    SeaderBerTlvView request_id = {0};
    SeaderBerTlvView error_status = {0};
    SeaderBerTlvView error_index = {0};
    SeaderBerTlvView varbinds = {0};
    seader_ber_cursor_init(&pdu_cursor, pdu.value.ptr, pdu.value.len);
    if(!seader_ber_next_tlv(&pdu_cursor, &request_id) || request_id.tag != 0x02U) return false;
    if(!seader_ber_next_tlv(&pdu_cursor, &error_status) || error_status.tag != 0x02U) return false;
    if(!seader_ber_next_tlv(&pdu_cursor, &error_index) || error_index.tag != 0x02U) return false;
    if(!seader_ber_next_tlv(&pdu_cursor, &varbinds) || varbinds.tag != 0x30U) return false;

    view->context_engine_id = context_engine.value;
    view->usm_engine_id = usm_engine_id.value;
    view->usm_username = usm_username.value;
    view->varbind_sequence = varbinds.value;
    view->pdu_tag = pdu.tag;

    if(!seader_ber_parse_uint32(usm_engine_boots.value, &view->usm_engine_boots) ||
       !seader_ber_parse_uint32(usm_engine_time.value, &view->usm_engine_time) ||
       !seader_ber_parse_uint32(error_status.value, &view->error_status) ||
       !seader_ber_parse_uint32(error_index.value, &view->error_index)) {
        return false;
    }

    return true;
}

bool seader_snmp_find_varbind_octet_value(
    SeaderBytesView varbind_sequence,
    SeaderBytesView expected_oid,
    SeaderBytesView* value) {
    SeaderBerCursor cursor = {0};
    seader_ber_cursor_init(&cursor, varbind_sequence.ptr, varbind_sequence.len);

    while(cursor.offset < cursor.len) {
        SeaderBerTlvView varbind = {0};
        SeaderBerTlvView oid = {0};
        SeaderBerTlvView object_value = {0};
        SeaderBerCursor inner = {0};

        if(!seader_ber_next_tlv(&cursor, &varbind) || varbind.tag != 0x30U) return false;
        seader_ber_cursor_init(&inner, varbind.value.ptr, varbind.value.len);
        if(!seader_ber_next_tlv(&inner, &oid) || oid.tag != 0x06U) return false;
        if(!seader_ber_next_tlv(&inner, &object_value)) return false;

        if(oid.value.len == expected_oid.len &&
           memcmp(oid.value.ptr, expected_oid.ptr, expected_oid.len) == 0) {
            if(object_value.tag != 0x04U) return false;
            if(value) *value = object_value.value;
            return true;
        }
    }

    return false;
}
