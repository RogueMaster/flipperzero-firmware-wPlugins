#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool seader_snmp_build_discovery_request(
    uint8_t* scratch,
    size_t scratch_capacity,
    uint8_t* message,
    size_t message_capacity,
    size_t* message_len);

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
    size_t* message_len);
