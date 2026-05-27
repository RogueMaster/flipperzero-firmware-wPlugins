static void test_pin_auth_block_state_clears_on_reinit_but_keeps_retries(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set baseline PIN");

    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_INVALID,
           "first wrong PIN should be invalid");
    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_INVALID,
           "second wrong PIN should be invalid");
    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_AUTH_BLOCKED,
           "third wrong PIN should auth-block");

    expect(zerofido_pin_init(&storage, &restored), "re-init PIN state");
    expect(!restored.pin_auth_blocked, "re-init should clear temporary auth-block state");
    expect(restored.pin_consecutive_mismatches == 0,
           "re-init should clear temporary pinAuth mismatch count");
    expect(restored.pin_retries == (ZF_PIN_RETRIES_MAX - 3),
           "re-init should keep the decremented retry counter");
    expect(zerofido_pin_verify_plaintext(&storage, &restored, "1234") == ZF_CTAP_SUCCESS,
           "correct PIN should work again after re-init clears the temporary auth block");
    expect(restored.pin_retries == ZF_PIN_RETRIES_MAX,
           "successful PIN verification after re-init should reset retries");
}

static void test_pin_plaintext_accepts_ctap_max_utf8_length(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    char max_pin[ZF_PIN_NEW_PIN_BLOCK_MAX_LEN];
    char too_long_pin[ZF_PIN_NEW_PIN_BLOCK_MAX_LEN + 1U];
    uint8_t max_pin_block[ZF_PIN_NEW_PIN_BLOCK_MAX_LEN] = {0};
    size_t unpadded_len = 0;

    test_storage_reset();
    memset(max_pin, '7', sizeof(max_pin) - 1U);
    max_pin[sizeof(max_pin) - 1U] = '\0';
    memset(too_long_pin, '8', sizeof(too_long_pin) - 1U);
    too_long_pin[sizeof(too_long_pin) - 1U] = '\0';
    memset(max_pin_block, '9', ZF_PIN_NEW_PIN_BLOCK_MAX_LEN - 1U);

    expect(zerofido_pin_init(&storage, &state), "init PIN state for max-length PIN");
    expect(zerofido_pin_set_plaintext(&storage, &state, max_pin) == ZF_CTAP_SUCCESS,
           "63-byte PIN should be accepted");
    expect(zerofido_pin_verify_plaintext(&storage, &state, max_pin) == ZF_CTAP_SUCCESS,
           "63-byte PIN should verify");
    expect(zf_pin_validate_plaintext_block(max_pin_block, sizeof(max_pin_block), &unpadded_len),
           "63-byte padded newPinEnc block should be accepted");
    expect(unpadded_len == ZF_PIN_NEW_PIN_BLOCK_MAX_LEN - 1U,
           "max PIN block should report 63 unpadded bytes");
    expect(zf_pin_validate_plaintext_policy((const uint8_t *)too_long_pin, strlen(too_long_pin)) ==
               ZF_CTAP_ERR_PIN_POLICY_VIOLATION,
           "64-byte PIN should be rejected");
}

static void test_pin_resume_auth_attempts_clears_persisted_block(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0};
    uint8_t pin_auth[ZF_PIN_AUTH_LEN];
    bool uv_verified = false;

    test_storage_reset();
    memset(pin_auth, 0x01, sizeof(pin_auth));
    expect(zerofido_pin_init(&storage, &state), "init PIN state");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set baseline PIN");
    mark_pin_token_issued(&state);

    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "first pinAuth mismatch should be invalid");
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "second pinAuth mismatch should be invalid");
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_BLOCKED,
           "third pinAuth mismatch should auth-block");
    expect(zerofido_pin_resume_auth_attempts(&storage, &state),
           "resume PIN attempts should persist");

    expect(zerofido_pin_init(&storage, &restored), "re-init PIN state after resume");
    expect(!restored.pin_auth_blocked, "resumed PIN state should not stay auth-blocked");
    expect(restored.pin_consecutive_mismatches == 0,
           "resumed PIN mismatch count should be cleared");
}

static void test_pin_clear_removes_persisted_state_and_resets_runtime(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for clear test");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for clear test");
    expect(state.pin_set, "PIN should be set before clear");
    expect(g_pin_file_exists, "PIN file should exist before clear");

    expect(zerofido_pin_clear(&storage, &state), "clear PIN should succeed");
    expect(!state.pin_set, "runtime PIN state should be cleared");
    expect(state.pin_retries == ZF_PIN_RETRIES_MAX, "clear should restore retry count");
    expect(!state.pin_auth_blocked, "clear should remove auth block state");
    expect(!state.pin_token_active, "clear should invalidate the active pin token");
    expect(!g_pin_file_exists, "clear should remove the persisted PIN file");

    expect(zerofido_pin_init(&storage, &restored), "re-init after clear should succeed");
    expect(!restored.pin_set, "re-init after clear should not restore a PIN");
}

static void test_pin_init_rejects_unsupported_format_state(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfPinFileRecordBase invalid = {0};

    test_storage_reset();
    invalid.magic = ZF_PIN_FILE_MAGIC;
    invalid.version = 0U;
    invalid.pin_retries = 5;
    invalid.pin_consecutive_mismatches = 2;
    invalid.flags = ZF_PIN_FILE_FLAG_AUTH_BLOCKED;
    memset(invalid.iv, 0xA5, sizeof(invalid.iv));
    memset(invalid.encrypted_pin_hash, 0x5C, sizeof(invalid.encrypted_pin_hash));
    memcpy(g_pin_file_data, &invalid, sizeof(invalid));
    g_pin_file_size = sizeof(invalid);
    g_pin_file_exists = true;

    expect(!zerofido_pin_init(&storage, &state), "unsupported PIN state format should be rejected");
    expect(zerofido_pin_init_with_result(&storage, &state) == ZfPinInitInvalidPersistedState,
           "unsupported PIN state format should be classified as invalid persisted state");
}

static void test_pin_init_rejects_unsupported_current_format_version(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for unsupported-version test");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for unsupported-version test");

    ((ZfPinFileRecord *)g_pin_file_data)->base.version = 0U;
    expect(!zerofido_pin_init(&storage, &restored),
           "current-format PIN state with unsupported version should be rejected");
}

static void test_pin_init_rejects_tampered_retry_state(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for tamper test");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for tamper test");

    ((ZfPinFileRecord *)g_pin_file_data)->base.pin_retries--;
    expect(!zerofido_pin_init(&storage, &restored),
           "tampered PIN retry state should fail sealed-state verification");
}

static void test_pin_auth_blocks_after_three_mismatches(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0};
    uint8_t pin_auth[ZF_PIN_AUTH_LEN];
    bool uv_verified = false;

    test_storage_reset();
    state.pin_set = true;
    memset(state.pin_token, 0, sizeof(state.pin_token));
    mark_pin_token_issued(&state);
    memset(pin_auth, 0x01, sizeof(pin_auth));

    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "first pinAuth mismatch should be invalid");
    expect(!uv_verified, "uv flag should stay false after first pinAuth mismatch");
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "second pinAuth mismatch should be invalid");
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_BLOCKED,
           "third pinAuth mismatch should auth-block");
}

static void test_ui_format_approval_header_prefixes_protocol(void) {
    char out[32];

    zf_ui_format_approval_header(out, sizeof(out), ZfUiProtocolFido2, "Authenticate");
    expect(strcmp(out, "Use passkey") == 0, "FIDO2 approvals should use friendly passkey text");
}

static void test_ui_format_approval_body_uses_protocol_specific_target_label(void) {
    char out[160];

    zf_ui_format_approval_body(out, sizeof(out), ZfUiProtocolFido2, "example.com", "User: alice");
    expect(!contains_text((const uint8_t *)out, strlen(out), "Protocol:"),
           "FIDO2 approval body should hide protocol jargon");
    expect(contains_text((const uint8_t *)out, strlen(out), "Website: example.com"),
           "FIDO2 approval body should use website label");
}

static void test_ui_format_fido2_credential_label_uses_passkey_text(void) {
    ZfCredentialRecord record = {0};
    char out[128];

    record.in_use = true;
    record.resident_key = true;
    strncpy(record.rp_id, "example.com", sizeof(record.rp_id) - 1);
    strncpy(record.user_name, "alice", sizeof(record.user_name) - 1);
    strncpy(record.user_display_name, "Alice Example", sizeof(record.user_display_name) - 1);
    zf_ui_format_fido2_credential_label(&record, out, sizeof(out));
    expect(strcmp(out, "Alice Example | example.com") == 0,
           "passkey labels should prefer display name and website");

    record.resident_key = false;
    record.user_name[0] = '\0';
    record.user_display_name[0] = '\0';
    zf_ui_format_fido2_credential_label(&record, out, sizeof(out));
    expect(strcmp(out, "example.com") == 0, "passkey labels should fall back to website");
}

static void test_ui_format_fido2_credential_detail_uses_user_facing_text(void) {
    ZfCredentialRecord record = {0};
    char out[256];

    record.in_use = true;
    record.resident_key = false;
    record.credential_id_len = ZF_CREDENTIAL_ID_LEN;
    memset(record.credential_id, 0xab, sizeof(record.credential_id));
    strncpy(record.rp_id, "example.com", sizeof(record.rp_id) - 1);
    strncpy(record.user_name, "alice", sizeof(record.user_name) - 1);
    record.created_at = 42;

    zf_ui_format_fido2_credential_detail(&record, out, sizeof(out));
    expect(contains_text((const uint8_t *)out, strlen(out), "Website: example.com"),
           "passkey detail should show the website");
    expect(contains_text((const uint8_t *)out, strlen(out), "Account: alice"),
           "passkey detail should show the account");
    expect(contains_text((const uint8_t *)out, strlen(out), "Type: Saved passkey"),
           "passkey detail should use user-facing type text");
    expect(!contains_text((const uint8_t *)out, strlen(out), "Used:"),
           "passkey detail should not expose the CTAP signature counter as a use count");
}

static void test_ui_hex_encode_truncated_limits_output(void) {
    uint8_t bytes[] = {0xaa, 0xbb, 0xcc, 0xdd};
    char out[5];

    zf_ui_hex_encode_truncated(bytes, sizeof(bytes), out, sizeof(out));
    expect(strcmp(out, "aabb") == 0, "hex encoder should truncate to available output space");
}

static void test_get_pin_token_wrong_pin_regenerates_key_agreement(void) {
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t buffer[256];
    uint8_t response[64];
    size_t response_len = 0;
    uint8_t original_public_x = 0;
    static const uint8_t wrong_pin_hash[ZF_PIN_HASH_LEN] = {
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    };

    app.pin_state.pin_set = true;
    app.pin_state.pin_retries = ZF_PIN_RETRIES_MAX;
    memset(app.pin_state.pin_hash, 0x11, sizeof(app.pin_state.pin_hash));
    expect(zf_crypto_generate_key_agreement_key(&app.pin_state.key_agreement),
           "generate initial key agreement");
    original_public_x = app.pin_state.key_agreement.public_x[0];

    expect(
        zerofido_pin_handle_command(
            &app, buffer,
            encode_client_pin_get_pin_token_request(buffer, sizeof(buffer), wrong_pin_hash, true),
            response, sizeof(response), &response_len) == ZF_CTAP_ERR_PIN_INVALID,
        "wrong PIN should return pin invalid");
    expect(app.pin_state.key_agreement.public_x[0] != original_public_x,
           "wrong PIN should regenerate key agreement");
}

static size_t encode_client_pin_change_pin_request_with_pin_auth(
    uint8_t *buffer, size_t capacity, const uint8_t current_pin_hash[ZF_PIN_HASH_LEN],
    const uint8_t *new_pin_block, size_t new_pin_block_len,
    const uint8_t pin_auth[ZF_PIN_AUTH_LEN]) {
    uint8_t combined[288];
    uint8_t computed_pin_auth[32];
    ZfCborEncoder enc;
    static const uint8_t shared_secret[32] = {[0 ... 31] = 0xAB};

    expect(new_pin_block_len >= 64 && (new_pin_block_len % 16) == 0, "changePin newPinEnc length");
    if (!pin_auth) {
        memcpy(combined, new_pin_block, new_pin_block_len);
        memcpy(combined + new_pin_block_len, current_pin_hash, ZF_PIN_HASH_LEN);
        zf_crypto_hmac_sha256(shared_secret, sizeof(shared_secret), combined,
                              new_pin_block_len + ZF_PIN_HASH_LEN, computed_pin_auth);
        pin_auth = computed_pin_auth;
    }

    expect(zf_cbor_encoder_init(&enc, buffer, capacity), "init clientPin changePin encoder");
    expect(zf_cbor_encode_map(&enc, 6), "encode changePin top-level map");
    expect(zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, 1), "encode pinProtocol");
    expect(zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_uint(&enc, 4), "encode subcommand");
    expect(zf_cbor_encode_uint(&enc, 3), "encode keyAgreement key");
    expect(zf_cbor_encode_map(&enc, 5), "encode keyAgreement map");
    expect(zf_cbor_encode_int(&enc, 1) && zf_cbor_encode_int(&enc, 2), "encode keyAgreement kty");
    expect(zf_cbor_encode_int(&enc, 3) && zf_cbor_encode_int(&enc, -25), "encode keyAgreement alg");
    expect(zf_cbor_encode_int(&enc, -1) && zf_cbor_encode_int(&enc, 1), "encode keyAgreement crv");
    expect(
        zf_cbor_encode_int(&enc, -2) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x11}, ZF_PUBLIC_KEY_LEN),
        "encode keyAgreement x");
    expect(
        zf_cbor_encode_int(&enc, -3) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x22}, ZF_PUBLIC_KEY_LEN),
        "encode keyAgreement y");
    expect(zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_bytes(&enc, pin_auth, ZF_PIN_AUTH_LEN),
           "encode pinAuth");
    expect(zf_cbor_encode_uint(&enc, 5) &&
               zf_cbor_encode_bytes(&enc, new_pin_block, new_pin_block_len),
           "encode newPinEnc");
    expect(zf_cbor_encode_uint(&enc, 6) &&
               zf_cbor_encode_bytes(&enc, current_pin_hash, ZF_PIN_HASH_LEN),
           "encode pinHashEnc");
    return zf_cbor_encoder_size(&enc);
}

static size_t encode_client_pin_change_pin_request(uint8_t *buffer, size_t capacity,
                                                   const uint8_t current_pin_hash[ZF_PIN_HASH_LEN],
                                                   const uint8_t *new_pin_block,
                                                   size_t new_pin_block_len) {
    return encode_client_pin_change_pin_request_with_pin_auth(
        buffer, capacity, current_pin_hash, new_pin_block, new_pin_block_len, NULL);
}

static size_t encode_client_pin_set_pin_request_with_pin_auth(
    uint8_t *buffer, size_t capacity, const uint8_t *new_pin_block, size_t new_pin_block_len,
    const uint8_t pin_auth[ZF_PIN_AUTH_LEN]) {
    uint8_t computed_pin_auth[32];
    ZfCborEncoder enc;
    static const uint8_t shared_secret[32] = {[0 ... 31] = 0xAB};

    expect(new_pin_block_len >= 64 && (new_pin_block_len % 16) == 0, "setPin newPinEnc length");
    if (!pin_auth) {
        zf_crypto_hmac_sha256(shared_secret, sizeof(shared_secret), new_pin_block,
                              new_pin_block_len, computed_pin_auth);
        pin_auth = computed_pin_auth;
    }

    expect(zf_cbor_encoder_init(&enc, buffer, capacity), "init clientPin setPin encoder");
    expect(zf_cbor_encode_map(&enc, 5), "encode setPin top-level map");
    expect(zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, 1), "encode pinProtocol");
    expect(zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_uint(&enc, 3), "encode subcommand");
    expect(zf_cbor_encode_uint(&enc, 3), "encode keyAgreement key");
    expect(zf_cbor_encode_map(&enc, 5), "encode keyAgreement map");
    expect(zf_cbor_encode_int(&enc, 1) && zf_cbor_encode_int(&enc, 2), "encode keyAgreement kty");
    expect(zf_cbor_encode_int(&enc, 3) && zf_cbor_encode_int(&enc, -25), "encode keyAgreement alg");
    expect(zf_cbor_encode_int(&enc, -1) && zf_cbor_encode_int(&enc, 1), "encode keyAgreement crv");
    expect(
        zf_cbor_encode_int(&enc, -2) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x11}, ZF_PUBLIC_KEY_LEN),
        "encode keyAgreement x");
    expect(
        zf_cbor_encode_int(&enc, -3) &&
            zf_cbor_encode_bytes(&enc, (const uint8_t[ZF_PUBLIC_KEY_LEN]){0x22}, ZF_PUBLIC_KEY_LEN),
        "encode keyAgreement y");
    expect(zf_cbor_encode_uint(&enc, 4) && zf_cbor_encode_bytes(&enc, pin_auth, ZF_PIN_AUTH_LEN),
           "encode pinAuth");
    expect(zf_cbor_encode_uint(&enc, 5) &&
               zf_cbor_encode_bytes(&enc, new_pin_block, new_pin_block_len),
           "encode newPinEnc");
    return zf_cbor_encoder_size(&enc);
}

static size_t encode_client_pin_get_key_agreement_request(uint8_t *buffer, size_t capacity) {
    ZfCborEncoder enc;

    expect(zf_cbor_encoder_init(&enc, buffer, capacity), "init clientPin getKeyAgreement encoder");
    expect(zf_cbor_encode_map(&enc, 2), "encode getKeyAgreement top-level map");
    expect(zf_cbor_encode_uint(&enc, 1) && zf_cbor_encode_uint(&enc, 1), "encode pinProtocol");
    expect(zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_uint(&enc, 2), "encode subcommand");
    return zf_cbor_encoder_size(&enc);
}

static void test_change_pin_invalid_new_pin_after_correct_current_pin_resets_retries(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for changePin");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for changePin");
    app.storage = &storage;
    app.pin_state.pin_retries = 2;
    app.pin_state.pin_consecutive_mismatches = 1;
    expect(zf_pin_persist_state(&storage, &app.pin_state), "persist retry state before changePin");

    memcpy(new_pin_block, "5678", 4);
    new_pin_block[4] = 0x00;
    new_pin_block[5] = 0x7F;

    expect(
        zerofido_pin_handle_command(
            &app, request,
            encode_client_pin_change_pin_request(request, sizeof(request), app.pin_state.pin_hash,
                                                 new_pin_block, sizeof(new_pin_block)),
            response, sizeof(response), &response_len) == ZF_CTAP_ERR_PIN_POLICY_VIOLATION,
        "invalid new PIN block should be rejected");
    expect(app.pin_state.pin_retries == ZF_PIN_RETRIES_MAX,
           "verified current PIN should reset retries before new PIN validation");
    expect(app.pin_state.pin_consecutive_mismatches == 0,
           "verified current PIN should clear mismatches before new PIN validation");
}

static void test_change_pin_preserves_key_agreement_on_success(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    ZfP256KeyAgreementKey original_key_agreement = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for changePin keyAgreement continuity");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for changePin keyAgreement continuity");
    app.storage = &storage;
    memcpy(new_pin_block, "5678", 4);
    memcpy(&original_key_agreement, &app.pin_state.key_agreement, sizeof(original_key_agreement));

    expect(
        zerofido_pin_handle_command(
            &app, request,
            encode_client_pin_change_pin_request(request, sizeof(request), app.pin_state.pin_hash,
                                                 new_pin_block, sizeof(new_pin_block)),
            response, sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
        "changePin should succeed before checking keyAgreement continuity");
    expect(memcmp(&app.pin_state.key_agreement, &original_key_agreement,
                  sizeof(original_key_agreement)) == 0,
           "changePin should preserve the active keyAgreement so browser follow-up requests work");
}

static void test_set_pin_invalid_new_pin_block_is_rejected(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for setPin padding test");
    app.storage = &storage;
    memcpy(new_pin_block, "1234", 4);
    new_pin_block[4] = 0x00;
    new_pin_block[5] = 0x7F;

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_set_pin_request_with_pin_auth(
                   request, sizeof(request), new_pin_block, sizeof(new_pin_block), NULL),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_PIN_POLICY_VIOLATION,
           "setPin should reject garbage after the first NUL in newPinEnc");
    expect(!app.pin_state.pin_set, "invalid setPin block should not set a PIN");
}

static void test_set_pin_new_pin_decrypt_failure_returns_pin_auth_invalid(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for setPin failure");
    app.storage = &storage;
    memcpy(new_pin_block, "1234", 4);
    g_fail_crypto_decrypt = true;

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_set_pin_request_with_pin_auth(
                   request, sizeof(request), new_pin_block, sizeof(new_pin_block), NULL),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "setPin newPinEnc decrypt failure should return pin auth invalid");
}

static void test_set_pin_rejects_oversized_new_pin_block(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[640];
    uint8_t response[64];
    uint8_t new_pin_block[ZF_PIN_NEW_PIN_BLOCK_MAX_LEN + 16U] = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for long setPin block");
    app.storage = &storage;
    memcpy(new_pin_block, "1234", 4);

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_set_pin_request_with_pin_auth(
                   request, sizeof(request), new_pin_block, sizeof(new_pin_block), NULL),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_PARAMETER,
           "setPin should reject oversized protocol 1 newPinEnc blocks");
    expect(!app.pin_state.pin_set, "oversized setPin block should not configure a PIN");
}

static void test_set_pin_preserves_key_agreement_on_success(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    ZfP256KeyAgreementKey original_key_agreement = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for setPin keyAgreement continuity");
    app.storage = &storage;
    memcpy(new_pin_block, "1234", 4);
    memcpy(&original_key_agreement, &app.pin_state.key_agreement, sizeof(original_key_agreement));

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_set_pin_request_with_pin_auth(
                   request, sizeof(request), new_pin_block, sizeof(new_pin_block), NULL),
               response, sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
           "setPin should succeed before checking keyAgreement continuity");
    expect(memcmp(&app.pin_state.key_agreement, &original_key_agreement,
                  sizeof(original_key_agreement)) == 0,
           "setPin should preserve the active keyAgreement so browser follow-up requests work");
}

static void test_set_pin_creates_app_data_dir_before_persisting(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    size_t response_len = 0;

    test_storage_reset();
    g_storage_root_exists = false;
    g_storage_app_data_exists = false;
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state before mkdir setPin");
    app.storage = &storage;
    memcpy(new_pin_block, "1234", 4);

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_set_pin_request_with_pin_auth(
                   request, sizeof(request), new_pin_block, sizeof(new_pin_block), NULL),
               response, sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
           "setPin should create the app data directory before persisting state");
    expect(g_storage_root_exists, "setPin should create the app data root when missing");
    expect(g_storage_app_data_exists, "setPin should create the app data directory when missing");
    expect(g_pin_file_exists, "setPin should persist the PIN state file");
}
