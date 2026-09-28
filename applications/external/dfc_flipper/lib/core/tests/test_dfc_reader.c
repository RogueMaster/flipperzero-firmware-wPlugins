// The reader against the emulator, in process: every exchange the reader
// builds is answered by the virtual card, so the two halves of the library are
// held to one wire.

#include "munit/munit.h"

#include "dfc_credential.h"
#include "dfc_reader.h"
#include "dfc_virtual_picc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t AID_AES[3] = {0x01, 0x00, 0x00};
static const uint8_t AID_ISO[3] = {0x02, 0x00, 0x00};
static const uint8_t AID_3K[3] = {0x03, 0x00, 0x00};
static const uint8_t AID_D40[3] = {0x04, 0x00, 0x00};

static const uint8_t AES_KEY_0[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
static const uint8_t AES_KEY_1[16] = {
    0x10, 0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98, 0xA9, 0xBA, 0xCB, 0xDC, 0xED, 0xFE, 0x0F};
static const uint8_t TDES_KEY_0[16] = {
    0xC6, 0xEF, 0x84, 0x7C, 0xAE, 0x0A, 0x2A, 0x56, 0xC0, 0x19, 0x2A, 0xA1, 0xC4, 0xE8, 0xD9, 0xFB};
static const uint8_t TDES3_KEY_0[24] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28};
static const uint8_t DES_KEY_0[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

enum {
    FilePlain = 0x01,
    FileMac = 0x02,
    FileFull = 0x03,
    FileValueMac = 0x04,
    FileValueFull = 0x05,
    FileLong = 0x06,
};

#define LONG_FILE_SIZE 100
#define D40_LONG_READ_SIZE 300

typedef struct {
    DfcCredential credential;
    DfcVirtualPiccSession* picc;
    DfcReaderSession session;
    DfcReaderFraming framing;
} Link;

static void add_data_file(DfcCredential* c, size_t app, uint8_t number, uint8_t comm, size_t size) {
    DfcFile* file = dfc_credential_create_file(c, app, number);
    munit_assert_not_null(file);
    file->type = DFC_FILE_TYPE_STANDARD_DATA;
    file->comm_settings = comm;
    file->access_rights = 0x0000;
    munit_assert_true(dfc_file_resize(c, file, size));
    uint8_t* bytes = dfc_file_data(c, file);
    for(size_t i = 0; i < size; i++) bytes[i] = (uint8_t)(number * 0x10 + i);
}

static void add_value_file(DfcCredential* c, size_t app, uint8_t number, uint8_t comm) {
    DfcFile* file = dfc_credential_create_file(c, app, number);
    munit_assert_not_null(file);
    file->type = DFC_FILE_TYPE_VALUE;
    file->comm_settings = comm;
    file->access_rights = 0x0000;
    file->value_lower_limit = 0;
    file->value_upper_limit = 1000;
    file->value = 100;
}

static DfcApplication*
    add_app(DfcCredential* c, const uint8_t aid[3], uint8_t ks2, uint8_t auth, const uint8_t* key0) {
    DfcApplication* app = dfc_credential_create_application_desfire_order(c, aid, 0x0F, ks2);
    munit_assert_not_null(app);
    app->auth_command = auth;
    app->has_auth_commands = true;
    app->auth_commands = dfc_credential_default_auth_commands(ks2, c->card.generation) |
                         DFC_AUTH_COMMAND_ISO7816;
    memcpy(dfc_credential_key(c, app, 0), key0, app->key_len);
    size_t index = dfc_credential_application_index(c, app);
    add_data_file(c, index, FilePlain, DFC_COMM_PLAIN, 32);
    add_data_file(c, index, FileMac, DFC_COMM_MAC, 32);
    add_data_file(c, index, FileFull, DFC_COMM_ENCIPHERED, 32);
    add_value_file(c, index, FileValueMac, DFC_COMM_MAC);
    add_value_file(c, index, FileValueFull, DFC_COMM_ENCIPHERED);
    add_data_file(c, index, FileLong, DFC_COMM_MAC, LONG_FILE_SIZE);
    return app;
}

static void link_open(Link* link, DfcGeneration generation, DfcReaderFraming framing) {
    DfcCredential* c = &link->credential;
    dfc_credential_clear(c);
    const uint8_t uid[] = {0x04, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(c->uid, uid, sizeof(uid));
    c->uid_len = sizeof(uid);
    c->card.generation = generation;
    DfcApplication* aes = add_app(c, AID_AES, DFC_KEY_TYPE_AES | 2, DFC_CMD_AUTHENTICATE_AES, AES_KEY_0);
    memcpy(dfc_credential_key(c, aes, 1), AES_KEY_1, sizeof(AES_KEY_1));
    add_app(c, AID_ISO, DFC_KEY_TYPE_DES_2K3DES | 2, DFC_CMD_AUTHENTICATE_ISO, TDES_KEY_0);
    add_app(c, AID_3K, DFC_KEY_TYPE_3K3DES | 2, DFC_CMD_AUTHENTICATE_ISO, TDES3_KEY_0);
    add_app(c, AID_D40, DFC_KEY_TYPE_DES_2K3DES | 2, DFC_CMD_AUTHENTICATE_LEGACY, DES_KEY_0);

    link->picc = dfc_virtual_picc_session_alloc(c);
    munit_assert_not_null(link->picc);
    DfcVirtualPiccActivation activation;
    munit_assert_int(dfc_virtual_picc_scan_iso14443a(link->picc, &activation), ==, DfcVirtualPiccStatusOk);
    dfc_reader_session_init(&link->session);
    link->framing = framing;
}

static void link_close(Link* link) {
    dfc_virtual_picc_session_free(link->picc);
}

// Set DFC_READER_TRACE to print every frame the loopback carries.
static void trace(const char* direction, const uint8_t* bytes, size_t len) {
    if(!getenv("DFC_READER_TRACE")) return;
    fprintf(stderr, "%s ", direction);
    for(size_t i = 0; i < len; i++) fprintf(stderr, "%02X", bytes[i]);
    fprintf(stderr, "\n");
}

static DfcReaderStatus run(Link* link, DfcReaderExchange* ex) {
    uint8_t tx[DFC_COMMAND_MAX_APDU];
    size_t tx_len = 0;
    uint8_t rx[512];
    size_t rx_len = 0;
    const uint8_t* answer = NULL;
    DfcReaderStatus st;
    while((st = dfc_reader_step(ex, answer, rx_len, tx, sizeof(tx), &tx_len)) == DfcReaderPending) {
        trace(">>", tx, tx_len);
        munit_assert_int(
            dfc_virtual_picc_iso_dep_exchange(link->picc, tx, tx_len, rx, sizeof(rx), &rx_len),
            ==,
            DfcVirtualPiccStatusOk);
        trace("<<", rx, rx_len);
        answer = rx;
    }
    return st;
}

static DfcReaderStatus send(Link* link, const DfcCommand* command, uint8_t comm, DfcReaderExchange* ex) {
    DfcReaderOptions options = {0};
    options.comm_mode = comm;
    munit_assert_int(
        dfc_reader_exchange_begin(ex, &link->session, link->framing, command, &options), ==, DfcReaderOk);
    return run(link, ex);
}

static void select_app(Link* link, const uint8_t aid[3]) {
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_select_application(&command, aid, NULL), ==, DfcCommandOk);
    munit_assert_int(send(link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_false(dfc_reader_session_is_authenticated(&link->session));
}

static DfcReaderStatus authenticate(Link* link, uint8_t cipher, uint8_t key_no, const uint8_t* key, size_t key_len) {
    uint8_t random_a[16];
    for(size_t i = 0; i < sizeof(random_a); i++) random_a[i] = (uint8_t)(0xA0 + i);
    DfcReaderExchange ex;
    munit_assert_int(
        dfc_reader_authenticate_begin(
            &ex,
            &link->session,
            link->framing,
            cipher,
            key_no,
            key,
            key_len,
            random_a,
            dfc_reader_challenge_length(cipher, key_len)),
        ==,
        DfcReaderOk);
    return run(link, &ex);
}

static void assert_file_bytes(const uint8_t* data, size_t len, uint8_t number, size_t offset) {
    for(size_t i = 0; i < len; i++) munit_assert_uint8(data[i], ==, (uint8_t)(number * 0x10 + offset + i));
}

static MunitResult test_native_command_chaining(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_AES);
    munit_assert_int(
        authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, sizeof(AES_KEY_0)),
        ==,
        DfcReaderOk);

    uint8_t bytes[LONG_FILE_SIZE];
    for(size_t i = 0; i < sizeof(bytes); i++) bytes[i] = (uint8_t)(0xA0 + i);
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(
        dfc_command_write_data(&command, FileLong, 0, bytes, sizeof(bytes)), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_MAC, &ex), ==, DfcReaderOk);

    DfcFile* file = dfc_credential_find_file_in_app(&link.credential, 0, FileLong);
    munit_assert_not_null(file);
    munit_assert_memory_equal(sizeof(bytes), dfc_file_data(&link.credential, file), bytes);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_authenticated_directory_framing(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);

    for(size_t i = link.credential.num_apps; i < DFC_MAX_APPS; i++) {
        uint8_t aid[3] = {(uint8_t)(0x40 + i), 0x55, 0x66};
        DfcApplication* app = dfc_credential_create_application_desfire_order(
            &link.credential, aid, 0x0F, DFC_KEY_TYPE_AES | 1);
        munit_assert_not_null(app);
    }
    for(size_t i = 0; i < link.credential.num_apps; i++) {
        DfcApplication* app = &link.credential.apps[i];
        app->has_iso_file_id = true;
        app->iso_file_id = (uint16_t)(0xE100 + i);
        app->iso_aid_len = 16;
        for(size_t n = 0; n < app->iso_aid_len; n++) app->iso_aid[n] = (uint8_t)(i + n);
    }

    munit_assert_true(dfc_credential_keys_resize(&link.credential, NULL, 1, 16));
    link.credential.picc_key_settings_2 = DFC_KEY_TYPE_AES | 1;
    link.credential.picc_auth_command = DFC_CMD_AUTHENTICATE_AES;
    memcpy(dfc_credential_key(&link.credential, NULL, 0), AES_KEY_0, sizeof(AES_KEY_0));
    munit_assert_int(
        authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, sizeof(AES_KEY_0)),
        ==,
        DfcReaderOk);

    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_get_application_ids(&command), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    size_t length = 0;
    dfc_reader_result_data(&ex, &length);
    munit_assert_size(length, ==, DFC_MAX_APPS * 3);

    munit_assert_int(dfc_command_get_df_names(&command), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    const uint8_t* names = dfc_reader_result_data(&ex, &length);
    munit_assert_size(length, ==, DFC_MAX_APPS * 21);
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        munit_assert_uint8(names[i * 21 + 5], ==, (uint8_t)i);
    }

    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_long_plain_read(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    add_data_file(&link.credential, 0, 7, DFC_COMM_PLAIN, 300);
    select_app(&link, AID_AES);
    munit_assert_int(
        authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, sizeof(AES_KEY_0)),
        ==,
        DfcReaderOk);

    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_read_data(&command, 7, 0, 300), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    size_t length = 0;
    const uint8_t* clear = dfc_reader_result_data(&ex, &length);
    munit_assert_size(length, ==, 300);
    assert_file_bytes(clear, length, 7, 0);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_streamed_secure_write(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    add_data_file(&link.credential, 0, 7, DFC_COMM_ENCIPHERED, 300);
    select_app(&link, AID_AES);
    munit_assert_int(
        authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, sizeof(AES_KEY_0)),
        ==,
        DfcReaderOk);

    uint8_t written[300];
    for(size_t i = 0; i < sizeof(written); i++) written[i] = (uint8_t)(i ^ 0xA5);
    DfcReaderExchange ex;
    munit_assert_int(
        dfc_reader_write_data_begin(
            &ex,
            &link.session,
            link.framing,
            7,
            0,
            written,
            sizeof(written),
            DFC_COMM_ENCIPHERED),
        ==,
        DfcReaderOk);
    munit_assert_int(run(&link, &ex), ==, DfcReaderOk);
    DfcFile* file = dfc_credential_find_file_in_app(&link.credential, 0, 7);
    munit_assert_not_null(file);
    munit_assert_memory_equal(sizeof(written), dfc_file_data(&link.credential, file), written);

    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_streamed_secure_record(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    DfcFile* file = dfc_credential_create_file(&link.credential, 0, 7);
    munit_assert_not_null(file);
    file->type = DFC_FILE_TYPE_LINEAR_RECORD;
    file->comm_settings = DFC_COMM_ENCIPHERED;
    file->access_rights = 0x0000;
    file->record_size = 300;
    file->max_records = 2;
    munit_assert_true(dfc_file_resize(&link.credential, file, 600));
    select_app(&link, AID_AES);
    munit_assert_int(
        authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, sizeof(AES_KEY_0)),
        ==,
        DfcReaderOk);

    uint8_t written[300];
    for(size_t i = 0; i < sizeof(written); i++) written[i] = (uint8_t)(i ^ 0x5A);
    DfcReaderExchange ex;
    munit_assert_int(
        dfc_reader_write_record_begin(
            &ex, &link.session, link.framing, 7, 0, written, sizeof(written), DFC_COMM_ENCIPHERED),
        ==,
        DfcReaderOk);
    munit_assert_int(run(&link, &ex), ==, DfcReaderOk);
    munit_assert_uint32(file->record_count, ==, 1);
    munit_assert_memory_equal(sizeof(written), dfc_file_data(&link.credential, file), written);

    DfcCommand command;
    munit_assert_int(dfc_command_commit_transaction(&command), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_int(dfc_command_read_records(&command, 7, 0, 1, false), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_ENCIPHERED, &ex), ==, DfcReaderOk);
    size_t read_len = 0;
    const uint8_t* read_data = dfc_reader_result_data(&ex, &read_len);
    munit_assert_size(read_len, ==, sizeof(written));
    munit_assert_memory_equal(read_len, read_data, written);
    link_close(&link);
    return MUNIT_OK;
}

static void read_file(Link* link, uint8_t number, uint8_t comm, uint32_t offset, uint32_t length) {
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_read_data(&command, number, offset, length), ==, DfcCommandOk);
    DfcReaderStatus st = send(link, &command, comm, &ex);
    if(st != DfcReaderOk) {
        munit_logf(MUNIT_LOG_WARNING, "read %02X: %s, status %02X", number, dfc_reader_status_name(st), dfc_reader_result_status(&ex));
    }
    munit_assert_int(st, ==, DfcReaderOk);
    size_t len = 0;
    const uint8_t* data = dfc_reader_result_data(&ex, &len);
    munit_assert_size(len, ==, length);
    assert_file_bytes(data, len, number, offset);
}

static void write_then_read(Link* link, uint8_t number, uint8_t comm) {
    const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_write_data(&command, number, 4, payload, sizeof(payload)), ==, DfcCommandOk);
    DfcReaderStatus st = send(link, &command, comm, &ex);
    if(st != DfcReaderOk) {
        munit_logf(MUNIT_LOG_WARNING, "write %02X: %s, status %02X", number, dfc_reader_status_name(st), dfc_reader_result_status(&ex));
    }
    munit_assert_int(st, ==, DfcReaderOk);
    munit_assert_true(dfc_reader_session_is_authenticated(&link->session));

    munit_assert_int(dfc_command_read_data(&command, number, 4, sizeof(payload)), ==, DfcCommandOk);
    munit_assert_int(send(link, &command, comm, &ex), ==, DfcReaderOk);
    size_t len = 0;
    const uint8_t* data = dfc_reader_result_data(&ex, &len);
    munit_assert_size(len, ==, sizeof(payload));
    munit_assert_memory_equal(len, data, payload);
}

static int32_t get_value(Link* link, uint8_t number, uint8_t comm) {
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_get_value(&command, number), ==, DfcCommandOk);
    DfcReaderStatus st = send(link, &command, comm, &ex);
    if(st != DfcReaderOk) {
        munit_logf(MUNIT_LOG_WARNING, "get value %02X: %s, status %02X", number, dfc_reader_status_name(st), dfc_reader_result_status(&ex));
    }
    munit_assert_int(st, ==, DfcReaderOk);
    size_t len = 0;
    const uint8_t* data = dfc_reader_result_data(&ex, &len);
    munit_assert_size(len, ==, 4);
    return (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                     ((uint32_t)data[3] << 24));
}

static void credit_and_commit(Link* link, uint8_t number, uint8_t comm) {
    int32_t before = get_value(link, number, comm);
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_credit(&command, number, 25), ==, DfcCommandOk);
    DfcReaderStatus st = send(link, &command, comm, &ex);
    if(st != DfcReaderOk) {
        munit_logf(MUNIT_LOG_WARNING, "credit %02X: %s, status %02X", number, dfc_reader_status_name(st), dfc_reader_result_status(&ex));
    }
    munit_assert_int(st, ==, DfcReaderOk);
    munit_assert_int(dfc_command_commit_transaction(&command), ==, DfcCommandOk);
    munit_assert_int(send(link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_int(get_value(link, number, comm), ==, before + 25);
}

static void exercise_files(Link* link) {
    read_file(link, FilePlain, DFC_COMM_PLAIN, 0, 32);
    read_file(link, FileMac, DFC_COMM_MAC, 2, 20);
    read_file(link, FileFull, DFC_COMM_ENCIPHERED, 0, 32);
    write_then_read(link, FilePlain, DFC_COMM_PLAIN);
    write_then_read(link, FileMac, DFC_COMM_MAC);
    write_then_read(link, FileFull, DFC_COMM_ENCIPHERED);
    credit_and_commit(link, FileValueMac, DFC_COMM_MAC);
    credit_and_commit(link, FileValueFull, DFC_COMM_ENCIPHERED);
    munit_assert_true(dfc_reader_session_is_authenticated(&link->session));
}

static MunitResult test_aes_session(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_AES);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, 16), ==, DfcReaderOk);
    munit_assert_uint8(link.session.auth_mode, ==, DFC_CMD_AUTHENTICATE_AES);
    exercise_files(&link);
    // A chained answer carries one CMAC over the whole of it.
    read_file(&link, FileLong, DFC_COMM_MAC, 0, LONG_FILE_SIZE);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_iso_session(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_ISO);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_ISO, 0, TDES_KEY_0, 16), ==, DfcReaderOk);
    exercise_files(&link);
    read_file(&link, FileLong, DFC_COMM_MAC, 0, LONG_FILE_SIZE);
    link_close(&link);
    return MUNIT_OK;
}

#if DFC_ENABLE_ISO7816_AUTH
static MunitResult test_iso7816_mutual_authentication(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static const struct {
        const uint8_t* aid;
        const uint8_t* key;
        size_t key_len;
        uint8_t algorithm;
        size_t challenge_len;
    } cases[] = {
        {AID_ISO, TDES_KEY_0, sizeof(TDES_KEY_0), DFC_ISO7816_AUTH_ALGORITHM_2TDEA,
         DFC_ISO7816_AUTH_CHALLENGE_2TDEA},
        {AID_3K, TDES3_KEY_0, sizeof(TDES3_KEY_0), DFC_ISO7816_AUTH_ALGORITHM_3TDEA,
         DFC_ISO7816_AUTH_CHALLENGE_LONG},
        {AID_AES, AES_KEY_0, sizeof(AES_KEY_0), DFC_ISO7816_AUTH_ALGORITHM_AES,
         DFC_ISO7816_AUTH_CHALLENGE_LONG},
    };
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        static Link link;
        link_open(&link, DfcGenerationEv3, DfcReaderFramingIso7816);
        select_app(&link, cases[i].aid);
        uint8_t first[DFC_ISO7816_AUTH_CHALLENGE_LONG];
        uint8_t second[DFC_ISO7816_AUTH_CHALLENGE_LONG];
        for(size_t j = 0; j < cases[i].challenge_len; j++) {
            first[j] = (uint8_t)(0xA0 + j);
            second[j] = (uint8_t)(0xB0 + j);
        }
        DfcReaderExchange ex;
        munit_assert_int(dfc_reader_authenticate_iso7816_begin(
            &ex, &link.session, DFC_ISO7816_AUTH_APP_REFERENCE,
            cases[i].key, cases[i].key_len, cases[i].algorithm,
            first, second, cases[i].challenge_len), ==, DfcReaderOk);
        munit_assert_int(run(&link, &ex), ==, DfcReaderOk);
        munit_assert_true(dfc_reader_session_is_authenticated(&link.session));
        DfcCommand command;
        munit_assert_int(dfc_command_get_key_settings(&command), ==, DfcCommandOk);
        munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
        link_close(&link);
    }
    return MUNIT_OK;
}
#endif

static MunitResult test_3k3des_session(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_3K);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_ISO, 0, TDES3_KEY_0, 24), ==, DfcReaderOk);
    munit_assert_size(link.session.sm.session_key_len, ==, 24);
    exercise_files(&link);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_d40_session(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_D40);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_LEGACY, 0, DES_KEY_0, 16), ==, DfcReaderOk);
    munit_assert_uint8(link.session.auth_mode, ==, DFC_CMD_AUTHENTICATE_LEGACY);
    exercise_files(&link);
    DfcApplication* app =
        dfc_credential_find_application_desfire_order(&link.credential, AID_D40);
    munit_assert_not_null(app);
    size_t app_index = dfc_credential_application_index(&link.credential, app);
    DfcFile* long_file =
        dfc_credential_find_file_in_app(&link.credential, app_index, FileLong);
    munit_assert_not_null(long_file);
    munit_assert_true(dfc_file_resize(&link.credential, long_file, D40_LONG_READ_SIZE));
    uint8_t* long_bytes = dfc_file_data(&link.credential, long_file);
    for(size_t index = 0; index < D40_LONG_READ_SIZE; index++) {
        long_bytes[index] = (uint8_t)(FileLong * 0x10 + index);
    }
    read_file(&link, FileLong, DFC_COMM_MAC, 0, D40_LONG_READ_SIZE);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_native_framing(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingNative);
    select_app(&link, AID_AES);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, 16), ==, DfcReaderOk);
    read_file(&link, FileMac, DFC_COMM_MAC, 0, 32);
    write_then_read(&link, FileFull, DFC_COMM_ENCIPHERED);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_get_version_under_session(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);

    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_get_version(&command), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    size_t plain_len = 0;
    const uint8_t* plain = dfc_reader_result_data(&ex, &plain_len);
    uint8_t expected[64];
    munit_assert_size(plain_len, <=, sizeof(expected));
    memcpy(expected, plain, plain_len);

    select_app(&link, AID_AES);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, 16), ==, DfcReaderOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    size_t secured_len = 0;
    const uint8_t* secured = dfc_reader_result_data(&ex, &secured_len);
    munit_assert_size(secured_len, ==, plain_len);
    munit_assert_memory_equal(plain_len, secured, expected);
    // The session is still in step after the chain.
    read_file(&link, FileMac, DFC_COMM_MAC, 0, 8);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_wrong_key_clears_session(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_AES);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, 16), ==, DfcReaderOk);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_1, 16), ==, DfcReaderCardError);
    munit_assert_false(dfc_reader_session_is_authenticated(&link.session));
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_change_key(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_AES);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, AES_KEY_0, 16), ==, DfcReaderOk);

    // Another key: the session carries on, chained from the cryptogram.
    const uint8_t new_key_1[16] = {0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A,
                                   0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5, 0xA5};
    uint8_t cryptogram[48];
    size_t cryptogram_len = 0;
    munit_assert_int(
        dfc_reader_change_key_cryptogram(
            &link.session, 1, new_key_1, 16, AES_KEY_1, true, 0x07, cryptogram, sizeof(cryptogram), &cryptogram_len),
        ==,
        DfcReaderOk);
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_change_key(&command, 1, cryptogram, cryptogram_len), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_true(dfc_reader_session_is_authenticated(&link.session));
    read_file(&link, FileMac, DFC_COMM_MAC, 0, 8);

    // The authenticated key: the card ends the session.
    const uint8_t new_key_0[16] = {0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
                                   0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00};
    munit_assert_int(
        dfc_reader_change_key_cryptogram(
            &link.session, 0, new_key_0, 16, NULL, true, 0x01, cryptogram, sizeof(cryptogram), &cryptogram_len),
        ==,
        DfcReaderOk);
    munit_assert_int(dfc_command_change_key(&command, 0, cryptogram, cryptogram_len), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_false(dfc_reader_session_is_authenticated(&link.session));

    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 1, new_key_1, 16), ==, DfcReaderOk);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, new_key_0, 16), ==, DfcReaderOk);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_d40_change_key(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv1, DfcReaderFramingIso7816);
    select_app(&link, AID_D40);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_LEGACY, 0, DES_KEY_0, 16), ==, DfcReaderOk);
    const uint8_t new_key[16] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xF0, 0x0F};
    uint8_t cryptogram[48];
    size_t cryptogram_len = 0;
    munit_assert_int(
        dfc_reader_change_key_cryptogram(
            &link.session, 1, new_key, 16, (const uint8_t[16]){0}, false, 0x00, cryptogram, sizeof(cryptogram), &cryptogram_len),
        ==,
        DfcReaderOk);
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_change_key(&command, 1, cryptogram, cryptogram_len), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_LEGACY, 1, new_key, 16), ==, DfcReaderOk);
    link_close(&link);
    return MUNIT_OK;
}

#if DFC_ENABLE_EV2_SECURE_MESSAGING
static DfcReaderStatus authenticate_ev2(Link* link, bool first, uint8_t key_no, const uint8_t key[16]) {
    uint8_t random_a[16];
    for(size_t i = 0; i < sizeof(random_a); i++) random_a[i] = (uint8_t)(0x30 + i);
    const uint8_t capabilities[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    DfcReaderExchange ex;
    munit_assert_int(
        dfc_reader_authenticate_ev2_begin(
            &ex,
            &link->session,
            link->framing,
            first,
            key_no,
            key,
            random_a,
            first ? capabilities : NULL,
            first ? sizeof(capabilities) : 0),
        ==,
        DfcReaderOk);
    return run(link, &ex);
}

static MunitResult test_ev2_session(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv2, DfcReaderFramingIso7816);
    select_app(&link, AID_AES);
    munit_assert_int(authenticate_ev2(&link, true, 0, AES_KEY_0), ==, DfcReaderOk);
    munit_assert_uint8(link.session.auth_mode, ==, DFC_READER_AUTH_EV2);
    munit_assert_uint16(link.session.ev2_command_counter, ==, 0);

    read_file(&link, FilePlain, DFC_COMM_PLAIN, 0, 32);
    munit_assert_uint16(link.session.ev2_command_counter, ==, 0);
    read_file(&link, FileMac, DFC_COMM_MAC, 0, 32);
    munit_assert_uint16(link.session.ev2_command_counter, ==, 1);
    read_file(&link, FileFull, DFC_COMM_ENCIPHERED, 0, 32);
    write_then_read(&link, FileMac, DFC_COMM_MAC);
    write_then_read(&link, FileFull, DFC_COMM_ENCIPHERED);
    credit_and_commit(&link, FileValueMac, DFC_COMM_MAC);
    credit_and_commit(&link, FileValueFull, DFC_COMM_ENCIPHERED);

    uint8_t transaction[4];
    memcpy(transaction, link.session.ev2_transaction_identifier, sizeof(transaction));
    // Key 1 opens a session but the files name key 0, so the card refuses the
    // read, and MACs the refusal.
    munit_assert_int(authenticate_ev2(&link, false, 1, AES_KEY_1), ==, DfcReaderOk);
    munit_assert_memory_equal(sizeof(transaction), link.session.ev2_transaction_identifier, transaction);
    munit_assert_uint16(link.session.ev2_command_counter, ==, 0);
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_read_data(&command, FileMac, 0, 16), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_MAC, &ex), ==, DfcReaderCardError);
    munit_assert_uint8(dfc_reader_result_status(&ex), ==, DFC_STATUS_AUTHENTICATION_ERR);
    munit_assert_uint16(link.session.ev2_command_counter, ==, 1);

    munit_assert_int(authenticate_ev2(&link, false, 0, AES_KEY_0), ==, DfcReaderOk);
    munit_assert_memory_equal(sizeof(transaction), link.session.ev2_transaction_identifier, transaction);
    read_file(&link, FileMac, DFC_COMM_MAC, 12, 16);
    link_close(&link);
    return MUNIT_OK;
}

static MunitResult test_ev2_change_key(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv2, DfcReaderFramingIso7816);
    DfcApplication* app = dfc_credential_get_application(&link.credential, 0);
    app->key_set_types[0] = DFC_KEY_SET_TYPE_AES;
    select_app(&link, AID_AES);
    munit_assert_int(authenticate_ev2(&link, true, 0, AES_KEY_0), ==, DfcReaderOk);

    const uint8_t new_key_1[16] = {0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
                                   0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x70};
    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(
        dfc_reader_change_key_ev2_command(&link.session, 0, 1, new_key_1, AES_KEY_1, 0x09, &command),
        ==,
        DfcReaderOk);
    DfcReaderStatus st = send(&link, &command, DFC_COMM_PLAIN, &ex);
    if(st != DfcReaderOk) munit_logf(MUNIT_LOG_WARNING, "status %02X", dfc_reader_result_status(&ex));
    munit_assert_int(st, ==, DfcReaderOk);
    munit_assert_true(dfc_reader_session_is_authenticated(&link.session));
    read_file(&link, FileMac, DFC_COMM_MAC, 0, 8);
    munit_assert_int(authenticate_ev2(&link, false, 1, new_key_1), ==, DfcReaderOk);

    // Key 1 may not change keys here; the application's ChangeKey key is key 0.
    munit_assert_int(
        dfc_reader_change_key_ev2_command(&link.session, 0, 1, AES_KEY_1, NULL, 0x0A, &command),
        ==,
        DfcReaderOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderCardError);
    munit_assert_uint8(dfc_reader_result_status(&ex), ==, DFC_STATUS_AUTHENTICATION_ERR);

    // The authenticated key itself: the card ends the session.
    munit_assert_int(authenticate_ev2(&link, true, 0, AES_KEY_0), ==, DfcReaderOk);
    munit_assert_int(
        dfc_reader_change_key_ev2_command(&link.session, 0, 0, AES_KEY_1, NULL, 0x0A, &command),
        ==,
        DfcReaderOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_false(dfc_reader_session_is_authenticated(&link.session));
    munit_assert_int(authenticate_ev2(&link, true, 0, AES_KEY_1), ==, DfcReaderOk);
    link_close(&link);
    return MUNIT_OK;
}
#endif

#if DFC_ENABLE_PROXIMITY_CHECK
static MunitResult test_proximity_check(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv3, DfcReaderFramingIso7816);
    const uint8_t key[16] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
                             0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19};
    link.credential.picc_has_proximity_key = true;
    memcpy(link.credential.picc_proximity_key, key, sizeof(key));
    link.credential.picc_proximity_option = 0x00;
    link.credential.picc_proximity_published_response_time = 0x0102;

    DfcCommand command;
    DfcReaderExchange ex;
    munit_assert_int(dfc_command_prepare_proximity_check(&command), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    munit_assert_uint8(dfc_reader_result_status(&ex), ==, DFC_STATUS_SPECIAL_SUCCESS);
    size_t published_len = 0;
    const uint8_t* published_data = dfc_reader_result_data(&ex, &published_len);
    uint8_t published[4];
    munit_assert_size(published_len, ==, 3);
    memcpy(published, published_data, published_len);

    uint8_t transcript[DFC_PROXIMITY_TRANSCRIPT_MAX];
    size_t transcript_len = 0;
    for(size_t round = 0; round < 2; round++) {
        uint8_t challenge[1 + 4];
        challenge[0] = 4;
        for(size_t i = 0; i < 4; i++) challenge[1 + i] = (uint8_t)(0xC0 + round * 4 + i);
        munit_assert_int(dfc_command_proximity_check(&command, challenge, sizeof(challenge)), ==, DfcCommandOk);
        munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
        size_t answer_len = 0;
        const uint8_t* answer = dfc_reader_result_data(&ex, &answer_len);
        munit_assert_size(answer_len, ==, 4);
        memcpy(transcript + transcript_len, answer, 4);
        memcpy(transcript + transcript_len + 4, challenge + 1, 4);
        transcript_len += 8;
    }

    uint8_t mac[8];
    munit_assert_true(dfc_reader_proximity_check_mac(key, false, published, published_len, transcript, transcript_len, mac));
    munit_assert_int(dfc_command_verify_proximity_check(&command, mac, sizeof(mac)), ==, DfcCommandOk);
    munit_assert_int(send(&link, &command, DFC_COMM_PLAIN, &ex), ==, DfcReaderOk);
    size_t card_mac_len = 0;
    const uint8_t* card_mac = dfc_reader_result_data(&ex, &card_mac_len);
    munit_assert_true(dfc_reader_proximity_check_mac(key, true, published, published_len, transcript, transcript_len, mac));
    munit_assert_size(card_mac_len, ==, sizeof(mac));
    munit_assert_memory_equal(sizeof(mac), card_mac, mac);
    link_close(&link);
    return MUNIT_OK;
}
#endif

#if DFC_ENABLE_VIRTUAL_CARD
static MunitResult test_virtual_card_select(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv2, DfcReaderFramingIso7816);
    DfcCredential* c = &link.credential;
    const uint8_t iid[] = {0xF0, 0x01, 0x02, 0x03, 0x04};
    const uint8_t enc_key[16] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
                                 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x20};
    const uint8_t mac_key[16] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                                 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x30};
    c->virtual_card_configured = true;
    c->virtual_card_authentication_mandatory = true;
    memcpy(c->virtual_card_installation_id, iid, sizeof(iid));
    c->virtual_card_installation_id_len = sizeof(iid);
    memcpy(c->virtual_card_select_encryption_key, enc_key, sizeof(enc_key));
    memcpy(c->virtual_card_select_mac_key, mac_key, sizeof(mac_key));
    c->virtual_card_information = 0x5A;
    c->virtual_card_uid_len = 7;
    memcpy(c->virtual_card_uid, c->uid, 7);

    uint8_t apdu[32];
    size_t apdu_len = 0;
    munit_assert_int(dfc_reader_virtual_card_select_apdu(iid, sizeof(iid), apdu, sizeof(apdu), &apdu_len), ==, DfcReaderOk);
    uint8_t rx[64];
    size_t rx_len = 0;
    munit_assert_int(dfc_virtual_picc_iso_dep_exchange(link.picc, apdu, apdu_len, rx, sizeof(rx), &rx_len), ==, DfcVirtualPiccStatusOk);
    uint8_t challenge[16];
    uint8_t clear[16];
    munit_assert_int(dfc_reader_virtual_card_open(enc_key, rx, rx_len, challenge, clear), ==, DfcReaderOk);
    munit_assert_uint8(clear[0], ==, 0x5A);
    munit_assert_memory_equal(7, clear + 3, c->uid);

    munit_assert_int(dfc_reader_virtual_card_authenticate_apdu(mac_key, challenge, clear, apdu, sizeof(apdu), &apdu_len), ==, DfcReaderOk);
    munit_assert_int(dfc_virtual_picc_iso_dep_exchange(link.picc, apdu, apdu_len, rx, sizeof(rx), &rx_len), ==, DfcVirtualPiccStatusOk);
    munit_assert_size(rx_len, ==, 2);
    munit_assert_uint8(rx[0], ==, 0x90);
    munit_assert_true(link.picc->emulator->virtual_card_selected);
    link_close(&link);
    return MUNIT_OK;
}
#endif

#if DFC_ENABLE_DELEGATED_APPLICATIONS
static MunitResult test_delegated_application(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static Link link;
    link_open(&link, DfcGenerationEv2, DfcReaderFramingIso7816);
    DfcCredential* c = &link.credential;
    const uint8_t dam_auth[16] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
                                  0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x40};
    const uint8_t dam_mac[16] = {0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
                                 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x50};
    const uint8_t dam_enc[16] = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
                                 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x60};
    // EV2 authentication at the PICC level needs an AES master key.
    c->picc_key_settings_2 = DFC_KEY_TYPE_AES | 1;
    c->picc_auth_command = DFC_CMD_AUTHENTICATE_AES;
    c->picc_has_auth_commands = true;
    c->picc_auth_commands = DFC_AUTH_COMMAND_AES | DFC_AUTH_COMMAND_EV2_FIRST;
    munit_assert_true(dfc_credential_keys_resize(c, NULL, 1, 16));
    c->picc_has_dam_keys = true;
    memcpy(c->picc_dam_auth_key, dam_auth, 16);
    memcpy(c->picc_dam_mac_key, dam_mac, 16);
    memcpy(c->picc_dam_encryption_key, dam_enc, 16);

    const uint8_t picc[3] = {0x00, 0x00, 0x00};
    select_app(&link, picc);
    munit_assert_int(authenticate_ev2(&link, true, DFC_DAM_AUTH_KEY_NUMBER, dam_auth), ==, DfcReaderOk);
    DfcCommandCreateDelegatedApplication app = {{0x0A, 0x0B, 0x0C}, 0x0001, 0x00, 0x0040, 0x0F, DFC_KEY_TYPE_AES | 1};
    const uint8_t prefix[7] = {1, 2, 3, 4, 5, 6, 7};
    const uint8_t initial[16] = {0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB, 0xAB,
                                 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD, 0xCD};
    DfcReaderExchange ex;
    munit_assert_int(
        dfc_reader_create_delegated_application_begin(
            &ex, &link.session, link.framing, &app, dam_enc, dam_mac, prefix, initial, sizeof(initial), 0x03),
        ==,
        DfcReaderOk);
    DfcReaderStatus st = run(&link, &ex);
    if(st != DfcReaderOk) munit_logf(MUNIT_LOG_WARNING, "status %02X", dfc_reader_result_status(&ex));
    munit_assert_int(st, ==, DfcReaderOk);
    munit_assert_uint16(link.session.ev2_command_counter, ==, 1);

    const uint8_t aid[3] = {0x0A, 0x0B, 0x0C};
    select_app(&link, aid);
    munit_assert_int(authenticate(&link, DFC_CMD_AUTHENTICATE_AES, 0, initial, 16), ==, DfcReaderOk);
    link_close(&link);
    return MUNIT_OK;
}
#endif

static MunitTest tests[] = {
    {"/aes-session", test_aes_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/iso-session", test_iso_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
#if DFC_ENABLE_ISO7816_AUTH
    {"/iso7816-mutual-authentication", test_iso7816_mutual_authentication, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
#endif
    {"/3k3des-session", test_3k3des_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/d40-session", test_d40_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/native-framing", test_native_framing, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/native-command-chaining",
     test_native_command_chaining,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/authenticated-directory-framing",
     test_authenticated_directory_framing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/long-plain-read", test_long_plain_read, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/streamed-secure-write",
     test_streamed_secure_write,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/streamed-secure-record",
     test_streamed_secure_record,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/get-version-under-session", test_get_version_under_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/wrong-key-clears-session", test_wrong_key_clears_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/change-key", test_change_key, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/d40-change-key", test_d40_change_key, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    {"/ev2-session", test_ev2_session, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/ev2-change-key", test_ev2_change_key, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    {"/proximity-check", test_proximity_check, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
#endif
#if DFC_ENABLE_VIRTUAL_CARD
    {"/virtual-card-select", test_virtual_card_select, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
#endif
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    {"/delegated-application", test_delegated_application, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
#endif
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"/dfc_reader", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
