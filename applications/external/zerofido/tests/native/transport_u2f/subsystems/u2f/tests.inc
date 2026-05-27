static void test_short_u2f_version_request_is_accepted(void) {
    uint8_t request[] = {0x00, U2F_CMD_VERSION, 0x00, 0x00, 0x00};

    expect(u2f_validate_request(request, sizeof(request)) == 0,
           "5-byte short U2F VERSION APDUs must validate");
}

static void test_header_only_u2f_version_request_is_accepted_for_stock_compatibility(void) {
    uint8_t request[] = {0x00, U2F_CMD_VERSION, 0x00, 0x00};

    expect(u2f_validate_request(request, sizeof(request)) == 0,
           "4-byte header-only U2F VERSION APDUs must validate for stock compatibility");
}

static void test_u2f_version_accepts_exact_response_buffer(void) {
    U2fData u2f = {0};
    uint8_t request[8] = {0x00, U2F_CMD_VERSION, 0x00, 0x00, 0x00};

    u2f.ready = true;
    expect(u2f_msg_parse(&u2f, request, 5, 8) == 8,
           "U2F VERSION should fit in an exact-size response buffer");
    expect(memcmp(request, "U2F_V2", 6) == 0,
           "U2F VERSION response should include the version string");
    expect(request[6] == 0x90 && request[7] == 0x00,
           "U2F VERSION response should include success status bytes");
}

static void test_u2f_version_accepts_extended_length_encoding(void) {
    U2fData u2f = {0};
    uint8_t request[8] = {0x00, U2F_CMD_VERSION, 0x00, 0x00, 0x00, 0x00, 0x00};

    u2f.ready = true;
    expect(u2f_validate_request(request, 7) == 0,
           "7-byte extended-length U2F VERSION APDUs must validate");
    expect(u2f_msg_parse(&u2f, request, 7, 8) == 8,
           "7-byte extended-length U2F VERSION APDUs should return U2F_V2");
    expect(memcmp(request, "U2F_V2", 6) == 0,
           "extended-length U2F VERSION should return the version string");
    expect(request[6] == 0x90 && request[7] == 0x00,
           "extended-length U2F VERSION should include success status bytes");
}

static void test_u2f_version_extended_length_with_data_returns_wrong_length(void) {
    const uint8_t raw_request[] = {
        0x00, 0x03, 0x00, 0x00, 0x00, 0x28, 0x2c, 0x7c, 0xd7, 0xb3, 0x5e, 0x16,
        0xfa, 0x3c, 0xc2, 0x57, 0xb0, 0x5e, 0xaf, 0xa7, 0xc7, 0xcc, 0x77, 0x92,
        0xf1, 0xef, 0x37, 0xe3, 0x1b, 0x1e, 0x4c, 0x77, 0x38, 0xac, 0x41, 0x08,
        0x44, 0x3b, 0x4a, 0x46, 0x72, 0x2d, 0xee, 0x2b, 0xea, 0x32, 0x00, 0x00,
    };
    uint8_t request[sizeof(raw_request)] = {0};
    uint8_t response[2] = {0};

    memcpy(request, raw_request, sizeof(request));
    expect(u2f_validate_request(request, sizeof(request)) == 2,
           "extended-length U2F VERSION APDU with data must be rejected");
    expect(request[0] == 0x67 && request[1] == 0x00,
           "extended-length U2F VERSION APDU with data must return SW_WRONG_LENGTH");
    memcpy(request, raw_request, sizeof(request));
    expect(u2f_validate_request_into_response(request, sizeof(request), response,
                                              sizeof(response)) == 2,
           "extended-length U2F VERSION APDU with data must encode a two-byte status response");
    expect(response[0] == 0x67 && response[1] == 0x00,
           "extended-length U2F VERSION APDU with data must encode SW_WRONG_LENGTH");
}

static void test_u2f_extended_length_wrap_is_rejected(void) {
    const uint8_t seven_byte_request[] = {
        0x00, U2F_CMD_AUTHENTICATE, U2fCheckOnly, 0x00, 0x00, 0xff, 0xfe,
    };
    const uint8_t eight_byte_request[] = {
        0x00, U2F_CMD_AUTHENTICATE, U2fCheckOnly, 0x00, 0x00, 0xff, 0xff, 0x00,
    };
    U2fParsedApdu apdu = {0};

    expect(!u2f_parse_apdu_header(seven_byte_request, sizeof(seven_byte_request), false, &apdu),
           "extended APDU length 0xfffe must not wrap to a seven-byte request");
    expect(!u2f_parse_apdu_header(eight_byte_request, sizeof(eight_byte_request), false, &apdu),
           "extended APDU length 0xffff must not wrap to an eight-byte request");
}

static void test_u2f_register_allows_nonzero_p1_p2_for_stock_compatibility(void) {
    uint8_t request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = {0};

    request[0] = 0x00;
    request[1] = U2F_CMD_REGISTER;
    request[2] = 0x03;
    request[3] = 0x01;
    request[4] = 0x00;
    request[5] = 0x00;
    request[6] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE;

    expect(u2f_validate_request(request, sizeof(request)) == 0,
           "REGISTER should accept nonzero P1/P2 to match stock U2F compatibility handling");
}

static void test_u2f_dont_enforce_authenticate_mode_is_accepted_without_presence(void) {
    uint8_t request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64] = {0};
    const char *operation = NULL;
    U2fData u2f = {0};
    uint16_t response_len = 0;

    request[0] = 0x00;
    request[1] = U2F_CMD_AUTHENTICATE;
    request[2] = U2fDontEnforce;
    request[3] = 0x00;
    request[4] = 0x00;
    request[5] = 0x00;
    request[6] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64;
    request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = 64;

    u2f.ready = true;
    expect(u2f_validate_request(request, sizeof(request)) == 0,
           "dont-enforce U2F authenticate mode should be accepted");
    expect(!u2f_request_needs_user_presence(request, sizeof(request), &operation),
           "dont-enforce U2F authenticate mode should not request user presence");
    response_len = u2f_msg_parse(&u2f, request, sizeof(request), sizeof(request));
    expect(response_len > sizeof(U2fAuthResp),
           "dont-enforce U2F authenticate mode should produce a signature response");
    expect(request[0] == 0x00,
           "dont-enforce U2F authenticate mode should not set the UP bit without presence");
    expect(request[response_len - 2] == 0x90 && request[response_len - 1] == 0x00,
           "dont-enforce U2F authenticate mode should return success status");
}

static void test_u2f_enforce_authenticate_rejects_invalid_handle_before_presence(void) {
    U2fData u2f = {0};
    uint8_t request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64] = {0};

    u2f.ready = true;
    request[0] = 0x00;
    request[1] = U2F_CMD_AUTHENTICATE;
    request[2] = U2fEnforce;
    request[3] = 0x00;
    request[4] = 0x00;
    request[5] = 0x00;
    request[6] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64;
    request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = 64;
    memset(&request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1], 0xAB, 64);

    expect(u2f_msg_parse(&u2f, request, sizeof(request), sizeof(request)) == 2,
           "invalid-handle U2F AUTHENTICATE should return a status word");
    expect(request[0] == 0x6A && request[1] == 0x80,
           "invalid-handle U2F AUTHENTICATE should return wrong data before user presence");
}

static void test_u2f_invalid_handle_clears_consumed_presence(void) {
    U2fData u2f = {0};
    uint8_t request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64] = {0};

    u2f.ready = true;
    u2f.user_present = true;
    request[0] = 0x00;
    request[1] = U2F_CMD_AUTHENTICATE;
    request[2] = U2fEnforce;
    request[3] = 0x00;
    request[4] = 0x00;
    request[5] = 0x00;
    request[6] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64;
    request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = 64;
    memset(&request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1], 0xAB, 64);

    expect(u2f_msg_parse(&u2f, request, sizeof(request), sizeof(request)) == 2,
           "invalid-handle U2F AUTHENTICATE should return a status word");
    expect(!u2f.user_present,
           "invalid-handle U2F AUTHENTICATE should not leave user presence latched");
}

static void test_u2f_enforce_authenticate_hash_includes_user_presence_flag(void) {
    U2fData u2f = {0};
    uint8_t request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64] = {0};
    uint16_t response_len = 0;

    test_reset();
    u2f.ready = true;
    u2f.user_present = true;
    u2f.counter = 7;
    request[0] = 0x00;
    request[1] = U2F_CMD_AUTHENTICATE;
    request[2] = U2fEnforce;
    request[3] = 0x00;
    request[4] = 0x00;
    request[5] = 0x00;
    request[6] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64;
    request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = 64;

    response_len = u2f_msg_parse(&u2f, request, sizeof(request), sizeof(request));

    expect(response_len > sizeof(U2fAuthResp),
           "enforce authenticate should produce a signature response when presence is available");
    const size_t preimage_len = U2F_APP_ID_SIZE + 1 + sizeof(uint32_t) + U2F_CHALLENGE_SIZE;
    const uint8_t *preimage = g_sha256_trace;

    expect(g_sha256_trace_len >= preimage_len,
           "authenticate signature preimage should cover appId, flags, counter, and challenge");
    preimage = &g_sha256_trace[g_sha256_trace_len - preimage_len];
    expect(preimage[U2F_APP_ID_SIZE] == 0x01,
           "authenticate signature preimage should include the UP flag when presence was consumed");
    expect(request[0] == 0x01, "authenticate response should also expose the UP flag");
}

static void test_u2f_authenticate_reserves_counter_window(void) {
    U2fData u2f = {0};
    uint8_t request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64] = {0};
    uint16_t response_len = 0;

    test_reset();
    u2f.ready = true;
    u2f.counter = 7;
    u2f.counter_high_water = 7;

    u2f.user_present = true;
    request[0] = 0x00;
    request[1] = U2F_CMD_AUTHENTICATE;
    request[2] = U2fEnforce;
    request[6] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64;
    request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = 64;
    response_len = u2f_msg_parse(&u2f, request, sizeof(request), sizeof(request));

    expect(response_len > sizeof(U2fAuthResp), "first authenticate should succeed");
    expect(u2f.counter == 8, "first authenticate should advance the in-memory counter");
    expect(g_u2f_cnt_reserve_count == 1, "first authenticate should reserve a counter window");
    expect(u2f.counter_high_water == g_u2f_last_reserved_counter,
           "reserved counter high-water should be cached in memory");

    memset(request, 0, sizeof(request));
    u2f.user_present = true;
    request[0] = 0x00;
    request[1] = U2F_CMD_AUTHENTICATE;
    request[2] = U2fEnforce;
    request[6] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE + 1 + 64;
    request[7 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = 64;
    response_len = u2f_msg_parse(&u2f, request, sizeof(request), sizeof(request));

    expect(response_len > sizeof(U2fAuthResp), "second authenticate should succeed");
    expect(u2f.counter == 9, "second authenticate should advance the in-memory counter");
    expect(g_u2f_cnt_reserve_count == 1,
           "second authenticate inside the window should not rewrite the counter file");
}

static void test_zf_u2f_adapter_init_allows_missing_attestation_assets(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_u2f_cert_assets_present = false;

    expect(zf_u2f_adapter_init(&app), "U2F init should generate missing attestation assets");
    expect(g_u2f_attestation_ensure_call_count == 1,
           "U2F init should synthesize local attestation assets when none are present");
    expect(app.u2f != NULL && app.u2f->ready,
           "U2F instance should be ready after generating attestation assets");
    expect(app.u2f->cert_ready, "generated U2F attestation should enable U2F register");

    zf_u2f_adapter_deinit(&app);
}

static void test_zf_u2f_adapter_init_creates_missing_device_key_and_counter(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_u2f_key_load_result = false;
    g_u2f_key_exists_result = false;
    g_u2f_cnt_read_result = false;
    g_u2f_cnt_exists_result = false;

    expect(zf_u2f_adapter_init(&app),
           "U2F init should generate missing filesystem-backed key material");
    expect(g_u2f_key_generate_count == 1,
           "U2F init should generate the device key when the key file is missing");
    expect(g_u2f_cnt_write_count == 1,
           "U2F init should create the counter file when the counter file is missing");
    expect(g_u2f_last_written_counter == 0,
           "U2F init should initialize a missing counter file to zero");
    expect(app.u2f != NULL && app.u2f->ready,
           "U2F instance should be ready after creating missing key material");

    zf_u2f_adapter_deinit(&app);
}

static void test_zf_u2f_adapter_init_allows_invalid_attestation_assets(void) {
    ZerofidoApp app = {0};

    test_reset();
    g_u2f_cert_assets_present = true;
    g_u2f_cert_check_result = false;

    expect(zf_u2f_adapter_init(&app),
           "U2F init should replace attestation assets that fail validation");
    expect(g_u2f_attestation_ensure_call_count == 1,
           "U2F init should synthesize local attestation assets when existing assets are invalid");
    expect(app.u2f != NULL && app.u2f->ready,
           "U2F instance should be ready after replacing invalid attestation assets");
    expect(app.u2f->cert_ready, "regenerated U2F attestation should enable U2F register");

    zf_u2f_adapter_deinit(&app);
}

static void test_u2f_adapter_auto_accept_bypasses_approval_prompt(void) {
    ZerofidoApp app = {0};
    uint8_t request[5 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = {0};
    uint8_t response[512] = {0};
    size_t response_len = 0;
    const size_t signature_preimage_len =
        1U + U2F_APP_ID_SIZE + U2F_CHALLENGE_SIZE + (U2F_HASH_SIZE * 2U) + sizeof(U2fPubKey);
    const uint8_t *signature_preimage = NULL;

    test_reset();
    g_auto_accept_requests = true;
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.runtime_config.auto_accept_requests = true;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_u2f_adapter_init(&app), "U2F init should succeed before auto-accept register");
    request[0] = 0x00;
    request[1] = U2F_CMD_REGISTER;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE;
    memset(request + 5, 0xA5, U2F_CHALLENGE_SIZE);
    memset(request + 5 + U2F_CHALLENGE_SIZE, 0x5A, U2F_APP_ID_SIZE);

    response_len = zf_u2f_adapter_handle_msg(&app, 0x01020304U, request, sizeof(request), response,
                                             sizeof(response));

    expect(response_len > 2, "auto-accept U2F register should still return a full response");
    expect(response[response_len - 2] == 0x90 && response[response_len - 1] == 0x00,
           "auto-accept U2F register should succeed");
    expect(g_approval_request_count == 0,
           "auto-accept U2F register should bypass the approval prompt");
    expect(g_sha256_trace_len >= signature_preimage_len,
           "short U2F register should hash a complete registration signature preimage");
    signature_preimage = &g_sha256_trace[g_sha256_trace_len - signature_preimage_len];
    expect(signature_preimage[0] == 0x00,
           "U2F register signature preimage should start with the reserved byte");
    expect(memcmp(signature_preimage + 1, request + 5 + U2F_CHALLENGE_SIZE, U2F_APP_ID_SIZE) == 0,
           "short U2F register signature preimage should use appId from the short APDU payload");
    expect(
        memcmp(signature_preimage + 1 + U2F_APP_ID_SIZE, request + 5, U2F_CHALLENGE_SIZE) == 0,
        "short U2F register signature preimage should use challenge from the short APDU payload");

    zf_u2f_adapter_deinit(&app);
}

static void test_u2f_adapter_short_register_approval_uses_payload_app_id(void) {
    ZerofidoApp app = {0};
    uint8_t request[5 + U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE] = {0};
    uint8_t response[512] = {0};
    size_t response_len = 0;

    test_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_u2f_adapter_init(&app), "U2F init should succeed before approval register");
    request[0] = 0x00;
    request[1] = U2F_CMD_REGISTER;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = U2F_CHALLENGE_SIZE + U2F_APP_ID_SIZE;
    memset(request + 5, 0xA5, U2F_CHALLENGE_SIZE);
    memset(request + 5 + U2F_CHALLENGE_SIZE, 0x5A, U2F_APP_ID_SIZE);

    response_len = zf_u2f_adapter_handle_msg(&app, 0x01020304U, request, sizeof(request), response,
                                             sizeof(response));

    expect(response_len > 2, "approved short U2F register should return a full response");
    expect(response[response_len - 2] == 0x90 && response[response_len - 1] == 0x00,
           "approved short U2F register should succeed");
    expect(g_approval_request_count == 1,
           "short U2F register should request approval when auto-accept is off");
    expect(strcmp(g_last_approval_rp_id, "app 5a5a5a5a5a5a5a5a...") == 0,
           "short U2F register approval should show appId from the APDU payload");

    zf_u2f_adapter_deinit(&app);
}

static void test_u2f_adapter_invalid_cla_returns_cla_not_supported(void) {
    ZerofidoApp app = {0};
    uint8_t request[5] = {0};
    uint8_t response[16] = {0};
    size_t response_len = 0;

    test_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(zf_u2f_adapter_init(&app), "U2F init should succeed before invalid-CLA adapter test");

    request[0] = 0x01;
    request[1] = U2F_CMD_VERSION;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = 0x00;

    response_len = zf_u2f_adapter_handle_msg(&app, 0x01020304U, request, sizeof(request), response,
                                             sizeof(response));

    expect(response_len == 2, "invalid-CLA U2F APDU should return only a status word");
    expect(response[0] == 0x6E && response[1] == 0x00,
           "invalid-CLA U2F APDU should return SW_CLA_NOT_SUPPORTED");
    expect(g_approval_request_count == 0,
           "invalid-CLA U2F APDU should not trigger an approval request");

    zf_u2f_adapter_deinit(&app);
}

static void test_u2f_adapter_invalid_cla_returns_cla_not_supported_without_live_backend(void) {
    ZerofidoApp app = {0};
    uint8_t request[5] = {0};
    uint8_t response[16] = {0};
    size_t response_len = 0;

    test_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    request[0] = 0x01;
    request[1] = U2F_CMD_VERSION;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = 0x00;

    response_len = zf_u2f_adapter_handle_msg(&app, 0x01020304U, request, sizeof(request), response,
                                             sizeof(response));

    expect(response_len == 2,
           "invalid-CLA U2F APDU should still return a status word without a live backend");
    expect(response[0] == 0x6E && response[1] == 0x00,
           "invalid-CLA U2F APDU should still return SW_CLA_NOT_SUPPORTED without a live backend");
}

static void test_u2f_adapter_version_returns_u2f_v2_without_live_backend(void) {
    ZerofidoApp app = {0};
    uint8_t request[5] = {0};
    uint8_t response[16] = {0};
    size_t response_len = 0;

    test_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    request[0] = 0x00;
    request[1] = U2F_CMD_VERSION;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = 0x00;

    response_len = zf_u2f_adapter_handle_msg(&app, 0x01020304U, request, sizeof(request), response,
                                             sizeof(response));

    expect(response_len == 8, "VERSION U2F APDU should still return U2F_V2 without a live backend");
    expect(memcmp(response, "U2F_V2", 6) == 0,
           "VERSION U2F APDU should return the version string without a live backend");
    expect(response[6] == 0x90 && response[7] == 0x00,
           "VERSION U2F APDU should return SW_NO_ERROR without a live backend");
}

static void test_u2f_adapter_version_with_data_returns_wrong_length_without_full_copy(void) {
    ZerofidoApp app = {0};
    uint8_t request[5 + 64] = {0};
    uint8_t response[2] = {0};
    size_t response_len = 0;

    test_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    request[0] = 0x00;
    request[1] = U2F_CMD_VERSION;
    request[2] = 0x00;
    request[3] = 0x00;
    request[4] = 64;
    memset(&request[5], 0xA5, 64);

    response_len = zf_u2f_adapter_handle_msg(&app, 0x01020304U, request, sizeof(request), response,
                                             sizeof(response));

    expect(response_len == 2, "VERSION APDU with unexpected data should return a status word "
                              "without a full request copy");
    expect(response[0] == 0x67 && response[1] == 0x00,
           "VERSION APDU with unexpected data should return SW_WRONG_LENGTH");
}

static void test_u2f_adapter_version_extended_length_with_data_returns_wrong_length(void) {
    ZerofidoApp app = {0};
    uint8_t request[] = {
        0x00, 0x03, 0x00, 0x00, 0x00, 0x28, 0x2c, 0x7c, 0xd7, 0xb3, 0x5e, 0x16,
        0xfa, 0x3c, 0xc2, 0x57, 0xb0, 0x5e, 0xaf, 0xa7, 0xc7, 0xcc, 0x77, 0x92,
        0xf1, 0xef, 0x37, 0xe3, 0x1b, 0x1e, 0x4c, 0x77, 0x38, 0xac, 0x41, 0x08,
        0x44, 0x3b, 0x4a, 0x46, 0x72, 0x2d, 0xee, 0x2b, 0xea, 0x32, 0x00, 0x00,
    };
    uint8_t response[2] = {0};
    size_t response_len = 0;

    test_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    response_len = zf_u2f_adapter_handle_msg(&app, 0x01020304U, request, sizeof(request), response,
                                             sizeof(response));

    expect(response_len == 2,
           "extended-length VERSION APDU with data should return only a status word");
    expect(response[0] == 0x67 && response[1] == 0x00,
           "extended-length VERSION APDU with data should return SW_WRONG_LENGTH");
}
