static void test_get_key_agreement_does_not_rotate_runtime_secrets(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[64];
    uint8_t response[160];
    uint8_t original_pin_token[ZF_PIN_TOKEN_LEN];
    ZfP256KeyAgreementKey original_key_agreement = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for getKeyAgreement");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for getKeyAgreement");
    app.storage = &storage;
    memcpy(original_pin_token, app.pin_state.pin_token, sizeof(original_pin_token));
    memcpy(&original_key_agreement, &app.pin_state.key_agreement, sizeof(original_key_agreement));

    expect(zerofido_pin_handle_command(
               &app, request, encode_client_pin_get_key_agreement_request(request, sizeof(request)),
               response, sizeof(response), &response_len) == ZF_CTAP_SUCCESS,
           "getKeyAgreement should succeed");
    expect(response_len > 0, "getKeyAgreement should return a COSE key response");
    expect(memcmp(app.pin_state.pin_token, original_pin_token, sizeof(original_pin_token)) == 0,
           "getKeyAgreement should not rotate the runtime pin token");
    expect(memcmp(&app.pin_state.key_agreement, &original_key_agreement,
                  sizeof(original_key_agreement)) == 0,
           "getKeyAgreement should not rotate the runtime key agreement key");
}

static void test_get_pin_token_success_rotates_pin_token(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[96];
    uint8_t original_pin_token[ZF_PIN_TOKEN_LEN];
    const uint8_t *token = NULL;
    size_t token_len = 0;
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for getPinToken success");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for getPinToken success");
    app.storage = &storage;
    memset(app.pin_state.pin_token, 0x11, sizeof(app.pin_state.pin_token));
    memcpy(original_pin_token, app.pin_state.pin_token, sizeof(original_pin_token));

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_get_pin_token_request(
                                           request, sizeof(request), app.pin_state.pin_hash, true),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_SUCCESS,
           "correct PIN should return a pin token");
    expect(parse_pin_token_response(response, response_len, &token, &token_len),
           "getPinToken response should encode a token");
    expect(token_len == ZF_PIN_TOKEN_LEN, "returned pin token should have the expected length");
    expect(memcmp(app.pin_state.pin_token, original_pin_token, sizeof(original_pin_token)) != 0,
           "successful getPinToken should rotate the runtime pin token");
    expect(memcmp(token, app.pin_state.pin_token, ZF_PIN_TOKEN_LEN) == 0,
           "returned pin token should match the rotated runtime token");
    expect(app.pin_state.pin_token_active,
           "successful getPinToken should mark the runtime token active");
    expect(app.pin_state.pin_token_issued_at == g_fake_tick,
           "successful getPinToken should timestamp the runtime token");
    expect(app.pin_state.pin_token_permissions == (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA),
           "legacy getPinToken should grant default mc|ga permissions");
    expect(!app.pin_state.pin_token_permissions_rp_id_set,
           "legacy getPinToken should not bind an RP ID until first use");
    expect(g_approval_request_count == 0, "legacy getPinToken should not request local consent");
}

static void test_get_pin_token_experimental_2_1_requires_consent(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[96];
    const uint8_t *token = NULL;
    size_t token_len = 0;
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for 2.1 getPinToken consent");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for 2.1 getPinToken consent");
    app.storage = &storage;
    test_enable_fido2_1_experimental(&app);

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_get_pin_token_request(
                                           request, sizeof(request), app.pin_state.pin_hash, true),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_SUCCESS,
           "experimental CTAP 2.1 getPinToken should return a pin token after consent");
    expect(parse_pin_token_response(response, response_len, &token, &token_len),
           "2.1 getPinToken response should encode a token");
    expect(token_len == ZF_PIN_TOKEN_LEN,
           "2.1 getPinToken should return the expected token length");
    expect(g_approval_request_count == 1, "2.1 getPinToken should request local consent");
    expect(app.pin_state.pin_token_permissions == (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA),
           "2.1 getPinToken should grant default mc|ga permissions");
    expect(app.pin_state.pin_token_permissions_managed,
           "2.1 getPinToken should issue a permission-managed token");
}

static void test_get_pin_token_protocol2_returns_iv_prefixed_token(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[128];
    uint8_t pin_hash_enc[ZF_PIN_ENCRYPTED_HASH_MAX_LEN] = {0xA5};
    const uint8_t *token = NULL;
    size_t token_len = 0;
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for protocol 2 getPinToken");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for protocol 2 getPinToken");
    memcpy(pin_hash_enc + ZF_PIN_PROTOCOL2_IV_LEN, app.pin_state.pin_hash, ZF_PIN_HASH_LEN);
    app.storage = &storage;
    test_enable_fido2_1_experimental(&app);

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_get_pin_token_request_with_protocol(
                                           request, sizeof(request), ZF_PIN_PROTOCOL_V2,
                                           pin_hash_enc, sizeof(pin_hash_enc), true),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_SUCCESS,
           "protocol 2 getPinToken should return a token");
    expect(parse_pin_token_response(response, response_len, &token, &token_len),
           "protocol 2 getPinToken response should encode a token");
    expect(token_len == ZF_PIN_ENCRYPTED_TOKEN_MAX_LEN,
           "protocol 2 tokenEnc should include the CBC IV");
    expect(memcmp(token + ZF_PIN_PROTOCOL2_IV_LEN, app.pin_state.pin_token, ZF_PIN_TOKEN_LEN) == 0,
           "protocol 2 tokenEnc payload should encrypt the runtime token");
}

static void test_client_pin_protocol2_requires_experimental_profile(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t pin_hash_enc[ZF_PIN_ENCRYPTED_HASH_MAX_LEN] = {0xA5};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for default-profile protocol 2 rejection");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for default-profile protocol 2 rejection");
    memcpy(pin_hash_enc + ZF_PIN_PROTOCOL2_IV_LEN, app.pin_state.pin_hash, ZF_PIN_HASH_LEN);
    app.storage = &storage;

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_get_pin_token_request_with_protocol(
                                           request, sizeof(request), ZF_PIN_PROTOCOL_V2,
                                           pin_hash_enc, sizeof(pin_hash_enc), true),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_INVALID_PARAMETER,
           "default CTAP 2.0 profile should reject unadvertised protocol 2");
    expect(!app.pin_state.pin_token_active,
           "default-profile protocol 2 rejection should not activate a token");
}

static void test_get_pin_token_decrypt_failure_consumes_retry(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t original_public_x = 0;
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for getPinToken failure");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for getPinToken failure");
    app.storage = &storage;
    app.pin_state.pin_retries = 2;
    original_public_x = app.pin_state.key_agreement.public_x[0];
    g_fail_crypto_decrypt = true;

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_get_pin_token_request(
                                           request, sizeof(request), app.pin_state.pin_hash, true),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_PIN_INVALID,
           "pinHash decrypt failure should consume a retry");
    expect(app.pin_state.pin_retries == 1, "pinHash decrypt failure should decrement retries");
    expect(app.pin_state.key_agreement.public_x[0] != original_public_x,
           "pinHash decrypt failure should regenerate key agreement");
}

static void test_get_pin_token_rejects_permissions_fields(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for permissions rejection");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for permissions rejection");
    app.storage = &storage;

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x05, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_MC, NULL),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_INVALID_PARAMETER,
           "legacy getPinToken should reject permissions");
    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_request_with_permissions(
                   request, sizeof(request), 0x05, app.pin_state.pin_hash, false, 0, "example.com"),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_PARAMETER,
           "legacy getPinToken should reject rpId");
}

static void test_client_pin_permissions_subcommand_requires_permissions(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for permissions missing-parameter test");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for permissions missing-parameter test");
    app.storage = &storage;

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_request_with_permissions(
                   request, sizeof(request), 0x09, app.pin_state.pin_hash, false, 0, "example.com"),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_SUBCOMMAND,
           "CTAP 2.0 profile should not expose the 0x09 permissions-token subcommand");
}

static void test_client_pin_permissions_subcommand_rejects_zero_permissions(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for zero permissions test");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for zero permissions test");
    app.storage = &storage;

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_request_with_permissions(
                   request, sizeof(request), 0x09, app.pin_state.pin_hash, true, 0, "example.com"),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_SUBCOMMAND,
           "CTAP 2.0 profile should not expose the 0x09 permissions-token subcommand");
}

static void test_client_pin_permissions_subcommand_rejects_unsupported_permissions(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for unsupported permissions test");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for unsupported permissions test");
    app.storage = &storage;

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_request_with_permissions(
                   request, sizeof(request), 0x09, app.pin_state.pin_hash, true, 0x04U, NULL),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_SUBCOMMAND,
           "CTAP 2.0 profile should not expose the 0x09 permissions-token subcommand");
}

static void test_client_pin_permissions_subcommand_requires_rp_id_for_mc_ga(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for permissions rpId test");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for permissions rpId test");
    app.storage = &storage;

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x09, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_MC, NULL),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_INVALID_SUBCOMMAND,
           "CTAP 2.0 profile should not expose the 0x09 permissions-token subcommand");
}

static void test_client_pin_permissions_subcommand_experimental_validates_parameters(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for 2.1 permission validation");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for 2.1 permission validation");
    app.storage = &storage;
    test_enable_fido2_1_experimental(&app);

    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_request_with_permissions(
                   request, sizeof(request), 0x09, app.pin_state.pin_hash, false, 0, "example.com"),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_MISSING_PARAMETER,
           "2.1 0x09 should require the permissions field");
    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_request_with_permissions(
                   request, sizeof(request), 0x09, app.pin_state.pin_hash, true, 0, "example.com"),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_PARAMETER,
           "2.1 0x09 should reject zero permissions");
    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x09, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_MC, NULL),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_MISSING_PARAMETER,
           "2.1 0x09 should require rpId for mc and ga permissions");
    expect(zerofido_pin_handle_command(
               &app, request,
               encode_client_pin_request_with_permissions(
                   request, sizeof(request), 0x09, app.pin_state.pin_hash, true, 0x40U, NULL),
               response, sizeof(response), &response_len) == ZF_CTAP_ERR_INVALID_PARAMETER,
           "2.1 0x09 should reject unknown permission bits as invalid parameters");
    expect(g_approval_request_count == 0,
           "invalid 2.1 0x09 requests should fail before local consent");
    expect(!app.pin_state.pin_token_active,
           "invalid 2.1 0x09 requests should not mint a pinUvAuthToken");
}

static void test_client_pin_permissions_subcommand_stores_permissions_and_rp_id(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    const uint8_t *token = NULL;
    size_t token_len = 0;
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for 0x09 token request");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for 0x09 token request");
    app.storage = &storage;
    test_enable_fido2_1_experimental(&app);

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x09, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_MC, "example.com"),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_SUCCESS,
           "experimental CTAP 2.1 profile should issue a permission-scoped pinUvAuthToken");
    expect(response_len > 0, "0x09 should return a CBOR token response");
    expect(parse_pin_token_response(response, response_len, &token, &token_len),
           "0x09 should encode a token response");
    expect(token_len == ZF_PIN_TOKEN_LEN, "0x09 should return the expected token length");
    expect(app.pin_state.pin_token_active, "0x09 should activate the runtime PIN token");
    expect(app.pin_state.pin_token_permissions == ZF_PIN_PERMISSION_MC,
           "0x09 should store the requested permission bits");
    expect(app.pin_state.pin_token_permissions_scoped,
           "0x09 should mark the runtime token as permission-scoped");
    expect(app.pin_state.pin_token_permissions_managed,
           "0x09 should mark the runtime token as permission-managed");
    expect(app.pin_state.pin_token_permissions_rp_id_set, "0x09 should bind the requested RP ID");
    uint8_t expected_rp_id_hash[32];
    zf_crypto_sha256((const uint8_t *)"example.com", strlen("example.com"), expected_rp_id_hash);
    expect(memcmp(app.pin_state.pin_token_permissions_rp_id_hash, expected_rp_id_hash,
                  sizeof(expected_rp_id_hash)) == 0,
           "0x09 should persist the requested RP ID hash");
    expect(g_approval_request_count == 1,
           "0x09 should require local consent before issuing a permission token");
}

static void test_client_pin_permissions_subcommand_issued_token_consumes_mc_after_up(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t token_request[256];
    uint8_t token_response[64];
    uint8_t ctap_request[256];
    uint8_t ctap_response[512];
    uint8_t pin_auth[32];
    size_t token_response_len = 0;
    size_t ctap_response_len = 0;
    static const uint8_t make_credential_client_data_hash[32] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    };

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for issued 0x09 token");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for issued 0x09 token");
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    test_enable_fido2_1_experimental(&app);

    expect(zerofido_pin_handle_command(
               &app, token_request,
               encode_client_pin_request_with_permissions(token_request, sizeof(token_request),
                                                          0x09, app.pin_state.pin_hash, true,
                                                          ZF_PIN_PERMISSION_MC, "example.com"),
               token_response, sizeof(token_response), &token_response_len) == ZF_CTAP_SUCCESS,
           "2.1 0x09 should mint an mc-scoped token");
    expect(app.pin_state.pin_token_permissions == ZF_PIN_PERMISSION_MC,
           "issued 0x09 token should start with mc permission");
    expect(app.pin_state.pin_token_permissions_managed,
           "issued 0x09 token should be permission-managed");
    expect(zf_crypto_hmac_sha256(app.pin_state.pin_token, sizeof(app.pin_state.pin_token),
                                 make_credential_client_data_hash,
                                 sizeof(make_credential_client_data_hash), pin_auth),
           "derive makeCredential pinAuth from issued 0x09 token");

    ctap_response_len =
        zerofido_handle_ctap2(&app, 0x01020304, ctap_request,
                              encode_make_credential_ctap_request_with_pin_auth(
                                  ctap_request, sizeof(ctap_request), false, false, true, pin_auth,
                                  ZF_PIN_AUTH_LEN, true, false, NULL, 0),
                              ctap_response, sizeof(ctap_response));

    expect(ctap_response_len > 1, "makeCredential should accept the issued 0x09 token once");
    expect(ctap_response[0] == ZF_CTAP_SUCCESS,
           "makeCredential should succeed with the issued 0x09 token");
    expect(app.pin_state.pin_token_permissions == 0,
           "UP-tested makeCredential should consume the issued mc permission");
    expect(!app.pin_state.pin_token_permissions_rp_id_set,
           "consumed 0x09 token should clear the RP binding");

    ctap_response_len =
        zerofido_handle_ctap2(&app, 0x01020304, ctap_request,
                              encode_make_credential_ctap_request_with_pin_auth(
                                  ctap_request, sizeof(ctap_request), false, false, true, pin_auth,
                                  ZF_PIN_AUTH_LEN, true, false, NULL, 0),
                              ctap_response, sizeof(ctap_response));

    expect(ctap_response_len == 1, "reused consumed 0x09 token should return status only");
    expect(ctap_response[0] == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "consumed 0x09 token should not authorize another makeCredential");
    expect(g_approval_request_count == 2,
           "consumed 0x09 token replay should fail before another approval prompt");
}

static void test_client_pin_permissions_subcommand_denied_consent_does_not_issue_token(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for denied permission-token consent");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for denied permission-token consent");
    app.storage = &storage;
    test_enable_fido2_1_experimental(&app);
    g_approval_result = false;
    g_approval_state = ZfApprovalDenied;

    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x09, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_MC, "example.com"),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_OPERATION_DENIED,
           "experimental CTAP 2.1 0x09 should fail when local consent is denied");
    expect(g_approval_request_count == 1, "denied 0x09 should request local consent once");
    expect(!app.pin_state.pin_token_active, "denied 0x09 should not activate a runtime PIN token");
    expect(app.pin_state.pin_token_permissions == 0,
           "denied 0x09 should not store requested permissions");
    expect(response_len == 0, "denied 0x09 should not return a token body");
}

static void test_client_pin_permissions_subcommand_rejects_unsupported_be_permission(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for unsupported BE permission");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for unsupported BE permission");
    app.storage = &storage;
    test_enable_fido2_1_experimental(&app);

    expect(
        zerofido_pin_handle_command(
            &app, request,
            encode_client_pin_request_with_permissions(
                request, sizeof(request), 0x09, app.pin_state.pin_hash, true,
                ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA | ZF_PIN_PERMISSION_BE, "example.com"),
            response, sizeof(response), &response_len) == ZF_CTAP_ERR_UNAUTHORIZED_PERMISSION,
        "experimental CTAP 2.1 0x09 should reject unsupported bioEnrollment permission");
    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x09, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_CM, NULL),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_UNAUTHORIZED_PERMISSION,
           "experimental CTAP 2.1 0x09 should reject unsupported credentialManagement permission");
    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x09, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_LBW, NULL),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_UNAUTHORIZED_PERMISSION,
           "experimental CTAP 2.1 0x09 should reject unsupported largeBlobWrite permission");
    expect(zerofido_pin_handle_command(&app, request,
                                       encode_client_pin_request_with_permissions(
                                           request, sizeof(request), 0x09, app.pin_state.pin_hash,
                                           true, ZF_PIN_PERMISSION_ACFG, NULL),
                                       response, sizeof(response),
                                       &response_len) == ZF_CTAP_ERR_UNAUTHORIZED_PERMISSION,
           "experimental CTAP 2.1 0x09 should reject unsupported authenticatorConfig permission");
    expect(!app.pin_state.pin_token_active,
           "unsupported BE permission should not mint a pinUvAuthToken");
    expect(app.pin_state.pin_token_permissions == 0,
           "unsupported BE permission should not store token permissions");
}

static void test_pin_auth_rejects_expired_pin_token(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0x5A};
    uint8_t pin_auth[32];
    bool uv_verified = false;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for token-expiry test");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for token-expiry test");
    mark_pin_token_issued(&state);
    state.pin_token_issued_at = g_fake_tick - ZF_PIN_TOKEN_TIMEOUT_MS;
    expect(zf_crypto_hmac_sha256(state.pin_token, sizeof(state.pin_token), client_data_hash,
                                 sizeof(client_data_hash), pin_auth),
           "derive pinAuth for expired token");

    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     ZF_PIN_AUTH_LEN, true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_TOKEN_EXPIRED,
           "expired pin token should return PIN_TOKEN_EXPIRED");
    expect(!state.pin_token_active, "expired pin token should be cleared after rejection");
    expect(!uv_verified, "expired pin token should not set UV");
}

static void test_pin_auth_protocol2_accepts_full_hmac(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0x7B};
    uint8_t pin_auth[32];
    bool uv_verified = false;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for protocol 2 pinAuth");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for protocol 2 pinAuth");
    mark_pin_token_issued(&state);
    state.pin_token_permissions = ZF_PIN_PERMISSION_MC;
    expect(zf_crypto_hmac_sha256(state.pin_token, sizeof(state.pin_token), client_data_hash,
                                 sizeof(client_data_hash), pin_auth),
           "derive protocol 2 pinAuth");

    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     ZF_PIN_AUTH_MAX_LEN, true, ZF_PIN_PROTOCOL_V2, NULL,
                                     ZF_PIN_PERMISSION_MC, &uv_verified) == ZF_CTAP_SUCCESS,
           "protocol 2 pinAuth should accept the full HMAC");
    expect(uv_verified, "protocol 2 pinAuth should set UV");
}

static void test_legacy_pin_token_allows_reuse_without_rp_binding(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0x5A};
    uint8_t pin_auth[32];
    bool uv_verified = false;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for legacy token rp scope");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for legacy token rp scope");
    memset(state.pin_token, 0x44, sizeof(state.pin_token));
    state.pin_token_permissions = ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA;
    mark_pin_token_issued(&state);
    expect(zf_crypto_hmac_sha256(state.pin_token, sizeof(state.pin_token), client_data_hash,
                                 sizeof(client_data_hash), pin_auth),
           "derive pinAuth for legacy token rp scope");

    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     ZF_PIN_AUTH_LEN, true, 1, "example.com", ZF_PIN_PERMISSION_MC,
                                     &uv_verified) == ZF_CTAP_SUCCESS,
           "legacy getPinToken should authorize the first RP without binding it");
    expect(uv_verified, "successful legacy token authorization should set UV");
    expect(!state.pin_token_permissions_rp_id_set,
           "legacy getPinToken should not bind an RP ID on first use");
    expect(state.pin_token_permissions == (ZF_PIN_PERMISSION_MC | ZF_PIN_PERMISSION_GA),
           "CTAP 2.0 legacy token should keep mc/ga permissions after UP use");

    uv_verified = false;
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     ZF_PIN_AUTH_LEN, true, 1, "other.example",
                                     ZF_PIN_PERMISSION_GA, &uv_verified) == ZF_CTAP_SUCCESS,
           "CTAP 2.0 legacy token should remain reusable for another RP");
    expect(uv_verified, "successful legacy token reuse should set UV");
}

static void test_pin_auth_rejects_missing_required_permission(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0x6B};
    uint8_t pin_auth[32];
    bool uv_verified = false;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for permission check");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for permission check");
    memset(state.pin_token, 0x55, sizeof(state.pin_token));
    zf_pin_set_token_permissions(&state, ZF_PIN_PERMISSION_MC, true, false, "example.com");
    mark_pin_token_issued(&state);
    expect(zf_crypto_hmac_sha256(state.pin_token, sizeof(state.pin_token), client_data_hash,
                                 sizeof(client_data_hash), pin_auth),
           "derive pinAuth for permission check");

    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     ZF_PIN_AUTH_LEN, true, 1, "example.com", ZF_PIN_PERMISSION_GA,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "pinAuth should reject tokens missing the required permission");
    expect(!uv_verified, "missing permission should not set UV");
}

static void test_pin_auth_rejects_scoped_token_for_wrong_rp(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0x6C};
    uint8_t pin_auth[32];
    bool uv_verified = false;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for scoped RP check");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for scoped RP check");
    memset(state.pin_token, 0x56, sizeof(state.pin_token));
    zf_pin_set_token_permissions(&state, ZF_PIN_PERMISSION_MC, true, false, "example.com");
    mark_pin_token_issued(&state);
    expect(zf_crypto_hmac_sha256(state.pin_token, sizeof(state.pin_token), client_data_hash,
                                 sizeof(client_data_hash), pin_auth),
           "derive pinAuth for scoped RP check");

    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     ZF_PIN_AUTH_LEN, true, 1, "other.example",
                                     ZF_PIN_PERMISSION_MC,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "pinAuth should reject permission-scoped tokens for a different RP");
    expect(!uv_verified, "wrong RP should not set UV");
}

static void test_change_pin_pin_hash_decrypt_failure_consumes_retry(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state), "init PIN state for changePin failure");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for changePin failure");
    app.storage = &storage;
    app.pin_state.pin_retries = 2;
    app.pin_state.pin_consecutive_mismatches = 1;
    expect(zf_pin_persist_state(&storage, &app.pin_state), "persist retry state before failure");
    memcpy(new_pin_block, "5678", 4);
    g_fail_crypto_decrypt = true;

    expect(
        zerofido_pin_handle_command(
            &app, request,
            encode_client_pin_change_pin_request(request, sizeof(request), app.pin_state.pin_hash,
                                                 new_pin_block, sizeof(new_pin_block)),
            response, sizeof(response), &response_len) == ZF_CTAP_ERR_PIN_INVALID,
        "pinHash decrypt failure during changePin should consume a retry");
    expect(app.pin_state.pin_retries == 1,
           "changePin pinHash decrypt failure should decrement retries");
    expect(app.pin_state.pin_consecutive_mismatches == 2,
           "changePin pinHash decrypt failure should count as a mismatch");
}

static void test_change_pin_new_pin_decrypt_failure_returns_pin_auth_invalid(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    uint8_t request[256];
    uint8_t response[64];
    uint8_t new_pin_block[64] = {0};
    size_t response_len = 0;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &app.pin_state),
           "init PIN state for changePin decrypt failure");
    expect(zerofido_pin_set_plaintext(&storage, &app.pin_state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for changePin decrypt failure");
    app.storage = &storage;
    memcpy(new_pin_block, "5678", 4);
    g_fail_crypto_decrypt_after = 1;

    expect(
        zerofido_pin_handle_command(
            &app, request,
            encode_client_pin_change_pin_request(request, sizeof(request), app.pin_state.pin_hash,
                                                 new_pin_block, sizeof(new_pin_block)),
            response, sizeof(response), &response_len) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
        "changePin newPinEnc decrypt failure should return pin auth invalid");
}

static void test_pin_init_cleans_temp_file(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};

    test_storage_reset();
    g_pin_temp_exists = true;
    g_pin_temp_size = 4;
    memcpy(g_pin_temp_data, "temp", 4);

    expect(zerofido_pin_init(&storage, &state), "init PIN state with temp file");
    expect(!g_pin_temp_exists, "pin init should clean temp file");
}

static void test_store_resident_records_for_same_user_coexist_after_reload(void) {
    Storage storage = {0};
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    store.records = store_records;
    ZfCredentialStore reloaded = {0};
    ZfCredentialIndexEntry reloaded_records[ZF_MAX_CREDENTIALS] = {0};
    reloaded.records = reloaded_records;
    ZfCredentialRecord first = {0};
    ZfCredentialRecord second = {0};
    ZfCredentialRecord loaded = {0};

    test_storage_reset();
    memset(first.credential_id, 0x11, sizeof(first.credential_id));
    first.credential_id_len = sizeof(first.credential_id);
    zf_store_record_format_hex_encode(first.credential_id, first.credential_id_len,
                                      first.file_name);
    strcpy(first.rp_id, "example.com");
    memcpy(first.user_id, "user-1", 6);
    first.user_id_len = 6;
    first.resident_key = true;
    first.in_use = true;
    first.created_at = 1;

    memset(second.credential_id, 0x22, sizeof(second.credential_id));
    second.credential_id_len = sizeof(second.credential_id);
    zf_store_record_format_hex_encode(second.credential_id, second.credential_id_len,
                                      second.file_name);
    strcpy(second.rp_id, "example.com");
    memcpy(second.user_id, "user-1", 6);
    second.user_id_len = 6;
    second.resident_key = true;
    second.in_use = true;
    second.created_at = 2;

    expect(test_zf_store_add_record(&storage, &store, &first), "write original resident record");
    expect(test_zf_store_add_record(&storage, &store, &second),
           "second resident record for the same user should coexist");
    expect(store.count == 2, "coexisting resident records should both remain in memory");
    expect(test_zf_store_record_format_load_record(&storage, first.file_name, &loaded),
           "original resident record should remain reloadable");
    expect(memcmp(loaded.credential_id, first.credential_id, sizeof(first.credential_id)) == 0,
           "loaded original record should keep its credential id");
    expect(test_zf_store_record_format_load_record(&storage, second.file_name, &loaded),
           "second resident record should load from its own file");
    expect(memcmp(loaded.credential_id, second.credential_id, sizeof(second.credential_id)) == 0,
           "loaded resident record should keep its credential id");
    expect(test_zf_store_init(&storage, &reloaded), "reloading store should preserve both records");
    expect(reloaded.count == 2, "store init must not collapse resident credentials for one user");
}

static void test_store_wipe_app_data_clears_credentials_and_pin_state(void) {
    Storage storage = {0};
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    store.records = store_records;
    ZfCredentialStore reloaded = {0};
    ZfCredentialIndexEntry reloaded_records[ZF_MAX_CREDENTIALS] = {0};
    reloaded.records = reloaded_records;
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};
    ZfCredentialRecord record = {0};

    test_storage_reset();
    expect(test_zf_store_init(&storage, &store), "init store before app-data wipe");
    expect(zerofido_pin_init(&storage, &state), "init PIN state before app-data wipe");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN before app-data wipe");
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential before app-data wipe");
    expect(test_zf_store_add_record(&storage, &store, &record),
           "persist credential before app-data wipe");
    expect(zf_store_count_saved(&store) == 1, "store should contain one credential before wipe");
    expect(g_pin_file_exists, "PIN file should exist before app-data wipe");

    expect(zf_store_wipe_app_data(&storage), "wipe ZeroFIDO app data");
    expect(g_remove_attempted_pin_v2_file, "app-data wipe should remove current v2 PIN file");
    expect(g_remove_attempted_pin_v2_temp, "app-data wipe should remove current v2 PIN temp file");
    expect(!g_pin_file_exists, "PIN file should be removed by app-data wipe");
    expect(test_zf_store_init(&storage, &reloaded), "re-init store after app-data wipe");
    expect(zf_store_count_saved(&reloaded) == 0, "store should be empty after app-data wipe");
    expect(zerofido_pin_init(&storage, &restored), "re-init PIN state after app-data wipe");
    expect(!restored.pin_set, "PIN should be cleared after app-data wipe");
}
