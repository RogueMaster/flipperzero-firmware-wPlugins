// DER codec round-trip and rejection tests.

#include "munit/munit.h"

#include "dfc_credential.h"
#include "dfc_der.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void build_basic(DfcCredential* c) {
    dfc_credential_clear(c);
    c->uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(c->uid, "\x04\x22\x33\x44\x55\x66\x77", 7);
    c->card.generation = DfcGenerationEv1;
    c->card.storage = 4096;
    c->card.uid_provenance = DfcUidProvenanceRandom;
    c->picc_key_settings_1 = 0x0F;
    c->picc_key_settings_2 = DFC_KEY_TYPE_DES_2K3DES | 1;
    c->picc_key_len = 16;
    c->picc_num_keys = 1;
    c->picc_random_id = true;
    c->picc_ats_len = 6;
    memcpy(c->picc_ats, "\x06\x75\x77\x81\x02\x80", 6);
    c->picc_has_sak = true;
    c->picc_sak = 0x20;

    const uint8_t aid[3] = {0x01, 0x00, 0x00}; // on-wire order
    DfcApplication* app =
        dfc_credential_create_application_desfire_order(c, aid, 0x0F, DFC_KEY_TYPE_AES | 1);
    munit_assert_not_null(app);
    app->has_iso_file_id = true;
    app->iso_file_id = 0xA1A2;
    memcpy(app->iso_aid, "APPONE", 6);
    app->iso_aid_len = 6;
    memcpy(
        dfc_credential_key(c, app, 0),
        "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA\xBB\xCC\xDD\xEE\xFF",
        16);
    app->key_versions[0] = 0x01;
}

static DfcFile* add_data_file(DfcCredential* c, uint8_t number, uint32_t size, const char* data,
                              size_t data_len, bool complete) {
    DfcFile* f = dfc_credential_create_file(c, 0, number);
    munit_assert_not_null(f);
    f->type = 0x00;
    f->comm_settings = 0x00;
    f->access_rights = 0xEEEE;
    f->declared_size = size;
    f->contents_complete = complete;
    if(data_len) {
        munit_assert_true(dfc_file_resize(c, f, data_len));
        memcpy(dfc_file_data(c, f), data, data_len);
    }
    return f;
}

static MunitResult test_round_trip(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential src;
    memset(&src, 0, sizeof(src));
    build_basic(&src);
    add_data_file(&src, 0x01, 32, "\x12\x34\x56\x78", 4, false);

    DfcFile* value = dfc_credential_create_file(&src, 0, 0x03);
    munit_assert_not_null(value);
    value->type = 0x02;
    value->comm_settings = 0x03;
    value->access_rights = 0x00EE;
    value->value_lower_limit = -100;
    value->value_upper_limit = 1000;
    value->value = 42;
    value->limited_credit = 0x03;

    DfcFile* rec = dfc_credential_create_file(&src, 0, 0x04);
    munit_assert_not_null(rec);
    rec->type = 0x03;
    rec->comm_settings = 0x00;
    rec->access_rights = 0xE000;
    rec->record_size = 4;
    rec->max_records = 5;
    rec->record_count = 2;
    rec->contents_complete = true;
    munit_assert_true(dfc_file_resize(&src, rec, 8));
    memcpy(dfc_file_data(&src, rec), "\x01\x02\x03\x04\x05\x06\x07\x08", 8);

    uint8_t buf[DFC_DER_MAX_SIZE];
    size_t len = 0;
    munit_assert_int(dfc_der_encode(&src, buf, sizeof(buf), &len), ==, DfcDerOk);
    munit_assert_size(len, >, 0);
    munit_assert_uint8(buf[0], ==, 0x60);

    DfcCredential dst;
    memset(&dst, 0, sizeof(dst));
    munit_assert_int(dfc_der_decode(&dst, buf, len), ==, DfcDerOk);

    munit_assert_memory_equal(7, dst.uid, src.uid);
    munit_assert_int(dst.card.generation, ==, DfcGenerationEv1);
    munit_assert_uint32(dst.card.storage, ==, 4096);
    munit_assert_int(dst.card.uid_provenance, ==, DfcUidProvenanceRandom);
    munit_assert_true(dst.picc_random_id);
    munit_assert_false(dst.picc_format_disabled);
    munit_assert_size(dst.picc_ats_len, ==, 6);
    munit_assert_true(dst.picc_has_sak);
    munit_assert_uint8(dst.picc_sak, ==, 0x20);
    munit_assert_false(dst.picc_has_atqa);
    munit_assert_size(dst.num_apps, ==, 1);
    munit_assert_size(dst.num_files, ==, 3);
    munit_assert_memory_equal(3, dst.apps[0].aid, src.apps[0].aid);
    munit_assert_true(dst.apps[0].has_iso_file_id);
    munit_assert_uint16(dst.apps[0].iso_file_id, ==, 0xA1A2);
    munit_assert_size(dst.apps[0].iso_aid_len, ==, 6);
    munit_assert_memory_equal(
        16,
        dfc_credential_key_const(&dst, &dst.apps[0], 0),
        dfc_credential_key_const(&src, &src.apps[0], 0));
    munit_assert_uint8(dst.apps[0].key_versions[0], ==, 0x01);

    const DfcFile* d0 = dfc_credential_find_file_in_app(&dst, 0, 0x01);
    munit_assert_not_null(d0);
    munit_assert_uint32(d0->declared_size, ==, 32);
    munit_assert_size(d0->data_len, ==, 4);
    munit_assert_false(d0->contents_complete);
    munit_assert_memory_equal(4, dfc_file_data_const(&dst, d0), "\x12\x34\x56\x78");

    const DfcFile* d1 = dfc_credential_find_file_in_app(&dst, 0, 0x03);
    munit_assert_not_null(d1);
    munit_assert_int32(d1->value_lower_limit, ==, -100);
    munit_assert_int32(d1->value_upper_limit, ==, 1000);
    munit_assert_int32(d1->value, ==, 42);
    munit_assert_uint8(d1->limited_credit, ==, 0x03);

    const DfcFile* d2 = dfc_credential_find_file_in_app(&dst, 0, 0x04);
    munit_assert_not_null(d2);
    munit_assert_uint32(d2->record_size, ==, 4);
    munit_assert_uint32(d2->max_records, ==, 5);
    munit_assert_uint32(d2->record_count, ==, 2);
    munit_assert_true(d2->contents_complete);
    munit_assert_size(d2->data_len, ==, 8);

    // Re-encoding the decoded model reproduces the octets, which is what makes
    // the encoding canonical.
    uint8_t again[DFC_DER_MAX_SIZE];
    size_t again_len = 0;
    munit_assert_int(dfc_der_encode(&dst, again, sizeof(again), &again_len), ==, DfcDerOk);
    munit_assert_size(again_len, ==, len);
    munit_assert_memory_equal(len, again, buf);

    return MUNIT_OK;
}

static MunitResult test_picc_level_file(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential src;
    memset(&src, 0, sizeof(src));
    build_basic(&src);

    // A PICC-level file is owned by the sentinel, not application 0.
    DfcFile* f = &src.files[src.num_files++];
    memset(f, 0, sizeof(*f));
    f->app_index = DFC_FILE_OWNER_PICC;
    f->number = 0x00;
    f->type = 0x00;
    f->access_rights = 0xEEEE;
    f->declared_size = 16;
    f->data_offset = DFC_FILE_POOL_NONE;
    munit_assert_true(dfc_file_resize(&src, f, 4));
    memcpy(dfc_file_data(&src, f), "\xDE\xAD\xBE\xEF", 4);

    uint8_t buf[DFC_DER_MAX_SIZE];
    size_t len = 0;
    munit_assert_int(dfc_der_encode(&src, buf, sizeof(buf), &len), ==, DfcDerOk);

    DfcCredential dst;
    memset(&dst, 0, sizeof(dst));
    munit_assert_int(dfc_der_decode(&dst, buf, len), ==, DfcDerOk);
    munit_assert_size(dst.num_files, ==, 1);
    munit_assert_size(dst.files[0].app_index, ==, DFC_FILE_OWNER_PICC);
    munit_assert_memory_equal(4, dfc_file_data_const(&dst, &dst.files[0]), "\xDE\xAD\xBE\xEF");
    return MUNIT_OK;
}

// Each mutation violates one rule and shall be refused.
static MunitResult test_rejections(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential src;
    memset(&src, 0, sizeof(src));
    build_basic(&src);
    add_data_file(&src, 0x01, 32, "\x12\x34\x56\x78", 4, false);

    uint8_t good[DFC_DER_MAX_SIZE];
    size_t good_len = 0;
    munit_assert_int(dfc_der_encode(&src, good, sizeof(good), &good_len), ==, DfcDerOk);

    DfcCredential dst;
    memset(&dst, 0, sizeof(dst));

    // Wrong outer tag.
    uint8_t bad_tag[DFC_DER_MAX_SIZE];
    memcpy(bad_tag, good, good_len);
    bad_tag[0] = 0x30;
    munit_assert_int(dfc_der_decode(&dst, bad_tag, good_len), ==, DfcDerMalformed);

    // Truncated.
    munit_assert_int(dfc_der_decode(&dst, good, good_len - 1), ==, DfcDerMalformed);

    // Trailing octet after a complete value.
    uint8_t trailing[DFC_DER_MAX_SIZE];
    memcpy(trailing, good, good_len);
    trailing[good_len] = 0x00;
    munit_assert_int(dfc_der_decode(&dst, trailing, good_len + 1), ==, DfcDerMalformed);

    // Version 5 is a later version, so unsupported rather than malformed.
    uint8_t v5[DFC_DER_MAX_SIZE];
    memcpy(v5, good, good_len);
    for(size_t i = 0; i + 2 < good_len; i++) {
        if(v5[i] == 0x80 && v5[i + 1] == 0x01 && v5[i + 2] == DFC_FORMAT_VERSION) {
            v5[i + 2] = DFC_FORMAT_VERSION + 1;
            break;
        }
    }
    munit_assert_int(dfc_der_decode(&dst, v5, good_len), ==, DfcDerUnsupported);

    // An explicit FALSE for a DEFAULT FALSE component is non-canonical.
    DfcCredential explicit_false = src;
    explicit_false.picc_random_id = false;
    uint8_t nf[DFC_DER_MAX_SIZE];
    size_t nf_len = 0;
    munit_assert_int(
        dfc_der_encode(&explicit_false, nf, sizeof(nf), &nf_len), ==, DfcDerOk);
    // The encoder omits it, so the octets shrink rather than carrying 00.
    munit_assert_size(nf_len, <, good_len);

    return MUNIT_OK;
}

static MunitResult test_model_validation(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    uint8_t buf[DFC_DER_MAX_SIZE];
    size_t len = 0;

    // A transaction-MAC file round-trips through its own contents alternative.
    DfcCredential tmac;
    memset(&tmac, 0, sizeof(tmac));
    build_basic(&tmac);
    // A transaction-MAC file requires EV2 or later.
    tmac.card.generation = DfcGenerationEv2;
    DfcFile* f = dfc_credential_create_file(&tmac, 0, 0x05);
    munit_assert_not_null(f);
    f->type = 0x05;
    f->transaction_counter = 9;
    memcpy(f->transaction_mac, "\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8);
    f->transaction_mac_key_type = 2;
    f->transaction_mac_key_version = 3;
    for(size_t i = 0; i < sizeof(f->transaction_mac_key); i++) {
        f->transaction_mac_key[i] = (uint8_t)(0xB0 + i);
        f->previous_reader_id[i] = (uint8_t)(0xC0 + i);
    }
    munit_assert_int(dfc_der_encode(&tmac, buf, sizeof(buf), &len), ==, DfcDerOk);

    DfcCredential tmac_back;
    munit_assert_int(dfc_der_decode(&tmac_back, buf, len), ==, DfcDerOk);
    munit_assert_size(tmac_back.num_files, ==, 1);
    const DfcFile* g = &tmac_back.files[0];
    munit_assert_uint8(g->type, ==, 0x05);
    munit_assert_uint32(g->transaction_counter, ==, 9);
    munit_assert_memory_equal(8, g->transaction_mac, f->transaction_mac);
    munit_assert_uint8(g->transaction_mac_key_type, ==, 2);
    munit_assert_uint8(g->transaction_mac_key_version, ==, 3);
    munit_assert_memory_equal(
        sizeof(g->transaction_mac_key), g->transaction_mac_key, f->transaction_mac_key);
    munit_assert_memory_equal(
        sizeof(g->previous_reader_id), g->previous_reader_id, f->previous_reader_id);

    static const uint8_t expected[] = {
        0x80, 0x01, 0x09, 0x81, 0x08, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD,
        0xEF, 0x82, 0x01, 0x02, 0x83, 0x01, 0x03, 0x84, 0x10, 0xB0, 0xB1, 0xB2,
        0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
        0xBF, 0x85, 0x10, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
        0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF};
    const uint8_t* container = NULL;
    for(size_t i = 0; i + 1 < len; i++) {
        if(buf[i] == 0xA8 && buf[i + 1] == sizeof(expected)) {
            container = &buf[i + 2];
            break;
        }
    }
    munit_assert_not_null(container);
    munit_assert_memory_equal(sizeof(expected), container, expected);

    // An EV2 feature on an EV1 credential is malformed, which is distinct from a
    // feature this build omits.
    DfcCredential early;
    memset(&early, 0, sizeof(early));
    build_basic(&early);
    DfcFile* wrong = dfc_credential_create_file(&early, 0, 0x05);
    munit_assert_not_null(wrong);
    wrong->type = 0x05;
    munit_assert_int(dfc_der_encode(&early, buf, sizeof(buf), &len), ==, DfcDerMalformed);

    // An EV3 feature on an EV2 credential is malformed for the same reason.
    DfcCredential ev2;
    memset(&ev2, 0, sizeof(ev2));
    build_basic(&ev2);
    ev2.card.generation = DfcGenerationEv2;
    ev2.picc_has_static_signature = true;
    munit_assert_int(dfc_der_encode(&ev2, buf, sizeof(buf), &len), ==, DfcDerMalformed);
    ev2.card.generation = DfcGenerationEv3;
    munit_assert_int(dfc_der_encode(&ev2, buf, sizeof(buf), &len), ==, DfcDerOk);

    // complete with contents shorter than the declared size.
    DfcCredential short_complete;
    memset(&short_complete, 0, sizeof(short_complete));
    build_basic(&short_complete);
    add_data_file(&short_complete, 0x01, 32, "\x12\x34", 2, true);
    munit_assert_int(
        dfc_der_encode(&short_complete, buf, sizeof(buf), &len), ==, DfcDerMalformed);

    // Value outside its own limits.
    DfcCredential bad_value;
    memset(&bad_value, 0, sizeof(bad_value));
    build_basic(&bad_value);
    DfcFile* v = dfc_credential_create_file(&bad_value, 0, 0x03);
    munit_assert_not_null(v);
    v->type = 0x02;
    v->value_lower_limit = 0;
    v->value_upper_limit = 100;
    v->value = 500;
    munit_assert_int(dfc_der_encode(&bad_value, buf, sizeof(buf), &len), ==, DfcDerMalformed);

    // A reserved ISO file ID.
    DfcCredential reserved;
    memset(&reserved, 0, sizeof(reserved));
    build_basic(&reserved);
    DfcFile* rf = add_data_file(&reserved, 0x01, 8, NULL, 0, false);
    rf->has_iso_file_id = true;
    rf->iso_file_id = 0x3F00;
    munit_assert_int(dfc_der_encode(&reserved, buf, sizeof(buf), &len), ==, DfcDerMalformed);

    // A cyclic record file needs capacity for two records.
    DfcCredential cyclic;
    memset(&cyclic, 0, sizeof(cyclic));
    build_basic(&cyclic);
    DfcFile* cf = dfc_credential_create_file(&cyclic, 0, 0x06);
    munit_assert_not_null(cf);
    cf->type = 0x04;
    cf->record_size = 2;
    cf->max_records = 1;
    cf->record_count = 0;
    munit_assert_int(dfc_der_encode(&cyclic, buf, sizeof(buf), &len), ==, DfcDerMalformed);

    return MUNIT_OK;
}

static MunitResult test_capacity(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential src;
    memset(&src, 0, sizeof(src));
    build_basic(&src);

    size_t needed = 0;
    munit_assert_int(dfc_der_encoded_size(&src, &needed), ==, DfcDerOk);
    munit_assert_size(needed, >, 0);

    uint8_t small[8];
    size_t len = 0;
    munit_assert_int(dfc_der_encode(&src, small, sizeof(small), &len), ==, DfcDerCapacity);

    uint8_t exact[DFC_DER_MAX_SIZE];
    munit_assert_int(dfc_der_encode(&src, exact, needed, &len), ==, DfcDerOk);
    munit_assert_size(len, ==, needed);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/round-trip", test_round_trip, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/picc-level-file", test_picc_level_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/rejections", test_rejections, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/model-validation", test_model_validation, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/capacity", test_capacity, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/dfc_der",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
