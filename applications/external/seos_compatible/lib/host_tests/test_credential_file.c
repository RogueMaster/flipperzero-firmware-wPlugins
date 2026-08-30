/* Reading a saved credential.
 *
 * Fields may appear in any order. The reader underneath searches forward from
 * the cursor and never rewinds, and a miss leaves it at end of file, so the
 * parser must rewind before each field. The mock reproduces that behaviour.
 */
#include "munit.h"
#include "test_helpers.h"

#include <seos_credential_parse.h>

#include <lib/flipper_format/flipper_format.h>

#include <stdio.h>
#include <string.h>

static const char* whole_file = "Filetype: Flipper Seos Credential\n"
                                "Version: 1\n"
                                "Diversifier Length: 8\n"
                                "Diversifier: 01 02 03 04 05 06 07 08\n"
                                "SIO Length: 4\n"
                                "SIO: AA BB CC DD\n"
                                "Priv Key: 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F\n"
                                "Auth Key: 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F\n"
                                "ADF OID Length: 3\n"
                                "ADF OID: 06 07 08\n";

static SeosCredential parse(const char* contents, bool* ok) {
    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));

    FlipperFormat* file = flipper_format_string_alloc_from(contents);
    *ok = seos_credential_parse_seos(file, &credential);
    flipper_format_free(file);
    return credential;
}

static MunitResult test_reads_every_field(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    bool ok = false;
    SeosCredential credential = parse(whole_file, &ok);
    munit_assert_true(ok);

    const uint8_t diversifier[] = {1, 2, 3, 4, 5, 6, 7, 8};
    munit_assert_size(credential.diversifier_len, ==, sizeof(diversifier));
    munit_assert_memory_equal(sizeof(diversifier), credential.diversifier, diversifier);

    const uint8_t sio[] = {0xaa, 0xbb, 0xcc, 0xdd};
    munit_assert_size(credential.sio_len, ==, sizeof(sio));
    munit_assert_memory_equal(sizeof(sio), credential.sio, sio);

    munit_assert_uint8(credential.priv_key[0], ==, 0x10);
    munit_assert_uint8(credential.priv_key[15], ==, 0x1f);
    munit_assert_uint8(credential.auth_key[0], ==, 0x20);
    munit_assert_uint8(credential.auth_key[15], ==, 0x2f);

    const uint8_t oid[] = {0x06, 0x07, 0x08};
    munit_assert_size(credential.adf_oid_len, ==, sizeof(oid));
    munit_assert_memory_equal(sizeof(oid), credential.adf_oid, oid);

    return MUNIT_OK;
}

/* Fields may be listed in any order. */
static MunitResult test_reads_fields_in_any_order(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    /* The same fields as whole_file, reversed. */
    const char* shuffled = "Filetype: Flipper Seos Credential\n"
                           "Version: 1\n"
                           "ADF OID: 06 07 08\n"
                           "ADF OID Length: 3\n"
                           "Auth Key: 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F\n"
                           "Priv Key: 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F\n"
                           "SIO: AA BB CC DD\n"
                           "SIO Length: 4\n"
                           "Diversifier: 01 02 03 04 05 06 07 08\n"
                           "Diversifier Length: 8\n";

    bool ok = false;
    SeosCredential credential = parse(shuffled, &ok);
    munit_assert_true(ok);

    const uint8_t diversifier[] = {1, 2, 3, 4, 5, 6, 7, 8};
    munit_assert_size(credential.diversifier_len, ==, sizeof(diversifier));
    munit_assert_memory_equal(sizeof(diversifier), credential.diversifier, diversifier);

    const uint8_t sio[] = {0xaa, 0xbb, 0xcc, 0xdd};
    munit_assert_size(credential.sio_len, ==, sizeof(sio));
    munit_assert_memory_equal(sizeof(sio), credential.sio, sio);

    munit_assert_uint8(credential.priv_key[0], ==, 0x10);
    munit_assert_uint8(credential.auth_key[15], ==, 0x2f);
    munit_assert_size(credential.adf_oid_len, ==, 3);

    return MUNIT_OK;
}

/* The mock must not find a key behind the cursor, and must leave the cursor at
 * end of file after a miss. Without this the parser's rewinds would be
 * untested. */
static MunitResult test_reader_searches_forward_only(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    FlipperFormat* file = flipper_format_string_alloc_from(whole_file);

    FuriString* header = furi_string_alloc();
    uint32_t version = 0;
    munit_assert_true(flipper_format_read_header(file, header, &version));

    uint32_t diversifier_len = 0;
    uint32_t sio_len = 0;
    munit_assert_true(flipper_format_read_uint32(file, "Diversifier Length", &diversifier_len, 1));
    munit_assert_true(flipper_format_read_uint32(file, "SIO Length", &sio_len, 1));

    /* Diversifier is behind the cursor now. */
    uint8_t diversifier[8];
    munit_assert_false(flipper_format_read_hex(file, "Diversifier", diversifier, 8));
    munit_assert_true(flipper_format_mock_at_end(file));

    /* After a rewind it is found again. */
    munit_assert_true(flipper_format_rewind(file));
    munit_assert_true(flipper_format_read_hex(file, "Diversifier", diversifier, 8));

    furi_string_free(header);
    flipper_format_free(file);
    return MUNIT_OK;
}

/* A length the field cannot hold is refused rather than written past. */
static MunitResult test_refuses_oversized_lengths(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    bool ok = false;

    const char* long_diversifier = "Filetype: Flipper Seos Credential\n"
                                   "Version: 1\n"
                                   "Diversifier Length: 64\n"
                                   "Diversifier: 01 02\n"
                                   "SIO Length: 4\n"
                                   "SIO: AA BB CC DD\n";
    parse(long_diversifier, &ok);
    munit_assert_false(ok);

    const char* long_sio = "Filetype: Flipper Seos Credential\n"
                           "Version: 1\n"
                           "Diversifier Length: 2\n"
                           "Diversifier: 01 02\n"
                           "SIO Length: 4096\n"
                           "SIO: AA BB\n";
    parse(long_sio, &ok);
    munit_assert_false(ok);

    const char* long_oid = "Filetype: Flipper Seos Credential\n"
                           "Version: 1\n"
                           "Diversifier Length: 2\n"
                           "Diversifier: 01 02\n"
                           "SIO Length: 2\n"
                           "SIO: AA BB\n"
                           "ADF OID Length: 900\n"
                           "ADF OID: 06 07\n";
    parse(long_oid, &ok);
    munit_assert_false(ok);

    return MUNIT_OK;
}

/* Keys and the application identifier are optional. */
static MunitResult test_optional_fields_may_be_absent(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const char* bare = "Filetype: Flipper Seos Credential\n"
                       "Version: 1\n"
                       "Diversifier Length: 2\n"
                       "Diversifier: 09 0A\n"
                       "SIO Length: 2\n"
                       "SIO: BB CC\n";

    bool ok = false;
    SeosCredential credential = parse(bare, &ok);
    munit_assert_true(ok);
    munit_assert_size(credential.diversifier_len, ==, 2);
    munit_assert_size(credential.sio_len, ==, 2);

    /* Absent fields read as zero. */
    const uint8_t empty[16] = {0};
    munit_assert_memory_equal(sizeof(empty), credential.priv_key, empty);
    munit_assert_memory_equal(sizeof(empty), credential.auth_key, empty);
    munit_assert_size(credential.adf_oid_len, ==, 0);

    return MUNIT_OK;
}

/* One absent optional field must not swallow the ones listed after it. */
static MunitResult test_an_absent_field_keeps_the_rest(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    /* No private key, but fields listed after where it would have been. */
    const char* no_priv_key = "Filetype: Flipper Seos Credential\n"
                              "Version: 1\n"
                              "Diversifier Length: 2\n"
                              "Diversifier: 09 0A\n"
                              "SIO Length: 2\n"
                              "SIO: BB CC\n"
                              "Auth Key: 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F\n"
                              "ADF OID Length: 3\n"
                              "ADF OID: 06 07 08\n";

    bool ok = false;
    SeosCredential credential = parse(no_priv_key, &ok);
    munit_assert_true(ok);

    munit_assert_uint8(credential.auth_key[0], ==, 0x20);
    munit_assert_uint8(credential.auth_key[15], ==, 0x2f);
    munit_assert_size(credential.adf_oid_len, ==, 3);

    return MUNIT_OK;
}

static MunitResult test_refuses_a_wrong_header(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    bool ok = false;

    const char* wrong_type = "Filetype: Something Else\n"
                             "Version: 1\n"
                             "Diversifier Length: 2\n"
                             "Diversifier: 01 02\n"
                             "SIO Length: 2\n"
                             "SIO: AA BB\n";
    parse(wrong_type, &ok);
    munit_assert_false(ok);

    const char* wrong_version = "Filetype: Flipper Seos Credential\n"
                                "Version: 99\n"
                                "Diversifier Length: 2\n"
                                "Diversifier: 01 02\n"
                                "SIO Length: 2\n"
                                "SIO: AA BB\n";
    parse(wrong_version, &ok);
    munit_assert_false(ok);

    return MUNIT_OK;
}

/* A file that stops partway through is refused, at whatever point it stops. */
static MunitResult test_refuses_a_truncated_file(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    size_t full = strlen(whole_file);

    /* Required fields end with the credential. A cut before the end of that
     * line loses one of them; a cut after it loses only optional fields. */
    const char* sio_line = strstr(whole_file, "\nSIO: ");
    size_t required = (size_t)(strchr(sio_line + 1, '\n') - whole_file);

    for(size_t cut = 0; cut < required; cut++) {
        if(whole_file[cut] != '\n') continue;

        char partial[1024];
        memcpy(partial, whole_file, cut + 1);
        partial[cut + 1] = '\0';

        bool ok = false;
        parse(partial, &ok);
        munit_assert_false(ok);
    }

    /* Cut at the end of the credential line: all required fields present. */
    char just_enough[1024];
    memcpy(just_enough, whole_file, required + 1);
    just_enough[required + 1] = '\0';

    bool ok = false;
    SeosCredential credential = parse(just_enough, &ok);
    munit_assert_true(ok);
    munit_assert_size(credential.sio_len, ==, 4);
    munit_assert_size(credential.adf_oid_len, ==, 0);

    /* And the whole thing still parses. */
    parse(whole_file, &ok);
    munit_assert_true(ok);
    munit_assert_size(full, >, required);

    return MUNIT_OK;
}


/* ---- the other tool's format ---- */

/* Builds a file in that format: fixed size fields, no stated lengths. */
static void build_seader_file(char* out, size_t out_cap, size_t sio_payload_len, size_t divisor_zeros) {
    char sio[64 * 3];
    size_t at = 0;
    uint8_t bytes[64];
    memset(bytes, 0, sizeof(bytes));
    bytes[0] = 0x30;
    bytes[1] = (uint8_t)sio_payload_len;
    for(size_t i = 2; i < sizeof(bytes); i++)
        bytes[i] = (uint8_t)(i + 0x40);

    for(size_t i = 0; i < sizeof(bytes); i++)
        at += (size_t)snprintf(sio + at, sizeof(sio) - at, i ? " %02X" : "%02X", bytes[i]);

    char diversifier[8 * 3];
    at = 0;
    for(size_t i = 0; i < 8; i++) {
        uint8_t b = i >= 8 - divisor_zeros ? 0x00 : (uint8_t)(0xa0 + i);
        at += (size_t)snprintf(diversifier + at, sizeof(diversifier) - at, i ? " %02X" : "%02X", b);
    }

    snprintf(
        out,
        out_cap,
        "Filetype: Flipper Seader Credential\n"
        "Version: 1\n"
        "SIO: %s\n"
        "Diversifier: %s\n",
        sio,
        diversifier);
}

static SeosCredential parse_seader(const char* contents, bool* ok) {
    SeosCredential credential;
    memset(&credential, 0, sizeof(credential));

    FlipperFormat* file = flipper_format_string_alloc_from(contents);
    *ok = seos_credential_parse_seader(file, &credential);
    flipper_format_free(file);
    return credential;
}

/* The credential length comes from inside the credential, not from the file. */
static MunitResult test_seader_takes_length_from_contents(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    char contents[1024];
    build_seader_file(contents, sizeof(contents), 20, 0);

    bool ok = false;
    SeosCredential credential = parse_seader(contents, &ok);
    munit_assert_true(ok);
    /* Two bytes of tag and length before, two after. */
    munit_assert_size(credential.sio_len, ==, 24);
    munit_assert_size(credential.diversifier_len, ==, 8);
    munit_assert_uint8(credential.diversifier[0], ==, 0xa0);

    return MUNIT_OK;
}

/* The diversifier field is fixed size, so a shorter one is zero padded and
 * the real length runs to the first zero. */
static MunitResult test_seader_trims_the_diversifier(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    char contents[1024];
    build_seader_file(contents, sizeof(contents), 16, 3);

    bool ok = false;
    SeosCredential credential = parse_seader(contents, &ok);
    munit_assert_true(ok);
    munit_assert_size(credential.diversifier_len, ==, 5);

    return MUNIT_OK;
}

/* A stated length larger than the field is refused. */
static MunitResult test_seader_refuses_an_oversized_length(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    char contents[1024];
    build_seader_file(contents, sizeof(contents), 0xfe, 0);

    bool ok = false;
    parse_seader(contents, &ok);
    munit_assert_false(ok);

    return MUNIT_OK;
}

static MunitResult test_seader_refuses_a_wrong_header(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    char contents[1024];
    build_seader_file(contents, sizeof(contents), 16, 0);

    char* version = strstr(contents, "Version: 1");
    version[9] = '7';

    bool ok = false;
    parse_seader(contents, &ok);
    munit_assert_false(ok);

    /* And a file this parser does not serve at all. */
    parse_seader(whole_file, &ok);
    munit_assert_false(ok);

    return MUNIT_OK;
}

/* Both required fields have to be there. */
static MunitResult test_seader_refuses_missing_fields(const MunitParameter p[], void* d) {
    (void)p;
    (void)d;
    const char* no_sio = "Filetype: Flipper Seader Credential\n"
                         "Version: 1\n"
                         "Diversifier: A0 A1 A2 A3 A4 A5 A6 A7\n";
    bool ok = false;
    parse_seader(no_sio, &ok);
    munit_assert_false(ok);

    char contents[1024];
    build_seader_file(contents, sizeof(contents), 16, 0);
    char* diversifier = strstr(contents, "Diversifier:");
    *diversifier = '\0';
    parse_seader(contents, &ok);
    munit_assert_false(ok);

    return MUNIT_OK;
}

static MunitTest test_credential_file_cases[] = {
    {(char*)"/reads-every-field", test_reads_every_field, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/any-order", test_reads_fields_in_any_order, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/reader-forward-only",
     test_reader_searches_forward_only,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/refuses-oversized",
     test_refuses_oversized_lengths,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/optional-absent",
     test_optional_fields_may_be_absent,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/absent-keeps-rest",
     test_an_absent_field_keeps_the_rest,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/wrong-header", test_refuses_a_wrong_header, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/truncated", test_refuses_a_truncated_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/seader/length-from-contents",
     test_seader_takes_length_from_contents,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/seader/trims-diversifier",
     test_seader_trims_the_diversifier,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/seader/oversized",
     test_seader_refuses_an_oversized_length,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/seader/wrong-header",
     test_seader_refuses_a_wrong_header,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/seader/missing-fields",
     test_seader_refuses_missing_fields,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

MunitSuite test_credential_file_suite = {
    (char*)"/credential-file",
    test_credential_file_cases,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};
