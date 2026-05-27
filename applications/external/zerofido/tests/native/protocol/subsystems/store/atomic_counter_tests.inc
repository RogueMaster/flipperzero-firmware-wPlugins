static void test_store_cleanup_restores_backup_when_primary_is_missing(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    size_t size = 0;
    TestStorageFile *primary = NULL;
    TestStorageFile *backup = NULL;
    char primary_path[160];
    char backup_path[160];

    test_storage_reset();
    size = encode_complete_record(buffer, sizeof(buffer));
    snprintf(primary_path, sizeof(primary_path), "%s/%s", ZF_APP_DATA_DIR,
             k_repeated_credential_file_name);
    snprintf(backup_path, sizeof(backup_path), "%s/%s.bak", ZF_APP_DATA_DIR,
             k_repeated_credential_file_name);

    primary = test_storage_file_slot(primary_path, true);
    backup = test_storage_file_slot(backup_path, true);
    expect(primary && backup, "allocate storage slots for cleanup recovery");
    memcpy(backup->data, buffer, size);
    backup->size = size;
    backup->exists = true;
    primary->size = 0;
    primary->exists = false;

    zf_store_recovery_cleanup_temp_files(NULL);

    expect(primary->exists, "cleanup should restore the backup into the primary path");
    expect(!backup->exists, "cleanup should remove the restored backup file");
}

static void test_store_cleanup_restores_backup_when_primary_is_corrupt(void) {
    Storage storage = {0};
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry records[ZF_MAX_CREDENTIALS] = {0};
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    size_t size = 0;
    TestStorageFile *primary = NULL;
    TestStorageFile *backup = NULL;
    char primary_path[160];
    char backup_path[160];

    test_storage_reset();
    store.records = records;
    store.capacity = ZF_MAX_CREDENTIALS;
    size = encode_complete_record(buffer, sizeof(buffer));
    snprintf(primary_path, sizeof(primary_path), "%s/%s", ZF_APP_DATA_DIR,
             k_repeated_credential_file_name);
    snprintf(backup_path, sizeof(backup_path), "%s/%s.bak", ZF_APP_DATA_DIR,
             k_repeated_credential_file_name);

    primary = test_storage_file_slot(primary_path, true);
    backup = test_storage_file_slot(backup_path, true);
    expect(primary && backup, "allocate storage slots for corrupt-primary cleanup recovery");
    memcpy(primary->data, "bad", 3U);
    primary->size = 3U;
    primary->exists = true;
    memcpy(backup->data, buffer, size);
    backup->size = size;
    backup->exists = true;

    expect(test_zf_store_init(&storage, &store),
           "store bootstrap should recover from a valid backup");

    expect(primary->exists, "cleanup should restore the valid backup over a corrupt primary");
    expect(primary->size == size, "restored primary should use the backup record size");
    expect(memcmp(primary->data, buffer, size) == 0,
           "restored primary should use the backup record contents");
    expect(!backup->exists, "cleanup should remove the consumed backup file");
    expect(store.count == 1, "bootstrap should index the recovered credential");
}

static void test_store_cleanup_keeps_current_counter_when_backup_exists(void) {
    static const uint8_t current_counter[] = {0x10, 0x11, 0x12, 0x13};
    static const uint8_t stale_counter[] = {0x01, 0x02, 0x03, 0x04};
    Storage storage = {0};
    TestStorageFile *counter = NULL;
    TestStorageFile *backup = NULL;
    char counter_path[128];
    char backup_path[128];

    test_storage_reset();
    zf_store_build_counter_floor_path(k_repeated_credential_file_name, counter_path,
                                      sizeof(counter_path));
    snprintf(backup_path, sizeof(backup_path), "%s.bak", counter_path);

    counter = test_storage_file_slot(counter_path, true);
    backup = test_storage_file_slot(backup_path, true);
    expect(counter && backup, "allocate slots for counter backup cleanup");
    memcpy(counter->data, current_counter, sizeof(current_counter));
    counter->size = sizeof(current_counter);
    counter->exists = true;
    memcpy(backup->data, stale_counter, sizeof(stale_counter));
    backup->size = sizeof(stale_counter);
    backup->exists = true;

    zf_store_recovery_cleanup_temp_files(&storage);

    expect(counter->exists, "cleanup should keep the current counter floor primary");
    expect(counter->size == sizeof(current_counter),
           "cleanup should keep the current counter floor size");
    expect(memcmp(counter->data, current_counter, sizeof(current_counter)) == 0,
           "cleanup should not restore stale counter backup over the current primary");
    expect(!backup->exists, "cleanup should remove the stale counter backup");
}

static void test_store_remove_record_paths_deletes_atomic_backups(void) {
    Storage storage = {0};
    TestStorageFile *record = NULL;
    TestStorageFile *record_temp = NULL;
    TestStorageFile *record_backup = NULL;
    TestStorageFile *counter = NULL;
    TestStorageFile *counter_temp = NULL;
    TestStorageFile *counter_backup = NULL;
    char record_path[128];
    char record_temp_path[128];
    char record_backup_path[128];
    char counter_path[128];
    char counter_temp_path[128];
    char counter_backup_path[128];

    test_storage_reset();
    zf_store_build_record_path(k_repeated_credential_file_name, record_path, sizeof(record_path));
    zf_store_build_temp_path(k_repeated_credential_file_name, record_temp_path,
                             sizeof(record_temp_path));
    zf_store_build_counter_floor_path(k_repeated_credential_file_name, counter_path,
                                      sizeof(counter_path));
    zf_store_build_counter_floor_temp_path(k_repeated_credential_file_name, counter_temp_path,
                                           sizeof(counter_temp_path));
    snprintf(record_backup_path, sizeof(record_backup_path), "%s.bak", record_path);
    snprintf(counter_backup_path, sizeof(counter_backup_path), "%s.bak", counter_path);

    record = test_storage_file_slot(record_path, true);
    record_temp = test_storage_file_slot(record_temp_path, true);
    record_backup = test_storage_file_slot(record_backup_path, true);
    counter = test_storage_file_slot(counter_path, true);
    counter_temp = test_storage_file_slot(counter_temp_path, true);
    counter_backup = test_storage_file_slot(counter_backup_path, true);
    expect(record && record_temp && record_backup && counter && counter_temp && counter_backup,
           "allocate record remove slots");
    record->exists = true;
    record_temp->exists = true;
    record_backup->exists = true;
    counter->exists = true;
    counter_temp->exists = true;
    counter_backup->exists = true;

    expect(zf_store_recovery_remove_record_paths(&storage, k_repeated_credential_file_name),
           "record path removal should include atomic companions");
    expect(!record->exists, "record removal should delete primary record");
    expect(!record_temp->exists, "record removal should delete temp record");
    expect(!record_backup->exists, "record removal should delete backup record");
    expect(!counter->exists, "record removal should delete counter floor");
    expect(!counter_temp->exists, "record removal should delete counter temp");
    expect(!counter_backup->exists, "record removal should delete counter backup");
}

static void test_atomic_file_write_overwrites_primary_without_predelete(void) {
    Storage storage = {0};
    TestStorageFile *primary = NULL;
    static const char path[] = ZF_APP_DATA_DIR "/atomic.bin";
    static const char temp_path[] = ZF_APP_DATA_DIR "/atomic.tmp";
    static const uint8_t old_value[] = {0x10, 0x11, 0x12};
    static const uint8_t new_value[] = {0x20, 0x21, 0x22, 0x23};

    test_storage_reset();
    primary = test_storage_file_slot(path, true);
    expect(primary != NULL, "allocate primary slot for atomic write overwrite");
    memcpy(primary->data, old_value, sizeof(old_value));
    primary->size = sizeof(old_value);
    primary->exists = true;

    expect(zf_storage_write_file_atomic(&storage, path, temp_path, new_value, sizeof(new_value)),
           "atomic write should publish temp over existing primary");
    expect(primary->exists, "atomic write should leave a primary file after overwrite");
    expect(primary->size == sizeof(new_value), "atomic write should replace primary size");
    expect(memcmp(primary->data, new_value, sizeof(new_value)) == 0,
           "atomic write should replace primary contents");
}

static void test_atomic_file_write_failure_keeps_primary(void) {
    Storage storage = {0};
    TestStorageFile *primary = NULL;
    TestStorageFile *temp = NULL;
    static const char path[] = ZF_APP_DATA_DIR "/atomic.bin";
    static const char temp_path[] = ZF_APP_DATA_DIR "/atomic.tmp";
    static const uint8_t old_value[] = {0x30, 0x31, 0x32};
    static const uint8_t new_value[] = {0x40, 0x41, 0x42, 0x43};

    test_storage_reset();
    primary = test_storage_file_slot(path, true);
    temp = test_storage_file_slot(temp_path, true);
    expect(primary != NULL && temp != NULL, "allocate slots for failed atomic write");
    memcpy(primary->data, old_value, sizeof(old_value));
    primary->size = sizeof(old_value);
    primary->exists = true;

    g_storage_fail_copy_match = temp_path;
    expect(!zf_storage_write_file_atomic(&storage, path, temp_path, new_value, sizeof(new_value)),
           "atomic write should report failed publish");
    g_storage_fail_copy_match = NULL;

    expect(primary->exists, "failed atomic publish should keep the old primary");
    expect(primary->size == sizeof(old_value), "failed atomic publish should keep primary size");
    expect(memcmp(primary->data, old_value, sizeof(old_value)) == 0,
           "failed atomic publish should keep primary contents");
    expect(!temp->exists, "failed atomic publish should clean up the temp file");
}

static void test_atomic_file_recovery_restores_backup_when_primary_missing(void) {
    Storage storage = {0};
    TestStorageFile *primary = NULL;
    TestStorageFile *backup = NULL;
    TestStorageFile *temp = NULL;
    static const char path[] = ZF_APP_DATA_DIR "/atomic.bin";
    static const char temp_path[] = ZF_APP_DATA_DIR "/atomic.tmp";
    static const char backup_path[] = ZF_APP_DATA_DIR "/atomic.bin.bak";
    static const uint8_t old_value[] = {0x50, 0x51, 0x52};
    static const uint8_t temp_value[] = {0x60, 0x61, 0x62};

    test_storage_reset();
    primary = test_storage_file_slot(path, true);
    backup = test_storage_file_slot(backup_path, true);
    temp = test_storage_file_slot(temp_path, true);
    expect(primary != NULL && backup != NULL && temp != NULL,
           "allocate slots for atomic backup recovery");
    memcpy(backup->data, old_value, sizeof(old_value));
    backup->size = sizeof(old_value);
    backup->exists = true;
    memcpy(temp->data, temp_value, sizeof(temp_value));
    temp->size = sizeof(temp_value);
    temp->exists = true;

    expect(zf_storage_recover_atomic_file(&storage, path, temp_path),
           "atomic recovery should restore the backup");
    expect(primary->exists, "atomic recovery should restore primary existence");
    expect(primary->size == sizeof(old_value), "atomic recovery should restore primary size");
    expect(memcmp(primary->data, old_value, sizeof(old_value)) == 0,
           "atomic recovery should restore old primary contents");
    expect(!backup->exists, "atomic recovery should consume the backup");
    expect(!temp->exists, "atomic recovery should remove stale temp data");
}

static void test_atomic_file_remove_deletes_primary_temp_and_backup(void) {
    Storage storage = {0};
    TestStorageFile *primary = NULL;
    TestStorageFile *backup = NULL;
    TestStorageFile *temp = NULL;
    static const char path[] = ZF_APP_DATA_DIR "/atomic.bin";
    static const char temp_path[] = ZF_APP_DATA_DIR "/atomic.tmp";
    static const char backup_path[] = ZF_APP_DATA_DIR "/atomic.bin.bak";

    test_storage_reset();
    primary = test_storage_file_slot(path, true);
    backup = test_storage_file_slot(backup_path, true);
    temp = test_storage_file_slot(temp_path, true);
    expect(primary != NULL && backup != NULL && temp != NULL, "allocate slots for atomic remove");
    primary->exists = true;
    primary->size = 1U;
    backup->exists = true;
    backup->size = 1U;
    temp->exists = true;
    temp->size = 1U;

    expect(zf_storage_remove_atomic_file(&storage, path, temp_path),
           "atomic remove should remove all related paths");
    expect(!primary->exists, "atomic remove should remove primary");
    expect(!backup->exists, "atomic remove should remove backup");
    expect(!temp->exists, "atomic remove should remove temp");
}

static void test_pin_auth_mismatch_keeps_block_state_when_persist_fails(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0};
    uint8_t pin_auth[ZF_PIN_AUTH_LEN];
    bool uv_verified = false;

    test_storage_reset();
    memset(pin_auth, 0x01, sizeof(pin_auth));
    expect(zerofido_pin_init(&storage, &state), "init PIN state for pinAuth persistence failure");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for pinAuth persistence failure");
    mark_pin_token_issued(&state);

    g_storage_fail_rename_match = "client_pin";
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "first pinAuth mismatch should stay invalid when persistence fails");
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "second pinAuth mismatch should stay invalid when persistence fails");
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_BLOCKED,
           "third pinAuth mismatch should still block when persistence fails");
    expect(state.pin_consecutive_mismatches == 3,
           "in-memory pinAuth mismatch count should still advance");
    expect(state.pin_auth_blocked, "in-memory pinAuth block should stick");
}

static void test_wrong_pin_keeps_retry_state_when_persist_fails(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for wrong PIN persistence failure");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for wrong PIN persistence failure");

    g_storage_fail_rename_match = "client_pin";
    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_INVALID,
           "first wrong PIN should stay invalid when persistence fails");
    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_INVALID,
           "second wrong PIN should stay invalid when persistence fails");
    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_AUTH_BLOCKED,
           "third wrong PIN should still auth-block when persistence fails");
    expect(state.pin_retries == (ZF_PIN_RETRIES_MAX - 3),
           "in-memory PIN retries should still decrement");
    expect(state.pin_consecutive_mismatches == 3,
           "in-memory wrong-PIN mismatch count should still advance");
    expect(state.pin_auth_blocked, "in-memory wrong-PIN auth block should stick");
}

static void test_pin_persist_failure_poison_blocks_reinit_after_wrong_pin(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for fail-closed wrong-PIN test");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for fail-closed wrong-PIN test");

    g_storage_fail_rename_match = "client_pin";
    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_INVALID,
           "wrong PIN should still return pin invalid under persistence failure");
    expect(!zerofido_pin_init(&storage, &restored),
           "re-init should fail closed after wrong-PIN persistence failure");
}

static void test_pin_persist_failure_poison_blocks_reinit_after_pin_auth_mismatch(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0};
    uint8_t pin_auth[ZF_PIN_AUTH_LEN];
    bool uv_verified = false;

    test_storage_reset();
    memset(pin_auth, 0x01, sizeof(pin_auth));
    expect(zerofido_pin_init(&storage, &state), "init PIN state for fail-closed pinAuth test");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for fail-closed pinAuth test");
    mark_pin_token_issued(&state);

    g_storage_fail_rename_match = "client_pin";
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     sizeof(pin_auth), true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_PIN_AUTH_INVALID,
           "pinAuth mismatch should still report invalid under persistence failure");
    expect(!zerofido_pin_init(&storage, &restored),
           "re-init should fail closed after pinAuth persistence failure");
}

static void test_pin_persist_failure_falls_back_to_remove_when_poison_write_fails(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    ZfClientPinState restored = {0};

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state), "init PIN state for poison-write fallback test");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for poison-write fallback test");

    g_storage_fail_rename_match = "client_pin";
    g_storage_fail_write_match = "client_pin";
    expect(zerofido_pin_verify_plaintext(&storage, &state, "9999") == ZF_CTAP_ERR_PIN_INVALID,
           "wrong PIN should stay invalid when persistence and poison write both fail");
    expect(zerofido_pin_init(&storage, &restored),
           "re-init should succeed once the persisted PIN file has been removed");
    expect(!restored.pin_set, "poison-write fallback should leave no persisted PIN state behind");
}

static void test_correct_pin_auth_keeps_retry_state_when_persist_fails(void) {
    Storage storage = {0};
    ZfClientPinState state = {0};
    uint8_t client_data_hash[ZF_CLIENT_DATA_HASH_LEN] = {0x5A};
    uint8_t pin_auth[32];
    bool uv_verified = false;

    test_storage_reset();
    expect(zerofido_pin_init(&storage, &state),
           "init PIN state for successful pinAuth persistence failure");
    expect(zerofido_pin_set_plaintext(&storage, &state, "1234") == ZF_CTAP_SUCCESS,
           "set PIN for successful pinAuth persistence failure");
    mark_pin_token_issued(&state);
    state.pin_consecutive_mismatches = 2;
    state.pin_auth_blocked = false;
    expect(zf_pin_persist_state(&storage, &state), "persist mismatch state before successful auth");
    expect(zf_crypto_hmac_sha256(state.pin_token, sizeof(state.pin_token), client_data_hash,
                                 sizeof(client_data_hash), pin_auth),
           "derive valid pinAuth before persistence failure");

    g_storage_fail_rename_match = "client_pin";
    expect(zerofido_pin_require_auth(&storage, &state, false, true, client_data_hash, pin_auth,
                                     ZF_PIN_AUTH_LEN, true, 1, NULL, 0,
                                     &uv_verified) == ZF_CTAP_ERR_OTHER,
           "successful pinAuth should fail closed when clearing retry state cannot persist");
    expect(state.pin_consecutive_mismatches == 2,
           "failed clear should preserve in-memory pinAuth mismatch count");
    expect(!state.pin_auth_blocked, "failed clear should preserve in-memory auth block flag");
}

static void test_get_assertion_polls_control_without_holding_ui_mutex(void) {
    Storage storage = {0};
    ZerofidoApp app = {0};
    ZfCredentialIndexEntry app_store_records[ZF_MAX_CREDENTIALS] = {0};
    app.store.records = app_store_records;
    FuriMutex ui_mutex = {0};
    uint8_t request[256];
    uint8_t response[512];
    size_t response_len = 0;
    ZfCredentialRecord record = {0};

    test_storage_reset();
    app.storage = &storage;
    app.ui_mutex = &ui_mutex;
    seed_assertion_queue_record(&record, 0xAB, 4);
    strcpy(record.rp_id, "example.com");
    zf_store_index_entry_from_record(&record, &app.store.records[0]);
    app.store.count = 1;
    g_transport_poll_requires_unlocked = true;

    response_len = zerofido_handle_ctap2(
        &app, 0x01020304, request, encode_get_assertion_ctap_request(request, sizeof(request)),
        response, sizeof(response));

    expect(response_len > 1, "GetAssertion should still succeed when poll requires unlocked state");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "GetAssertion should poll transport control without holding the UI mutex");
}

static void test_get_next_assertion_polls_control_without_holding_ui_mutex(void) {
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
           (const uint8_t[ZF_CLIENT_DATA_HASH_LEN]){0x7D}, ZF_CLIENT_DATA_HASH_LEN);
    app.assertion_queue.record_indices[0] = 0;
    app.assertion_queue.record_indices[1] = 1;
    g_transport_poll_requires_unlocked = true;

    response_len =
        zerofido_handle_ctap2(&app, 0x01020304, (const uint8_t[]){ZfCtapeCmdGetNextAssertion}, 1,
                              response, sizeof(response));

    expect(response_len > 1, "GetNextAssertion should still succeed when poll needs unlocked UI");
    expect(response[0] == ZF_CTAP_SUCCESS,
           "GetNextAssertion should poll transport control without holding the UI mutex");
}

static void test_store_add_record_failure_keeps_original_record(void) {
    Storage storage = {0};
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    store.records = store_records;
    ZfCredentialRecord first = {0};
    ZfCredentialRecord second = {0};
    ZfCredentialRecord loaded = {0};

    test_storage_reset();
    strcpy(first.rp_id, "example.com");
    strcpy(second.rp_id, "example.com");
    memcpy(first.user_id, "user", 4);
    memcpy(second.user_id, "user", 4);
    first.user_id_len = 4;
    second.user_id_len = 4;
    first.in_use = true;
    second.in_use = true;
    first.resident_key = true;
    second.resident_key = true;
    first.credential_id_len = ZF_CREDENTIAL_ID_LEN;
    second.credential_id_len = ZF_CREDENTIAL_ID_LEN;
    memset(first.credential_id, 0x10, ZF_CREDENTIAL_ID_LEN);
    memset(second.credential_id, 0x20, ZF_CREDENTIAL_ID_LEN);
    strcpy(first.file_name, k_repeated_credential_file_name);
    strcpy(second.file_name, "2020202020202020202020202020202020202020202020202020202020202020");

    expect(test_zf_store_add_record(&storage, &store, &first), "write original resident record");

    g_storage_fail_rename_match = second.file_name;
    expect(!test_zf_store_add_record(&storage, &store, &second),
           "adding a second resident record should fail when its rename fails");
    expect(store.count == 1, "failed add must keep in-memory store unchanged");
    expect(memcmp(store.records[0].credential_id, first.credential_id, ZF_CREDENTIAL_ID_LEN) == 0,
           "failed add must keep original credential in memory");
    expect(test_zf_store_record_format_load_record(&storage, first.file_name, &loaded),
           "failed add should keep original file on disk");
    expect(!test_zf_store_record_format_load_record(&storage, second.file_name, &loaded),
           "failed add must not leave the second record committed");
}

static void test_store_file_advances_record_rollback_to_counter_high_water(void) {
    Storage storage = {0};
    ZfCredentialRecord record = {0};
    ZfCredentialRecord loaded = {0};
    TestStorageFile *record_slot = NULL;
    TestStorageFile *counter_slot = NULL;
    uint8_t original_bytes[TEST_STORAGE_MAX_FILE_SIZE];
    size_t original_size = 0;
    uint32_t high_water = 0;
    char record_path[128];
    char counter_path[128];

    test_storage_reset();
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare rollback test credential");
    record.sign_count = 3;
    expect(test_zf_store_record_format_write_record(&storage, &record),
           "write initial credential record for rollback test");

    snprintf(record_path, sizeof(record_path), "%s/%s", ZF_APP_DATA_DIR, record.file_name);
    snprintf(counter_path, sizeof(counter_path), "%s/%s.counter", ZF_APP_DATA_DIR,
             record.file_name);
    record_slot = test_storage_file_slot(record_path, false);
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(record_slot != NULL && record_slot->exists,
           "initial credential record should exist in storage");
    expect(counter_slot == NULL || !counter_slot->exists,
           "initial credential write should not publish a companion counter file");

    memcpy(original_bytes, record_slot->data, record_slot->size);
    original_size = record_slot->size;

    record.sign_count = 9;
    expect(zf_store_record_format_reserve_counter(&storage, &record, &high_water),
           "reserve counter high-water for rollback test");
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(counter_slot != NULL && counter_slot->exists && counter_slot->size > 0,
           "counter reservation should publish the companion counter file");

    memcpy(record_slot->data, original_bytes, original_size);
    record_slot->size = original_size;
    record_slot->exists = true;

    expect(test_zf_store_record_format_load_record(&storage, record.file_name, &loaded),
           "rolled-back records should load using the counter high-water mark");
    expect(loaded.sign_count == high_water,
           "rolled-back record sign count should advance to the counter high-water mark");
}

static void test_store_file_preserves_reserved_floor_when_counter_file_is_missing(void) {
    Storage storage = {0};
    ZfCredentialRecord record = {0};
    ZfCredentialRecord loaded = {0};
    TestStorageFile *counter_slot = NULL;
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    uint32_t high_water = 0;
    char counter_path[128];

    test_storage_reset();
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare missing-counter rollback test credential");
    record.sign_count = 3;
    expect(test_zf_store_record_format_write_record(&storage, &record),
           "write initial credential record for missing-counter rollback test");

    record.sign_count = 9;
    expect(zf_store_record_format_reserve_counter_with_buffer(&storage, &record, buffer,
                                                              sizeof(buffer), &high_water),
           "reserve counter high-water in record and companion files");

    zf_store_build_counter_floor_path(record.file_name, counter_path, sizeof(counter_path));
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(counter_slot != NULL && counter_slot->exists,
           "counter reservation should publish the companion counter file before deletion");
    counter_slot->exists = false;
    counter_slot->size = 0;

    expect(test_zf_store_record_format_load_record(&storage, record.file_name, &loaded),
           "record should load from embedded high-water if companion counter file is missing");
    expect(loaded.sign_count == high_water,
           "missing companion counter file must not lower the reserved sign counter");
}

static void test_store_file_bootstraps_legacy_record_without_embedded_counter_floor(void) {
    Storage storage = {0};
    ZfCredentialRecord loaded = {0};
    TestStorageFile *record_slot = NULL;
    TestStorageFile *counter_slot = NULL;
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    size_t size = 0;
    char record_path[128];
    char counter_path[128];

    test_storage_reset();
    size = encode_complete_record_with_version(buffer, sizeof(buffer), ZF_STORE_VERSION);
    zf_store_build_record_path(k_repeated_credential_file_name, record_path, sizeof(record_path));
    zf_store_build_counter_floor_path(k_repeated_credential_file_name, counter_path,
                                      sizeof(counter_path));
    record_slot = test_storage_file_slot(record_path, true);
    expect(record_slot != NULL, "allocate legacy record slot");
    memcpy(record_slot->data, buffer, size);
    record_slot->size = size;
    record_slot->exists = true;

    expect(
        test_zf_store_record_format_load_record(&storage, k_repeated_credential_file_name, &loaded),
        "legacy records without embedded counter floors should bootstrap");
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(counter_slot != NULL && counter_slot->exists,
           "legacy record bootstrap should publish a companion counter floor");
    expect(loaded.sign_count == 7, "legacy bootstrap should preserve record sign count");
}

static void test_store_file_rejects_corrupt_embedded_counter_floor(void) {
    Storage storage = {0};
    ZfCredentialRecord record = {0};
    ZfCredentialRecord loaded = {0};
    TestStorageFile *record_slot = NULL;
    TestStorageFile *counter_slot = NULL;
    size_t floor_offset = 0;
    size_t floor_len = 0;
    char record_path[128];
    char counter_path[128];

    test_storage_reset();
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential for corrupt embedded floor test");
    record.sign_count = 5;
    expect(test_zf_store_record_format_write_record(&storage, &record),
           "write credential with embedded counter floor");

    snprintf(record_path, sizeof(record_path), "%s/%s", ZF_APP_DATA_DIR, record.file_name);
    snprintf(counter_path, sizeof(counter_path), "%s/%s.counter", ZF_APP_DATA_DIR,
             record.file_name);
    record_slot = test_storage_file_slot(record_path, false);
    expect(record_slot != NULL && record_slot->exists, "record should exist before corruption");
    expect(find_record_bytes_field_offset(record_slot->data, record_slot->size,
                                          ZfRecordKeyCounterFloor, &floor_offset, &floor_len),
           "encoded record should expose embedded counter floor");
    expect(floor_len == sizeof(ZfCounterFloorFile), "embedded counter floor should have seal size");
    record_slot->data[floor_offset] ^= 0x01;

    expect(!test_zf_store_record_format_load_record(&storage, record.file_name, &loaded),
           "corrupt embedded counter floor should fail closed");
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(counter_slot == NULL || !counter_slot->exists,
           "corrupt embedded counter floor must not be blessed into a companion file");
}

static void test_store_file_external_counter_floor_wins_over_embedded_floor(void) {
    Storage storage = {0};
    ZfCredentialRecord record = {0};
    ZfCredentialRecord loaded = {0};
    TestStorageFile *record_slot = NULL;
    size_t floor_offset = 0;
    size_t floor_len = 0;
    uint32_t high_water = 0;
    char record_path[128];

    test_storage_reset();
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential for external floor precedence test");
    record.sign_count = 3;
    expect(test_zf_store_record_format_write_record(&storage, &record),
           "write credential with embedded counter floor");
    record.sign_count = 9;
    expect(zf_store_record_format_reserve_counter(&storage, &record, &high_water),
           "reserve external high-water before corrupting embedded floor");

    snprintf(record_path, sizeof(record_path), "%s/%s", ZF_APP_DATA_DIR, record.file_name);
    record_slot = test_storage_file_slot(record_path, false);
    expect(record_slot != NULL && record_slot->exists, "record should exist before corruption");
    expect(find_record_bytes_field_offset(record_slot->data, record_slot->size,
                                          ZfRecordKeyCounterFloor, &floor_offset, &floor_len),
           "encoded record should expose embedded counter floor for precedence test");
    expect(floor_len == sizeof(ZfCounterFloorFile), "embedded counter floor should have seal size");
    record_slot->data[floor_offset] ^= 0x01;

    expect(test_zf_store_record_format_load_record(&storage, record.file_name, &loaded),
           "external counter floor should win over corrupt embedded floor");
    expect(loaded.sign_count == high_water,
           "external counter floor should advance the loaded sign count");
}

static void test_store_advance_counter_uses_reserved_counter_window(void) {
    Storage storage = {0};
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    ZfCredentialRecord record = {0};

    test_storage_reset();
    store.records = store_records;
    store.count = 1;
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential for counter high-water test");
    zf_store_index_entry_from_record(&record, &store.records[0]);
    store.records[0].counter_high_water = 2;

    record.sign_count = 1;
    expect(zf_store_advance_counter(&storage, &store, &record),
           "counter inside high-water window should advance in memory");
    expect(g_storage_counter_rename_count == 0,
           "counter inside high-water window should not rewrite the counter file");
    expect(store.records[0].sign_count == 1, "in-memory sign count should advance");

    record.sign_count = 3;
    expect(zf_store_advance_counter(&storage, &store, &record),
           "counter past high-water window should reserve a new window");
    expect(g_storage_counter_rename_count == 1,
           "crossing high-water should rewrite only the small counter file");
    expect(store.records[0].counter_high_water > record.sign_count,
           "new counter high-water should reserve future assertions");

    record.sign_count = 4;
    expect(zf_store_advance_counter(&storage, &store, &record),
           "counter inside the new high-water window should advance");
    expect(g_storage_counter_rename_count == 1,
           "counter file should not be rewritten again inside the new window");
}

static void test_store_update_record_reserves_counter_floor_before_counter_advance(void) {
    Storage storage = {0};
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    ZfCredentialRecord record = {0};
    uint8_t buffer[ZF_STORE_RECORD_IO_SIZE];
    char counter_path[128];
    TestStorageFile *counter_slot = NULL;

    test_storage_reset();
    store.records = store_records;
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential for update counter reservation test");
    record.sign_count = 3;
    expect(test_zf_store_add_record(&storage, &store, &record),
           "add credential before update counter reservation test");

    record.sign_count = 2;
    expect(!zf_store_update_record_with_buffer(&storage, &store, &record, buffer, sizeof(buffer)),
           "record update should reject counter rollback");

    record.sign_count = 9;
    expect(zf_store_update_record_with_buffer(&storage, &store, &record, buffer, sizeof(buffer)),
           "record update should reserve counter floor before advancing sign count");
    snprintf(counter_path, sizeof(counter_path), "%s/%s.counter", ZF_APP_DATA_DIR,
             record.file_name);
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(counter_slot != NULL && counter_slot->exists,
           "counter-advancing record update should publish a companion counter floor");
    expect(store.records[0].sign_count == 9, "record update should publish the new sign count");
    expect(store.records[0].counter_high_water > store.records[0].sign_count,
           "record update should keep the reserved high-water in the index");
}

static void test_store_file_write_and_load_fail_closed_without_counter_floor_crypto(void) {
    Storage storage = {0};
    ZfCredentialRecord record = {0};
    ZfCredentialRecord loaded = {0};
    char record_path[128];
    char counter_path[128];
    TestStorageFile *record_slot = NULL;
    TestStorageFile *counter_slot = NULL;

    test_storage_reset();
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential for counter floor crypto failure test");
    record.sign_count = 4;
    g_fail_crypto_enclave_load_key = true;
    expect(!test_zf_store_record_format_write_record(&storage, &record),
           "record write should fail closed without counter floor encryption");

    snprintf(record_path, sizeof(record_path), "%s/%s", ZF_APP_DATA_DIR, record.file_name);
    snprintf(counter_path, sizeof(counter_path), "%s/%s.counter", ZF_APP_DATA_DIR,
             record.file_name);
    record_slot = test_storage_file_slot(record_path, false);
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(record_slot == NULL || !record_slot->exists,
           "record file must not be committed when counter floor encryption fails");
    expect(counter_slot == NULL || !counter_slot->exists,
           "counter floor should be absent when encryption is unavailable");

    expect(!test_zf_store_record_format_load_record(&storage, record.file_name, &loaded),
           "record load should fail closed without a committed record");
}

static void test_store_counter_reservation_failure_does_not_publish_counter(void) {
    Storage storage = {0};
    ZfCredentialRecord record = {0};
    char record_path[128];
    char counter_path[128];
    TestStorageFile *record_slot = NULL;
    TestStorageFile *counter_slot = NULL;
    uint32_t high_water = 0;

    test_storage_reset();
    expect(zf_store_prepare_credential(&record, "example.com", (const uint8_t *)"user-1", 6,
                                       "alice", "Alice", true),
           "prepare credential for counter floor failure test");
    record.sign_count = 4;
    expect(test_zf_store_record_format_write_record(&storage, &record),
           "embedded counter floor should allow the record to commit by itself");

    g_storage_fail_rename_match = ".counter";

    expect(!zf_store_record_format_reserve_counter(&storage, &record, &high_water),
           "counter reservation should fail when the counter commit fails");

    snprintf(record_path, sizeof(record_path), "%s/%s", ZF_APP_DATA_DIR, record.file_name);
    snprintf(counter_path, sizeof(counter_path), "%s/%s.counter", ZF_APP_DATA_DIR,
             record.file_name);
    record_slot = test_storage_file_slot(record_path, false);
    counter_slot = test_storage_file_slot(counter_path, false);
    expect(record_slot != NULL && record_slot->exists,
           "failed counter reservation must not remove the committed record");
    expect(counter_slot == NULL || !counter_slot->exists,
           "failed counter floor commit must not publish a counter file");
}
