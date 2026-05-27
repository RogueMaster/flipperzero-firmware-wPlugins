static void test_get_assertion_parse_treats_empty_allow_list_as_omitted(void) {
    uint8_t buffer[128];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init getAssertion encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 0),
           "encode getAssertion request with empty allow list");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "empty allow list should be accepted");
    expect(!zf_ctap_request_uses_allow_list(&request),
           "empty allow list should be treated as omitted");
}

static void test_get_assertion_parse_skips_unknown_float_member(void) {
    uint8_t buffer[128];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t float32_one[] = {0xFA, 0x3F, 0x80, 0x00, 0x00};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init float extension encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 99),
           "encode getAssertion request with unknown float key");
    expect(enc.offset + sizeof(float32_one) <= enc.capacity, "float extension should fit");
    memcpy(enc.buf + enc.offset, float32_one, sizeof(float32_one));
    enc.offset += sizeof(float32_one);

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "unknown float-valued members should be skipped cleanly");
    expect(strcmp(request.assertion.rp_id, "example.com") == 0,
           "parser should preserve required fields");
}

static void test_get_assertion_parse_rejects_duplicate_top_level_key(void) {
    uint8_t buffer[160];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init duplicate getAssertion encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "duplicate.example") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)),
           "encode getAssertion request with duplicate top-level key");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "duplicate getAssertion top-level keys should be rejected");
}

static void test_get_assertion_parse_rejects_duplicate_option_key(void) {
    uint8_t buffer[160];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init duplicate options encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "up") && zf_cbor_encode_bool(&enc, true) &&
               zf_cbor_encode_text(&enc, "up") && zf_cbor_encode_bool(&enc, false),
           "encode getAssertion request with duplicate option key");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "duplicate options keys should be rejected");
}

static void test_get_assertion_parse_ignores_unknown_true_option(void) {
    uint8_t buffer[160];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init unknown true option encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "unknownOption") && zf_cbor_encode_bool(&enc, true),
           "encode getAssertion request with unknown true option");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "unknown true options should be ignored");
}

static void test_get_assertion_parse_ignores_unknown_false_option(void) {
    uint8_t buffer[160];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init unknown false option encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "unknownOption") && zf_cbor_encode_bool(&enc, false),
           "encode getAssertion request with unknown false option");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "unknown false options should be ignored");
}

static void test_get_assertion_parse_rejects_zero_length_descriptor_id(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init zero-length descriptor encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, (const uint8_t *)"", 0) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode zero-length descriptor id");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "zero-length descriptor id should be rejected");
}

static void test_get_assertion_parse_accepts_oversized_descriptor_id(void) {
    uint8_t buffer[512];
    uint8_t oversized_id[ZF_CREDENTIAL_ID_LEN + 1];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    memset(oversized_id, 0xAB, sizeof(oversized_id));
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init oversized descriptor encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, oversized_id, sizeof(oversized_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode oversized descriptor id");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "oversized descriptor id should be accepted");
    expect(request.allow_list.count == 1, "oversized descriptor id should be counted");
    expect(zf_ctap_request_uses_allow_list(&request),
           "oversized public-key descriptor should still make an allow-list request");
    expect(!zf_ctap_descriptor_list_contains_id(&request.allow_list, oversized_id,
                                                ZF_CREDENTIAL_ID_LEN),
           "oversized descriptor id should not match by truncated bytes");
}

static void test_get_assertion_parse_rejects_duplicate_descriptor_type_after_invalid_value(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t credential_id[16] = {0xAB};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init duplicate descriptor type encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id, sizeof(credential_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "bogus") &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode getAssertion request with rescuable duplicate descriptor type");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "duplicate descriptor type should be rejected even if a later value is valid");
}

static void test_get_assertion_parse_rejects_embedded_nul_rp_id(void) {
    uint8_t buffer[128];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const char rp_id_with_nul[] = {'e', 'x', 'a', 'm', 'p',  'l', 'e',
                                          '.', 'c', 'o', 'm', '\0', 'x'};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init nul rpId encoder");
    expect(zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text_n(&enc, rp_id_with_nul, sizeof(rp_id_with_nul)) &&
               zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)),
           "encode getAssertion request with embedded nul rpId");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "embedded nul rpId should be rejected");
}

static void test_get_assertion_parse_rejects_invalid_utf8_rp_id(void) {
    uint8_t buffer[128];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t invalid_utf8[] = {0xC3, 0x28};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init invalid utf8 rpId encoder");
    expect(zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, invalid_utf8, sizeof(invalid_utf8)) &&
               zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)),
           "encode getAssertion request with invalid utf8 rpId");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "invalid utf8 rpId should be rejected");
}

static void test_get_assertion_parse_accepts_rk_false_option(void) {
    uint8_t buffer[128];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init rk option encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "rk") && zf_cbor_encode_bool(&enc, false),
           "encode getAssertion request with rk option");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "GetAssertion rk=false option should be ignored");
}

static void test_get_assertion_parse_rejects_rk_true_option_with_unsupported_option(void) {
    uint8_t buffer[128];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init rk=true option encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "rk") && zf_cbor_encode_bool(&enc, true),
           "encode getAssertion request with rk=true option");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_UNSUPPORTED_OPTION,
           "GetAssertion rk=true option should be rejected as unsupported option");
}

static void test_make_credential_parse_skips_unknown_simple_value(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init makeCredential encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 99) &&
               zf_cbor_encode_bool(&enc, true),
           "encode makeCredential request with unknown bool");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "unknown simple value should be skipped");
}

static void test_make_credential_parse_ignores_unknown_true_option(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init makeCredential unknown option encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "makeTea") &&
               zf_cbor_encode_bool(&enc, true),
           "encode makeCredential request with unknown true option");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "unknown true options should be ignored");
    expect(!request.has_up && !request.has_uv && !request.has_rk,
           "unknown option should not set any known option flags");
}

static void test_make_credential_parse_rejects_duplicate_user_name(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init duplicate user-name encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 3) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "mallory") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key"),
           "encode makeCredential request with duplicate user name");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "duplicate user-name keys should be rejected");
}

static void test_make_credential_parse_accepts_64_byte_display_name(void) {
    uint8_t buffer[512];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};
    static const char display_name[] =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

    expect(sizeof(display_name) - 1 == 64, "displayName fixture should be exactly 64 bytes");
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init exact-64-byte displayName encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "displayName") &&
               zf_cbor_encode_text_n(&enc, display_name, sizeof(display_name) - 1) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key"),
           "encode makeCredential request with exact-64-byte displayName");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "exact-64-byte displayName should be accepted");
    expect(strcmp(request.user_display_name, display_name) == 0,
           "exact-64-byte displayName should round-trip");
}

static void test_make_credential_parse_rejects_non_text_rp_name(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init non-text rp.name encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key"),
           "encode makeCredential request with non-text rp.name");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "non-text rp.name should be rejected");
}

static void test_make_credential_parse_rejects_non_text_rp_icon(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init non-text rp.icon encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_text(&enc, "icon") && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key"),
           "encode makeCredential request with non-text rp.icon");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "non-text rp.icon should be rejected");
}

static void test_make_credential_parse_rejects_non_text_user_icon(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init non-text user.icon encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "icon") && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key"),
           "encode makeCredential request with non-text user.icon");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "non-text user.icon should be rejected");
}

static void test_get_assertion_parse_accepts_253_byte_rp_id(void) {
    uint8_t buffer[512];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const char rp_id[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
                                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb."
                                "ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc."
                                "ddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

    expect(sizeof(rp_id) - 1 == 253, "rpId fixture should be exactly 253 bytes");
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init long rpId encoder");
    expect(zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text_n(&enc, rp_id, sizeof(rp_id) - 1) &&
               zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)),
           "encode getAssertion request with 253-byte rpId");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "253-byte rpId should be accepted");
    expect(strcmp(request.assertion.rp_id, rp_id) == 0, "253-byte rpId should round-trip");
}

static void test_make_credential_parse_rejects_duplicate_exclude_descriptors(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0x01};
    static const uint8_t credential_id[] = {0xAA, 0xBB, 0xCC, 0xDD};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init duplicate makeCredential excludeList encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 5) &&
               zf_cbor_encode_array(&enc, 2) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id, sizeof(credential_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key") &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id, sizeof(credential_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode makeCredential request with duplicate excludeList descriptors");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_PARAMETER,
           "duplicate excludeList descriptors should be rejected");
}

static void test_get_assertion_parse_rejects_duplicate_allow_list_descriptors(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t credential_id[] = {0xAA, 0xBB, 0xCC, 0xDD};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init duplicate getAssertion allowList encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 2) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id, sizeof(credential_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key") &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id, sizeof(credential_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode getAssertion request with duplicate allowList descriptors");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_PARAMETER,
           "duplicate allowList descriptors should be rejected");
}

static void test_get_assertion_parse_rejects_too_many_allow_list_descriptors(void) {
    uint8_t buffer[1024];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init oversized getAssertion allowList encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, ZF_MAX_ALLOW_LIST + 1U),
           "encode oversized getAssertion allowList header");
    for (size_t i = 0; i <= ZF_MAX_ALLOW_LIST; ++i) {
        uint8_t credential_id[] = {(uint8_t)(i + 1U)};
        expect(zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
                   zf_cbor_encode_bytes(&enc, credential_id, sizeof(credential_id)) &&
                   zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
               "encode allowList descriptor");
    }

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_PARAMETER,
           "too many effective allowList descriptors should be rejected");
}

static void test_get_assertion_parse_rejects_duplicate_oversized_allow_list_descriptors(void) {
    uint8_t buffer[512];
    uint8_t oversized_id[ZF_CREDENTIAL_ID_LEN + 1];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    memset(oversized_id, 0xBC, sizeof(oversized_id));
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init duplicate oversized allowList encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 2) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, oversized_id, sizeof(oversized_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key") &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, oversized_id, sizeof(oversized_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode duplicate oversized allowList descriptors");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_PARAMETER,
           "duplicate oversized allowList descriptors should be rejected during insertion");
}

static void test_assertion_response_user_fields_follow_uv(void) {
    ZfGetAssertionRequest request = {0};
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    ZfCredentialRecord record = {0};
    ZfAssertionResponseScratch scratch = {0};
    uint8_t response[256];
    size_t response_len = 0;

    strcpy(request.assertion.rp_id, "example.com");
    memset(request.assertion.client_data_hash, 0x5A, sizeof(request.assertion.client_data_hash));
    memset(record.credential_id, 0x10, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    memcpy(record.user_id, "user-1", 6);
    record.user_id_len = 6;
    strcpy(record.user_name, "alice");
    strcpy(record.user_display_name, "Alice Example");

    expect(zf_ctap_build_assertion_response_with_scratch(
               &scratch, &request.assertion, &record, true, true, 8, true, true, 2, false, false,
               NULL, 0, response, sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
           "uv assertion response should build");
    expect(contains_text(response, response_len, "name"), "uv response should include name key");
    expect(contains_text(response, response_len, "displayName"),
           "uv response should include displayName key");
    expect(contains_text(response, response_len, "alice"), "uv response should include user name");
    expect(contains_text(response, response_len, "Alice Example"),
           "uv response should include display name value");

    expect(zf_ctap_build_assertion_response_with_scratch(
               &scratch, &request.assertion, &record, true, false, 9, false, false, 1, false, false,
               NULL, 0, response, sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
           "non-uv assertion response should build");
    expect(!contains_text(response, response_len, "displayName"),
           "non-uv response omits displayName");
    expect(!contains_text(response, response_len, "alice"), "non-uv response omits user name");
}

static void test_client_pin_parse_rejects_trailing_bytes(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t buffer[16];
    uint8_t response[64];
    size_t response_len = 0;
    ZfCborEncoder enc;

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init clientPin retries encoder");
    expect(zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_uint(&enc, 1),
           "encode clientPin getRetries request");
    buffer[zf_cbor_encoder_size(&enc)] = 0xFF;

    expect(zerofido_pin_handle_command(&app, buffer, zf_cbor_encoder_size(&enc) + 1, response,
                                       sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_CBOR,
           "clientPin request should reject trailing bytes");
}

static void test_client_pin_get_retries_accepts_missing_pin_protocol(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t buffer[16];
    uint8_t response[64];
    size_t response_len = 0;
    ZfCborEncoder enc;
    ZfCborCursor cursor;
    size_t pairs = 0;
    uint64_t key = 0;
    uint64_t retries = 0;

    test_storage_reset();
    app.pin_state.pin_retries = 7;

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init clientPin getRetries encoder");
    expect(zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_uint(&enc, ZF_CLIENT_PIN_SUBCMD_GET_RETRIES),
           "encode clientPin getRetries without pinProtocol");

    expect(zerofido_pin_handle_command(&app, buffer, zf_cbor_encoder_size(&enc), response,
                                       sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
           "clientPin getRetries should not require pinProtocol");
    zf_cbor_cursor_init(&cursor, response, response_len);
    expect(zf_cbor_read_map_start(&cursor, &pairs) && pairs == 1, "decode retries response map");
    expect(zf_cbor_read_uint(&cursor, &key) && key == 3, "decode retries response key");
    expect(zf_cbor_read_uint(&cursor, &retries) && retries == 7, "decode retries value");
    expect(cursor.ptr == cursor.end, "retries response should not have trailing bytes");
}

static void test_client_pin_get_key_agreement_requires_pin_protocol(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t buffer[16];
    uint8_t response[64];
    size_t response_len = 0;
    ZfCborEncoder enc;

    test_storage_reset();
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init clientPin getKeyAgreement missing protocol encoder");
    expect(zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_uint(&enc, ZF_CLIENT_PIN_SUBCMD_GET_KEY_AGREEMENT),
           "encode clientPin getKeyAgreement without pinProtocol");

    expect(zerofido_pin_handle_command(&app, buffer, zf_cbor_encoder_size(&enc), response,
                                       sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_MISSING_PARAMETER,
           "clientPin getKeyAgreement should still require pinProtocol");
}

static void test_client_pin_parse_rejects_duplicate_top_level_keys(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t buffer[32];
    uint8_t response[64];
    size_t response_len = 0;
    ZfCborEncoder enc;

    test_storage_reset();
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init duplicate clientPin key encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_uint(&enc, ZF_CLIENT_PIN_SUBCMD_GET_RETRIES) &&
               zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_uint(&enc, ZF_CLIENT_PIN_SUBCMD_GET_KEY_AGREEMENT),
           "encode duplicate clientPin subCommand key");

    expect(zerofido_pin_handle_command(&app, buffer, zf_cbor_encoder_size(&enc), response,
                                       sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_CBOR,
           "clientPin should reject duplicate top-level keys");
}

static void test_client_pin_parse_rejects_duplicate_key_agreement_keys(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t buffer[256];
    uint8_t response[64];
    size_t response_len = 0;
    ZfCborEncoder enc;

    test_storage_reset();
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init duplicate keyAgreement encoder");
    expect(zf_cbor_encode_map(&enc, 4), "encode clientPin top-level map");
    expect(zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, 1), "encode pinProtocol");
    expect(zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_uint(&enc, ZF_CLIENT_PIN_SUBCMD_GET_PIN_TOKEN),
           "encode subCommand");
    expect(zf_cbor_encode_uint(&enc, 3), "encode keyAgreement key");
    expect(zf_cbor_encode_map(&enc, 6), "encode malformed keyAgreement map");
    expect(zf_cbor_encode_int(&enc, 1) && zf_cbor_encode_int(&enc, 2), "encode kty");
    expect(zf_cbor_encode_int(&enc, 3) && zf_cbor_encode_int(&enc, -25), "encode alg");
    expect(zf_cbor_encode_int(&enc, -1) && zf_cbor_encode_int(&enc, 1), "encode crv");
    expect(
        zf_cbor_encode_int(&enc, -2) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x11}, ZF_PUBLIC_KEY_LEN),
        "encode first x");
    expect(
        zf_cbor_encode_int(&enc, -2) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x12}, ZF_PUBLIC_KEY_LEN),
        "encode duplicate x");
    expect(
        zf_cbor_encode_int(&enc, -3) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x22}, ZF_PUBLIC_KEY_LEN),
        "encode y");
    expect(zf_cbor_encode_uint(&enc, 6) &&
               zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PIN_HASH_LEN]){0x33}, ZF_PIN_HASH_LEN),
           "encode pinHashEnc");

    expect(zerofido_pin_handle_command(&app, buffer, zf_cbor_encoder_size(&enc), response,
                                       sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_CBOR,
           "clientPin should reject duplicate keyAgreement keys");
}

static size_t encode_client_pin_get_pin_token_request_with_protocol(
    uint8_t *buffer, size_t capacity, uint64_t pin_protocol, const uint8_t *pin_hash_enc,
    size_t pin_hash_enc_len, bool include_alg) {
    ZfCborEncoder enc;

    expect(zf_cbor_encoder_init(&enc, buffer, capacity), "init clientPin getPinToken encoder");
    expect(zf_cbor_encode_map(&enc, 4), "encode top-level clientPin map");
    expect(zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, pin_protocol),
           "encode pinProtocol");
    expect(zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_uint(&enc, 5), "encode subcommand");
    expect(zf_cbor_encode_uint(&enc, 3), "encode keyAgreement key");
    expect(zf_cbor_encode_map(&enc, include_alg ? 5 : 4), "encode keyAgreement map");
    expect(zf_cbor_encode_int(&enc, 1) && zf_cbor_encode_int(&enc, 2), "encode keyAgreement kty");
    if (include_alg) {
        expect(zf_cbor_encode_int(&enc, 3) && zf_cbor_encode_int(&enc, -25),
               "encode keyAgreement alg");
    }
    expect(zf_cbor_encode_int(&enc, -1) && zf_cbor_encode_int(&enc, 1), "encode keyAgreement crv");
    expect(
        zf_cbor_encode_int(&enc, -2) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x11}, ZF_PUBLIC_KEY_LEN),
        "encode keyAgreement x");
    expect(
        zf_cbor_encode_int(&enc, -3) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x22}, ZF_PUBLIC_KEY_LEN),
        "encode keyAgreement y");
    expect(zf_cbor_encode_uint(&enc, 6) &&
               zf_cbor_encode_bytes(&enc, pin_hash_enc, pin_hash_enc_len),
           "encode pinHashEnc");
    return zf_cbor_encoder_size(&enc);
}

static size_t encode_client_pin_get_pin_token_request(uint8_t *buffer, size_t capacity,
                                                      const uint8_t pin_hash[ZF_PIN_HASH_LEN],
                                                      bool include_alg) {
    return encode_client_pin_get_pin_token_request_with_protocol(
        buffer, capacity, ZF_PIN_PROTOCOL_V1, pin_hash, ZF_PIN_HASH_LEN, include_alg);
}

static size_t encode_client_pin_request_with_permissions(uint8_t *buffer, size_t capacity,
                                                         uint64_t subcommand,
                                                         const uint8_t pin_hash[ZF_PIN_HASH_LEN],
                                                         bool include_permissions,
                                                         uint64_t permissions, const char *rp_id) {
    ZfCborEncoder enc;
    size_t map_size = 4;

    if (include_permissions) {
        map_size++;
    }
    if (rp_id) {
        map_size++;
    }

    expect(zf_cbor_encoder_init(&enc, buffer, capacity), "init clientPin permissions encoder");
    expect(zf_cbor_encode_map(&enc, map_size), "encode permissions top-level clientPin map");
    expect(zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, 1), "encode pinProtocol");
    expect(zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_uint(&enc, subcommand),
           "encode permissions subcommand");
    expect(zf_cbor_encode_uint(&enc, 3), "encode permissions keyAgreement key");
    expect(zf_cbor_encode_map(&enc, 5), "encode permissions keyAgreement map");
    expect(zf_cbor_encode_int(&enc, 1) && zf_cbor_encode_int(&enc, 2), "encode permissions kty");
    expect(zf_cbor_encode_int(&enc, 3) && zf_cbor_encode_int(&enc, -25), "encode permissions alg");
    expect(zf_cbor_encode_int(&enc, -1) && zf_cbor_encode_int(&enc, 1), "encode permissions crv");
    expect(
        zf_cbor_encode_int(&enc, -2) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x11}, ZF_PUBLIC_KEY_LEN),
        "encode permissions x");
    expect(
        zf_cbor_encode_int(&enc, -3) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x22}, ZF_PUBLIC_KEY_LEN),
        "encode permissions y");
    expect(zf_cbor_encode_uint(&enc, 6) && zf_cbor_encode_bytes(&enc, pin_hash, ZF_PIN_HASH_LEN),
           "encode permissions pinHashEnc");
    if (include_permissions) {
        expect(zf_cbor_encode_uint(&enc, 9) && zf_cbor_encode_uint(&enc, permissions),
               "encode permissions field");
    }
    if (rp_id) {
        expect(zf_cbor_encode_uint(&enc, 10) && zf_cbor_encode_text(&enc, rp_id),
               "encode rpId field");
    }
    return zf_cbor_encoder_size(&enc);
}

static size_t encode_make_credential_ctap_request_with_pin_auth(
    uint8_t *buffer, size_t capacity, bool include_uv, bool uv_value, bool include_pin_auth,
    const uint8_t *pin_auth, size_t pin_auth_len, bool include_pin_protocol, bool include_exclude,
    const uint8_t *exclude_id, size_t exclude_len);

static void test_client_pin_key_agreement_requires_alg(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t buffer[256];
    uint8_t response[64];
    size_t response_len = 0;
    static const uint8_t wrong_pin_hash[ZF_PIN_HASH_LEN] = {
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    };

    expect(
        zerofido_pin_handle_command(
            &app, buffer,
            encode_client_pin_get_pin_token_request(buffer, sizeof(buffer), wrong_pin_hash, false),
            response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_CBOR,
        "clientPin keyAgreement should reject missing alg");
}

static void test_make_credential_parse_rejects_malformed_pubkey_cred_params(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init malformed pubKeyCredParams encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 2) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key") &&
               zf_cbor_encode_text(&enc, "alg") && zf_cbor_encode_int(&enc, -7),
           "encode malformed pubKeyCredParams request");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "malformed pubKeyCredParams should be rejected");
}

static void
test_make_credential_parse_rejects_non_public_key_pubkey_cred_param_as_unsupported_algorithm(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0xAA};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init unsupported pubKeyCredParams type encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "bogus-key"),
           "encode unsupported pubKeyCredParams type request");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_UNSUPPORTED_ALGORITHM,
           "non-public-key pubKeyCredParams type should map to unsupported algorithm");
}

static void test_make_credential_parse_rejects_zero_length_exclude_id(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0xAA};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init zero-length exclude id encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 5) &&
               zf_cbor_encode_array(&enc, 1) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, (const uint8_t *)"", 0) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode zero-length exclude id");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_CBOR,
           "zero-length exclude id should be rejected");
}

static void test_make_credential_parse_accepts_oversized_exclude_id(void) {
    uint8_t buffer[512];
    uint8_t oversized_id[ZF_CREDENTIAL_ID_LEN + 1];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0xAA};

    memset(oversized_id, 0xCD, sizeof(oversized_id));
    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init oversized exclude id encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 5) &&
               zf_cbor_encode_array(&enc, 1) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, oversized_id, sizeof(oversized_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode oversized exclude id");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "oversized exclude id should be accepted");
    expect(request.exclude_list.count == 1, "oversized exclude id should be counted");
    expect(!zf_ctap_descriptor_list_contains_id(&request.exclude_list, oversized_id,
                                                ZF_CREDENTIAL_ID_LEN),
           "oversized exclude id should not match by truncated bytes");
}

static void test_make_credential_parse_ignores_non_public_key_exclude_descriptor(void) {
    uint8_t buffer[512];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t user_id[] = {0xAA};
    static const uint8_t exclude_id[] = {0x10, 0x20, 0x30};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init non-public-key exclude descriptor encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 5) &&
               zf_cbor_encode_array(&enc, 1) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, exclude_id, sizeof(exclude_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "bogus-key"),
           "encode excludeList descriptor with non-public-key type");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "excludeList entries with non-public-key type should be ignored");
    expect(request.exclude_list.count == 0,
           "non-public-key exclude descriptors should not populate the parsed list");
}

static void test_make_credential_parse_rejects_empty_user_id(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfMakeCredentialRequest request;
    TEST_INIT_MAKE_CREDENTIAL_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init empty user id encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, (const uint8_t *)"", 0) && zf_cbor_encode_uint(&enc, 4) &&
               zf_cbor_encode_array(&enc, 1) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "alg") && zf_cbor_encode_int(&enc, -7) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode empty user id request");

    expect(zf_ctap_parse_make_credential(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_ERR_INVALID_PARAMETER,
           "empty user id should be rejected");
}

static void test_get_assertion_parse_ignores_non_public_key_allow_list_descriptor(void) {
    uint8_t buffer[256];
    ZfCborEncoder enc;
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    static const uint8_t client_data_hash[32] = {0};
    static const uint8_t credential_id[] = {0xAB};

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init non-public-key allowList descriptor encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id, sizeof(credential_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "bogus-key"),
           "encode allowList descriptor with non-public-key type");

    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "allowList entries with non-public-key type should be ignored");
    expect(request.allow_list.count == 0,
           "non-public-key allowList descriptors should not populate the parsed list");
}

static void test_store_find_by_rp_orders_records_by_newest_first(void) {
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    store.records = store_records;
    uint16_t matches[ZF_MAX_CREDENTIALS] = {0};

    store.count = 3;
    for (size_t i = 0; i < store.count; ++i) {
        store.records[i].in_use = true;
        store.records[i].resident_key = true;
        strcpy(store.records[i].rp_id, "example.com");
        memset(store.records[i].credential_id, (int)(0x10 + i), ZF_CREDENTIAL_ID_LEN);
        store.records[i].credential_id_len = ZF_CREDENTIAL_ID_LEN;
    }
    store.records[0].created_at = 10;
    store.records[1].created_at = 30;
    store.records[2].created_at = 20;

    expect(zf_store_find_by_rp(NULL, &store, "example.com", matches, ZF_MAX_CREDENTIALS) == 3,
           "should find all resident credentials for rp");
    expect(matches[0] == 1, "newest credential should be first");
    expect(matches[1] == 2, "second-newest credential should be second");
    expect(matches[2] == 0, "oldest credential should be last");
}

static void test_store_descriptor_list_ignores_oversized_descriptor_ids(void) {
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    ZfGetAssertionRequest request;
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    store.records = store_records;
    uint16_t matches[ZF_MAX_CREDENTIALS] = {0};
    uint8_t buffer[512];
    uint8_t oversized_id[ZF_CREDENTIAL_ID_LEN + 1];
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {0};

    memset(oversized_id, 0xAB, sizeof(oversized_id));
    store.count = 1;
    store.records[0].in_use = true;
    strcpy(store.records[0].rp_id, "example.com");
    store.records[0].credential_id_len = ZF_CREDENTIAL_ID_LEN;
    memset(store.records[0].credential_id, 0xAB, ZF_CREDENTIAL_ID_LEN);

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)),
           "init oversized store allowList encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, oversized_id, sizeof(oversized_id)) &&
               zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
           "encode oversized store allowList descriptor");
    expect(zf_ctap_parse_get_assertion(buffer, zf_cbor_encoder_size(&enc), &request) ==
               ZF_CTAP_SUCCESS,
           "parse oversized store allowList descriptor");

    expect(zf_store_find_by_rp_filtered(NULL, &store, request.assertion.rp_id,
                                        zf_ctap_store_entry_matches_descriptor_list,
                                        &request.allow_list, matches, ZF_MAX_CREDENTIALS) == 0,
           "oversized descriptor ids should be ignored rather than matched");
}

static size_t encode_get_assertion_ctap_request(uint8_t *buffer, size_t capacity) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    buffer[0] = ZfCtapeCmdGetAssertion;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1), "init CTAP getAssertion encoder");
    expect(zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)),
           "encode CTAP getAssertion request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_get_assertion_ctap_request_with_up(uint8_t *buffer, size_t capacity, bool up) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    buffer[0] = ZfCtapeCmdGetAssertion;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP getAssertion up option encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "up") && zf_cbor_encode_bool(&enc, up),
           "encode CTAP getAssertion up option request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_get_assertion_ctap_request_with_allow_list(uint8_t *buffer, size_t capacity,
                                                                const uint8_t *credential_id,
                                                                size_t credential_id_len) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    buffer[0] = ZfCtapeCmdGetAssertion;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP getAssertion allowList encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id, credential_id_len),
           "encode CTAP getAssertion allowList request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_get_assertion_ctap_request_with_two_allow_list_entries(
    uint8_t *buffer, size_t capacity, const uint8_t *credential_id_a, size_t credential_id_len_a,
    const uint8_t *credential_id_b, size_t credential_id_len_b) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    buffer[0] = ZfCtapeCmdGetAssertion;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP getAssertion two-entry allowList encoder");
    expect(zf_cbor_encode_map(&enc, 3) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_array(&enc, 2) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id_a, credential_id_len_a) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, credential_id_b, credential_id_len_b),
           "encode CTAP getAssertion two-entry allowList request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static void seed_assertion_queue_record_full(ZfCredentialRecord *record, uint8_t id_byte,
                                             uint32_t sign_count);
static void seed_assertion_queue_record_index(ZfCredentialIndexEntry *record, uint8_t id_byte,
                                              uint32_t sign_count);
#define seed_assertion_queue_record(record, id_byte, sign_count)                                   \
    _Generic((record),                                                                             \
        ZfCredentialRecord *: seed_assertion_queue_record_full,                                    \
        ZfCredentialIndexEntry *: seed_assertion_queue_record_index)(record, id_byte, sign_count)

static size_t encode_get_assertion_ctap_request_with_pin_auth(
    uint8_t *buffer, size_t capacity, bool include_uv, bool uv_value, bool include_pin_auth,
    const uint8_t *pin_auth, size_t pin_auth_len, bool include_pin_protocol) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };
    size_t pair_count = 2;

    if (include_uv) {
        pair_count++;
    }
    if (include_pin_auth) {
        pair_count++;
    }
    if (include_pin_protocol) {
        pair_count++;
    }

    buffer[0] = ZfCtapeCmdGetAssertion;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP getAssertion pinAuth encoder");
    expect(zf_cbor_encode_map(&enc, pair_count) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_text(&enc, "example.com") && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)),
           "encode CTAP getAssertion pinAuth request");
    if (include_uv) {
        expect(zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_map(&enc, 1) &&
                   zf_cbor_encode_text(&enc, "uv") && zf_cbor_encode_bool(&enc, uv_value),
               "encode getAssertion uv option");
    }
    if (include_pin_auth) {
        expect(zf_cbor_encode_uint(&enc, 6) && zf_cbor_encode_bytes(&enc, pin_auth, pin_auth_len),
               "encode getAssertion pinAuth");
    }
    if (include_pin_protocol) {
        expect(zf_cbor_encode_uint(&enc, 7) && zf_cbor_encode_uint(&enc, 1),
               "encode getAssertion pinProtocol");
    }
    return zf_cbor_encoder_size(&enc) + 1;
}
