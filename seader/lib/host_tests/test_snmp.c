#include <ctype.h>
#include <string.h>

#include "munit.h"
#include "snmp_codec.h"
#include "uhf_snmp_probe.h"
#include "snmp_response_view.h"
#include "uhf_tag_config_view.h"

static const char* snmp_discovery_request_hex =
    "3038020103300E020100020202F4040104020201010410300E0400020100020100040004000400301104000400A00B0201000201000201003000";
static const char* snmp_discovery_response_hex =
    "308200A80201033082000E02010002010004020000020201010482003A30820036041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F020105020101040D2B0601040181E438010104080F0400040030820051041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F040D2B0601040181E438010104080FA08200210201000201000201003082001430820010060A2B060106030F0101040002020141";
static const char* snmp_ice_response_hex =
    "308200A80201033082000E02010002010004020000020201010482003A30820036041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F020105020102040D2B0601040181E438010104080F0400040030820051041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F040D2B0601040181E438010104080FA2820021020100020100020100308200143082001006050301070138040749434531383033";
static const char* snmp_uhf_config_response_hex =
    "308200F40201033082000E02010002010004020000020201010482003A30820036041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F020105020103040D2B0601040181E438010104080F040004003082009D041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F040D2B0601040181E438010104080FA282006D020100020100020100308200603082005C0606030107030B00045204E2003412112B0601040181E438010102012201010101112B0601040181E43801010201220101020104E2801105112B0601040181E438010102011E01010101112B0601040181E438010102011E01010201";
static const char* snmp_ice_request_hex =
    "307A020103300E020100020202F40401040202010104383036041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F020105020101040D2B0601040181E438010104080F04000400302B04000400A025020100020100020100301A30180604030003060410300E0605030107013802010002020100";
static const char* snmp_monza_request_hex =
    "308186020103300E020100020202F40401040202010104383036041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F020105020101040D2B0601040181E438010104080F04000400303704000400A03102010002010002010030263024060403000306041C301A06112B0601040181E438010102011E0101010102010002020100";
static const char* snmp_higgs_request_hex =
    "308186020103300E020100020202F40401040202010104383036041B2B0601040181E438010103050F8C9088CDA2A8D885C0B298D7FF7F020105020101040D2B0601040181E438010104080F04000400303704000400A03102010002010002010030263024060403000306041C301A06112B0601040181E43801010201220101010102010002020100";

static const uint8_t oid_elite_ice[] = {0x03, 0x01, 0x07, 0x01, 0x38};
static const uint8_t oid_uhf_tags_config[] = {0x03, 0x01, 0x07, 0x03, 0x0B, 0x00};
static const uint8_t oid_monza4qt_access_key[] = {
    0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xE4, 0x38, 0x01, 0x01, 0x02, 0x01, 0x1E, 0x01, 0x01, 0x01, 0x01};
static const uint8_t oid_higgs3_access_key[] = {
    0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xE4, 0x38, 0x01, 0x01, 0x02, 0x01, 0x22, 0x01, 0x01, 0x01, 0x01};
static const uint8_t live_engine_id[] = {
    0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xE4, 0x38, 0x01, 0x01, 0x03,
    0x05, 0x0F, 0x8C, 0x90, 0x88, 0xCD, 0xA2, 0xA8, 0xD8, 0x85, 0xC0,
    0xB2, 0x98, 0xD7, 0xFF, 0x7F};
static const uint8_t live_username[] = {
    0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xE4, 0x38, 0x01, 0x01, 0x04, 0x08, 0x0F};

static size_t test_hex_to_bytes(const char* hex, uint8_t* out, size_t out_size) {
    size_t len = 0U;
    int high_nibble = -1;

    for(const char* p = hex; *p; ++p) {
        int value = -1;
        if(*p >= '0' && *p <= '9') value = *p - '0';
        else if(*p >= 'A' && *p <= 'F') value = *p - 'A' + 10;
        else if(*p >= 'a' && *p <= 'f') value = *p - 'a' + 10;
        else if(isspace((unsigned char)*p)) continue;
        else munit_error("invalid hex character");

        if(high_nibble < 0) {
            high_nibble = value;
        } else {
            if(len >= out_size) munit_error("hex output buffer too small");
            out[len++] = (uint8_t)((high_nibble << 4) | value);
            high_nibble = -1;
        }
    }

    if(high_nibble >= 0) munit_error("odd-length hex string");
    return len;
}

static MunitResult test_build_discovery_request_matches_live_vector(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t message[512] = {0};
    uint8_t scratch[512] = {0};
    uint8_t expected[512] = {0};
    size_t message_len = 0U;
    size_t expected_len = test_hex_to_bytes(snmp_discovery_request_hex, expected, sizeof(expected));

    munit_assert_true(seader_snmp_build_discovery_request(
        scratch, sizeof(scratch), message, sizeof(message), &message_len));
    munit_assert_size(message_len, ==, expected_len);
    munit_assert_memory_equal(expected_len, message, expected);
    return MUNIT_OK;
}

static MunitResult test_build_get_data_requests_match_live_vectors(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t message[512] = {0};
    uint8_t scratch[512] = {0};
    uint8_t expected[512] = {0};
    size_t message_len = 0U;
    size_t expected_len = test_hex_to_bytes(snmp_ice_request_hex, expected, sizeof(expected));

    munit_assert_true(seader_snmp_build_get_data_request(
        live_engine_id, sizeof(live_engine_id), live_username, sizeof(live_username), 5U, 1U,
        oid_elite_ice, sizeof(oid_elite_ice), scratch, sizeof(scratch), message, sizeof(message), &message_len));
    munit_assert_size(message_len, ==, expected_len);
    munit_assert_memory_equal(expected_len, message, expected);

    expected_len = test_hex_to_bytes(snmp_monza_request_hex, expected, sizeof(expected));
    munit_assert_true(seader_snmp_build_get_data_request(
        live_engine_id, sizeof(live_engine_id), live_username, sizeof(live_username), 5U, 1U,
        oid_monza4qt_access_key, sizeof(oid_monza4qt_access_key), scratch, sizeof(scratch), message, sizeof(message), &message_len));
    munit_assert_size(message_len, ==, expected_len);
    munit_assert_memory_equal(expected_len, message, expected);

    expected_len = test_hex_to_bytes(snmp_higgs_request_hex, expected, sizeof(expected));
    munit_assert_true(seader_snmp_build_get_data_request(
        live_engine_id, sizeof(live_engine_id), live_username, sizeof(live_username), 5U, 1U,
        oid_higgs3_access_key, sizeof(oid_higgs3_access_key), scratch, sizeof(scratch), message, sizeof(message), &message_len));
    munit_assert_size(message_len, ==, expected_len);
    munit_assert_memory_equal(expected_len, message, expected);

    return MUNIT_OK;
}

static MunitResult test_parse_response_and_zero_copy_views(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[256] = {0};
    size_t response_len = test_hex_to_bytes(snmp_discovery_response_hex, response, sizeof(response));
    SeaderSnmpResponseView view = {0};

    munit_assert_true(seader_snmp_parse_response_view(response, response_len, &view));
    munit_assert_uint32(view.error_status, ==, 0U);
    munit_assert_uint32(view.error_index, ==, 0U);
    munit_assert_uint32(view.usm_engine_boots, ==, 5U);
    munit_assert_uint32(view.usm_engine_time, ==, 1U);
    munit_assert_memory_equal(sizeof(live_engine_id), view.context_engine_id.ptr, live_engine_id);
    munit_assert_memory_equal(sizeof(live_engine_id), view.usm_engine_id.ptr, live_engine_id);
    munit_assert_memory_equal(sizeof(live_username), view.usm_username.ptr, live_username);
    munit_assert_true(view.context_engine_id.ptr >= response);
    munit_assert_true(view.context_engine_id.ptr < response + response_len);
    return MUNIT_OK;
}

static MunitResult test_parse_ice_and_tag_config_values(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[512] = {0};
    size_t response_len = test_hex_to_bytes(snmp_ice_response_hex, response, sizeof(response));
    SeaderSnmpResponseView view = {0};
    SeaderBytesView value = {0};

    munit_assert_true(seader_snmp_parse_response_view(response, response_len, &view));
    munit_assert_true(seader_snmp_find_varbind_octet_value(
        view.varbind_sequence, (SeaderBytesView){oid_elite_ice, sizeof(oid_elite_ice)}, &value));
    munit_assert_size(value.len, ==, 7);
    munit_assert_memory_equal(7, value.ptr, "ICE1803");

    response_len = test_hex_to_bytes(snmp_uhf_config_response_hex, response, sizeof(response));
    munit_assert_true(seader_snmp_parse_response_view(response, response_len, &view));
    munit_assert_true(seader_snmp_find_varbind_octet_value(
        view.varbind_sequence, (SeaderBytesView){oid_uhf_tags_config, sizeof(oid_uhf_tags_config)}, &value));
    munit_assert_size(value.len, >, 80);
    munit_assert_true(value.ptr >= response);
    munit_assert_true(value.ptr < response + response_len);
    return MUNIT_OK;
}

static MunitResult test_probe_stages(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[512] = {0};
    size_t response_len = test_hex_to_bytes(snmp_discovery_response_hex, response, sizeof(response));
    SeaderUhfSnmpProbe probe = {0};
    uint8_t message[512] = {0};
    uint8_t scratch[512] = {0};
    size_t message_len = 0U;

    seader_uhf_snmp_probe_init(&probe);
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageDiscovery);
    munit_assert_true(seader_uhf_snmp_probe_build_next_request(
        &probe, scratch, sizeof(scratch), message, sizeof(message), &message_len));
    munit_assert_size(message_len, >, 0);

    munit_assert_true(seader_uhf_snmp_probe_consume_response(&probe, response, response_len));
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageReadIce);
    memset(response, 0xA5, response_len);
    munit_assert_true(seader_uhf_snmp_probe_build_next_request(
        &probe, scratch, sizeof(scratch), message, sizeof(message), &message_len));

    response_len = test_hex_to_bytes(snmp_ice_response_hex, response, sizeof(response));
    munit_assert_true(seader_uhf_snmp_probe_consume_response(&probe, response, response_len));
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageReadTagConfig);
    munit_assert_size(probe.ice_value_len, ==, 7);
    munit_assert_memory_equal(7, probe.ice_value_storage, "ICE1803");

    response_len = test_hex_to_bytes(snmp_uhf_config_response_hex, response, sizeof(response));
    munit_assert_true(seader_uhf_snmp_probe_consume_response(&probe, response, response_len));
    munit_assert_true(probe.has_monza4qt);
    munit_assert_true(probe.has_higgs3);
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageReadMonza4QtKey);

    munit_assert_true(seader_uhf_snmp_probe_consume_error(
        &probe, 0x06U, (const uint8_t*)"\x69\x82", 2U));
    munit_assert_true(probe.monza4qt_key_present);
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageReadHiggs3Key);

    munit_assert_true(seader_uhf_snmp_probe_consume_error(
        &probe, 0x06U, (const uint8_t*)"\x69\x82", 2U));
    munit_assert_true(probe.higgs3_key_present);
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageDone);
    return MUNIT_OK;
}

static MunitResult test_probe_full_sequence_succeeds_with_runtime_sized_buffers(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    SeaderUhfSnmpProbe probe = {0};
    uint8_t message[176] = {0};
    uint8_t scratch[240] = {0};
    uint8_t response[512] = {0};
    size_t message_len = 0U;
    size_t response_len = 0U;

    seader_uhf_snmp_probe_init(&probe);

    munit_assert_true(seader_uhf_snmp_probe_build_next_request(
        &probe, scratch, sizeof(scratch), message, sizeof(message), &message_len));
    response_len = test_hex_to_bytes(snmp_discovery_response_hex, response, sizeof(response));
    munit_assert_true(seader_uhf_snmp_probe_consume_response(&probe, response, response_len));

    memset(message, 0, sizeof(message));
    memset(scratch, 0, sizeof(scratch));
    munit_assert_true(seader_uhf_snmp_probe_build_next_request(
        &probe, scratch, sizeof(scratch), message, sizeof(message), &message_len));
    response_len = test_hex_to_bytes(snmp_ice_response_hex, response, sizeof(response));
    munit_assert_true(seader_uhf_snmp_probe_consume_response(&probe, response, response_len));

    memset(message, 0, sizeof(message));
    memset(scratch, 0, sizeof(scratch));
    munit_assert_true(seader_uhf_snmp_probe_build_next_request(
        &probe, scratch, sizeof(scratch), message, sizeof(message), &message_len));
    response_len = test_hex_to_bytes(snmp_uhf_config_response_hex, response, sizeof(response));
    munit_assert_true(seader_uhf_snmp_probe_consume_response(&probe, response, response_len));

    memset(message, 0, sizeof(message));
    memset(scratch, 0, sizeof(scratch));
    munit_assert_true(seader_uhf_snmp_probe_build_next_request(
        &probe, scratch, sizeof(scratch), message, sizeof(message), &message_len));
    munit_assert_true(seader_uhf_snmp_probe_consume_error(
        &probe, 0x06U, (const uint8_t*)"\x69\x82", 2U));

    memset(message, 0, sizeof(message));
    memset(scratch, 0, sizeof(scratch));
    munit_assert_true(seader_uhf_snmp_probe_build_next_request(
        &probe, scratch, sizeof(scratch), message, sizeof(message), &message_len));
    munit_assert_true(seader_uhf_snmp_probe_consume_error(
        &probe, 0x06U, (const uint8_t*)"\x69\x82", 2U));

    munit_assert_true(probe.monza4qt_key_present);
    munit_assert_true(probe.higgs3_key_present);
    munit_assert_int(probe.stage, ==, SeaderUhfSnmpProbeStageDone);
    return MUNIT_OK;
}

static MunitResult test_get_data_request_fits_bounded_transport_buffer(
    const MunitParameter params[],
    void* fixture) {
    (void)params;
    (void)fixture;

    uint8_t engine[SEADER_UHF_SNMP_MAX_ID_LEN];
    uint8_t username[SEADER_UHF_SNMP_MAX_ID_LEN];
    uint8_t scratch[240] = {0};
    uint8_t message[176] = {0};
    size_t message_len = 0U;

    memset(engine, 0xAA, sizeof(engine));
    memset(username, 0xBB, sizeof(username));

    munit_assert_true(seader_snmp_build_get_data_request(
        engine,
        sizeof(engine),
        username,
        sizeof(username),
        UINT32_MAX,
        UINT32_MAX,
        oid_monza4qt_access_key,
        sizeof(oid_monza4qt_access_key),
        scratch,
        sizeof(scratch),
        message,
        sizeof(message),
        &message_len));
    munit_assert_size(message_len, <=, sizeof(message));
    return MUNIT_OK;
}

static MunitResult test_tag_config_view_extracts_known_entries(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    uint8_t response[512] = {0};
    size_t response_len = test_hex_to_bytes(snmp_uhf_config_response_hex, response, sizeof(response));
    SeaderSnmpResponseView snmp = {0};
    SeaderBytesView config_payload = {0};
    SeaderUhfTagConfigView view = {0};
    SeaderUhfTagConfigEntryView entry = {0};

    munit_assert_true(seader_snmp_parse_response_view(response, response_len, &snmp));
    munit_assert_true(seader_snmp_find_varbind_octet_value(
        snmp.varbind_sequence, (SeaderBytesView){oid_uhf_tags_config, sizeof(oid_uhf_tags_config)}, &config_payload));
    munit_assert_true(seader_uhf_tag_config_parse(config_payload, &view));
    munit_assert_true(view.has_higgs3);
    munit_assert_true(view.has_monza4qt);
    munit_assert_size(view.entry_count, ==, 4);

    munit_assert_true(seader_uhf_tag_config_get_entry(&view, 0U, &entry));
    munit_assert_int(entry.kind, ==, SeaderUhfTagConfigEntryHiggs3Access);
    munit_assert_size(entry.oid.len, ==, 17U);

    munit_assert_true(seader_uhf_tag_config_get_entry(&view, 2U, &entry));
    munit_assert_int(entry.kind, ==, SeaderUhfTagConfigEntryMonza4QtAccess);
    return MUNIT_OK;
}

static MunitResult test_response_rejects_truncated_length(const MunitParameter params[], void* fixture) {
    (void)params;
    (void)fixture;
    const uint8_t malformed[] = {0x30, 0x82, 0x01};
    SeaderSnmpResponseView view = {0};
    munit_assert_false(seader_snmp_parse_response_view(malformed, sizeof(malformed), &view));
    return MUNIT_OK;
}

static MunitTest test_snmp_cases[] = {
    {(char*)"/build-discovery", test_build_discovery_request_matches_live_vector, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/build-get-data", test_build_get_data_requests_match_live_vectors, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/parse-response", test_parse_response_and_zero_copy_views, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/parse-values", test_parse_ice_and_tag_config_values, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/probe", test_probe_stages, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/probe-runtime-buffers", test_probe_full_sequence_succeeds_with_runtime_sized_buffers, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/bounded-get-data", test_get_data_request_fits_bounded_transport_buffer, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/tag-config", test_tag_config_view_extracts_known_entries, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/malformed-length", test_response_rejects_truncated_length, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, 0, NULL},
};

MunitSuite test_snmp_suite = {
    "",
    test_snmp_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
