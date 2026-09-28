// Self-contained text codec tests.

#include "munit/munit.h"

#include "dfc_credential.h"
#include "dfc_der.h"
#include "dfc_text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void log_detail(const DfcTextError* detail) {
    munit_logf(MUNIT_LOG_INFO, "line %u: %s", (unsigned)detail->line, detail->message);
}

static void assert_written_v6(const char* output) {
    munit_assert_not_null(strstr(output, "Version: 6\n"));
    munit_assert_not_null(strstr(output, "PICC Authentication Commands: D40, ISO\n"));
    munit_assert_not_null(strstr(output, "Application 00 Authentication Commands: AES"));
    static DfcCredential decoded;
    static char again[DFC_TEXT_MAX_SIZE];
    DfcTextError detail = {0};
    munit_assert_int(dfc_text_parse(&decoded, output, strlen(output), &detail), ==, DfcTextOk);
    size_t again_len = 0;
    munit_assert_int(dfc_text_write(&decoded, again, sizeof(again), &again_len), ==, DfcTextOk);
    munit_assert_size(again_len, ==, strlen(output));
    munit_assert_memory_equal(again_len, again, output);
}


// The smaller worked example, which exercises the
// canonical omissions: no ATS, no SAK, no random ID, no PICC files.
static const char MINIMAL[] =
    "Filetype: DFC Credential\n"
    "Version: 4\n"
    "Card Generation: EV1\n"
    "Card Storage: 2048\n"
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
    "Application 00 Key Settings 2: 81\n"
    "Application 00 Authentication Mode: AES\n"
    "Application 00 Key Count: 1\n"
    "Application 00 Key 00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
    "Application 00 Key 00 Version: 00\n"
    "Application 00 File Count: 1\n"
    "Application 00 File 00 Number: 01\n"
    "Application 00 File 00 Type: Standard Data\n"
    "Application 00 File 00 Communication Settings: 00\n"
    "Application 00 File 00 Access Rights: EE EE\n"
    "Application 00 File 00 Size: 32\n"
    "Application 00 File 00 Data: 12 34 56 78\n"
    "Application 00 File 00 Data Complete: 0\n";

// A transaction-MAC file records six fields in the canonical order.
static const char TMAC[] =
    "Filetype: DFC Credential\n"
    "Version: 4\n"
    "Card Generation: EV2\n"
    "Card Storage: 2048\n"
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
    "Application 00 Key Settings 2: 81\n"
    "Application 00 Authentication Mode: AES\n"
    "Application 00 Key Count: 1\n"
    "Application 00 Key 00: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n"
    "Application 00 Key 00 Version: 00\n"
    "Application 00 File Count: 1\n"
    "Application 00 File 00 Number: 1F\n"
    "Application 00 File 00 Type: Transaction MAC\n"
    "Application 00 File 00 Communication Settings: 03\n"
    "Application 00 File 00 Access Rights: 10 00\n"
    "Application 00 File 00 Transaction Counter: 9\n"
    "Application 00 File 00 Transaction MAC: 01 23 45 67 89 AB CD EF\n"
    "Application 00 File 00 Transaction Key Type: 2\n"
    "Application 00 File 00 Transaction Key Version: 03\n"
    "Application 00 File 00 Transaction Key: B0 B1 B2 B3 B4 B5 B6 B7 B8 B9 BA BB BC BD BE BF\n"
    "Application 00 File 00 Previous Reader ID: C0 C1 C2 C3 C4 C5 C6 C7 C8 C9 CA CB CC CD CE CF\n";

static MunitResult test_transaction_mac_round_trip(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static DfcCredential c;
    memset(&c, 0, sizeof(c));
    DfcTextError detail = {0};
    size_t len = sizeof(TMAC) - 1;
    DfcTextStatus st = dfc_text_parse(&c, TMAC, len, &detail);
    if(st != DfcTextOk) log_detail(&detail);
    munit_assert_int(st, ==, DfcTextOk);
    munit_assert_size(c.num_files, ==, 1);

    const DfcFile* f = &c.files[0];
    munit_assert_uint8(f->type, ==, 0x05);
    munit_assert_uint32(f->transaction_counter, ==, 9);
    munit_assert_uint8(f->transaction_mac_key_type, ==, 2);
    munit_assert_uint8(f->transaction_mac_key_version, ==, 3);
    munit_assert_memory_equal(8, f->transaction_mac, "\x01\x23\x45\x67\x89\xAB\xCD\xEF");

    // The text is canonical, so writing the model reproduces it exactly.
    static char out[DFC_TEXT_MAX_SIZE];
    size_t out_len = 0;
    munit_assert_int(dfc_text_write(&c, out, sizeof(out), &out_len), ==, DfcTextOk);
    assert_written_v6(out);

    // The model survives a trip through the binary encoding as well.
    static uint8_t der[DFC_DER_MAX_SIZE];
    size_t der_len = 0;
    munit_assert_int(dfc_der_encode(&c, der, sizeof(der), &der_len), ==, DfcDerOk);
    static DfcCredential back;
    munit_assert_int(dfc_der_decode(&back, der, der_len), ==, DfcDerOk);
    munit_assert_int(dfc_text_write(&back, out, sizeof(out), &out_len), ==, DfcTextOk);
    assert_written_v6(out);
    return MUNIT_OK;
}

static MunitResult test_minimal_round_trip(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static DfcCredential c;
    memset(&c, 0, sizeof(c));
    DfcTextError detail = {0};
    size_t len = sizeof(MINIMAL) - 1;
    DfcTextStatus st = dfc_text_parse(&c, MINIMAL, len, &detail);
    if(st != DfcTextOk) log_detail(&detail);
    munit_assert_int(st, ==, DfcTextOk);
    munit_assert_size(c.num_apps, ==, 1);
    munit_assert_size(c.num_files, ==, 1);
    munit_assert_uint32(c.card.storage, ==, 2048);
    munit_assert_false(c.picc_random_id);
    munit_assert_size(c.picc_ats_len, ==, 0);

    static char out[DFC_TEXT_MAX_SIZE];
    size_t out_len = 0;
    munit_assert_int(dfc_text_write(&c, out, sizeof(out), &out_len), ==, DfcTextOk);
    assert_written_v6(out);

    // Comments and blank lines do not disturb canonical field order.
    static char shuffled[sizeof(MINIMAL) + 64];
    snprintf(
        shuffled,
        sizeof(shuffled),
        "Filetype: DFC Credential\nVersion: 4\n# a comment\n\n%s",
        MINIMAL + 36);
    static DfcCredential other;
    memset(&other, 0, sizeof(other));
    st = dfc_text_parse(&other, shuffled, strlen(shuffled), &detail);
    if(st != DfcTextOk) log_detail(&detail);
    munit_assert_int(st, ==, DfcTextOk);
    munit_assert_size(other.num_files, ==, 1);
    return MUNIT_OK;
}

// One loader takes either encoding and tells them apart by the first octet, so
// a host can hand it whatever file it was given.
static MunitResult test_load_either_encoding(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static DfcCredential from_text;
    static DfcCredential from_binary;
    static uint8_t octets[DFC_DER_MAX_SIZE];
    DfcTextError detail = {0};

    DfcTextStatus st = dfc_credential_load(
        &from_text, (const uint8_t*)MINIMAL, sizeof(MINIMAL) - 1, &detail);
    if(st != DfcTextOk) log_detail(&detail);
    munit_assert_int(st, ==, DfcTextOk);
    munit_assert_false(
        dfc_credential_content_is_binary((const uint8_t*)MINIMAL, sizeof(MINIMAL) - 1));

    size_t octets_len = 0;
    munit_assert_int(dfc_der_encode(&from_text, octets, sizeof(octets), &octets_len), ==, DfcDerOk);
    munit_assert_true(dfc_credential_content_is_binary(octets, octets_len));
    munit_assert_int(dfc_credential_load(&from_binary, octets, octets_len, &detail), ==, DfcTextOk);
    munit_assert_size(from_binary.num_apps, ==, from_text.num_apps);
    munit_assert_size(from_binary.num_files, ==, from_text.num_files);

    // A truncated binary credential fails as binary, and says so.
    st = dfc_credential_load(&from_binary, octets, octets_len - 1, &detail);
    munit_assert_int(st, ==, DfcTextMalformed);
    munit_assert_size(detail.line, ==, 0);
    munit_assert_not_null(strstr(detail.message, "binary"));

    munit_assert_int(dfc_credential_load(&from_binary, NULL, 0, &detail), !=, DfcTextOk);
    return MUNIT_OK;
}

// A mutated copy of MINIMAL, so every rejection differs from an accepted
// document in exactly one way.
static DfcTextStatus mutate(const char* find, const char* replace, DfcTextError* detail) {
    static char buf[sizeof(MINIMAL) + 128];
    const char* at = strstr(MINIMAL, find);
    munit_assert_not_null(at);
    size_t head = (size_t)(at - MINIMAL);
    size_t len = 0;
    memcpy(buf, MINIMAL, head);
    len = head;
    size_t rn = strlen(replace);
    memcpy(buf + len, replace, rn);
    len += rn;
    const char* tail = at + strlen(find);
    size_t tn = strlen(tail);
    memcpy(buf + len, tail, tn);
    len += tn;

    static DfcCredential c;
    memset(&c, 0, sizeof(c));
    return dfc_text_parse(&c, buf, len, detail);
}

static MunitResult test_rejections(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcTextError detail = {0};

    // A duplicate key.
    munit_assert_int(
        mutate("Card Storage: 2048\n", "Card Storage: 2048\nCard Storage: 4096\n", &detail),
        ==,
        DfcTextMalformed);
    munit_assert_size(detail.line, >, 0);
    munit_logf(MUNIT_LOG_INFO, "duplicate: line %u: %s", (unsigned)detail.line, detail.message);

    // An unrecognised key: the text encoding has no extension mechanism.
    munit_assert_int(
        mutate("Card Storage: 2048\n", "Card Storage: 2048\nCard Colour: Red\n", &detail),
        ==,
        DfcTextMalformed);
    munit_assert_size(detail.line, >, 0);
    munit_logf(MUNIT_LOG_INFO, "unrecognised: line %u: %s", (unsigned)detail.line, detail.message);

    // An empty value.
    munit_assert_int(mutate("Card Storage: 2048", "Card Storage:", &detail), ==, DfcTextMalformed);
    munit_logf(MUNIT_LOG_INFO, "empty value: line %u: %s", (unsigned)detail.line, detail.message);

    // Trailing whitespace.
    munit_assert_int(
        mutate("Card Storage: 2048", "Card Storage: 2048 ", &detail), ==, DfcTextMalformed);
    munit_assert_size(detail.line, ==, 4);
    munit_logf(MUNIT_LOG_INFO, "trailing: line %u: %s", (unsigned)detail.line, detail.message);

    // A key count above the 14 the format allows.
    munit_assert_int(
        mutate("PICC Key Count: 1", "PICC Key Count: 15", &detail), ==, DfcTextMalformed);
    munit_logf(MUNIT_LOG_INFO, "key count: line %u: %s", (unsigned)detail.line, detail.message);

    // A reserved ISO file ID.
    munit_assert_int(
        mutate(
            "Application 00 File 00 Access Rights: EE EE\n",
            "Application 00 File 00 Access Rights: EE EE\n"
            "Application 00 File 00 ISO File ID: 3F 00\n",
            &detail),
        ==,
        DfcTextMalformed);
    munit_logf(MUNIT_LOG_INFO, "reserved fid: line %u: %s", (unsigned)detail.line, detail.message);

    // Lower-case hex is not the canonical lexical form.
    munit_assert_int(mutate("UID: 04 22", "UID: 04 2a", &detail), ==, DfcTextMalformed);

    // A missing required key.
    munit_assert_int(mutate("PICC Key Settings 1: 0F\n", "", &detail), ==, DfcTextMalformed);

    // Wrong Filetype, and a later version is refused on its version.
    munit_assert_int(
        mutate("Filetype: DFC Credential", "Filetype: Something Else", &detail),
        ==,
        DfcTextMalformed);
    munit_assert_int(mutate("Version: 4", "Version: 7", &detail), ==, DfcTextUnsupported);
    munit_assert_int(
        mutate(
            "Card Storage: 2048\n",
            "Card Storage: 2048\nCard Hardware Version: 04 01 01 12 00 18 05\n",
            &detail),
        ==,
        DfcTextMalformed);

    // A boolean shall be 0 or 1, and an integer carries no leading zeros.
    munit_assert_int(
        mutate("File 00 Data Complete: 0", "File 00 Data Complete: false", &detail),
        ==,
        DfcTextMalformed);
    munit_assert_int(
        mutate("Card Storage: 2048", "Card Storage: 02048", &detail), ==, DfcTextMalformed);

    // Data longer than the declared size, and a completeness flag that lies.
    munit_assert_int(mutate("File 00 Size: 32", "File 00 Size: 2", &detail), ==, DfcTextMalformed);
    munit_assert_int(
        mutate("File 00 Data Complete: 0", "File 00 Data Complete: 1", &detail),
        ==,
        DfcTextMalformed);

    // An index that is not contiguous from 00 leaves an unclaimed line.
    munit_assert_int(
        mutate(
            "Application 00 Key 00 Version: 00\n",
            "Application 00 Key 00 Version: 00\n"
            "Application 00 Key 01: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n",
            &detail),
        ==,
        DfcTextMalformed);

    return MUNIT_OK;
}

static MunitResult test_v5_version_overrides(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static DfcCredential c;
    DfcTextError detail = {0};
    const char* overrides =
        "Card Hardware Version: 04 01 01 12 00 18 05\n"
        "Card Software Version: 04 01 01 02 01 18 05\n";
    const char* version = strstr(MINIMAL, "Version: 4\n");
    munit_assert_not_null(version);
    const char* picc = strstr(MINIMAL, "PICC Key Settings 1:");
    munit_assert_not_null(picc);
    static char text[sizeof(MINIMAL) + 128];
    size_t head = (size_t)(version - MINIMAL);
    size_t card_len = (size_t)(picc - MINIMAL);
    int written = snprintf(
        text,
        sizeof(text),
        "%.*s%s%s",
        (int)card_len,
        MINIMAL,
        overrides,
        picc);
    munit_assert_int(written, >, 0);
    text[head + strlen("Version: ")] = '5';
    DfcTextStatus st = dfc_text_parse(&c, text, (size_t)written, &detail);
    if(st != DfcTextOk) log_detail(&detail);
    munit_assert_int(st, ==, DfcTextOk);
    munit_assert_true(c.card.has_hardware_version);
    munit_assert_memory_equal(
        7, c.card.hardware_version, ((uint8_t[]){0x04, 0x01, 0x01, 0x12, 0x00, 0x18, 0x05}));
    munit_assert_true(c.card.has_software_version);
    munit_assert_memory_equal(
        7, c.card.software_version, ((uint8_t[]){0x04, 0x01, 0x01, 0x02, 0x01, 0x18, 0x05}));

    static char out[DFC_TEXT_MAX_SIZE];
    size_t out_len = 0;
    munit_assert_int(dfc_text_write(&c, out, sizeof(out), &out_len), ==, DfcTextOk);
    munit_assert_not_null(strstr(out, "Version: 6\n"));
    munit_assert_not_null(strstr(out, "Card Hardware Version: 04 01 01 12 00 18 05\n"));
    munit_assert_not_null(strstr(out, "Card Software Version: 04 01 01 02 01 18 05\n"));
    return MUNIT_OK;
}

static MunitResult test_v5_rejects_short_version_override(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcTextError detail = {0};
    const char* version = strstr(MINIMAL, "Version: 4\n");
    munit_assert_not_null(version);
    static char text[sizeof(MINIMAL) + 64];
    int written = snprintf(
        text,
        sizeof(text),
        "%.*sVersion: 5\nCard Hardware Version: 04 01 01 12 00 18\n%s",
        (int)(version - MINIMAL),
        MINIMAL,
        version + strlen("Version: 4\n"));
    munit_assert_int(written, >, 0);
    static DfcCredential c;
    munit_assert_int(dfc_text_parse(&c, text, (size_t)written, &detail), ==, DfcTextMalformed);
    munit_assert_size(detail.line, ==, 3);

    written = snprintf(
        text,
        sizeof(text),
        "%.*sVersion: 5\nCard Software Version: 04 01 01 02 01 18\n%s",
        (int)(version - MINIMAL),
        MINIMAL,
        version + strlen("Version: 4\n"));
    munit_assert_int(written, >, 0);
    munit_assert_int(dfc_text_parse(&c, text, (size_t)written, &detail), ==, DfcTextMalformed);
    munit_assert_size(detail.line, ==, 3);
    return MUNIT_OK;
}

static MunitResult test_writer_capacity(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static DfcCredential c;
    memset(&c, 0, sizeof(c));
    DfcTextError detail = {0};
    munit_assert_int(dfc_text_parse(&c, MINIMAL, sizeof(MINIMAL) - 1, &detail), ==, DfcTextOk);

    char small[16];
    size_t len = 0;
    munit_assert_int(dfc_text_write(&c, small, sizeof(small), &len), ==, DfcTextCapacity);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/minimal-round-trip", test_minimal_round_trip, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/transaction-mac-round-trip",
     test_transaction_mac_round_trip,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/rejections", test_rejections, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/v5-version-overrides", test_v5_version_overrides, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/v5-short-version-override",
     test_v5_rejects_short_version_override,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/writer-capacity", test_writer_capacity, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/load-either-encoding", test_load_either_encoding, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/dfc_text",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
