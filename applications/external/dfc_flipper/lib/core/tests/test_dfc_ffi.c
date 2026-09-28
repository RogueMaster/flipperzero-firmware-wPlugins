// Smoke test for the shared library, driven only through ffi/dfc_ffi.h the
// way a foreign runtime drives it.

#include "dfc_ffi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if(!(cond)) {                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while(0)

static const char CREDENTIAL[] =
    "Filetype: DFC Credential\n"
    "Version: 4\n"
    "Card Generation: EV2\n"
    "Card Storage: 4096\n"
    "UID: 04 22 33 44 55 66 77\n"
    "UID Provenance: Real\n"
    "PICC Key Settings 1: 0F\n"
    "PICC Key Settings 2: 01\n"
    "PICC Authentication Mode: D40\n"
    "PICC Key Count: 1\n"
    "PICC Key 00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
    "PICC Key 00 Version: 00\n"
    "PICC File Count: 0\n"
    "Application Count: 1\n"
    "Application 00 AID: 00 00 01\n"
    "Application 00 Key Settings 1: 0F\n"
    "Application 00 Key Settings 2: 82\n"
    "Application 00 Authentication Mode: AES\n"
    "Application 00 Key Count: 2\n"
    "Application 00 Key 00: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF\n"
    "Application 00 Key 00 Version: 01\n"
    "Application 00 Key 01: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
    "Application 00 Key 01 Version: 00\n"
    "Application 00 File Count: 2\n"
    "Application 00 File 00 Number: 01\n"
    "Application 00 File 00 Type: Standard Data\n"
    "Application 00 File 00 Communication Settings: 01\n"
    "Application 00 File 00 Access Rights: 00 00\n"
    "Application 00 File 00 Size: 16\n"
    "Application 00 File 00 Data: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n"
    "Application 00 File 00 Data Complete: 1\n"
    "Application 00 File 01 Number: 02\n"
    "Application 00 File 01 Type: Value\n"
    "Application 00 File 01 Communication Settings: 00\n"
    "Application 00 File 01 Access Rights: 00 00\n"
    "Application 00 File 01 Value Lower Limit: 0\n"
    "Application 00 File 01 Value Upper Limit: 1000\n"
    "Application 00 File 01 Value: 42\n"
    "Application 00 File 01 Limited Credit: 00\n";

static const uint8_t APP_KEY[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

static size_t random_calls;

static void counting_random(void* context, uint8_t* buffer, size_t len) {
    size_t* calls = context;
    (*calls)++;
    for(size_t i = 0; i < len; i++) buffer[i] = (uint8_t)(0x40 + i);
}

static uint8_t* encode(const DfcFfiCredential* credential, size_t* len) {
    CHECK(dfc_ffi_credential_encode(credential, NULL, 0, len) == 0);
    uint8_t* out = malloc(*len);
    CHECK(dfc_ffi_credential_encode(credential, out, *len, len) == 0);
    return out;
}

// Rebuild a credential from nothing but what the walk returns.
static DfcFfiCredential* rebuild(const DfcFfiCredential* source) {
    DfcFfiCredential* built = NULL;
    CHECK(dfc_ffi_credential_new(&built) == 0);
    DfcFfiCard card;
    DfcFfiPiccSettings picc;
    CHECK(dfc_ffi_credential_get_card(source, &card) == 0);
    CHECK(dfc_ffi_credential_get_picc(source, &picc) == 0);
    CHECK(dfc_ffi_credential_set_card(built, &card) == 0);
    CHECK(dfc_ffi_credential_set_picc(built, &picc) == 0);
    for(uint32_t slot = 0; slot < picc.num_keys; slot++) {
        DfcFfiKey key;
        CHECK(dfc_ffi_credential_get_key(source, DFC_FFI_OWNER_PICC, 0, slot, &key) == 0);
        CHECK(dfc_ffi_credential_set_key(built, DFC_FFI_OWNER_PICC, 0, slot, &key) == 0);
    }
    uint32_t apps = dfc_ffi_credential_application_count(source);
    for(uint32_t i = 0; i < apps; i++) {
        DfcFfiApplication app;
        uint32_t index = 0;
        CHECK(dfc_ffi_credential_get_application(source, i, &app) == 0);
        CHECK(dfc_ffi_credential_add_application(built, &app, &index) == 0);
        CHECK(index == i);
        for(uint32_t slot = 0; slot < app.num_keys; slot++) {
            DfcFfiKey key;
            CHECK(dfc_ffi_credential_get_key(source, (int32_t)i, 0, slot, &key) == 0);
            CHECK(dfc_ffi_credential_set_key(built, (int32_t)i, 0, slot, &key) == 0);
        }
    }
    uint32_t files = dfc_ffi_credential_file_count(source);
    for(uint32_t i = 0; i < files; i++) {
        DfcFfiFile file;
        uint32_t index = 0;
        CHECK(dfc_ffi_credential_get_file(source, i, &file) == 0);
        CHECK(dfc_ffi_credential_add_file(built, &file, &index) == 0);
        size_t len = 0;
        CHECK(dfc_ffi_credential_get_file_data(source, i, NULL, 0, &len) == 0);
        if(len) {
            uint8_t* data = malloc(len);
            CHECK(dfc_ffi_credential_get_file_data(source, i, data, len, &len) == 0);
            CHECK(dfc_ffi_credential_set_file_data(built, index, data, len) == 0);
            free(data);
        }
    }
    DfcFfiError error;
    CHECK(dfc_ffi_credential_finish(built, &error) == 0);
    return built;
}

// Drive one reader exchange against the PICC until it completes.
static int32_t run(DfcFfiPicc* picc, DfcFfiReaderExchange* exchange) {
    uint8_t tx[300];
    uint8_t rx[600];
    size_t tx_len = 0;
    size_t rx_len = 0;
    const uint8_t* answer = NULL;
    int32_t st;
    while((st = dfc_ffi_reader_step(exchange, answer, rx_len, tx, sizeof(tx), &tx_len)) == 1) {
        CHECK(dfc_ffi_picc_exchange(picc, tx, tx_len, rx, sizeof(rx), &rx_len) == 0);
        answer = rx;
    }
    return st;
}

int main(void) {
    CHECK(dfc_ffi_abi_version() == DFC_FFI_ABI_VERSION);
    enum { RandomSampleLength = 16 };
    uint8_t random_bytes[RandomSampleLength];
    CHECK(dfc_ffi_random_fill(random_bytes, sizeof(random_bytes)) == DFC_FFI_OK);
    CHECK(dfc_ffi_random_fill(NULL, sizeof(random_bytes)) == DFC_FFI_INVALID_ARGUMENT);
    uint8_t same_bytes[RandomSampleLength];
    memcpy(same_bytes, random_bytes, sizeof(same_bytes));
    CHECK(dfc_ffi_fixed_time_equal(random_bytes, same_bytes, sizeof(same_bytes)));
    same_bytes[0] ^= 1;
    CHECK(!dfc_ffi_fixed_time_equal(random_bytes, same_bytes, sizeof(same_bytes)));
    dfc_ffi_secure_zero(random_bytes, sizeof(random_bytes));
    for(size_t index = 0; index < sizeof(random_bytes); index++) CHECK(random_bytes[index] == 0);
    DfcFfiCapabilities caps;
    dfc_ffi_capabilities(&caps);
    CHECK(caps.emulator && caps.reader && caps.text_codec);
    DfcFfiStructSizes sizes;
    dfc_ffi_struct_sizes(&sizes);
    CHECK(sizes.card == sizeof(DfcFfiCard));
    CHECK(sizes.file == sizeof(DfcFfiFile));
    CHECK(sizes.application == sizeof(DfcFfiApplication));

    // Text in, binary out, and back.
    DfcFfiCredential* credential = NULL;
    DfcFfiError error;
    int32_t st = dfc_ffi_credential_parse_text(CREDENTIAL, sizeof(CREDENTIAL) - 1, &credential, &error);
    if(st != 0) fprintf(stderr, "parse: %d line %u %s\n", st, error.line, error.message);
    CHECK(st == 0);
    size_t dfcb_len = 0;
    uint8_t* dfcb = encode(credential, &dfcb_len);
    DfcFfiCredential* decoded = NULL;
    CHECK(dfc_ffi_credential_load(dfcb, dfcb_len, &decoded, &error) == 0);
    size_t text_len = 0;
    CHECK(dfc_ffi_credential_write_text(decoded, NULL, 0, &text_len) == 0);
    CHECK(text_len > 0);

    // Malformed text reports the line.
    DfcFfiCredential* rejected = NULL;
    const char broken[] = "Filetype: DFC Credential\nVersion: 4\nCard Generation: EV9\n";
    CHECK(dfc_ffi_credential_parse_text(broken, sizeof(broken) - 1, &rejected, &error) != 0);
    CHECK(rejected == NULL);
    CHECK(error.line == 3);

    // The walk and the builder agree with the codec.
    DfcFfiCredential* built = rebuild(decoded);
    size_t rebuilt_len = 0;
    uint8_t* rebuilt = encode(built, &rebuilt_len);
    CHECK(rebuilt_len == dfcb_len && memcmp(rebuilt, dfcb, dfcb_len) == 0);

    // A virtual PICC answering a reader, both through the interface.
    DfcFfiPicc* picc = NULL;
    CHECK(dfc_ffi_picc_create(built, counting_random, &random_calls, &picc) == 0);
    DfcFfiActivation activation;
    CHECK(dfc_ffi_picc_activate(picc, &activation) == 0);
    CHECK(activation.uid_len == 7);

    DfcFfiReaderSession* session = NULL;
    DfcFfiReaderExchange* exchange = NULL;
    CHECK(dfc_ffi_reader_session_new(&session) == 0);
    CHECK(dfc_ffi_reader_exchange_new(&exchange) == 0);
    // The wire carries an AID least significant octet first; the model stores
    // it the way the text writes it.
    const uint8_t wire_aid[3] = {0x01, 0x00, 0x00};
    const uint8_t aid[3] = {0x00, 0x00, 0x01};
    CHECK(dfc_ffi_reader_exchange_begin(exchange, session, 0, 0x5A, wire_aid, 3, 0, -1) == 0);
    st = run(picc, exchange);
    if(st != 0) fprintf(stderr, "select: %d status %02X\n", st, dfc_ffi_reader_result_status(exchange));
    CHECK(st == 0);

    uint8_t random_a[16];
    for(size_t i = 0; i < sizeof(random_a); i++) random_a[i] = (uint8_t)i;
    CHECK(dfc_ffi_reader_authenticate_begin(exchange, session, 0, 0xAA, 0, APP_KEY, 16, random_a, 16) == 0);
    CHECK(run(picc, exchange) == 0);
    CHECK(random_calls > 0);
    DfcFfiReaderState state;
    CHECK(dfc_ffi_reader_session_state(session, &state) == 0);
    CHECK(state.auth_mode == 0xAA);

    const uint8_t read[] = {0x01, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00};
    CHECK(dfc_ffi_reader_exchange_begin(exchange, session, 0, 0xBD, read, sizeof(read), 1, -1) == 0);
    CHECK(run(picc, exchange) == 0);
    uint8_t data[32];
    size_t data_len = 0;
    CHECK(dfc_ffi_reader_result_data(exchange, data, sizeof(data), &data_len) == 0);
    CHECK(data_len == 16);
    for(size_t i = 0; i < data_len; i++) CHECK(data[i] == i);

    DfcFfiSnapshot snapshot;
    CHECK(dfc_ffi_picc_snapshot(picc, &snapshot) == 0);
    CHECK(snapshot.auth_mode == 0xAA);
    CHECK(snapshot.selected == 2);
    CHECK(memcmp(snapshot.selected_aid, aid, 3) == 0);

    DfcFfiCredential* exported = NULL;
    CHECK(dfc_ffi_picc_export(picc, &exported) == 0);
    CHECK(dfc_ffi_credential_validate(exported) == 0);

    dfc_ffi_credential_free(exported);
    dfc_ffi_reader_exchange_free(exchange);
    dfc_ffi_reader_session_free(session);
    dfc_ffi_picc_free(picc);
    free(rebuilt);
    free(dfcb);
    dfc_ffi_credential_free(built);
    dfc_ffi_credential_free(decoded);
    dfc_ffi_credential_free(credential);

    if(failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("ffi smoke test passed\n");
    return 0;
}
