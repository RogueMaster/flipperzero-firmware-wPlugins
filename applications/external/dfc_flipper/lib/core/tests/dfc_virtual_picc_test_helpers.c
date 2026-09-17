#include "dfc_virtual_picc_test_helpers.h"

void load_standard_credential(DfcCredential* credential) {
    dfc_credential_clear(credential);
    uint8_t uid[] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(credential->uid, uid, sizeof(uid));
    credential->uid_len = sizeof(uid);

    const uint8_t aid_desfire_order[] = {0x4F, 0x49, 0xD3};
    DfcApplication* app = dfc_credential_create_application_desfire_order(
        credential, aid_desfire_order, 0x0F, DFC_KEY_TYPE_DES_2K3DES | 2);
    munit_assert_not_null(app);
    uint8_t iso_aid[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
    memcpy(app->iso_aid, iso_aid, sizeof(iso_aid));
    app->iso_aid_len = sizeof(iso_aid);
    app->auth_command = DFC_CMD_AUTHENTICATE_ISO;
    app->key_versions[1] = 0x42;
    uint8_t key1[] = {
        0xC6,
        0xEF,
        0x84,
        0x7C,
        0xAE,
        0x0A,
        0x2A,
        0x56,
        0xC0,
        0x19,
        0x2A,
        0xA1,
        0xC4,
        0xE8,
        0xD9,
        0xFB};
    memcpy(dfc_credential_key(credential, app, 1), key1, sizeof(key1));

    DfcFile* file = dfc_credential_create_file(credential, 0, 0x0F);
    munit_assert_not_null(file);
    file->type = 0x00;
    file->comm_settings = DFC_COMM_MAC;
    file->access_rights = 0x1230;
    munit_assert_true(dfc_file_resize(credential, file, 3));
    uint8_t* bytes = dfc_file_data(credential, file);
    munit_assert_not_null(bytes);
    bytes[0] = 0x30;
    bytes[1] = 0x01;
    bytes[2] = 0x00;
}

static void test_des_ecb_crypt(
    bool encrypt,
    const uint8_t* key,
    size_t key_len,
    const uint8_t input[8],
    uint8_t output[8]) {
    munit_assert_true(dfc_crypto_des_ecb(encrypt, key, key_len, input, output));
}

static void test_d40_send_crypt(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* plain,
    size_t plain_len,
    uint8_t* encrypted) {
    uint8_t previous[8] = {0};
    for(size_t offset = 0; offset < plain_len; offset += 8) {
        uint8_t block[8];
        for(size_t i = 0; i < 8; i++) {
            block[i] = plain[offset + i] ^ previous[i];
        }
        test_des_ecb_crypt(false, key, key_len, block, encrypted + offset);
        memcpy(previous, encrypted + offset, sizeof(previous));
    }
}

static uint16_t test_crc16_iso14443(const uint8_t* data, size_t len) {
    uint16_t crc = 0x6363;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i] ^ (crc & 0xFF);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ ((uint16_t)byte << 8) ^ ((uint16_t)byte << 3) ^ ((uint16_t)byte >> 4);
    }
    return crc;
}

static void test_set_des_key_version(uint8_t* key, size_t key_len, uint8_t version) {
    for(size_t i = 0; i < key_len; i++) {
        key[i] &= 0xFE;
    }
    for(size_t i = 0; i < 8; i++) {
        uint8_t bit = (version >> (7 - i)) & 0x01;
        key[i] = (key[i] & 0xFE) | bit;
        if(key_len == 16) {
            key[i + 8] = (key[i + 8] & 0xFE) | ((~bit) & 0x01);
        }
    }
}

void authenticate_legacy_key0(
    DfcVirtualPiccSession* session,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t expected_status) {
    authenticate_legacy_key(
        session,
        0x00,
        key,
        key_len,
        response,
        response_capacity,
        response_len,
        session_key,
        session_key_len,
        expected_status);
}

void authenticate_legacy_key(
    DfcVirtualPiccSession* session,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t expected_status) {
    const uint8_t authenticate[] = {0x90, 0x0A, 0x00, 0x00, 0x01, key_no, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, authenticate, sizeof(authenticate), response, response_capacity, response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(*response_len, ==, 10);
    munit_assert_uint8(response[8], ==, 0x91);
    munit_assert_uint8(response[9], ==, 0xAF);

    uint8_t iv[8] = {0};
    uint8_t rnd_b[8];
    dfc_worker_des_cbc_decrypt(key, key_len, iv, 8, response, rnd_b);

    uint8_t rnd_b_rot[8];
    memcpy(rnd_b_rot, rnd_b, sizeof(rnd_b));
    dfc_rotate_left(rnd_b_rot, sizeof(rnd_b_rot));

    uint8_t rnd_a[8] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint8_t challenge[16];
    memcpy(challenge, rnd_a, sizeof(rnd_a));
    memcpy(challenge + sizeof(rnd_a), rnd_b_rot, sizeof(rnd_b_rot));

    uint8_t encrypted_challenge[16];
    test_d40_send_crypt(key, key_len, challenge, sizeof(challenge), encrypted_challenge);

    uint8_t additional_frame[22] = {0x90, 0xAF, 0x00, 0x00, 0x10};
    memcpy(additional_frame + 5, encrypted_challenge, sizeof(encrypted_challenge));
    additional_frame[21] = 0x00;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            additional_frame,
            sizeof(additional_frame),
            response,
            response_capacity,
            response_len),
        ==,
        DfcVirtualPiccStatusOk);
    if(expected_status == DFC_STATUS_OK) {
        munit_assert_size(*response_len, ==, 10);
        munit_assert_uint8(response[8], ==, 0x91);
        munit_assert_uint8(response[9], ==, DFC_STATUS_OK);
        if(!session_key || !session_key_len) return;
        dfc_derive_session_key(
            DFC_CMD_AUTHENTICATE_LEGACY, key, key_len, rnd_a, rnd_b, session_key, session_key_len);
    } else {
        munit_assert_size(*response_len, ==, 2);
        munit_assert_uint8(response[0], ==, 0x91);
        munit_assert_uint8(response[1], ==, expected_status);
    }
}

size_t build_legacy_change_key_payload(
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* new_key,
    size_t new_key_len,
    uint8_t version,
    uint8_t* out) {
    uint8_t clear[DFC_MAX_KEY_LEN + 8];
    memset(clear, 0, sizeof(clear));
    memcpy(clear, new_key, new_key_len);
    test_set_des_key_version(clear, new_key_len, version);

    uint16_t crc = test_crc16_iso14443(clear, new_key_len);
    clear[new_key_len] = crc & 0xFF;
    clear[new_key_len + 1] = crc >> 8;

    size_t clear_len = ((new_key_len + 2 + 7) / 8) * 8;
    out[0] = 0x00;
    test_d40_send_crypt(session_key, session_key_len, clear, clear_len, out + 1);
    return clear_len + 1;
}

size_t build_legacy_encrypted_payload(
    const uint8_t* session_key,
    size_t session_key_len,
    const uint8_t* clear,
    size_t clear_len,
    uint8_t* out) {
    uint8_t padded[32];
    memset(padded, 0, sizeof(padded));
    memcpy(padded, clear, clear_len);
    uint16_t crc = test_crc16_iso14443(clear, clear_len);
    padded[clear_len] = crc & 0xFF;
    padded[clear_len + 1] = crc >> 8;

    size_t padded_len = ((clear_len + 2 + 7) / 8) * 8;
    test_d40_send_crypt(session_key, session_key_len, padded, padded_len, out);
    return padded_len;
}

size_t build_legacy_change_other_key_payload(
    const uint8_t* session_key,
    size_t session_key_len,
    uint8_t key_no,
    const uint8_t* old_key,
    const uint8_t* new_key,
    size_t key_len,
    uint8_t version,
    uint8_t* out) {
    uint8_t new_stored[DFC_MAX_KEY_LEN];
    memcpy(new_stored, new_key, key_len);
    test_set_des_key_version(new_stored, key_len, version);

    uint8_t clear[DFC_MAX_KEY_LEN + 8];
    memset(clear, 0, sizeof(clear));
    for(size_t i = 0; i < key_len; i++) {
        clear[i] = new_stored[i] ^ old_key[i];
    }

    uint16_t crc = test_crc16_iso14443(clear, key_len);
    clear[key_len] = crc & 0xFF;
    clear[key_len + 1] = crc >> 8;

    uint16_t new_crc = test_crc16_iso14443(new_stored, key_len);
    clear[key_len + 2] = new_crc & 0xFF;
    clear[key_len + 3] = new_crc >> 8;

    size_t clear_len = ((key_len + 4 + 7) / 8) * 8;
    out[0] = key_no;
    test_d40_send_crypt(session_key, session_key_len, clear, clear_len, out + 1);
    return clear_len + 1;
}

static uint32_t test_crc32_dfc(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for(size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(size_t bit = 0; bit < 8; bit++) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

static void test_write_crc32_le(uint8_t* dest, uint32_t crc) {
    dest[0] = (uint8_t)(crc & 0xFF);
    dest[1] = (uint8_t)((crc >> 8) & 0xFF);
    dest[2] = (uint8_t)((crc >> 16) & 0xFF);
    dest[3] = (uint8_t)((crc >> 24) & 0xFF);
}

size_t build_ev1_change_key_payload(
    uint8_t cipher,
    const uint8_t* session_key,
    size_t session_key_len,
    uint8_t* iv,
    uint8_t auth_key_no,
    uint8_t target_key_no,
    const uint8_t* current_key,
    const uint8_t* new_key,
    size_t key_len,
    bool aes_key,
    uint8_t new_version,
    uint8_t* out) {
    bool different = auth_key_no != target_key_no;
    size_t block = cipher == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
    size_t version_len = aes_key ? 1 : 0;
    size_t content = key_len + version_len + 4 + (different ? 4 : 0);
    size_t clear_len = ((content + block - 1) / block) * block;

    uint8_t clear[48];
    memset(clear, 0, sizeof(clear));
    if(different) {
        for(size_t i = 0; i < key_len; i++) {
            clear[i] = (uint8_t)(new_key[i] ^ current_key[i]);
        }
    } else {
        memcpy(clear, new_key, key_len);
    }
    size_t cursor = key_len;
    if(aes_key) {
        clear[cursor++] = new_version;
    }

    uint8_t crc_input[2 + DFC_MAX_KEY_LEN + 1];
    crc_input[0] = DFC_CMD_CHANGE_KEY;
    crc_input[1] = target_key_no;
    memcpy(crc_input + 2, clear, cursor);
    test_write_crc32_le(clear + cursor, test_crc32_dfc(crc_input, 2 + cursor));
    cursor += 4;
    if(different) {
        test_write_crc32_le(clear + cursor, test_crc32_dfc(new_key, key_len));
        cursor += 4;
    }
    (void)cursor;

    out[0] = target_key_no;
    if(cipher == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_encrypt(session_key, session_key_len, iv, clear_len, clear, out + 1);
    } else {
        dfc_worker_des_cbc_encrypt(session_key, session_key_len, iv, clear_len, clear, out + 1);
    }
    return 1 + clear_len;
}

static void authenticate_ev1_key(
    DfcVirtualPiccSession* session,
    uint8_t auth_cmd,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t* session_iv,
    uint8_t expected_status) {
    size_t block = auth_cmd == DFC_CMD_AUTHENTICATE_AES ? 16 : 8;
    uint8_t authenticate[7] = {0x90, auth_cmd, 0x00, 0x00, 0x01, key_no, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session, authenticate, sizeof(authenticate), response, response_capacity, response_len),
        ==,
        DfcVirtualPiccStatusOk);
    munit_assert_size(*response_len, ==, block + 2);
    munit_assert_uint8(response[block], ==, 0x91);
    munit_assert_uint8(response[block + 1], ==, 0xAF);

    uint8_t iv[16] = {0};
    uint8_t rnd_b[16];
    if(auth_cmd == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_decrypt(key, key_len, iv, block, response, rnd_b);
    } else {
        dfc_worker_des_cbc_decrypt(key, key_len, iv, block, response, rnd_b);
    }

    uint8_t rnd_b_rot[16];
    memcpy(rnd_b_rot, rnd_b, block);
    dfc_rotate_left(rnd_b_rot, block);

    uint8_t rnd_a[16];
    for(size_t i = 0; i < block; i++) {
        rnd_a[i] = (uint8_t)(0x10 + i);
    }
    uint8_t challenge[32];
    memcpy(challenge, rnd_a, block);
    memcpy(challenge + block, rnd_b_rot, block);

    memcpy(iv, response, block);
    uint8_t encrypted_challenge[32];
    if(auth_cmd == DFC_CMD_AUTHENTICATE_AES) {
        dfc_worker_aes_cbc_encrypt(
            key, key_len, iv, block * 2, challenge, encrypted_challenge);
    } else {
        dfc_worker_des_cbc_encrypt(
            key, key_len, iv, block * 2, challenge, encrypted_challenge);
    }

    uint8_t additional_frame[5 + 32 + 1];
    additional_frame[0] = 0x90;
    additional_frame[1] = 0xAF;
    additional_frame[2] = 0x00;
    additional_frame[3] = 0x00;
    additional_frame[4] = (uint8_t)(block * 2);
    memcpy(additional_frame + 5, encrypted_challenge, block * 2);
    additional_frame[5 + block * 2] = 0x00;
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(
            session,
            additional_frame,
            6 + block * 2,
            response,
            response_capacity,
            response_len),
        ==,
        DfcVirtualPiccStatusOk);

    if(expected_status == DFC_STATUS_OK) {
        munit_assert_size(*response_len, ==, block + 2);
        munit_assert_uint8(response[block], ==, 0x91);
        munit_assert_uint8(response[block + 1], ==, DFC_STATUS_OK);
        if(session_key && session_key_len) {
            dfc_derive_session_key(
                auth_cmd, key, key_len, rnd_a, rnd_b, session_key, session_key_len);
        }
        if(session_iv) {
            // Post-auth EV1 IV is zero until the first command CMAC / encrypted block.
            memset(session_iv, 0, 16);
        }
    } else {
        munit_assert_size(*response_len, ==, 2);
        munit_assert_uint8(response[0], ==, 0x91);
        munit_assert_uint8(response[1], ==, expected_status);
    }
}

void authenticate_aes_key(
    DfcVirtualPiccSession* session,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t* session_iv,
    uint8_t expected_status) {
    authenticate_ev1_key(
        session,
        DFC_CMD_AUTHENTICATE_AES,
        key_no,
        key,
        key_len,
        response,
        response_capacity,
        response_len,
        session_key,
        session_key_len,
        session_iv,
        expected_status);
}

void authenticate_iso_key(
    DfcVirtualPiccSession* session,
    uint8_t key_no,
    const uint8_t* key,
    size_t key_len,
    uint8_t* response,
    size_t response_capacity,
    size_t* response_len,
    uint8_t* session_key,
    size_t* session_key_len,
    uint8_t* session_iv,
    uint8_t expected_status) {
    authenticate_ev1_key(
        session,
        DFC_CMD_AUTHENTICATE_ISO,
        key_no,
        key,
        key_len,
        response,
        response_capacity,
        response_len,
        session_key,
        session_key_len,
        session_iv,
        expected_status);
}
