#include "snmp_codec.h"

#include <string.h>

static bool
    seader_snmp_append_length(uint8_t* dst, size_t dst_size, size_t* offset, size_t value_len) {
    if(!dst || !offset) return false;

    if(value_len < 0x80U) {
        if(*offset + 1U > dst_size) return false;
        dst[(*offset)++] = (uint8_t)value_len;
        return true;
    }
    if(value_len <= 0xFFU) {
        if(*offset + 2U > dst_size) return false;
        dst[(*offset)++] = 0x81U;
        dst[(*offset)++] = (uint8_t)value_len;
        return true;
    }
    if(value_len <= 0xFFFFU) {
        if(*offset + 3U > dst_size) return false;
        dst[(*offset)++] = 0x82U;
        dst[(*offset)++] = (uint8_t)(value_len >> 8U);
        dst[(*offset)++] = (uint8_t)value_len;
        return true;
    }
    if(value_len <= 0xFFFFFFU) {
        if(*offset + 4U > dst_size) return false;
        dst[(*offset)++] = 0x83U;
        dst[(*offset)++] = (uint8_t)(value_len >> 16U);
        dst[(*offset)++] = (uint8_t)(value_len >> 8U);
        dst[(*offset)++] = (uint8_t)value_len;
        return true;
    }

    return false;
}

static bool seader_snmp_append_tlv(
    uint8_t* dst,
    size_t dst_size,
    size_t* offset,
    uint8_t tag,
    const uint8_t* value,
    size_t value_len) {
    if(!dst || !offset) return false;
    if(*offset + 1U > dst_size) return false;

    dst[(*offset)++] = tag;
    if(!seader_snmp_append_length(dst, dst_size, offset, value_len)) return false;

    if(value_len > 0U) {
        if(!value || *offset + value_len > dst_size) return false;
        memcpy(dst + *offset, value, value_len);
        *offset += value_len;
    }

    return true;
}

static bool
    seader_snmp_append_uint32(uint8_t* dst, size_t dst_size, size_t* offset, uint32_t value) {
    uint8_t encoded[5] = {0};
    size_t encoded_len = 0U;

    do {
        encoded[4U - encoded_len] = (uint8_t)(value & 0xFFU);
        encoded_len++;
        value >>= 8U;
    } while(value != 0U && encoded_len < 5U);

    const uint8_t* integer_value = encoded + (5U - encoded_len);
    uint8_t prefixed[6] = {0};
    size_t prefixed_len = 0U;

    if(integer_value[0] & 0x80U) {
        prefixed[prefixed_len++] = 0x00U;
    }
    memcpy(prefixed + prefixed_len, integer_value, encoded_len);
    prefixed_len += encoded_len;

    return seader_snmp_append_tlv(dst, dst_size, offset, 0x02U, prefixed, prefixed_len);
}

static bool seader_snmp_wrap_tlv_inplace(
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t value_len,
    uint8_t tag,
    size_t* encoded_len) {
    if(!buffer || !encoded_len) return false;

    uint8_t header[5] = {tag};
    size_t header_len = 1U;
    if(!seader_snmp_append_length(header, sizeof(header), &header_len, value_len)) return false;
    if(header_len + value_len > buffer_capacity) return false;

    memmove(buffer + header_len, buffer, value_len);
    memcpy(buffer, header, header_len);
    *encoded_len = header_len + value_len;
    return true;
}

static bool seader_snmp_build_get_data_payload(
    const uint8_t* target_oid,
    size_t target_oid_len,
    uint32_t offset_value,
    uint32_t max_chunk_len,
    uint8_t* payload,
    size_t payload_capacity,
    size_t* payload_len) {
    if(!target_oid || target_oid_len == 0U || !payload || !payload_len) return false;
    *payload_len = 0U;

    if(!seader_snmp_append_tlv(
           payload, payload_capacity, payload_len, 0x06U, target_oid, target_oid_len)) {
        return false;
    }
    if(!seader_snmp_append_uint32(payload, payload_capacity, payload_len, offset_value))
        return false;
    if(!seader_snmp_append_uint32(payload, payload_capacity, payload_len, max_chunk_len))
        return false;

    return seader_snmp_wrap_tlv_inplace(
        payload, payload_capacity, *payload_len, 0x30U, payload_len);
}

static bool seader_snmp_build_reportable_get_request(
    const uint8_t* engine_id,
    size_t engine_id_len,
    const uint8_t* user_name,
    size_t user_name_len,
    uint32_t engine_boots,
    uint32_t engine_time,
    const uint8_t* data_oid,
    size_t data_oid_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t* scratch,
    size_t scratch_capacity,
    uint8_t* message,
    size_t message_capacity,
    size_t* message_len) {
    size_t scratch_len = 0U;
    size_t scoped_pdu_len = 0U;
    uint8_t global_data_value[16] = {0};
    uint8_t global_data[24] = {0};
    uint8_t version[8] = {0};
    size_t global_data_value_len = 0U;
    size_t global_data_len = 0U;
    size_t version_len = 0U;
    const uint8_t flags[] = {0x04U};

    if(!scratch || !message || !message_len) return false;
    *message_len = 0U;

    if(data_oid && data_oid_len > 0U) {
        if(!seader_snmp_append_tlv(
               scratch, scratch_capacity, &scratch_len, 0x06U, data_oid, data_oid_len)) {
            return false;
        }
        if(!seader_snmp_append_tlv(
               scratch, scratch_capacity, &scratch_len, 0x04U, data, data_len)) {
            return false;
        }
        if(!seader_snmp_wrap_tlv_inplace(
               scratch, scratch_capacity, scratch_len, 0x30U, &scratch_len)) {
            return false;
        }
    }

    memcpy(message, scratch, scratch_len);
    *message_len = scratch_len;
    if(!seader_snmp_wrap_tlv_inplace(message, message_capacity, *message_len, 0x30U, message_len)) {
        return false;
    }

    scratch_len = 0U;
    if(!seader_snmp_append_uint32(scratch, scratch_capacity, &scratch_len, 0U) ||
       !seader_snmp_append_uint32(scratch, scratch_capacity, &scratch_len, 0U) ||
       !seader_snmp_append_uint32(scratch, scratch_capacity, &scratch_len, 0U)) {
        return false;
    }
    if(scratch_len + *message_len > scratch_capacity) return false;
    memcpy(scratch + scratch_len, message, *message_len);
    scratch_len += *message_len;
    if(!seader_snmp_wrap_tlv_inplace(scratch, scratch_capacity, scratch_len, 0xA0U, &scratch_len)) {
        return false;
    }

    *message_len = 0U;
    if(!seader_snmp_append_tlv(message, message_capacity, message_len, 0x04U, NULL, 0U) ||
       !seader_snmp_append_tlv(message, message_capacity, message_len, 0x04U, NULL, 0U)) {
        return false;
    }
    if(*message_len + scratch_len > message_capacity) return false;
    memcpy(message + *message_len, scratch, scratch_len);
    *message_len += scratch_len;
    if(!seader_snmp_wrap_tlv_inplace(
           message, message_capacity, *message_len, 0x30U, &scoped_pdu_len)) {
        return false;
    }

    scratch_len = 0U;
    if(!seader_snmp_append_tlv(
           scratch, scratch_capacity, &scratch_len, 0x04U, engine_id, engine_id_len) ||
       !seader_snmp_append_uint32(scratch, scratch_capacity, &scratch_len, engine_boots) ||
       !seader_snmp_append_uint32(scratch, scratch_capacity, &scratch_len, engine_time) ||
       !seader_snmp_append_tlv(
           scratch, scratch_capacity, &scratch_len, 0x04U, user_name, user_name_len) ||
       !seader_snmp_append_tlv(scratch, scratch_capacity, &scratch_len, 0x04U, NULL, 0U) ||
       !seader_snmp_append_tlv(scratch, scratch_capacity, &scratch_len, 0x04U, NULL, 0U)) {
        return false;
    }
    if(!seader_snmp_wrap_tlv_inplace(scratch, scratch_capacity, scratch_len, 0x30U, &scratch_len) ||
       !seader_snmp_wrap_tlv_inplace(scratch, scratch_capacity, scratch_len, 0x04U, &scratch_len)) {
        return false;
    }

    if(!seader_snmp_append_uint32(
           global_data_value, sizeof(global_data_value), &global_data_value_len, 0U) ||
       !seader_snmp_append_uint32(
           global_data_value, sizeof(global_data_value), &global_data_value_len, 756U) ||
       !seader_snmp_append_tlv(
           global_data_value,
           sizeof(global_data_value),
           &global_data_value_len,
           0x04U,
           flags,
           sizeof(flags)) ||
       !seader_snmp_append_uint32(
           global_data_value, sizeof(global_data_value), &global_data_value_len, 0x0101U)) {
        return false;
    }
    if(!seader_snmp_append_tlv(
           global_data,
           sizeof(global_data),
           &global_data_len,
           0x30U,
           global_data_value,
           global_data_value_len)) {
        return false;
    }
    if(!seader_snmp_append_uint32(version, sizeof(version), &version_len, 3U)) return false;

    if(version_len + global_data_len + scratch_len + scoped_pdu_len > message_capacity)
        return false;
    memmove(message + version_len + global_data_len + scratch_len, message, scoped_pdu_len);
    memcpy(message, version, version_len);
    memcpy(message + version_len, global_data, global_data_len);
    memcpy(message + version_len + global_data_len, scratch, scratch_len);

    *message_len = version_len + global_data_len + scratch_len + scoped_pdu_len;
    return seader_snmp_wrap_tlv_inplace(
        message, message_capacity, *message_len, 0x30U, message_len);
}

bool seader_snmp_build_discovery_request(
    uint8_t* scratch,
    size_t scratch_capacity,
    uint8_t* message,
    size_t message_capacity,
    size_t* message_len) {
    return seader_snmp_build_reportable_get_request(
        NULL,
        0U,
        NULL,
        0U,
        0U,
        0U,
        NULL,
        0U,
        NULL,
        0U,
        scratch,
        scratch_capacity,
        message,
        message_capacity,
        message_len);
}

bool seader_snmp_build_get_data_request(
    const uint8_t* engine_id,
    size_t engine_id_len,
    const uint8_t* user_name,
    size_t user_name_len,
    uint32_t engine_boots,
    uint32_t engine_time,
    const uint8_t* target_oid,
    size_t target_oid_len,
    uint8_t* scratch,
    size_t scratch_capacity,
    uint8_t* message,
    size_t message_capacity,
    size_t* message_len) {
    static const uint8_t oid_get_data[] = {0x03U, 0x00U, 0x03U, 0x06U};
    size_t payload_len = 0U;

    if(!scratch || !message) return false;
    if(!seader_snmp_build_get_data_payload(
           target_oid, target_oid_len, 0U, 0x100U, message, message_capacity, &payload_len)) {
        return false;
    }

    return seader_snmp_build_reportable_get_request(
        engine_id,
        engine_id_len,
        user_name,
        user_name_len,
        engine_boots,
        engine_time,
        oid_get_data,
        sizeof(oid_get_data),
        message,
        payload_len,
        scratch,
        scratch_capacity,
        message,
        message_capacity,
        message_len);
}
