static size_t encode_make_credential_ctap_request(uint8_t *buffer, size_t capacity) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP makeCredential encoder");
    expect(zf_cbor_encode_map(&enc, 4) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key"),
           "encode CTAP makeCredential request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_make_credential_ctap_request_with_unknown_true_option(uint8_t *buffer,
                                                                           size_t capacity) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP makeCredential unknown option encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "makeTea") &&
               zf_cbor_encode_bool(&enc, true),
           "encode CTAP makeCredential request with unknown true option");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_make_credential_ctap_request_with_attestation_formats(
    uint8_t *buffer, size_t capacity, const char *first_format, const char *second_format) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};
    size_t formats_count = second_format ? 2U : 1U;

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP makeCredential attestation preference encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 11) &&
               zf_cbor_encode_array(&enc, formats_count) &&
               zf_cbor_encode_text(&enc, first_format) &&
               (!second_format || zf_cbor_encode_text(&enc, second_format)),
           "encode CTAP makeCredential request with attestation preference");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_resident_make_credential_ctap_request(uint8_t *buffer, size_t capacity) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP resident makeCredential encoder");
    expect(zf_cbor_encode_map(&enc, 5) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "rk") &&
               zf_cbor_encode_bool(&enc, true),
           "encode CTAP resident makeCredential request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_resident_make_credential_ctap_request_with_cred_protect(uint8_t *buffer,
                                                                             size_t capacity,
                                                                             uint8_t cred_protect) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP resident makeCredential credProtect encoder");
    expect(zf_cbor_encode_map(&enc, 6) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 6) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "credProtect") &&
               zf_cbor_encode_uint(&enc, cred_protect) && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "rk") &&
               zf_cbor_encode_bool(&enc, true),
           "encode CTAP resident makeCredential credProtect request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_resident_make_credential_ctap_request_with_hmac_secret(uint8_t *buffer,
                                                                            size_t capacity) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP resident makeCredential hmac-secret encoder");
    expect(zf_cbor_encode_map(&enc, 6) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 6) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "hmac-secret") &&
               zf_cbor_encode_bool(&enc, true) && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_map(&enc, 1) && zf_cbor_encode_text(&enc, "rk") &&
               zf_cbor_encode_bool(&enc, true),
           "encode CTAP resident makeCredential hmac-secret request");
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_make_credential_ctap_request_with_pin_auth_protocol(
    uint8_t *buffer, size_t capacity, bool include_uv, bool uv_value, bool include_pin_auth,
    const uint8_t *pin_auth, size_t pin_auth_len, bool include_pin_protocol, uint64_t pin_protocol,
    bool include_exclude, const uint8_t *exclude_id, size_t exclude_len) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};
    size_t pair_count = 4;

    if (include_uv) {
        pair_count++;
    }
    if (include_pin_auth) {
        pair_count++;
    }
    if (include_pin_protocol) {
        pair_count++;
    }
    if (include_exclude) {
        pair_count++;
    }

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP makeCredential pinAuth encoder");
    expect(zf_cbor_encode_map(&enc, pair_count) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key"),
           "encode CTAP makeCredential pinAuth request");
    if (include_uv) {
        expect(zf_cbor_encode_uint(&enc, 7) && zf_cbor_encode_map(&enc, 1) &&
                   zf_cbor_encode_text(&enc, "uv") && zf_cbor_encode_bool(&enc, uv_value),
               "encode makeCredential uv option");
    }
    if (include_pin_auth) {
        expect(zf_cbor_encode_uint(&enc, 8) && zf_cbor_encode_bytes(&enc, pin_auth, pin_auth_len),
               "encode makeCredential pinAuth");
    }
    if (include_pin_protocol) {
        expect(zf_cbor_encode_uint(&enc, 9) && zf_cbor_encode_uint(&enc, pin_protocol),
               "encode makeCredential pinProtocol");
    }
    if (include_exclude) {
        expect(zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_array(&enc, 1) &&
                   zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "id") &&
                   zf_cbor_encode_bytes(&enc, exclude_id, exclude_len) &&
                   zf_cbor_encode_text(&enc, "type") && zf_cbor_encode_text(&enc, "public-key"),
               "encode makeCredential excludeList");
    }
    return zf_cbor_encoder_size(&enc) + 1;
}

static size_t encode_make_credential_ctap_request_with_pin_auth(
    uint8_t *buffer, size_t capacity, bool include_uv, bool uv_value, bool include_pin_auth,
    const uint8_t *pin_auth, size_t pin_auth_len, bool include_pin_protocol, bool include_exclude,
    const uint8_t *exclude_id, size_t exclude_len) {
    return encode_make_credential_ctap_request_with_pin_auth_protocol(
        buffer, capacity, include_uv, uv_value, include_pin_auth, pin_auth, pin_auth_len,
        include_pin_protocol, ZF_PIN_PROTOCOL_V1, include_exclude, exclude_id, exclude_len);
}

static size_t encode_resident_make_credential_ctap_request_with_pin_auth(
    uint8_t *buffer, size_t capacity, bool include_uv, bool uv_value, bool include_pin_auth,
    const uint8_t *pin_auth, size_t pin_auth_len, bool include_pin_protocol) {
    ZfCborEncoder enc;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t user_id[] = {0xAA};
    size_t pair_count = 5;
    size_t options_count = 1;

    if (include_pin_auth) {
        pair_count++;
    }
    if (include_pin_protocol) {
        pair_count++;
    }
    if (include_uv) {
        options_count++;
    }

    buffer[0] = ZfCtapeCmdMakeCredential;
    expect(zf_cbor_encoder_init(&enc, buffer + 1, capacity - 1),
           "init CTAP resident makeCredential pinAuth encoder");
    expect(zf_cbor_encode_map(&enc, pair_count) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_bytes(&enc, client_data_hash, sizeof(client_data_hash)) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_map(&enc, 1) &&
               zf_cbor_encode_text(&enc, "id") && zf_cbor_encode_text(&enc, "example.com") &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_map(&enc, 2) &&
               zf_cbor_encode_text(&enc, "id") &&
               zf_cbor_encode_bytes(&enc, user_id, sizeof(user_id)) &&
               zf_cbor_encode_text(&enc, "name") && zf_cbor_encode_text(&enc, "alice") &&
               zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_array(&enc, 1) &&
               zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_text(&enc, "alg") &&
               zf_cbor_encode_int(&enc, -7) && zf_cbor_encode_text(&enc, "type") &&
               zf_cbor_encode_text(&enc, "public-key") && zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_map(&enc, options_count) && zf_cbor_encode_text(&enc, "rk") &&
               zf_cbor_encode_bool(&enc, true),
           "encode CTAP resident makeCredential pinAuth request");
    if (include_uv) {
        expect(zf_cbor_encode_text(&enc, "uv") && zf_cbor_encode_bool(&enc, uv_value),
               "encode resident makeCredential uv option");
    }
    if (include_pin_auth) {
        expect(zf_cbor_encode_uint(&enc, 8) && zf_cbor_encode_bytes(&enc, pin_auth, pin_auth_len),
               "encode resident makeCredential pinAuth");
    }
    if (include_pin_protocol) {
        expect(zf_cbor_encode_uint(&enc, 9) && zf_cbor_encode_uint(&enc, 1),
               "encode resident makeCredential pinProtocol");
    }
    return zf_cbor_encoder_size(&enc) + 1;
}

static void test_ctap_error_response_omits_body_when_store_add_fails(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    g_storage_fail_rename_match = ".tmp";
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len == 1, "CTAP error response should not include a stale body");
    expect(response[0] == ZF_CTAP_ERR_KEY_STORE_FULL,
           "MakeCredential store failure should surface");
}

static void test_make_credential_rejects_local_maintenance_window(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.maintenance_busy = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len == 1, "maintenance rejection should only return a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_NOT_ALLOWED,
           "MakeCredential should reject while local maintenance owns the state");
    expect(g_approval_request_count == 0,
           "maintenance rejection should happen before any approval prompt");
}

static void test_make_credential_empty_pin_auth_requires_pin_protocol(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true,
                                             (const uint8_t *)"", 0, false, false, NULL, 0),
                                         response, sizeof(response));

    expect(response_len == 1, "missing pinProtocol should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_MISSING_PARAMETER,
           "empty pinAuth probes must still require pinProtocol");
    expect(g_approval_request_count == 0,
           "missing pinProtocol should fail before any approval prompt");
}

static void test_get_assertion_empty_pin_auth_requires_pin_protocol(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_pin_auth(request, sizeof(request), false, false,
                                                        true, (const uint8_t *)"", 0, false),
        response, sizeof(response));

    expect(response_len == 1, "missing pinProtocol should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_MISSING_PARAMETER,
           "GetAssertion empty pinAuth probes must still require pinProtocol");
    expect(g_approval_request_count == 0,
           "GetAssertion missing pinProtocol should fail before approval");
}

static void test_make_credential_empty_pin_auth_probe_returns_pin_status_after_touch(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true,
                                             (const uint8_t *)"", 0, true, false, NULL, 0),
                                         response, sizeof(response));

    expect(response_len == 1, "empty pinAuth probe should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_PIN_NOT_SET,
           "empty pinAuth probe should return PIN_NOT_SET when no PIN is configured");
    expect(g_approval_request_count == 1,
           "empty pinAuth probe should still require one approval/touch");

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true,
                                             (const uint8_t *)"", 0, true, false, NULL, 0),
                                         response, sizeof(response));

    expect(response_len == 1, "empty pinAuth probe with PIN should return only status");
    expect(response[0] == ZF_CTAP_ERR_PIN_INVALID,
           "empty pinAuth probe should return PIN_INVALID when a PIN is configured");
    expect(g_approval_request_count == 1,
           "empty pinAuth probe with PIN should still require one approval/touch");
}

static void test_get_assertion_empty_pin_auth_probe_returns_pin_status_after_touch(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_pin_auth(request, sizeof(request), false, false,
                                                        true, (const uint8_t *)"", 0, true),
        response, sizeof(response));

    expect(response_len == 1, "empty pinAuth probe should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_PIN_NOT_SET,
           "GetAssertion empty pinAuth probe should return PIN_NOT_SET when no PIN is configured");
    expect(g_approval_request_count == 1,
           "GetAssertion empty pinAuth probe should still require one approval/touch");

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_pin_auth(request, sizeof(request), false, false,
                                                        true, (const uint8_t *)"", 0, true),
        response, sizeof(response));

    expect(response_len == 1, "empty pinAuth probe with PIN should return only status");
    expect(response[0] == ZF_CTAP_ERR_PIN_INVALID,
           "GetAssertion empty pinAuth probe should return PIN_INVALID when a PIN is configured");
    expect(g_approval_request_count == 1,
           "GetAssertion empty pinAuth probe with PIN should still require one approval/touch");
}

static void test_get_assertion_uv_without_pin_auth_returns_unsupported_option(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, request,
                              encode_get_assertion_ctap_request_with_pin_auth(
                                  request, sizeof(request), true, true, false, NULL, 0, false),
                              response, sizeof(response));

    expect(response_len == 1, "unsupported uv request should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_UNSUPPORTED_OPTION,
           "GetAssertion uv=true without pinAuth should return UNSUPPORTED_OPTION");
    expect(g_approval_request_count == 0,
           "GetAssertion uv=true without pinAuth should fail before approval");
}

static void test_make_credential_uv_without_pin_auth_returns_unsupported_option(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_make_credential_ctap_request_with_pin_auth(request, sizeof(request), true, true,
                                                          false, NULL, 0, false, false, NULL, 0),
        response, sizeof(response));

    expect(response_len == 1, "unsupported uv makeCredential should return only status");
    expect(response[0] == ZF_CTAP_ERR_UNSUPPORTED_OPTION,
           "MakeCredential uv=true without pinAuth should return UNSUPPORTED_OPTION");
    expect(g_approval_request_count == 0,
           "MakeCredential uv=true without pinAuth should fail before approval");
}

static void test_make_credential_pin_auth_takes_precedence_over_uv(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x5A, sizeof(app.pin_state.pin_token));
    app.pin_state.pin_token_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive makeCredential pinAuth");

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), true, true, true, pin_auth,
                                             ZF_PIN_AUTH_LEN, true, false, NULL, 0),
                                         response, sizeof(response));

    expect(response_len > 1, "valid pinAuth plus uv=true should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "pinAuth should take precedence over uv=true on MakeCredential");
    expect(g_approval_request_count == 1,
           "successful MakeCredential should still request approval");
}

static void test_make_credential_pin_auth_clears_permission_scoped_token_after_up(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x66, sizeof(app.pin_state.pin_token));
    zf_pin_set_token_permissions(&app.pin_state, ZF_PIN_PERMISSION_MC, true, true,
                                 "example.com");
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive makeCredential pinAuth for permission-scoped token");

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true, pin_auth,
                                             ZF_PIN_AUTH_LEN, true, false, NULL, 0),
                                         response, sizeof(response));

    expect(response_len > 1, "permission-scoped makeCredential should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "permission-scoped makeCredential should return success");
    expect(app.pin_state.pin_token_active,
           "permission-scoped token should remain active after makeCredential");
    expect(app.pin_state.pin_token_permissions == 0,
           "makeCredential should clear mc/ga token permissions after UP");
    expect(!app.pin_state.pin_token_permissions_rp_id_set,
           "makeCredential should clear the consumed permission RP binding");
    uint8_t zero_rp_id_hash[32] = {0};
    expect(memcmp(app.pin_state.pin_token_permissions_rp_id_hash, zero_rp_id_hash,
                  sizeof(zero_rp_id_hash)) == 0,
           "makeCredential should clear the consumed permission RP ID hash");
}

static void test_make_credential_legacy_pin_token_keeps_mc_ga_after_up(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x6A, sizeof(app.pin_state.pin_token));
    app.pin_state.pin_token_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive makeCredential pinAuth for legacy token");

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true, pin_auth,
                                             ZF_PIN_AUTH_LEN, true, false, NULL, 0),
                                         response, sizeof(response));

    expect(response_len > 1, "legacy-token makeCredential should succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "legacy-token makeCredential should return success");
    expect(app.pin_state.pin_token_permissions == (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA),
           "CTAP 2.0 legacy getPinToken permissions should survive successful UP");
}

static void test_make_credential_allows_pinless_non_resident_when_pin_is_set(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "non-resident MakeCredential without pinAuth should succeed");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "non-resident MakeCredential should remain PIN-less when UV is discouraged");
    expect(g_approval_request_count == 1,
           "non-resident MakeCredential without pinAuth should still require approval");
}

static void test_make_credential_requires_pin_auth_for_resident_when_pin_is_set(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_resident_make_credential_ctap_request(request, sizeof(request)), response,
        sizeof(response));

    expect(response_len == 1,
           "resident MakeCredential without pinAuth should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_PIN_REQUIRED,
           "resident MakeCredential without pinAuth should require PIN when a PIN is set");
    expect(g_approval_request_count == 0,
           "resident MakeCredential without pinAuth should fail before approval");
}

static void test_make_credential_2_1_make_cred_uv_not_required_allows_pinless_non_resident(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_fido2_1_experimental(&app);

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1,
           "CTAP 2.1 makeCredUvNotRqd should allow pinless non-resident MakeCredential");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "pinless non-resident MakeCredential should succeed when makeCredUvNotRqd is true");
    expect(g_approval_request_count == 1,
           "pinless non-resident MakeCredential should still require user presence");
}

static void
test_make_credential_2_1_make_cred_uv_not_required_rejects_resident_without_pin_auth(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_fido2_1_experimental(&app);

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_resident_make_credential_ctap_request(request, sizeof(request)), response,
        sizeof(response));

    expect(response_len == 1,
           "resident MakeCredential without pinAuth should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_PIN_REQUIRED,
           "makeCredUvNotRqd should not allow resident credentials without PIN auth");
    expect(g_approval_request_count == 0,
           "resident MakeCredential without pinAuth should fail before approval");
}

static void test_make_credential_auto_accept_bypasses_approval_prompt(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "auto-accept makeCredential should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "auto-accept makeCredential should return success");
    expect(g_approval_request_count == 0,
           "auto-accept makeCredential should bypass the approval prompt");
}

static void test_make_credential_unknown_option_succeeds(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_make_credential_ctap_request_with_unknown_true_option(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "makeCredential with unknown option should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "makeCredential should ignore unknown true options");
}

#if ZF_PACKED_ATTESTATION
static void test_make_credential_returns_packed_attestation(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);
    app.runtime_config.attestation_mode = ZfAttestationModePacked;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "MakeCredential should return a CTAP response body");
    expect(response[0] == ZF_CTAP_SUCCESS, "MakeCredential should return success");
    expect(parse_make_credential_packed_attestation(response + 1, response_len - 1),
           "MakeCredential should return packed attestation with one leaf certificate");
}
#endif

#if !ZF_PACKED_ATTESTATION
static void test_make_credential_packed_off_returns_none_attestation(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, request,
                              encode_make_credential_ctap_request_with_attestation_formats(
                                  request, sizeof(request), "packed", NULL),
                              response, sizeof(response));

    expect(response_len > 1, "packed-off makeCredential should return a CTAP response body");
    expect(response[0] == ZF_CTAP_SUCCESS, "packed-off makeCredential should return success");
    expect(parse_make_credential_none_attestation(response + 1, response_len - 1),
           "packed-off build should return fmt none even for a packed-only preference");
}
#endif

static void test_make_credential_runtime_none_returns_none_attestation(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);
    app.runtime_config.attestation_mode = ZfAttestationModeNone;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "runtime none makeCredential should return a CTAP response body");
    expect(response[0] == ZF_CTAP_SUCCESS, "runtime none makeCredential should return success");
    expect(parse_make_credential_none_attestation(response + 1, response_len - 1),
           "runtime none mode should return fmt none with empty attStmt");
}

static void test_make_credential_attestation_preference_overrides_runtime_default(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, request,
                              encode_make_credential_ctap_request_with_attestation_formats(
                                  request, sizeof(request), "none", NULL),
                              response, sizeof(response));

    expect(response_len > 1, "none attestation preference should return a CTAP response body");
    expect(response[0] == ZF_CTAP_SUCCESS, "none attestation preference should return success");
    expect(parse_make_credential_none_attestation(response + 1, response_len - 1),
           "single none attestation preference should omit packed attestation");
}

#if ZF_PACKED_ATTESTATION
static void test_make_credential_attestation_preference_uses_lowest_supported_index(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);
    app.runtime_config.attestation_mode = ZfAttestationModeNone;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, request,
                              encode_make_credential_ctap_request_with_attestation_formats(
                                  request, sizeof(request), "packed", "none"),
                              response, sizeof(response));

    expect(response_len > 1, "packed-first attestation preference should return a CTAP body");
    expect(response[0] == ZF_CTAP_SUCCESS, "packed-first attestation preference should succeed");
    expect(parse_make_credential_packed_attestation(response + 1, response_len - 1),
           "packed should win when it is the lowest-index supported preference");
}
#endif

#if ZF_PACKED_ATTESTATION
static void test_make_credential_response_does_not_downgrade_required_packed(void) {
    ZfMakeCredentialResponseScratch scratch;
    ZfCredentialRecord record = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0};
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    strcpy(record.rp_id, "example.com");
    memset(record.credential_id, 0x42, sizeof(record.credential_id));
    record.credential_id_len = ZF_CREDENTIAL_ID_LEN;
    memset(record.public_x, 0x11, sizeof(record.public_x));
    memset(record.public_y, 0x22, sizeof(record.public_y));

    g_attestation_ensure_ready = false;
    expect(zf_ctap_build_packed_make_credential_response_with_scratch(
               &scratch, record.rp_id, &record, client_data_hash, false, false, false, response,
               sizeof(response), &response_len) == ZF_CTAP_ERR_OTHER,
           "required packed attestation should fail when assets are unavailable");

    response_len = 0;
    expect(zf_ctap_build_none_make_credential_response_with_scratch(
               &scratch, record.rp_id, &record, false, false, false, response, sizeof(response),
               &response_len) == ZF_CTAP_SUCCESS,
           "explicit none attestation should remain available");
    expect(parse_make_credential_none_attestation(response, response_len),
           "explicit none attestation should encode fmt none with empty attStmt");
}
#endif

static void test_make_credential_nfc_auto_accept_keeps_uv_clear(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;
    uint8_t flags = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.transport_auto_accept_transaction = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_make_credential_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "NFC auto-accept makeCredential should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "NFC auto-accept makeCredential should return success");
    expect(parse_make_credential_auth_flags(response + 1, response_len - 1, &flags),
           "NFC makeCredential response should expose authenticator data flags");
    expect((flags & 0x04U) == 0U,
           "NFC auto-accept makeCredential should not fake user verification");
    expect(g_approval_request_count == 0,
           "NFC auto-accept makeCredential should bypass the approval prompt");
    expect(g_status_update_count == 0,
           "NFC auto-accept makeCredential should not update UI status before the APDU response");
}

static void test_make_credential_nfc_auto_accept_rejects_uv_option_without_pin(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.transport_auto_accept_transaction = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_make_credential_ctap_request_with_pin_auth(request, sizeof(request), true, true,
                                                          false, NULL, 0, false, false, NULL, 0),
        response, sizeof(response));

    expect(response_len == 1, "NFC auto-accept uv=true makeCredential should return only status");
    expect(response[0] == ZF_CTAP_ERR_UNSUPPORTED_OPTION,
           "NFC auto-accept must not satisfy uv=true without real UV/PIN support");
    expect(g_approval_request_count == 0,
           "NFC auto-accept uv=true rejection should happen before approval");
}

static void test_get_assertion_pin_auth_takes_precedence_over_uv(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    ZfCredentialRecord record = {0};
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x33, sizeof(app.pin_state.pin_token));
    app.pin_state.pin_token_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive getAssertion pinAuth");

    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    zf_store_index_entry_from_record(&record, &app.store.records[0]);
    app.store.count = 1;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_pin_auth(request, sizeof(request), true, true, true,
                                                        pin_auth, ZF_PIN_AUTH_LEN, true),
        response, sizeof(response));

    expect(response_len > 1, "valid pinAuth plus uv=true should still build an assertion");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "pinAuth should take precedence over uv=true on GetAssertion");
    expect(g_approval_request_count == 1, "successful GetAssertion should still request approval");
}

static void test_get_assertion_pin_auth_clears_permission_scoped_token_after_up(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x77, sizeof(app.pin_state.pin_token));
    zf_pin_set_token_permissions(&app.pin_state, ZF_PIN_PERMISSION_GA, true, true,
                                 "example.com");
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive getAssertion pinAuth for permission-scoped token");

    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    app.store.records[0] = record;
    app.store.count = 1;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_pin_auth(request, sizeof(request), false, false,
                                                        true, pin_auth, ZF_PIN_AUTH_LEN, true),
        response, sizeof(response));

    expect(response_len > 1, "permission-scoped getAssertion should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "permission-scoped getAssertion should return success");
    expect(app.pin_state.pin_token_active,
           "permission-scoped token should remain active after getAssertion");
    expect(app.pin_state.pin_token_permissions == 0,
           "getAssertion should clear mc/ga token permissions after UP");
    expect(!app.pin_state.pin_token_permissions_rp_id_set,
           "getAssertion should clear the consumed permission RP binding");
    uint8_t zero_rp_id_hash[32] = {0};
    expect(memcmp(app.pin_state.pin_token_permissions_rp_id_hash, zero_rp_id_hash,
                  sizeof(zero_rp_id_hash)) == 0,
           "getAssertion should clear the consumed permission RP ID hash");
}

static void test_permission_scoped_pin_token_rejects_browser_style_replay_after_mc(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t make_credential_pin_auth[32];
    uint8_t get_assertion_pin_auth[32];
    size_t response_len = 0;
    static const uint8_t make_credential_client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };
    static const uint8_t get_assertion_client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x79, sizeof(app.pin_state.pin_token));
    zf_pin_set_token_permissions(&app.pin_state, ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA,
                                 true, true, "example.com");
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 make_credential_client_data_hash,
                                 sizeof(make_credential_client_data_hash),
                                 make_credential_pin_auth),
           "derive browser-style makeCredential pinAuth");

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_resident_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true,
                                             make_credential_pin_auth, ZF_PIN_AUTH_LEN, true),
                                         response, sizeof(response));

    expect(response_len > 1, "browser-style makeCredential should succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "browser-style makeCredential should return success");
    expect(app.store.count == 1, "browser-style makeCredential should persist a credential");
    expect(app.pin_state.pin_token_permissions == 0,
           "browser-style makeCredential should consume mc/ga token permissions");
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 get_assertion_client_data_hash,
                                 sizeof(get_assertion_client_data_hash), get_assertion_pin_auth),
           "derive browser-style getAssertion pinAuth");

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_get_assertion_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true,
                                             get_assertion_pin_auth, ZF_PIN_AUTH_LEN, true),
                                         response, sizeof(response));

    expect(response_len == 1, "browser-style getAssertion replay should return only status");
    expect(response[0] == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "permission-scoped token should not remain usable after makeCredential");
    expect(g_approval_request_count == 1,
           "permission-scoped token replay should fail before another approval request");
}

static void test_get_assertion_legacy_pin_token_keeps_mc_ga_after_up(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x7A, sizeof(app.pin_state.pin_token));
    app.pin_state.pin_token_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive getAssertion pinAuth for legacy token");

    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    app.store.records[0] = record;
    app.store.count = 1;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_pin_auth(request, sizeof(request), false, false,
                                                        true, pin_auth, ZF_PIN_AUTH_LEN, true),
        response, sizeof(response));

    expect(response_len > 1, "legacy-token getAssertion should succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "legacy-token getAssertion should return success");
    expect(app.pin_state.pin_token_permissions == (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA),
           "CTAP 2.0 legacy getPinToken permissions should survive GetAssertion UP");
}

static void test_get_assertion_allows_discouraged_uv_when_pin_is_set(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    app.store.records[0] = record;
    app.store.count = 1;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1,
           "GetAssertion should still succeed without pinAuth when UV is discouraged");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "GetAssertion should allow discouraged UV requests without pinAuth");
    expect(g_approval_request_count == 1,
           "GetAssertion without pinAuth should still request approval");
}

static void test_get_assertion_auto_accept_bypasses_approval_prompt(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);
    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    app.store.records[0] = record;
    app.store.count = 1;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "auto-accept GetAssertion should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "auto-accept GetAssertion should return success");
    expect(g_approval_request_count == 0,
           "auto-accept GetAssertion should bypass the approval prompt");
}

static void test_get_assertion_discovers_no_credentials_without_uv_for_cred_protect_2(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    record.cred_protect = ZF_CRED_PROTECT_UV_OPTIONAL_WITH_CRED_ID;
    app.store.records[0] = record;
    app.store.count = 1;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len == 1, "discoverable credProtect=2 without UV should return status only");
    expect(response[0] == ZF_CTAP_ERR_NO_CREDENTIALS,
           "discoverable credProtect=2 without UV should not reveal the credential");
}

static void test_get_assertion_with_pin_auth_allows_discoverable_cred_protect_2(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};
    static const uint8_t client_data_hash[32] = {
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B,
        0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    record.cred_protect = ZF_CRED_PROTECT_UV_OPTIONAL_WITH_CRED_ID;
    app.store.records[0] = record;
    app.store.count = 1;
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for credProtect assertion");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for credProtect assertion");
    mark_pin_token_issued(&app.pin_state);
    app.pin_state.pin_token_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive pinAuth for discoverable credProtect assertion");

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_pin_auth(request, sizeof(request), false, false,
                                                        true, pin_auth, ZF_PIN_AUTH_LEN, true),
        response, sizeof(response));

    expect(response_len > 1, "discoverable credProtect=2 should succeed with pinAuth");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "discoverable credProtect=2 should succeed when UV is satisfied via pinAuth");
}

static void test_make_credential_overwrites_resident_credential_for_same_user(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_resident_make_credential_ctap_request(request, sizeof(request)), response,
        sizeof(response));
    expect(response_len > 1, "first resident makeCredential should succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "first resident makeCredential should return success");
    expect(app.store.count == 1, "first resident makeCredential should store one credential");

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_resident_make_credential_ctap_request(request, sizeof(request)), response,
        sizeof(response));
    expect(response_len > 1, "second resident makeCredential should also succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "second resident makeCredential should return success");
    expect(app.store.count == 1,
           "resident makeCredential should overwrite the previous credential for the same user");
    expect(app.store.records[0].resident_key,
           "resident overwrite should preserve discoverable credential storage");
}

static void test_make_credential_cred_protect_round_trips_into_auth_data_and_store(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t cred_protect = 0;
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_resident_make_credential_ctap_request_with_cred_protect(
            request, sizeof(request), ZF_CRED_PROTECT_UV_OPTIONAL_WITH_CRED_ID),
        response, sizeof(response));

    expect(response_len > 1, "resident makeCredential with credProtect should succeed");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "resident makeCredential with credProtect should return success");
    expect(parse_make_credential_cred_protect_output(response + 1, response_len - 1, &cred_protect),
           "makeCredential response should expose credProtect extension output");
    expect(cred_protect == ZF_CRED_PROTECT_UV_OPTIONAL_WITH_CRED_ID,
           "makeCredential response should echo the requested credProtect policy");
    expect(app.store.count == 1, "resident makeCredential should persist the credential");
    expect(app.store.records[0].cred_protect == ZF_CRED_PROTECT_UV_OPTIONAL_WITH_CRED_ID,
           "resident credential should persist the requested credProtect policy");
}

static void test_make_credential_hmac_secret_round_trips_into_auth_data_and_store(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;
    bool hmac_secret = false;
    ZfCredentialRecord stored_record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_resident_make_credential_ctap_request_with_hmac_secret(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "resident makeCredential with hmac-secret should succeed");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "resident makeCredential with hmac-secret should return success");
    expect(parse_make_credential_hmac_secret_output(response + 1, response_len - 1, &hmac_secret),
           "makeCredential response should expose hmac-secret extension output");
    expect(hmac_secret, "makeCredential response should acknowledge hmac-secret creation");
    expect(app.store.count == 1, "resident hmac-secret credential should persist");
    expect(test_zf_store_record_format_load_record(&storage, app.store.records[0].file_name,
                                                   &stored_record),
           "resident hmac-secret credential should load from storage");
    expect(stored_record.hmac_secret, "stored credential should carry hmac-secret material");
}

static void test_get_assertion_hmac_secret_builds_protocol1_output(void) {
    ZfClientPinState pin_state = {0};
    ZfGetAssertionRequest request = {0};
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    ZfCredentialRecord record = {0};
    uint8_t extension[112];
    ZfHmacSecretScratch scratch = {0};
    size_t extension_len = 0;
    uint8_t expected_salt_auth[32];
    uint8_t expected_output[32];
    const uint8_t *hmac_secret_enc = NULL;
    size_t hmac_secret_enc_len = 0;

    request.assertion.has_hmac_secret = true;
    request.assertion.hmac_secret_pin_protocol = ZF_PIN_PROTOCOL_V1;
    memset(request.assertion.hmac_secret_platform_x, 0x11,
           sizeof(request.assertion.hmac_secret_platform_x));
    memset(request.assertion.hmac_secret_platform_y, 0x22,
           sizeof(request.assertion.hmac_secret_platform_y));
    for (size_t i = 0; i < 32U; ++i) {
        request.assertion.hmac_secret_salt_enc[i] = (uint8_t)i;
    }
    request.assertion.hmac_secret_salt_enc_len = 32U;
    expect(zf_crypto_hmac_sha256((const uint8_t[32]){[0 ... 31] = 0xAB}, 32,
                                 request.assertion.hmac_secret_salt_enc,
                                 request.assertion.hmac_secret_salt_enc_len, expected_salt_auth),
           "compute host hmac-secret saltAuth");
    memcpy(request.assertion.hmac_secret_salt_auth, expected_salt_auth, ZF_PIN_AUTH_LEN);
    request.assertion.hmac_secret_salt_auth_len = ZF_PIN_AUTH_LEN;

    record.hmac_secret = true;
    memset(record.hmac_secret_without_uv, 0x55, sizeof(record.hmac_secret_without_uv));
    memset(record.hmac_secret_with_uv, 0x66, sizeof(record.hmac_secret_with_uv));
    expect(zf_crypto_generate_key_agreement_key(&pin_state.key_agreement),
           "generate hmac-secret key agreement");

    expect(zf_ctap_hmac_secret_build_extension(&pin_state, &request.assertion, &record, false,
                                               &scratch, extension, sizeof(extension),
                                               &extension_len) == ZF_CTAP_SUCCESS,
           "hmac-secret assertion extension should build");
    expect(parse_hmac_secret_extension_output(extension, extension_len, &hmac_secret_enc,
                                              &hmac_secret_enc_len),
           "hmac-secret assertion extension should parse");
    expect(hmac_secret_enc_len == sizeof(expected_output),
           "protocol 1 hmac-secret output should contain one encrypted HMAC");
    expect(zf_crypto_hmac_sha256(record.hmac_secret_without_uv,
                                 sizeof(record.hmac_secret_without_uv),
                                 request.assertion.hmac_secret_salt_enc, 32U, expected_output),
           "compute expected hmac-secret output");
    expect(memcmp(hmac_secret_enc, expected_output, sizeof(expected_output)) == 0,
           "protocol 1 hmac-secret output should match credential secret HMAC");
}

static void test_assertion_response_preserves_scratch_hmac_secret_extension(void) {
    ZfClientPinState pin_state = {0};
    ZfGetAssertionRequest request = {0};
    TEST_INIT_GET_ASSERTION_REQUEST(request);
    ZfCredentialRecord record = {0};
    ZfAssertionResponseScratch response_scratch = {0};
    uint8_t response[256];
    size_t response_len = 0;
    size_t extension_len = 0;
    uint8_t expected_salt_auth[32];
    uint8_t expected_output[32];
    const uint8_t *hmac_secret_enc = NULL;
    size_t hmac_secret_enc_len = 0;

    strcpy(request.assertion.rp_id, "example.com");
    memset(request.assertion.client_data_hash, 0xA5, sizeof(request.assertion.client_data_hash));
    request.assertion.has_hmac_secret = true;
    request.assertion.hmac_secret_pin_protocol = ZF_PIN_PROTOCOL_V1;
    memset(request.assertion.hmac_secret_platform_x, 0x11,
           sizeof(request.assertion.hmac_secret_platform_x));
    memset(request.assertion.hmac_secret_platform_y, 0x22,
           sizeof(request.assertion.hmac_secret_platform_y));
    for (size_t i = 0; i < 32U; ++i) {
        request.assertion.hmac_secret_salt_enc[i] = (uint8_t)i;
    }
    request.assertion.hmac_secret_salt_enc_len = 32U;
    expect(zf_crypto_hmac_sha256((const uint8_t[32]){[0 ... 31] = 0xAB}, 32,
                                 request.assertion.hmac_secret_salt_enc,
                                 request.assertion.hmac_secret_salt_enc_len, expected_salt_auth),
           "compute response hmac-secret saltAuth");
    memcpy(request.assertion.hmac_secret_salt_auth, expected_salt_auth, ZF_PIN_AUTH_LEN);
    request.assertion.hmac_secret_salt_auth_len = ZF_PIN_AUTH_LEN;

    memset(record.credential_id, 0x10, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    memcpy(record.user_id, "user-1", 6);
    record.user_id_len = 6;
    record.hmac_secret = true;
    memset(record.hmac_secret_without_uv, 0x55, sizeof(record.hmac_secret_without_uv));
    memset(record.hmac_secret_with_uv, 0x66, sizeof(record.hmac_secret_with_uv));
    expect(zf_crypto_generate_key_agreement_key(&pin_state.key_agreement),
           "generate response hmac-secret key agreement");

    expect(zf_ctap_hmac_secret_build_extension(
               &pin_state, &request.assertion, &record, false, &response_scratch.hmac_secret,
               response_scratch.extension_data, sizeof(response_scratch.extension_data),
               &extension_len) == ZF_CTAP_SUCCESS,
           "hmac-secret extension should build into assertion scratch");
    expect(zf_ctap_build_assertion_response_with_scratch(
               &response_scratch, &request.assertion, &record, true, false, 7, false, false, 1,
               false, false, response_scratch.extension_data, extension_len, response,
               sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
           "assertion response should build with scratch-owned hmac-secret extension");
    expect(parse_assertion_response_hmac_secret_output(response, response_len, &hmac_secret_enc,
                                                       &hmac_secret_enc_len),
           "assertion response should retain hmac-secret extension in authData");
    expect(hmac_secret_enc_len == sizeof(expected_output),
           "assertion hmac-secret output should contain one encrypted HMAC");
    expect(zf_crypto_hmac_sha256(record.hmac_secret_without_uv,
                                 sizeof(record.hmac_secret_without_uv),
                                 request.assertion.hmac_secret_salt_enc, 32U, expected_output),
           "compute expected response hmac-secret output");
    expect(memcmp(hmac_secret_enc, expected_output, sizeof(expected_output)) == 0,
           "assertion response hmac-secret output should match credential secret HMAC");
}

static void test_make_credential_excluded_credential_returns_excluded_after_timeout(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    g_approval_result = false;
    g_approval_state = ZfApprovalTimedOut;

    memset(record.credential_id, 0x44, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    strcpy(record.rp_id, "example.com");
    record.in_use = true;
    app.store.records[0] = record;
    app.store.count = 1;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, request,
                              encode_make_credential_ctap_request_with_pin_auth(
                                  request, sizeof(request), false, false, false, NULL, 0, false,
                                  true, record.credential_id, record.credential_id_len),
                              response, sizeof(response));

    expect(response_len == 1, "excludeList hits should return only the CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_CREDENTIAL_EXCLUDED,
           "excludeList hits must report credential excluded even after timeout");
    expect(g_approval_request_count == 1,
           "excludeList hits should still wait for one approval/touch");
}

static void test_make_credential_exclude_list_hides_uv_required_credential_without_uv(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    memset(record.credential_id, 0x45, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    strcpy(record.rp_id, "example.com");
    record.in_use = true;
    record.cred_protect = ZF_CRED_PROTECT_UV_REQUIRED;
    app.store.records[0] = record;
    app.store.count = 1;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, request,
                              encode_make_credential_ctap_request_with_pin_auth(
                                  request, sizeof(request), false, false, false, NULL, 0, false,
                                  true, record.credential_id, record.credential_id_len),
                              response, sizeof(response));

    expect(response_len > 1,
           "uv-required excludeList hit without UV should not reveal the existing credential");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "uv-required excludeList hit without UV should continue credential creation");
    expect(g_approval_request_count == 1,
           "non-excluded MakeCredential should still request registration approval");
}

static void test_make_credential_exclude_list_reveals_uv_required_credential_with_pin_auth(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[32];
    uint8_t pin_auth[32];
    size_t response_len = 0;
    ZfCredentialIndexEntry record = {0};
    static const uint8_t client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.pin_state.pin_set = true;
    memset(app.pin_state.pin_token, 0x5A, sizeof(app.pin_state.pin_token));
    app.pin_state.pin_token_permissions = ZF_PIN_PERMISSION_MC;
    mark_pin_token_issued(&app.pin_state);
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 client_data_hash, sizeof(client_data_hash), pin_auth),
           "derive makeCredential pinAuth for uv-required excludeList");

    memset(record.credential_id, 0x46, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    strcpy(record.rp_id, "example.com");
    record.in_use = true;
    record.cred_protect = ZF_CRED_PROTECT_UV_REQUIRED;
    app.store.records[0] = record;
    app.store.count = 1;

    response_len = zerofido_handle_ctap2(&app, 0x01020304, request,
                                         encode_make_credential_ctap_request_with_pin_auth(
                                             request, sizeof(request), false, false, true, pin_auth,
                                             ZF_PIN_AUTH_LEN, true, true, record.credential_id,
                                             record.credential_id_len),
                                         response, sizeof(response));

    expect(response_len == 1,
           "uv-required excludeList hit with UV should return only a CTAP status byte");
    expect(response[0] == ZF_CTAP_ERR_CREDENTIAL_EXCLUDED,
           "uv-required excludeList hit with UV should report credential excluded");
    expect(g_approval_request_count == 1,
           "uv-required excludeList hit with UV should still wait for user presence");
}

static void
test_get_assertion_without_matching_credential_waits_for_approval_before_no_credentials(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len == 1, "GetAssertion error response should only include status");
    expect(response[0] == ZF_CTAP_ERR_NO_CREDENTIALS,
           "GetAssertion without a matching credential should still return no credentials");
    expect(g_approval_request_count == 1,
           "GetAssertion without credentials should require one approval before disclosure");
}

static void test_get_assertion_invalid_allow_list_waits_for_approval_before_no_credentials(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;
    static const uint8_t credential_id[] = {0x44, 0x55, 0x66, 0x77};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, request,
                              encode_get_assertion_ctap_request_with_allow_list(
                                  request, sizeof(request), credential_id, sizeof(credential_id)),
                              response, sizeof(response));

    expect(response_len == 1,
           "GetAssertion invalid allowList error response should only include status");
    expect(response[0] == ZF_CTAP_ERR_NO_CREDENTIALS,
           "GetAssertion invalid allowList should still return no credentials");
    expect(g_approval_request_count == 1, "GetAssertion invalid allowList should require one "
                                          "approval/touch before revealing no credentials");
}

static void run_get_assertion_multi_match_uses_account_selection(bool enable_fido2_1,
                                                                 bool expect_user_selected_key) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t next_response[64];
    size_t response_len = 0;
    size_t next_response_len = 0;
    const uint8_t *credential_id = NULL;
    size_t credential_id_len = 0;
    bool user_selected = false;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    if (enable_fido2_1) {
        test_enable_fido2_1_experimental(&app);
    }
    app.store.count = 4;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    strcpy(app.store.records[0].user_name, "alice");
    app.store.records[0].created_at = 10;
    seed_assertion_queue_record(&app.store.records[2], 0x22, 7);
    strcpy(app.store.records[2].user_display_name, "Bob Example");
    app.store.records[2].created_at = 30;
    seed_assertion_queue_record(&app.store.records[3], 0x33, 9);
    strcpy(app.store.records[3].user_name, "carol");
    app.store.records[3].created_at = 20;
    g_selection_index = 1;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "multi-match discoverable GetAssertion should return an assertion");
    expect(response[0] == ZF_CTAP_SUCCESS, "multi-match discoverable GetAssertion should succeed");
    expect(g_selection_request_count == 1,
           "multi-match discoverable GetAssertion should open chooser");
    expect(g_last_selection_match_count == 3, "chooser should receive every matching credential");
    expect(g_approval_request_count == 0,
           "multi-match discoverable GetAssertion should not also require approval");
    expect(parse_assertion_response_credential_id(response + 1, response_len - 1, &credential_id,
                                                  &credential_id_len),
           "chooser response should include a credential descriptor");
    expect(credential_id_len == app.store.records[3].credential_id_len,
           "chooser response should return the selected credential length");
    expect(memcmp(credential_id, app.store.records[3].credential_id, credential_id_len) == 0,
           "chooser response should return the selected credential");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 5),
           "chooser response should omit numberOfCredentials");
    if (expect_user_selected_key) {
        expect(cbor_map_uint_key_bool_value(response + 1, response_len - 1, 6, &user_selected) &&
                   user_selected,
               "CTAP 2.1 chooser response should include userSelected=true");
    } else {
        expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 6),
               "CTAP 2.0 chooser response should omit userSelected");
    }
    expect(!app.assertion_queue.active,
           "chooser-based GetAssertion should not seed the GetNextAssertion queue");
    expect(app.store.records[0].sign_count == 4,
           "non-selected credential should keep its sign count");
    expect(app.store.records[2].sign_count == 7,
           "newest non-selected credential should be unchanged");
    expect(app.store.records[3].sign_count == 10,
           "selected credential should increment sign count");
    expect(!app.store.records[1].in_use, "sparse store slot should remain unused");

    next_response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              next_response, sizeof(next_response));

    expect(next_response_len == 1, "GetNextAssertion after chooser path should return status only");
    expect(next_response[0] == ZF_CTAP_ERR_NOT_ALLOWED,
           "GetNextAssertion after chooser path should be rejected");
}

static void test_get_assertion_multi_match_uses_account_selection(void) {
    run_get_assertion_multi_match_uses_account_selection(false, false);
}

static void test_get_assertion_multi_match_experimental_2_1_includes_user_selected(void) {
    run_get_assertion_multi_match_uses_account_selection(true, true);
}

static void test_get_assertion_up_false_multi_match_reports_count_and_queues_next(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t next_response[512];
    size_t response_len = 0;
    size_t next_response_len = 0;
    uint8_t flags = 0xFFU;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 3;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    app.store.records[0].created_at = 10;
    seed_assertion_queue_record(&app.store.records[1], 0x22, 7);
    app.store.records[1].created_at = 30;
    seed_assertion_queue_record(&app.store.records[2], 0x33, 9);
    app.store.records[2].created_at = 20;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_up(request, sizeof(request), false), response,
        sizeof(response));

    expect(response_len > 1, "silent multi-match GetAssertion should return an assertion");
    expect(response[0] == ZF_CTAP_SUCCESS, "silent multi-match GetAssertion should succeed");
    expect(g_selection_request_count == 0, "silent GetAssertion should not open chooser");
    expect(g_approval_request_count == 0, "silent GetAssertion should not request approval");
    expect(parse_make_credential_auth_flags(response + 1, response_len - 1, &flags),
           "silent assertion response should contain authData flags");
    expect((flags & 0x01U) == 0, "silent assertion must not set the UP flag");
    expect(get_info_has_uint_field(response + 1, response_len - 1, 5, 3),
           "silent multi-match GetAssertion should report numberOfCredentials");
    expect(app.assertion_queue.active,
           "silent multi-match GetAssertion should seed the GetNextAssertion queue");
    expect(app.assertion_queue.count == 3, "silent multi-match queue should include all matches");
    expect(app.assertion_queue.index == 1, "silent multi-match queue should start at next match");

    next_response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              next_response, sizeof(next_response));

    expect(next_response_len > 1, "silent queued GetNextAssertion should return an assertion");
    expect(next_response[0] == ZF_CTAP_SUCCESS,
           "silent queued GetNextAssertion should succeed on same channel");
}

static void test_get_assertion_multi_match_selection_denied_returns_operation_denied(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);
    g_selection_request_ok = false;
    g_approval_state = ZfApprovalDenied;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len == 1, "denied chooser GetAssertion should return status only");
    expect(response[0] == ZF_CTAP_ERR_OPERATION_DENIED,
           "denied chooser GetAssertion should return operation denied");
    expect(g_selection_request_count == 1, "denied chooser should still use the selection flow");
    expect(g_approval_request_count == 0, "denied chooser should not fall back to approval");
}

static void test_get_assertion_multi_match_selection_cancel_returns_keepalive_cancel(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);
    g_selection_request_ok = false;
    g_approval_state = ZfApprovalCanceled;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len == 1, "cancelled chooser GetAssertion should return status only");
    expect(response[0] == ZF_CTAP_ERR_KEEPALIVE_CANCEL,
           "cancelled chooser GetAssertion should return keepalive cancel");
}

static void test_get_assertion_multi_match_selection_timeout_returns_operation_denied(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);
    g_selection_request_ok = false;
    g_approval_state = ZfApprovalTimedOut;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len == 1, "timed out chooser GetAssertion should return status only");
    expect(response[0] == ZF_CTAP_ERR_OPERATION_DENIED,
           "timed out chooser GetAssertion should return operation denied");
}

static void test_get_assertion_multi_match_keeps_account_selection_when_auto_accept_is_on(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_auto_accept_requests(&app);
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);
    g_selection_index = 0;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "chooser GetAssertion should still succeed with auto-accept");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "chooser GetAssertion should still return success with auto-accept");
    expect(g_selection_request_count == 1,
           "auto-accept should not bypass discoverable account selection");
    expect(g_approval_request_count == 0,
           "chooser flow should not route through the approval prompt");
}

static void test_get_assertion_allow_list_does_not_open_account_selection(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    uint8_t next_response[512];
    size_t response_len = 0;
    size_t next_response_len = 0;
    const uint8_t *credential_id = NULL;
    size_t credential_id_len = 0;
    bool user_selected = true;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);
    app.store.records[0].created_at = 20;
    app.store.records[1].created_at = 10;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request,
        encode_get_assertion_ctap_request_with_two_allow_list_entries(
            request, sizeof(request), app.store.records[0].credential_id,
            app.store.records[0].credential_id_len, app.store.records[1].credential_id,
            app.store.records[1].credential_id_len),
        response, sizeof(response));

    expect(response_len > 1, "allowList GetAssertion should still succeed");
    expect(response[0] == ZF_CTAP_SUCCESS, "allowList GetAssertion should return success");
    expect(g_selection_request_count == 0,
           "allowList GetAssertion should not use account selection");
    expect(g_approval_request_count == 1,
           "allowList GetAssertion should keep the approval-only path");
    expect(parse_assertion_response_credential_id(response + 1, response_len - 1, &credential_id,
                                                  &credential_id_len),
           "allowList GetAssertion should include a credential descriptor");
    expect(credential_id_len == app.store.records[0].credential_id_len,
           "allowList GetAssertion should return the selected credential length");
    expect(memcmp(credential_id, app.store.records[0].credential_id, credential_id_len) == 0,
           "allowList GetAssertion should return one matching credential");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 5),
           "allowList GetAssertion should omit numberOfCredentials for multiple matches");
    expect(!cbor_map_uint_key_bool_value(response + 1, response_len - 1, 6, &user_selected),
           "allowList GetAssertion should omit userSelected");
    expect(!app.assertion_queue.active,
           "allowList GetAssertion should not seed the GetNextAssertion queue");

    next_response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              next_response, sizeof(next_response));

    expect(next_response_len == 1, "allowList GetNextAssertion should return status only");
    expect(next_response[0] == ZF_CTAP_ERR_NOT_ALLOWED,
           "allowList GetNextAssertion should be rejected when no queue was seeded");
}

static void seed_assertion_queue_record_full(ZfCredentialRecord *record, uint8_t id_byte,
                                             uint32_t sign_count) {
    memset(record, 0, sizeof(*record));
    record->in_use = true;
    record->resident_key = true;
    strcpy(record->file_name, "test-record");
    strcpy(record->rp_id, "example.com");
    memcpy(record->user_id, "user", 4);
    record->user_id_len = 4;
    record->credential_id_len = ZF_CREDENTIAL_ID_LEN;
    memset(record->credential_id, id_byte, ZF_CREDENTIAL_ID_LEN);
    record->sign_count = sign_count;
}

static void seed_assertion_queue_record_index(ZfCredentialIndexEntry *record, uint8_t id_byte,
                                              uint32_t sign_count) {
    memset(record, 0, sizeof(*record));
    record->in_use = true;
    record->resident_key = true;
    strcpy(record->rp_id, "example.com");
    strcpy(record->file_name, "test-record");
    memcpy(record->user_id, "user", 4);
    record->user_id_len = 4;
    record->credential_id_len = ZF_CREDENTIAL_ID_LEN;
    memset(record->credential_id, id_byte, ZF_CREDENTIAL_ID_LEN);
    record->sign_count = sign_count;
}

static void test_get_next_assertion_rejects_wrong_cid(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t response[256];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);

    app.assertion_queue.active = true;
    app.assertion_queue.session_id = 0x01020304;
    app.assertion_queue.count = 2;
    app.assertion_queue.index = 1;
    app.assertion_queue.expires_at = g_fake_tick + ZF_ASSERTION_QUEUE_TIMEOUT_MS;
    strcpy(app.assertion_queue.request.rp_id, "example.com");
    memcpy(app.assertion_queue.request.client_data_hash,
           (const uint8_t[ZF_CLIENT_DATA_HASH_LEN]){0x5A}, ZF_CLIENT_DATA_HASH_LEN);
    app.assertion_queue.record_indices[0] = 0;
    app.assertion_queue.record_indices[1] = 1;

    response_len =
        zerofido_handle_ctap2(&app, 0xAABBCCDD, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              response, sizeof(response));

    expect(response_len == 1, "wrong-CID GetNextAssertion should only return status");
    expect(response[0] == ZF_CTAP_ERR_INVALID_CHANNEL,
           "wrong-CID GetNextAssertion should be rejected as invalid channel");
    expect(app.assertion_queue.active,
           "wrong-CID rejection should preserve the queued assertion state");
}

static void test_get_next_assertion_refreshes_queue_expiry(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t response[256];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 3;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);
    seed_assertion_queue_record(&app.store.records[2], 0x33, 6);

    app.assertion_queue.active = true;
    app.assertion_queue.session_id = 0x01020304;
    app.assertion_queue.count = 3;
    app.assertion_queue.index = 1;
    app.assertion_queue.expires_at = g_fake_tick + 50;
    strcpy(app.assertion_queue.request.rp_id, "example.com");
    memcpy(app.assertion_queue.request.client_data_hash,
           (const uint8_t[ZF_CLIENT_DATA_HASH_LEN]){0x6B}, ZF_CLIENT_DATA_HASH_LEN);
    for (size_t i = 0; i < app.store.count; ++i) {
        app.assertion_queue.record_indices[i] = (uint16_t)i;
    }

    g_fake_tick = 1025;
    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              response, sizeof(response));

    expect(response_len > 1, "GetNextAssertion should return a response body on success");
    expect(response[0] == ZF_CTAP_SUCCESS, "GetNextAssertion should succeed for the active CID");
    expect(app.assertion_queue.active, "queue should remain active while more assertions remain");
    expect(app.assertion_queue.index == 2, "GetNextAssertion should advance the queue index");
    expect(app.assertion_queue.expires_at == g_fake_tick + ZF_ASSERTION_QUEUE_TIMEOUT_MS,
           "GetNextAssertion should refresh the queue expiry after success");
}

static void test_get_next_assertion_rejects_empty_queue(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t response[256];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              response, sizeof(response));

    expect(response_len == 1, "empty GetNextAssertion queue should only return status");
    expect(response[0] == ZF_CTAP_ERR_NOT_ALLOWED,
           "empty GetNextAssertion queue should be rejected");
    expect(!app.assertion_queue.active, "empty queue rejection should leave the queue cleared");
}

static void test_get_next_assertion_rejects_expired_queue(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t response[256];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);

    app.assertion_queue.active = true;
    app.assertion_queue.session_id = 0x01020304;
    app.assertion_queue.count = 2;
    app.assertion_queue.index = 1;
    app.assertion_queue.expires_at = g_fake_tick - 1;
    strcpy(app.assertion_queue.request.rp_id, "example.com");
    memcpy(app.assertion_queue.request.client_data_hash,
           (const uint8_t[ZF_CLIENT_DATA_HASH_LEN]){0x6C}, ZF_CLIENT_DATA_HASH_LEN);
    app.assertion_queue.record_indices[0] = 0;
    app.assertion_queue.record_indices[1] = 1;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              response, sizeof(response));

    expect(response_len == 1, "expired GetNextAssertion queue should only return status");
    expect(response[0] == ZF_CTAP_ERR_NOT_ALLOWED,
           "expired GetNextAssertion queue should be rejected");
    expect(!app.assertion_queue.active, "expired queue rejection should clear queue state");
    expect(app.assertion_queue.count == 0, "expired queue rejection should reset queue count");
}

static void test_get_next_assertion_clears_final_queue_state(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t response[256];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);

    app.assertion_queue.active = true;
    app.assertion_queue.session_id = 0x01020304;
    app.assertion_queue.count = 2;
    app.assertion_queue.index = 1;
    app.assertion_queue.expires_at = g_fake_tick + ZF_ASSERTION_QUEUE_TIMEOUT_MS;
    strcpy(app.assertion_queue.request.rp_id, "example.com");
    memcpy(app.assertion_queue.request.client_data_hash,
           (const uint8_t[ZF_CLIENT_DATA_HASH_LEN]){0x6D}, ZF_CLIENT_DATA_HASH_LEN);
    app.assertion_queue.record_indices[0] = 0;
    app.assertion_queue.record_indices[1] = 1;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              response, sizeof(response));

    expect(response_len > 1, "final queued GetNextAssertion should still return a body");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "final queued GetNextAssertion should succeed for the active CID");
    expect(!app.assertion_queue.active, "final queued assertion should clear queue state");
    expect(app.assertion_queue.index == 0, "final queued assertion should reset queue index");
    expect(app.store.records[1].sign_count == 6,
           "final queued assertion should persist the advanced sign counter");
}

static void test_get_next_assertion_missing_record_clears_queue(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t response[256];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 1;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);

    app.assertion_queue.active = true;
    app.assertion_queue.session_id = 0x01020304;
    app.assertion_queue.count = 2;
    app.assertion_queue.index = 1;
    app.assertion_queue.expires_at = g_fake_tick + ZF_ASSERTION_QUEUE_TIMEOUT_MS;
    strcpy(app.assertion_queue.request.rp_id, "example.com");
    memcpy(app.assertion_queue.request.client_data_hash,
           (const uint8_t[ZF_CLIENT_DATA_HASH_LEN]){0x6E}, ZF_CLIENT_DATA_HASH_LEN);
    app.assertion_queue.record_indices[0] = 0;
    app.assertion_queue.record_indices[1] = 99;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              response, sizeof(response));

    expect(response_len == 1,
           "missing queued credential state should only return a GetNextAssertion status");
    expect(response[0] == ZF_CTAP_ERR_NOT_ALLOWED,
           "missing queued credential state should fail closed");
    expect(!app.assertion_queue.active,
           "missing queued credential state should clear the queue before returning");
}

static void test_get_next_assertion_rejects_trailing_payload(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t response[256];
    size_t response_len = 0;

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    app.store.count = 2;
    seed_assertion_queue_record(&app.store.records[0], 0x11, 4);
    seed_assertion_queue_record(&app.store.records[1], 0x22, 5);

    app.assertion_queue.active = true;
    app.assertion_queue.session_id = 0x01020304;
    app.assertion_queue.count = 2;
    app.assertion_queue.index = 1;
    app.assertion_queue.expires_at = g_fake_tick + ZF_ASSERTION_QUEUE_TIMEOUT_MS;
    strcpy(app.assertion_queue.request.rp_id, "example.com");
    memcpy(app.assertion_queue.request.client_data_hash,
           (const uint8_t[ZF_CLIENT_DATA_HASH_LEN]){0x7C}, ZF_CLIENT_DATA_HASH_LEN);
    app.assertion_queue.record_indices[0] = 0;
    app.assertion_queue.record_indices[1] = 1;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion, 0xAA},
                              2, response, sizeof(response));

    expect(response_len == 1, "GetNextAssertion trailing payload should return only status");
    expect(response[0] == ZF_CTAP_ERR_INVALID_LENGTH,
           "GetNextAssertion trailing payload should be rejected");
    expect(app.assertion_queue.active,
           "queue should remain intact after malformed GetNextAssertion");
    expect(app.assertion_queue.index == 1,
           "malformed GetNextAssertion should not consume queued assertions");
    expect(app.store.records[1].sign_count == 5,
           "malformed GetNextAssertion should not advance sign counters");
}

static void test_get_info_rejects_trailing_payload(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[128];
    size_t response_len = 0;

    test_storage_reset();
    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo, 0x00}, 2,
                              response, sizeof(response));

    expect(response_len == 1, "GetInfo trailing payload should return only status");
    expect(response[0] == ZF_CTAP_ERR_INVALID_LENGTH,
           "GetInfo trailing payload should be rejected");
    expect(test_log_contains("cmd=GI status=LEN body=0"),
           "failed GetInfo should emit a serial diagnostic");
    expect(g_status_update_count == 0, "failed GetInfo diagnostic should not update UI status");
}

static void test_get_info_success_updates_status_banner(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "GetInfo should succeed");
    expect(test_log_contains("cmd=GI status=OK"),
           "successful GetInfo should emit a serial diagnostic");
    expect(g_status_update_count == 0, "successful GetInfo diagnostic should not update UI status");
}

static void test_get_info_advertises_cred_protect_extension(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "GetInfo should succeed");
    expect(get_info_has_cred_protect_extension(response + 1, response_len - 1),
           "GetInfo should advertise the credProtect extension");
}

static void test_get_info_advertises_hmac_secret_extension(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "GetInfo should succeed");
    expect(get_info_has_extension(response + 1, response_len - 1, "hmac-secret"),
           "GetInfo should advertise the hmac-secret extension");
}

static void test_get_info_advertises_fido_2_0_without_fido_2_1(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;
    bool pin_uv_auth_token = true;

    test_storage_reset();
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "GetInfo should succeed");
    expect(get_info_has_version(response + 1, response_len - 1, "FIDO_2_0"),
           "GetInfo should advertise FIDO_2_0");
    expect(!get_info_has_version(response + 1, response_len - 1, "FIDO_2_1"),
           "CTAP 2.0 profile should not advertise FIDO_2_1");
    expect(
        !get_info_option_bool(response + 1, response_len - 1, "pinUvAuthToken", &pin_uv_auth_token),
        "CTAP 2.0 profile should not advertise pinUvAuthToken");
    expect(get_info_has_pin_uv_auth_protocol(response + 1, response_len - 1, 1),
           "CTAP 2.0 profile should advertise PIN/UV auth protocol 1");
    expect(!get_info_has_pin_uv_auth_protocol(response + 1, response_len - 1, 2),
           "CTAP 2.0 profile should not advertise PIN/UV auth protocol 2");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 9),
           "CTAP 2.0 profile should omit CTAP 2.1 transports");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 10),
           "CTAP 2.0 profile should omit CTAP 2.1 algorithms");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 13),
           "CTAP 2.0 profile should omit CTAP 2.1 minPINLength");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 14),
           "CTAP 2.0 profile should omit CTAP 2.1 firmwareVersion");
}

static void test_get_info_nfc_ctap2_0_advertises_transport_compactly(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    zf_runtime_config_load_defaults(&app.runtime_config);
    app.runtime_config.transport_mode = ZfTransportModeNfc;
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "NFC GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "NFC GetInfo should succeed");
    expect(get_info_has_version(response + 1, response_len - 1, "FIDO_2_0"),
           "NFC CTAP 2.0 profile should advertise FIDO_2_0");
    expect(!get_info_has_version(response + 1, response_len - 1, "FIDO_2_1"),
           "NFC CTAP 2.0 profile should not advertise FIDO_2_1");
    expect(cbor_map_contains_uint_key(response + 1, response_len - 1, 9),
           "NFC CTAP 2.0 profile should advertise transports");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 10),
           "NFC CTAP 2.0 profile should keep optional algorithms out of compact GetInfo");
    expect(contains_text(response + 1, response_len - 1, "nfc"),
           "NFC GetInfo transport list should include nfc");
    expect(response_len + 2U <= 120U,
           "NFC CTAP 2.0 GetInfo APDU response should stay below the conservative frame cap");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 13),
           "NFC CTAP 2.0 profile should omit CTAP 2.1 minPINLength");
    expect(!cbor_map_contains_uint_key(response + 1, response_len - 1, 14),
           "NFC CTAP 2.0 profile should omit CTAP 2.1 firmwareVersion");
}

static void test_get_info_experimental_2_1_advertises_pin_uv_auth_token(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;
    bool pin_uv_auth_token = false;
    bool make_cred_uv_not_rqd = false;
    bool client_pin = false;
    const uint64_t expected_protocols[] = {2, 1};

    test_storage_reset();
    test_enable_fido2_1_experimental(&app);
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "experimental 2.1 GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "experimental 2.1 GetInfo should succeed");
    expect(get_info_has_version(response + 1, response_len - 1, "FIDO_2_0"),
           "experimental 2.1 GetInfo should keep CTAP 2.0 compatibility");
    expect(get_info_has_version(response + 1, response_len - 1, "FIDO_2_1"),
           "experimental 2.1 GetInfo should advertise FIDO_2_1");
    expect(get_info_option_bool(response + 1, response_len - 1, "pinUvAuthToken",
                                &pin_uv_auth_token) &&
               pin_uv_auth_token,
           "experimental 2.1 GetInfo should advertise pinUvAuthToken=true");
    expect(get_info_option_bool(response + 1, response_len - 1, "makeCredUvNotRqd",
                                &make_cred_uv_not_rqd) &&
               make_cred_uv_not_rqd,
           "experimental 2.1 GetInfo should advertise makeCredUvNotRqd=true");
    expect(get_info_option_bool(response + 1, response_len - 1, "clientPin", &client_pin) &&
               client_pin,
           "experimental 2.1 GetInfo should require and advertise a configured PIN");
    expect(get_info_pin_uv_auth_protocols_equal(response + 1, response_len - 1, expected_protocols,
                                                sizeof(expected_protocols) /
                                                    sizeof(expected_protocols[0])),
           "experimental 2.1 GetInfo should prefer protocol 2 before protocol 1");
    expect(cbor_map_contains_uint_key(response + 1, response_len - 1, 9),
           "experimental 2.1 GetInfo should advertise transports");
    expect(cbor_map_contains_uint_key(response + 1, response_len - 1, 10),
           "experimental 2.1 GetInfo should advertise algorithms");
    expect(get_info_has_uint_field(response + 1, response_len - 1, 13, ZF_MIN_PIN_LENGTH),
           "experimental 2.1 GetInfo should advertise minPINLength");
    expect(get_info_has_uint_field(response + 1, response_len - 1, 14, ZF_FIRMWARE_VERSION),
           "experimental 2.1 GetInfo should advertise firmwareVersion");
}

static void test_get_info_advertises_u2f(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "GetInfo should succeed");
    expect(get_info_has_version(response + 1, response_len - 1, "U2F_V2"),
           "GetInfo should advertise legacy U2F compatibility");
}

static void test_get_info_reports_firmware_version(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[512];
    size_t response_len = 0;

    test_storage_reset();
    test_enable_fido2_1_experimental(&app);
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetInfo}, 1,
                                         response, sizeof(response));

    expect(response_len > 1, "experimental 2.1 GetInfo should return a CBOR body");
    expect(response[0] == ZF_CTAP_SUCCESS, "experimental 2.1 GetInfo should succeed");
    expect(get_info_has_uint_field(response + 1, response_len - 1, 14, ZF_FIRMWARE_VERSION),
           "experimental 2.1 GetInfo should report the firmwareVersion field");
}

static void test_ctap_reset_succeeds_and_wipes_runtime_state(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    Storage storage = {0};
    uint8_t response[128];
    size_t response_len = 0;
    ZfCredentialRecord record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = (FuriMutex *)0x1;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;

    expect(test_zf_store_init(&storage, &app.store), "init store before reset");
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state before reset");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN before reset");
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential before reset");
    expect(test_zf_store_add_record(&storage, &app.store, &record),
           "persist credential before reset");
    app.assertion_queue.active = true;
    app.assertion_queue.count = 1;

    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdReset}, 1,
                                         response, sizeof(response));

    expect(response_len == 1, "Reset should return status only");
    expect(response[0] == ZF_CTAP_SUCCESS, "Reset should succeed after approval");
    expect(g_approval_request_count == 1, "Reset should require user presence approval");
    expect(zf_store_count_saved(&app.store) == 0, "Reset should wipe stored credentials");
    expect(!app.pin_state.pin_set, "Reset should clear the PIN state");
    expect(!g_pin_file_exists, "Reset should remove persisted PIN state");
    expect(!app.assertion_queue.active && app.assertion_queue.count == 0,
           "Reset should clear pending assertion queue state");
    expect(test_log_contains("cmd=RST status=OK"),
           "successful Reset should emit a serial diagnostic");
    expect(g_status_update_count == 0, "Reset diagnostic should not update UI status");
}

static void test_ctap_reset_auto_accept_bypasses_approval_prompt(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    Storage storage = {0};
    uint8_t response[128];
    size_t response_len = 0;
    ZfCredentialRecord record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = (FuriMutex *)0x1;
    test_enable_auto_accept_requests(&app);

    expect(test_zf_store_init(&storage, &app.store), "init store before auto-accept reset");
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state before auto-accept reset");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN before auto-accept reset");
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential before auto-accept reset");
    expect(test_zf_store_add_record(&storage, &app.store, &record),
           "persist credential before auto-accept reset");

    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdReset}, 1,
                                         response, sizeof(response));

    expect(response_len == 1, "auto-accept Reset should return status only");
    expect(response[0] == ZF_CTAP_SUCCESS, "auto-accept Reset should succeed");
    expect(g_approval_request_count == 0, "auto-accept Reset should bypass the approval prompt");
}

static void test_ctap_reset_succeeds_when_u2f_reinit_fails(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    Storage storage = {0};
    uint8_t response[128];
    size_t response_len = 0;
    ZfCredentialRecord record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = (FuriMutex *)0x1;
    zf_runtime_config_load_defaults(&app.runtime_config);
    zf_runtime_config_resolve_capabilities(&app.runtime_config, &app.capabilities);
    app.capabilities_resolved = true;
    app.capabilities.u2f_enabled = true;
    g_u2f_adapter_init_ok = false;

    expect(test_zf_store_init(&storage, &app.store), "init store before U2F-failing reset");
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state before U2F-failing reset");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN before U2F-failing reset");
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential before U2F-failing reset");
    expect(test_zf_store_add_record(&storage, &app.store, &record),
           "persist credential before U2F-failing reset");

    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdReset}, 1,
                                         response, sizeof(response));

    expect(response_len == 1, "Reset with failed U2F reinit should return status only");
    expect(response[0] == ZF_CTAP_SUCCESS, "Reset should not fail when U2F reinit fails");
    expect(zf_store_count_saved(&app.store) == 0,
           "Reset should still wipe stored credentials when U2F reinit fails");
    expect(!app.pin_state.pin_set, "Reset should still clear PIN when U2F reinit fails");
    expect(g_u2f_adapter_deinit_count == 1, "Reset should deinit U2F before wiping");
    expect(g_u2f_adapter_init_count == 1, "Reset should attempt best-effort U2F reinit");
}

static void test_ctap_reset_rejects_trailing_payload(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[128];
    size_t response_len = 0;

    test_storage_reset();
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdReset, 0xAA},
                                         2, response, sizeof(response));

    expect(response_len == 1, "Reset trailing payload should return status only");
    expect(response[0] == ZF_CTAP_ERR_INVALID_LENGTH, "Reset trailing payload should be rejected");
    expect(g_approval_request_count == 0, "malformed Reset should fail before approval");
}

static void test_client_pin_get_key_agreement_updates_status_banner(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[128];
    uint8_t response[512];
    size_t request_len = 0;

    test_storage_reset();
    request[0] = ZfCtapeCmdClientPin;
    request_len = encode_client_pin_get_key_agreement_request(request + 1, sizeof(request) - 1) + 1;
    zerofido_handle_ctap2(&app, 0x01020304, request, request_len, response, sizeof(response));

    expect(response[0] == ZF_CTAP_SUCCESS, "ClientPIN getKeyAgreement should succeed");
    expect(test_log_contains("cmd=CP-GA status=OK"),
           "ClientPIN getKeyAgreement should emit a serial diagnostic");
    expect(g_status_update_count == 0, "ClientPIN diagnostic should not update UI status");
}

static void test_selection_touch_succeeds_and_updates_status_banner(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[128];
    size_t response_len = 0;

    test_storage_reset();
    test_enable_fido2_1_experimental(&app);
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdSelection},
                                         1, response, sizeof(response));

    expect(response_len == 1, "Selection should return status only");
    expect(response[0] == ZF_CTAP_SUCCESS, "Selection should succeed after approval");
    expect(g_approval_request_count == 1, "Selection should request touch approval");
    expect(test_log_contains("cmd=SEL status=OK"), "Selection should emit a serial diagnostic");
    expect(g_status_update_count == 0, "Selection diagnostic should not update UI status");
}

static void test_selection_touch_rejects_default_ctap2_0_profile(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[128];
    size_t response_len = 0;

    test_storage_reset();
    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdSelection},
                                         1, response, sizeof(response));

    expect(response_len == 1, "CTAP 2.0 Selection should return status only");
    expect(response[0] == ZF_CTAP_ERR_INVALID_COMMAND,
           "CTAP 2.0 Selection should be rejected as an unsupported command");
    expect(g_approval_request_count == 0, "CTAP 2.0 Selection should fail before approval");
}

static void test_selection_touch_auto_accept_bypasses_approval_prompt(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t response[128];
    size_t response_len = 0;

    test_storage_reset();
    test_enable_auto_accept_fido2_1_experimental(&app);

    response_len = zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdSelection},
                                         1, response, sizeof(response));

    expect(response_len == 1, "auto-accept Selection should return status only");
    expect(response[0] == ZF_CTAP_SUCCESS, "auto-accept Selection should succeed");
    expect(g_approval_request_count == 0,
           "auto-accept Selection should bypass the approval prompt");
}
