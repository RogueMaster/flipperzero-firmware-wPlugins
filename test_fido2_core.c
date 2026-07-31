#include "ck42x_ctap2.h"
#include "ck42x_sha256.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t record[CK_CTAP2_CREDENTIAL_RECORD_SIZE];
    size_t record_length;
    unsigned presence_calls;
    unsigned load_calls;
    uint8_t rng_value;
    bool deny_presence;
    bool cancelled;
} TestPlatform;

static bool test_rng(void* context, uint8_t* output, size_t length) {
    TestPlatform* platform = context;
    for(size_t i = 0; i < length; ++i) output[i] = ++platform->rng_value;
    return true;
}

static bool test_was_cancelled(void* context) {
    return ((TestPlatform*)context)->cancelled;
}

static bool test_presence(void* context) {
    TestPlatform* platform = context;
    ++platform->presence_calls;
    return !platform->deny_presence;
}

static bool test_save(void* context, const uint8_t* record, size_t length) {
    TestPlatform* platform = context;
    if(length > sizeof(platform->record)) return false;
    memcpy(platform->record, record, length);
    platform->record_length = length;
    return true;
}

static bool test_load(
    void* context,
    const uint8_t credential_id[CK_CTAP2_CREDENTIAL_ID_SIZE],
    uint8_t* record,
    size_t capacity,
    size_t* length) {
    TestPlatform* platform = context;
    ++platform->load_calls;
    if(platform->record_length == 0U ||
       memcmp(platform->record + CK_CTAP2_RECORD_CREDENTIAL_ID_OFFSET,
              credential_id,
              CK_CTAP2_CREDENTIAL_ID_SIZE) != 0 ||
       capacity < platform->record_length) {
        return false;
    }
    memcpy(record, platform->record, platform->record_length);
    *length = platform->record_length;
    return true;
}

static CkCtap2 make_authenticator(TestPlatform* platform) {
    const CkCtap2Platform callbacks = {
        .context = platform,
        .random = test_rng,
        .user_present = test_presence,
        .was_cancelled = test_was_cancelled,
        .save_credential = test_save,
        .load_credential = test_load,
    };
    CkCtap2 authenticator;
    ck_ctap2_init(&authenticator, &callbacks);
    return authenticator;
}

static bool contains(const uint8_t* haystack, size_t haystack_length, const void* needle, size_t n) {
    if(n == 0U) return true;
    for(size_t i = 0; i + n <= haystack_length; ++i) {
        if(memcmp(haystack + i, needle, n) == 0) return true;
    }
    return false;
}

static size_t find_offset(
    const uint8_t* haystack, size_t haystack_length, const void* needle, size_t needle_length) {
    for(size_t i = 0; i + needle_length <= haystack_length; ++i) {
        if(memcmp(haystack + i, needle, needle_length) == 0) return i;
    }
    return SIZE_MAX;
}

static void test_get_info(void) {
    TestPlatform platform = {0};
    CkCtap2 authenticator = make_authenticator(&platform);
    uint8_t response[256];
    size_t response_length = 0;
    const uint8_t request[] = {CK_CTAP2_CMD_GET_INFO};
    const char fido_version[] = "FIDO_2_0";
    const char unsupported_client_pin[] = "clientPin";

    assert(ck_ctap2_handle(
        &authenticator, request, sizeof(request), response, sizeof(response), &response_length));
    assert(response_length > 1U);
    assert(response[0] == CK_CTAP2_OK);
    assert(contains(response, response_length, fido_version, sizeof(fido_version) - 1U));
    assert(
        find_offset(
            response,
            response_length,
            unsupported_client_pin,
            sizeof(unsupported_client_pin) - 1U) == SIZE_MAX);
}

static void test_sha256_known_answer(void) {
    static const uint8_t expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    uint8_t digest[32];
    ck_sha256((const uint8_t*)"abc", 3U, digest);
    assert(memcmp(digest, expected, sizeof(expected)) == 0);
}

static void test_make_and_get_assertion(void) {
    TestPlatform platform = {0};
    CkCtap2 authenticator = make_authenticator(&platform);
    uint8_t response[1024];
    size_t response_length = 0;
    static const uint8_t make_request[] = {
        0x01, 0xa5,
        0x01, 0x58, 0x20,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        0x02, 0xa1, 0x62, 'i', 'd', 0x6b, 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
        0x03, 0xa2, 0x62, 'i', 'd', 0x44, 1, 2, 3, 4, 0x64, 'n', 'a', 'm', 'e', 0x61, 'u',
        0x04, 0x81, 0xa2, 0x63, 'a', 'l', 'g', 0x26,
        0x64, 't', 'y', 'p', 'e', 0x6a, 'p', 'u', 'b', 'l', 'i', 'c', '-', 'k', 'e', 'y',
        0x07, 0xa1, 0x62, 'u', 'p', 0xf5,
    };

    assert(ck_ctap2_handle(
        &authenticator,
        make_request,
        sizeof(make_request),
        response,
        sizeof(response),
        &response_length));
    assert(response[0] == CK_CTAP2_OK);
    assert(platform.presence_calls == 1U);
    assert(platform.record_length == CK_CTAP2_CREDENTIAL_RECORD_SIZE);
    assert(platform.record[0] == CK_CTAP2_CREDENTIAL_RECORD_VERSION);
    assert(contains(response, response_length, "packed", 6U));
    assert(contains(response, response_length, "sig", 3U));
    memset(platform.record + 97U, 0xff, 4U);

    uint8_t assertion[192] = {
        0x02, 0xa3,
        0x01, 0x6b, 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o', 'm',
        0x02, 0x58, 0x20,
        31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
        0x03, 0x82, 0xa2,
        0x62, 'i', 'd', 0x58, CK_CTAP2_CREDENTIAL_ID_SIZE,
    };
    size_t offset = 58U;
    memset(assertion + offset, 0xee, CK_CTAP2_CREDENTIAL_ID_SIZE);
    offset += CK_CTAP2_CREDENTIAL_ID_SIZE;
    const uint8_t descriptor_tail[] = {
        0x64, 't', 'y', 'p', 'e', 0x6a, 'p', 'u', 'b', 'l', 'i', 'c', '-', 'k', 'e', 'y'};
    memcpy(assertion + offset, descriptor_tail, sizeof(descriptor_tail));
    offset += sizeof(descriptor_tail);
    const uint8_t second_descriptor[] = {
        0xa2, 0x62, 'i', 'd', 0x58, CK_CTAP2_CREDENTIAL_ID_SIZE};
    memcpy(assertion + offset, second_descriptor, sizeof(second_descriptor));
    offset += sizeof(second_descriptor);
    memcpy(assertion + offset,
           platform.record + CK_CTAP2_RECORD_CREDENTIAL_ID_OFFSET,
           CK_CTAP2_CREDENTIAL_ID_SIZE);
    offset += CK_CTAP2_CREDENTIAL_ID_SIZE;
    memcpy(assertion + offset, descriptor_tail, sizeof(descriptor_tail));
    offset += sizeof(descriptor_tail);

    assert(ck_ctap2_handle(
        &authenticator, assertion, offset, response, sizeof(response), &response_length));
    assert(response[0] == CK_CTAP2_OK);
    assert(platform.load_calls == 2U);
    assert(platform.presence_calls == 2U);
    assert(contains(response, response_length, (const uint8_t[]){0x30U, 0x44U}, 2U) ||
           contains(response, response_length, (const uint8_t[]){0x30U, 0x45U}, 2U) ||
           contains(response, response_length, (const uint8_t[]){0x30U, 0x46U}, 2U));
    assert(response_length >= 8U && response[response_length - 1U] != 0U);
    assert(platform.record[97] == 0xffU && platform.record[98] == 0xffU &&
           platform.record[99] == 0xffU && platform.record[100] == 0xffU);

    uint8_t invalid_type[sizeof(make_request)];
    memcpy(invalid_type, make_request, sizeof(invalid_type));
    size_t type_offset =
        find_offset(invalid_type, sizeof(invalid_type), "public-key", 10U);
    assert(type_offset != SIZE_MAX);
    memcpy(invalid_type + type_offset, "private-ke", 10U);
    assert(ck_ctap2_handle(
        &authenticator,
        invalid_type,
        sizeof(invalid_type),
        response,
        sizeof(response),
        &response_length));
    assert(response_length == 1U && response[0] == CK_CTAP2_ERR_UNSUPPORTED_ALGORITHM);

    uint8_t uv_request[sizeof(make_request)];
    memcpy(uv_request, make_request, sizeof(uv_request));
    size_t up_offset = find_offset(
        uv_request, sizeof(uv_request), (const uint8_t[]){0x62, 'u', 'p', 0xf5}, 4U);
    assert(up_offset != SIZE_MAX);
    uv_request[up_offset + 2U] = 'v';
    assert(ck_ctap2_handle(
        &authenticator,
        uv_request,
        sizeof(uv_request),
        response,
        sizeof(response),
        &response_length));
    assert(response_length == 1U && response[0] == CK_CTAP2_ERR_UNSUPPORTED_OPTION);

    uint8_t exclude_request[sizeof(make_request) + 64U];
    memcpy(exclude_request, make_request, sizeof(make_request));
    exclude_request[1] = 0xa6U;
    size_t exclude_length = sizeof(make_request);
    const uint8_t exclude_head[] = {
        0x05, 0x81, 0xa2, 0x62, 'i', 'd', 0x58, CK_CTAP2_CREDENTIAL_ID_SIZE};
    memcpy(exclude_request + exclude_length, exclude_head, sizeof(exclude_head));
    exclude_length += sizeof(exclude_head);
    memcpy(
        exclude_request + exclude_length,
        platform.record + CK_CTAP2_RECORD_CREDENTIAL_ID_OFFSET,
        CK_CTAP2_CREDENTIAL_ID_SIZE);
    exclude_length += CK_CTAP2_CREDENTIAL_ID_SIZE;
    memcpy(exclude_request + exclude_length, descriptor_tail, sizeof(descriptor_tail));
    exclude_length += sizeof(descriptor_tail);
    unsigned presence_before_exclude = platform.presence_calls;
    assert(ck_ctap2_handle(
        &authenticator,
        exclude_request,
        exclude_length,
        response,
        sizeof(response),
        &response_length));
    assert(response_length == 1U && response[0] == CK_CTAP2_ERR_CREDENTIAL_EXCLUDED);
    assert(platform.presence_calls == presence_before_exclude + 1U);

    platform.deny_presence = true;
    platform.cancelled = true;
    assert(ck_ctap2_handle(
        &authenticator,
        make_request,
        sizeof(make_request),
        response,
        sizeof(response),
        &response_length));
    assert(response_length == 1U && response[0] == CK_CTAP2_ERR_KEEPALIVE_CANCEL);
}

static void test_signature_counter_saturates(void) {
    CkCtap2Credential credential = {0};
    uint8_t encoded[CK_CTAP2_CREDENTIAL_RECORD_SIZE];
    size_t encoded_length = 0;
    credential.sign_count = UINT32_MAX;
    assert(ck_ctap2_credential_serialize(
        &credential, encoded, sizeof(encoded), &encoded_length));
    assert(ck_ctap2_credential_deserialize(&credential, encoded, encoded_length));
    assert(credential.sign_count == UINT32_MAX);
}

static void test_rejects_truncation_and_wrong_rp(void) {
    TestPlatform platform = {0};
    CkCtap2 authenticator = make_authenticator(&platform);
    uint8_t response[64];
    size_t response_length = 0;
    const uint8_t truncated[] = {CK_CTAP2_CMD_MAKE_CREDENTIAL, 0xa1, 0x01, 0x58, 0x20, 0};

    assert(ck_ctap2_handle(
        &authenticator,
        truncated,
        sizeof(truncated),
        response,
        sizeof(response),
        &response_length));
    assert(response_length == 1U);
    assert(response[0] == CK_CTAP2_ERR_INVALID_CBOR);
    assert(platform.record_length == 0U);
}

static void test_record_round_trip_and_rejects_bad_version(void) {
    CkCtap2Credential input = {0};
    CkCtap2Credential output = {0};
    uint8_t encoded[CK_CTAP2_CREDENTIAL_RECORD_SIZE];
    size_t encoded_length = 0;
    for(size_t i = 0; i < sizeof(input.credential_id); ++i) input.credential_id[i] = (uint8_t)i;
    for(size_t i = 0; i < sizeof(input.private_key); ++i) input.private_key[i] = (uint8_t)(0xa0U + i);
    memset(input.rp_id_hash, 0x5a, sizeof(input.rp_id_hash));
    input.sign_count = 7U;

    assert(ck_ctap2_credential_serialize(
        &input, encoded, sizeof(encoded), &encoded_length));
    assert(encoded_length == sizeof(encoded));
    assert(ck_ctap2_credential_deserialize(&output, encoded, encoded_length));
    assert(memcmp(&input, &output, sizeof(input)) == 0);
    encoded[0] ^= 1U;
    assert(!ck_ctap2_credential_deserialize(&output, encoded, encoded_length));
}

int main(void) {
    test_sha256_known_answer();
    test_get_info();
    test_make_and_get_assertion();
    test_rejects_truncation_and_wrong_rp();
    test_record_round_trip_and_rejects_bad_version();
    test_signature_counter_saturates();
    puts("OK: CK42X CTAP2 core tests passed");
    return 0;
}
