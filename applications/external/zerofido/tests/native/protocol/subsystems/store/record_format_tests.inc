static size_t encode_complete_record(uint8_t *buffer, size_t capacity) {
    ZfCredentialRecord record = {0};
    size_t encoded_size = 0;

    memset(record.credential_id, 0x10, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    strcpy(record.file_name, k_repeated_credential_file_name);
    strcpy(record.rp_id, "example.com");
    memcpy(record.user_id, "user-1", 6);
    record.user_id_len = 6;
    strcpy(record.user_name, "alice");
    strcpy(record.user_display_name, "Alice Example");
    memset(record.public_x, 0x21, sizeof(record.public_x));
    memset(record.public_y, 0x22, sizeof(record.public_y));
    memset(record.private_wrapped, 0x23, sizeof(record.private_wrapped));
    memset(record.private_iv, 0x24, sizeof(record.private_iv));
    record.sign_count = 7;
    record.created_at = 1234;
    record.resident_key = true;

    expect(zf_store_record_format_encode(&record, buffer, &encoded_size), "encode complete record");
    expect(encoded_size <= capacity, "encoded record fits");
    return encoded_size;
}

static void fill_large_resident_hmac_secret_record(ZfCredentialRecord *record) {
    memset(record, 0, sizeof(*record));
    memset(record->credential_id, 0x10, sizeof(record->credential_id));
    record->credential_id_len = sizeof(record->credential_id);
    strcpy(record->file_name, k_repeated_credential_file_name);
    memset(record->rp_id, 'r', 253);
    record->rp_id[253] = '\0';
    memset(record->user_id, 0xA5, sizeof(record->user_id));
    record->user_id_len = sizeof(record->user_id);
    memset(record->user_name, 'n', ZF_MAX_USER_NAME_LEN - 1U);
    record->user_name[ZF_MAX_USER_NAME_LEN - 1U] = '\0';
    memset(record->user_display_name, 'd', ZF_MAX_DISPLAY_NAME_LEN - 1U);
    record->user_display_name[ZF_MAX_DISPLAY_NAME_LEN - 1U] = '\0';
    memset(record->public_x, 0x21, sizeof(record->public_x));
    memset(record->public_y, 0x22, sizeof(record->public_y));
    memset(record->private_wrapped, 0x23, sizeof(record->private_wrapped));
    memset(record->private_iv, 0x24, sizeof(record->private_iv));
    record->sign_count = UINT32_MAX - 1U;
    record->created_at = UINT32_MAX - 2U;
    record->resident_key = true;
    record->cred_protect = ZF_CRED_PROTECT_UV_OPTIONAL;
    record->hmac_secret = true;
    memset(record->hmac_secret_without_uv, 0x55, sizeof(record->hmac_secret_without_uv));
    memset(record->hmac_secret_with_uv, 0x66, sizeof(record->hmac_secret_with_uv));
    record->in_use = true;
}

static void fill_hmac_secret_record(ZfCredentialRecord *record) {
    memset(record, 0, sizeof(*record));
    memset(record->credential_id, 0x10, sizeof(record->credential_id));
    record->credential_id_len = sizeof(record->credential_id);
    strcpy(record->file_name, k_repeated_credential_file_name);
    strcpy(record->rp_id, "example.com");
    memcpy(record->user_id, "user-1", 6);
    record->user_id_len = 6;
    strcpy(record->user_name, "alice");
    strcpy(record->user_display_name, "Alice Example");
    memset(record->public_x, 0x21, sizeof(record->public_x));
    memset(record->public_y, 0x22, sizeof(record->public_y));
    memset(record->private_wrapped, 0x23, sizeof(record->private_wrapped));
    memset(record->private_iv, 0x24, sizeof(record->private_iv));
    record->sign_count = 7;
    record->created_at = 1234;
    record->resident_key = true;
    record->cred_protect = ZF_CRED_PROTECT_UV_OPTIONAL;
    record->hmac_secret = true;
    memset(record->hmac_secret_without_uv, 0x55, sizeof(record->hmac_secret_without_uv));
    memset(record->hmac_secret_with_uv, 0x66, sizeof(record->hmac_secret_with_uv));
}

static bool read_record_hmac_secret_storage_fields(const uint8_t *buffer, size_t size,
                                                   const uint8_t **wrapped, size_t *wrapped_len,
                                                   size_t *iv_len) {
    ZfCborCursor cursor;
    size_t pairs = 0;
    bool saw_wrapped = false;
    bool saw_iv = false;

    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }
    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        const uint8_t *bytes = NULL;
        size_t bytes_len = 0;

        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == ZfRecordKeyHmacSecretWrapped) {
            if (!zf_cbor_read_bytes_ptr(&cursor, &bytes, &bytes_len)) {
                return false;
            }
            *wrapped = bytes;
            *wrapped_len = bytes_len;
            saw_wrapped = true;
        } else if (key == ZfRecordKeyHmacSecretIv) {
            if (!zf_cbor_read_bytes_ptr(&cursor, &bytes, &bytes_len)) {
                return false;
            }
            *iv_len = bytes_len;
            saw_iv = true;
        } else if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }
    return saw_wrapped && saw_iv && cursor.ptr == cursor.end;
}

static bool find_record_bytes_field_offset(uint8_t *buffer, size_t size, uint64_t target_key,
                                           size_t *out_offset, size_t *out_len) {
    ZfCborCursor cursor;
    size_t pairs = 0;

    if (!buffer || !out_offset || !out_len) {
        return false;
    }
    zf_cbor_cursor_init(&cursor, buffer, size);
    if (!zf_cbor_read_map_start(&cursor, &pairs)) {
        return false;
    }
    for (size_t i = 0; i < pairs; ++i) {
        uint64_t key = 0;
        const uint8_t *bytes = NULL;
        size_t bytes_len = 0;

        if (!zf_cbor_read_uint(&cursor, &key)) {
            return false;
        }
        if (key == target_key) {
            if (!zf_cbor_read_bytes_ptr(&cursor, &bytes, &bytes_len)) {
                return false;
            }
            *out_offset = (size_t)(bytes - buffer);
            *out_len = bytes_len;
            return true;
        }
        if (!zf_cbor_skip(&cursor)) {
            return false;
        }
    }
    return false;
}

static bool append_duplicate_hmac_secret_field(uint8_t *buffer, size_t capacity, size_t *size,
                                               uint64_t key, size_t field_size) {
    ZfCborEncoder enc;
    uint8_t field[ZF_RECORD_HMAC_SECRET_STORAGE_LEN] = {0};

    if (!buffer || !size || *size >= capacity || field_size > sizeof(field) || buffer[0] != 0xB1) {
        return false;
    }
    buffer[0] = 0xB2;
    memset(field, 0xCC, field_size);
    if (!zf_cbor_encoder_init(&enc, buffer + *size, capacity - *size) ||
        !zf_cbor_encode_uint(&enc, key) || !zf_cbor_encode_bytes(&enc, field, field_size)) {
        return false;
    }
    *size += zf_cbor_encoder_size(&enc);
    return true;
}

static size_t encode_legacy_plaintext_hmac_secret_record(uint8_t *buffer, size_t capacity) {
    ZfCredentialRecord record = {0};
    ZfCborEncoder enc;

    fill_hmac_secret_record(&record);

    expect(zf_cbor_encoder_init(&enc, buffer, capacity), "init legacy hmac-secret record encoder");
    expect(zf_cbor_encode_map(&enc, 16) && zf_cbor_encode_uint(&enc, ZfRecordKeyVersion) &&
               zf_cbor_encode_uint(&enc, ZF_STORE_VERSION) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyCredentialId) &&
               zf_cbor_encode_bytes(&enc, record.credential_id, record.credential_id_len) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyRpId) &&
               zf_cbor_encode_text(&enc, record.rp_id) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyUserId) &&
               zf_cbor_encode_bytes(&enc, record.user_id, record.user_id_len) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyUserName) &&
               zf_cbor_encode_text(&enc, record.user_name) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyDisplayName) &&
               zf_cbor_encode_text(&enc, record.user_display_name) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyPublicX) &&
               zf_cbor_encode_bytes(&enc, record.public_x, sizeof(record.public_x)) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyPublicY) &&
               zf_cbor_encode_bytes(&enc, record.public_y, sizeof(record.public_y)) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyPrivateWrapped) &&
               zf_cbor_encode_bytes(&enc, record.private_wrapped, sizeof(record.private_wrapped)) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyPrivateIv) &&
               zf_cbor_encode_bytes(&enc, record.private_iv, sizeof(record.private_iv)) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeySignCount) &&
               zf_cbor_encode_uint(&enc, record.sign_count) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyCreatedAt) &&
               zf_cbor_encode_uint(&enc, record.created_at) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyResidentKey) &&
               zf_cbor_encode_bool(&enc, record.resident_key) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyCredProtect) &&
               zf_cbor_encode_uint(&enc, record.cred_protect) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyHmacSecretWrapped) &&
               zf_cbor_encode_bytes(&enc, record.hmac_secret_without_uv,
                                    sizeof(record.hmac_secret_without_uv)) &&
               zf_cbor_encode_uint(&enc, ZfRecordKeyHmacSecretIv) &&
               zf_cbor_encode_bytes(&enc, record.hmac_secret_with_uv,
                                    sizeof(record.hmac_secret_with_uv)),
           "encode legacy plaintext hmac-secret record");
    return zf_cbor_encoder_size(&enc);
}

static void test_record_decode_rejects_partial_record(void) {
    uint8_t buffer[128];
    ZfCborEncoder enc;
    ZfCredentialRecord record;

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init partial record encoder");
    expect(zf_cbor_encode_map(&enc, 2) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_uint(&enc, 3) &&
               zf_cbor_encode_text(&enc, "example.com"),
           "encode partial record");

    expect(!test_zf_record_decode(buffer, zf_cbor_encoder_size(&enc),
                                  k_repeated_credential_file_name, &record),
           "partial record should be rejected");
}

static void test_record_decode_accepts_complete_record(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord record;
    size_t size = encode_complete_record(buffer, sizeof(buffer));

    expect(test_zf_record_decode(buffer, size, k_repeated_credential_file_name, &record),
           "complete record should decode");
    expect(record.in_use, "decoded record should be marked in use");
    expect(record.resident_key, "decoded record should preserve resident key");
    expect(strcmp(record.rp_id, "example.com") == 0, "decoded rp id");
}

static void test_record_encode_accepts_large_resident_record_with_hmac_secret(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord record = {0};
    size_t encoded_size = 0;

    fill_large_resident_hmac_secret_record(&record);

    expect(zf_store_record_format_encode(&record, buffer, &encoded_size),
           "large resident hmac-secret record should encode");
    expect(encoded_size <= sizeof(buffer), "large resident hmac-secret record should fit buffer");
}

static void test_record_hmac_secret_storage_uses_wrapped_blob(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord record = {0};
    ZfCredentialRecord decoded = {0};
    size_t encoded_size = 0;
    const uint8_t *wrapped = NULL;
    size_t wrapped_len = 0;
    size_t iv_len = 0;

    fill_hmac_secret_record(&record);

    expect(zf_store_record_format_encode(&record, buffer, &encoded_size),
           "encode hmac-secret record with wrapped storage");
    expect(read_record_hmac_secret_storage_fields(buffer, encoded_size, &wrapped, &wrapped_len,
                                                  &iv_len),
           "encoded hmac-secret record should expose wrapped storage fields");
    expect(wrapped_len == ZF_RECORD_HMAC_SECRET_STORAGE_LEN,
           "hmac-secret storage should use one wrapped seed blob");
    expect(iv_len == ZF_WRAP_IV_LEN, "hmac-secret storage should carry a wrap IV");
    expect(memcmp(wrapped, record.hmac_secret_without_uv, sizeof(record.hmac_secret_without_uv)) !=
               0,
           "wrapped hmac-secret blob should not start with plaintext without-UV seed");
    expect(memcmp(wrapped + sizeof(record.hmac_secret_without_uv), record.hmac_secret_with_uv,
                  sizeof(record.hmac_secret_with_uv)) != 0,
           "wrapped hmac-secret blob should not end with plaintext with-UV seed");
    expect(test_zf_record_decode(buffer, encoded_size, k_repeated_credential_file_name, &decoded),
           "wrapped hmac-secret record should decode");
    expect(decoded.hmac_secret, "decoded record should preserve hmac-secret capability");
    expect(memcmp(decoded.hmac_secret_without_uv, record.hmac_secret_without_uv,
                  sizeof(record.hmac_secret_without_uv)) == 0,
           "decoded hmac-secret without-UV seed");
    expect(memcmp(decoded.hmac_secret_with_uv, record.hmac_secret_with_uv,
                  sizeof(record.hmac_secret_with_uv)) == 0,
           "decoded hmac-secret with-UV seed");
}

static void test_record_decode_rejects_legacy_plaintext_hmac_secret_storage(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord record = {0};
    size_t size = encode_legacy_plaintext_hmac_secret_record(buffer, sizeof(buffer));

    expect(!test_zf_record_decode(buffer, size, k_repeated_credential_file_name, &record),
           "legacy plaintext hmac-secret storage should be rejected");
}

static void test_record_decode_rejects_duplicate_hmac_secret_storage_fields(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord source = {0};
    ZfCredentialRecord record = {0};
    size_t size = 0;

    fill_hmac_secret_record(&source);
    expect(zf_store_record_format_encode(&source, buffer, &size),
           "encode hmac-secret record for duplicate wrapped test");
    expect(append_duplicate_hmac_secret_field(buffer, sizeof(buffer), &size,
                                              ZfRecordKeyHmacSecretWrapped,
                                              ZF_RECORD_HMAC_SECRET_STORAGE_LEN),
           "append duplicate hmac-secret wrapped field");
    expect(!test_zf_record_decode(buffer, size, k_repeated_credential_file_name, &record),
           "duplicate hmac-secret wrapped fields should be rejected");

    expect(zf_store_record_format_encode(&source, buffer, &size),
           "encode hmac-secret record for duplicate IV test");
    expect(append_duplicate_hmac_secret_field(buffer, sizeof(buffer), &size,
                                              ZfRecordKeyHmacSecretIv, ZF_WRAP_IV_LEN),
           "append duplicate hmac-secret IV field");
    expect(!test_zf_record_decode(buffer, size, k_repeated_credential_file_name, &record),
           "duplicate hmac-secret IV fields should be rejected");
}

static void test_record_decode_rejects_embedded_nul_text(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCborEncoder enc;
    ZfCredentialRecord record = {0};
    static const char rp_id_with_nul[] = {'e', 'x',  'a', 'm', 'p', 'l',
                                          'e', '\0', '.', 'c', 'o', 'm'};

    memset(record.credential_id, 0x10, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    memcpy(record.user_id, "user-1", 6);
    record.user_id_len = 6;
    strcpy(record.user_name, "alice");
    strcpy(record.user_display_name, "Alice Example");
    memset(record.public_x, 0x21, sizeof(record.public_x));
    memset(record.public_y, 0x22, sizeof(record.public_y));
    memset(record.private_wrapped, 0x23, sizeof(record.private_wrapped));
    memset(record.private_iv, 0x24, sizeof(record.private_iv));
    record.sign_count = 7;
    record.created_at = 1234;
    record.resident_key = true;

    expect(zf_cbor_encoder_init(&enc, buffer, sizeof(buffer)), "init embedded nul record encoder");
    expect(zf_cbor_encode_map(&enc, 13) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_uint(&enc, 2) && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, record.credential_id, record.credential_id_len) &&
               zf_cbor_encode_uint(&enc, 3) &&
               zf_cbor_encode_text_n(&enc, rp_id_with_nul, sizeof(rp_id_with_nul)) &&
               zf_cbor_encode_uint(&enc, 4) &&
               zf_cbor_encode_bytes(&enc, record.user_id, record.user_id_len) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_text(&enc, record.user_name) &&
               zf_cbor_encode_uint(&enc, 6) &&
               zf_cbor_encode_text(&enc, record.user_display_name) &&
               zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_bytes(&enc, record.public_x, sizeof(record.public_x)) &&
               zf_cbor_encode_uint(&enc, 8) &&
               zf_cbor_encode_bytes(&enc, record.public_y, sizeof(record.public_y)) &&
               zf_cbor_encode_uint(&enc, 9) &&
               zf_cbor_encode_bytes(&enc, record.private_wrapped, sizeof(record.private_wrapped)) &&
               zf_cbor_encode_uint(&enc, 10) &&
               zf_cbor_encode_bytes(&enc, record.private_iv, sizeof(record.private_iv)) &&
               zf_cbor_encode_uint(&enc, 11) && zf_cbor_encode_uint(&enc, record.sign_count) &&
               zf_cbor_encode_uint(&enc, 12) && zf_cbor_encode_uint(&enc, record.created_at) &&
               zf_cbor_encode_uint(&enc, 13) && zf_cbor_encode_bool(&enc, record.resident_key),
           "encode embedded nul record");

    expect(!test_zf_record_decode(
               buffer, zf_cbor_encoder_size(&enc),
               "1010101010101010101010101010101010101010101010101010101010101010",
               &record),
           "record with embedded nul text should be rejected");
}

static void test_record_decode_rejects_file_name_mismatch(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord record;
    size_t size = encode_complete_record(buffer, sizeof(buffer));

    expect(!test_zf_record_decode(buffer, size,
                                  "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
                                  &record),
           "record should reject mismatched file name");
}

static size_t encode_complete_record_with_version(uint8_t *buffer, size_t capacity,
                                                  uint64_t version) {
    ZfCredentialRecord record = {0};
    ZfCborEncoder enc;

    memset(record.credential_id, 0x10, sizeof(record.credential_id));
    record.credential_id_len = sizeof(record.credential_id);
    strcpy(record.file_name, k_repeated_credential_file_name);
    strcpy(record.rp_id, "example.com");
    memcpy(record.user_id, "user-1", 6);
    record.user_id_len = 6;
    strcpy(record.user_name, "alice");
    strcpy(record.user_display_name, "Alice Example");
    memset(record.public_x, 0x21, sizeof(record.public_x));
    memset(record.public_y, 0x22, sizeof(record.public_y));
    memset(record.private_wrapped, 0x23, sizeof(record.private_wrapped));
    memset(record.private_iv, 0x24, sizeof(record.private_iv));
    record.sign_count = 7;
    record.created_at = 1234;
    record.resident_key = true;

    expect(zf_cbor_encoder_init(&enc, buffer, capacity), "init versioned record encoder");
    expect(zf_cbor_encode_map(&enc, 14) && zf_cbor_encode_uint(&enc, 1) &&
               zf_cbor_encode_uint(&enc, version) && zf_cbor_encode_uint(&enc, 2) &&
               zf_cbor_encode_bytes(&enc, record.credential_id, record.credential_id_len) &&
               zf_cbor_encode_uint(&enc, 3) && zf_cbor_encode_text(&enc, record.rp_id) &&
               zf_cbor_encode_uint(&enc, 4) &&
               zf_cbor_encode_bytes(&enc, record.user_id, record.user_id_len) &&
               zf_cbor_encode_uint(&enc, 5) && zf_cbor_encode_text(&enc, record.user_name) &&
               zf_cbor_encode_uint(&enc, 6) &&
               zf_cbor_encode_text(&enc, record.user_display_name) &&
               zf_cbor_encode_uint(&enc, 7) &&
               zf_cbor_encode_bytes(&enc, record.public_x, sizeof(record.public_x)) &&
               zf_cbor_encode_uint(&enc, 8) &&
               zf_cbor_encode_bytes(&enc, record.public_y, sizeof(record.public_y)) &&
               zf_cbor_encode_uint(&enc, 9) &&
               zf_cbor_encode_bytes(&enc, record.private_wrapped, sizeof(record.private_wrapped)) &&
               zf_cbor_encode_uint(&enc, 10) &&
               zf_cbor_encode_bytes(&enc, record.private_iv, sizeof(record.private_iv)) &&
               zf_cbor_encode_uint(&enc, 11) && zf_cbor_encode_uint(&enc, record.sign_count) &&
               zf_cbor_encode_uint(&enc, 12) && zf_cbor_encode_uint(&enc, record.created_at) &&
               zf_cbor_encode_uint(&enc, 13) && zf_cbor_encode_bool(&enc, record.resident_key) &&
               zf_cbor_encode_uint(&enc, 14) &&
               zf_cbor_encode_uint(&enc, ZF_CRED_PROTECT_UV_OPTIONAL),
           "encode record with custom version");
    return zf_cbor_encoder_size(&enc);
}

static void test_record_decode_rejects_oversized_version(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord record;
    size_t size = encode_complete_record_with_version(buffer, sizeof(buffer), 4294967298ULL);

    expect(!test_zf_record_decode(buffer, size, k_repeated_credential_file_name, &record),
           "oversized version should be rejected");
}

static void test_record_decode_rejects_unsupported_record_version(void) {
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    ZfCredentialRecord record;
    size_t size = encode_complete_record(buffer, sizeof(buffer));

    expect(buffer[1] == 0x01 && buffer[2] == ZF_STORE_FORMAT_VERSION,
           "stored record version field should be at the expected prefix");
    buffer[2] = 0x00;

    expect(!test_zf_record_decode(buffer, size, k_repeated_credential_file_name, &record),
           "unsupported stored record versions should be rejected");
}

static void test_store_init_ignores_unsupported_record_file(void) {
    Storage storage = {0};
    ZfCredentialStore store = {0};
    ZfCredentialIndexEntry store_records[ZF_MAX_CREDENTIALS] = {0};
    store.records = store_records;
    TestStorageFile *slot = NULL;
    uint8_t buffer[ZF_STORE_RECORD_MAX_SIZE];
    size_t size = encode_complete_record(buffer, sizeof(buffer));
    char path[160];

    test_storage_reset();
    expect(buffer[1] == 0x01 && buffer[2] == ZF_STORE_FORMAT_VERSION,
           "stored record version field should be at the expected prefix");
    buffer[2] = 0x00;

    snprintf(path, sizeof(path), "%s/%s", ZF_APP_DATA_DIR, k_repeated_credential_file_name);
    slot = test_storage_file_slot(path, true);
    expect(slot != NULL, "allocate storage slot for unsupported-version record");
    memcpy(slot->data, buffer, size);
    slot->size = size;
    slot->exists = true;

    expect(test_zf_store_init(&storage, &store),
           "store init should succeed with unsupported-version record present");
    expect(store.count == 0, "store init should ignore unsupported-version record files");
    expect(slot->exists, "store init should not rewrite unsupported-version record files");
}
