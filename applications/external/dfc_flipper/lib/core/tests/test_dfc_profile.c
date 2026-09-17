// Profile admission checks. A build must
// refuse what it cannot hold, with the class and the reason distinguishable
// (sections 1.2, 1.6, 1.8, 2.4). These tests pin that, including the reason
// text being populated, because a capacity failure that does not name the limit
// is not much use to whoever hit it.

#include "munit/munit.h"

#include "dfc_credential.h"
#include "dfc_profile.h"

#include <string.h>

static void seed(DfcCredential* c) {
    dfc_credential_clear(c);
    c->uid_len = DFC_DESFIRE_UID_LEN;
    memcpy(c->uid, "\x04\x22\x33\x44\x55\x66\x77", 7);
    c->card.generation = DfcGenerationEv1;
    c->card.storage = 2048;
    c->picc_key_len = 16;
    c->picc_num_keys = 1;
}

static MunitResult test_default_profile_accepts(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    const uint8_t aid[3] = {0x01, 0x00, 0x00};
    munit_assert_not_null(
        dfc_credential_create_application_desfire_order(&c, aid, 0x0F, DFC_KEY_TYPE_AES | 1));
    DfcFile* f = dfc_credential_create_file(&c, 0, 0x01);
    munit_assert_not_null(f);
    f->declared_size = 32;

    DfcProfile p;
    dfc_profile_default(&p);
    char reason[DFC_PROFILE_REASON_MAX];
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);
    munit_assert_string_equal(reason, "");
    return MUNIT_OK;
}

static MunitResult test_picc_files_unsupported(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    DfcFile* f = &c.files[c.num_files++];
    memset(f, 0, sizeof(*f));
    f->app_index = DFC_FILE_OWNER_PICC;
    f->number = 0x07;
    f->declared_size = 16;
    f->data_offset = DFC_FILE_POOL_NONE;

    DfcProfile p;
    dfc_profile_default(&p);
    char reason[DFC_PROFILE_REASON_MAX];
    // Refused as unsupported, not malformed: the credential is well formed, this
    // target simply has no files at PICC level.
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerUnsupported);
    munit_assert_not_null(strstr(reason, "PICC level"));

    // A target that does implement them accepts the same credential.
    p.supports_picc_files = true;
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);
    return MUNIT_OK;
}

static MunitResult test_generation_unsupported(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    c.card.generation = DfcGenerationEv3;

    DfcProfile p;
    dfc_profile_default(&p);
    p.supported_generations &= (uint8_t)~DFC_PROFILE_GEN_EV3;
    char reason[DFC_PROFILE_REASON_MAX];
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerUnsupported);
    munit_assert_not_null(strstr(reason, "EV3"));

    p.supported_generations |= DFC_PROFILE_GEN_EV3;
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);
    return MUNIT_OK;
}

static MunitResult test_capacity_named(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    for(size_t i = 0; i < 3; i++) {
        uint8_t aid[3] = {(uint8_t)(i + 1), 0x00, 0x00};
        munit_assert_not_null(dfc_credential_create_application_desfire_order(
            &c, aid, 0x0F, DFC_KEY_TYPE_AES | 1));
    }

    DfcProfile p;
    dfc_profile_default(&p);
    p.max_apps = 2;
    char reason[DFC_PROFILE_REASON_MAX];
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerCapacity);
    munit_assert_not_null(strstr(reason, "applications"));
    munit_assert_not_null(strstr(reason, "3"));
    return MUNIT_OK;
}

static MunitResult test_declared_space_capacity(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    const uint8_t aid[3] = {0x01, 0x00, 0x00};
    munit_assert_not_null(
        dfc_credential_create_application_desfire_order(&c, aid, 0x0F, DFC_KEY_TYPE_AES | 1));

    // Two files whose declared allocations together exceed the profile, even
    // though almost no contents are actually known. Declared size is what the
    // card has to reserve, so that is what the limit applies to.
    DfcFile* a = dfc_credential_create_file(&c, 0, 0x01);
    DfcFile* b = dfc_credential_create_file(&c, 0, 0x02);
    munit_assert_not_null(a);
    munit_assert_not_null(b);
    a->declared_size = 600;
    b->declared_size = 600;

    // Keep the advertised card size inside the profile, so the failure isolates
    // the declared-allocation sum rather than tripping the storage check first.
    c.card.storage = 1024;

    DfcProfile p;
    dfc_profile_default(&p);
    p.max_storage = 1024;
    char reason[DFC_PROFILE_REASON_MAX];
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerCapacity);
    munit_assert_not_null(strstr(reason, "declared file space"));

    p.max_storage = 2048;
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);
    return MUNIT_OK;
}

static MunitResult test_record_capacity_uses_max(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    const uint8_t aid[3] = {0x01, 0x00, 0x00};
    munit_assert_not_null(
        dfc_credential_create_application_desfire_order(&c, aid, 0x0F, DFC_KEY_TYPE_AES | 1));
    DfcFile* r = dfc_credential_create_file(&c, 0, 0x04);
    munit_assert_not_null(r);
    r->type = 0x03;
    r->record_size = 64;
    r->max_records = 32; // 2048 octets reserved, regardless of how many are used
    r->record_count = 1;

    DfcProfile p;
    dfc_profile_default(&p);
    p.max_storage = 1024;
    char reason[DFC_PROFILE_REASON_MAX];
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerCapacity);
    return MUNIT_OK;
}

// A user ATS states its own length in its first octet. One that disagrees with
// the stored length cannot be answered with, only clamped or padded, so it is
// refused at admission rather than turned into a frame no card would send.
static MunitResult test_user_ats_length_octet(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);

    DfcProfile p;
    dfc_profile_default(&p);
    char reason[DFC_PROFILE_REASON_MAX];

    // No user ATS: nothing to check, the engine answers with its default.
    munit_assert_true(dfc_credential_picc_ats_is_consistent(&c));
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);

    // Self-consistent: first octet is the length of the whole answer.
    memcpy(c.picc_ats, "\x06\x75\x00\x81\x02\x80", 6);
    c.picc_ats_len = 6;
    munit_assert_true(dfc_credential_picc_ats_is_consistent(&c));
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);

    // Length octet too small for the stored octets.
    c.picc_ats[0] = 0x05;
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&c));
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerUnsupported);
    munit_assert_not_null(strstr(reason, "ATS"));

    // Length octet longer than the stored octets, which would run off the end.
    c.picc_ats[0] = 0x14;
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&c));
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerUnsupported);

    // Zero length octet, which describes no answer at all.
    c.picc_ats[0] = 0x00;
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&c));
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerUnsupported);

    // A one-octet ATS is the shortest self-consistent one.
    c.picc_ats[0] = 0x01;
    c.picc_ats_len = 1;
    munit_assert_true(dfc_credential_picc_ats_is_consistent(&c));
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);

    // Longer than the model can hold, which no codec produces but a caller could.
    c.picc_ats_len = DFC_PICC_ATS_MAX + 1;
    munit_assert_false(dfc_credential_picc_ats_is_consistent(&c));
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerUnsupported);
    return MUNIT_OK;
}

// Random ID and the recorded provenance of the stored UID are independent: a
// credential may carry a real UID and still present a generated one, and the
// admission check must not treat that as a contradiction.
static MunitResult test_random_id_with_real_uid_admitted(
    const MunitParameter params[],
    void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    c.card.uid_provenance = DfcUidProvenanceReal;
    c.picc_random_id = true;

    DfcProfile p;
    dfc_profile_default(&p);
    char reason[DFC_PROFILE_REASON_MAX];
    munit_assert_int(dfc_profile_check(&p, &c, reason, sizeof(reason)), ==, DfcDerOk);
    return MUNIT_OK;
}

// Deleting an application renumbers the applications after it, and the files
// belonging to them. A PICC-level file is owned by a sentinel that is larger
// than any real index, so a naive "greater than" test decrements the sentinel
// and the file silently stops being a PICC file.
static MunitResult test_delete_app_preserves_picc_owner(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    seed(&c);
    for(size_t i = 0; i < 2; i++) {
        uint8_t aid[3] = {(uint8_t)(i + 1), 0x00, 0x00};
        munit_assert_not_null(dfc_credential_create_application_desfire_order(
            &c, aid, 0x0F, DFC_KEY_TYPE_AES | 1));
    }
    // One file in the second application, one at PICC level.
    DfcFile* in_app = dfc_credential_create_file(&c, 1, 0x01);
    munit_assert_not_null(in_app);
    in_app->declared_size = 8;

    DfcFile* at_picc = &c.files[c.num_files++];
    memset(at_picc, 0, sizeof(*at_picc));
    at_picc->app_index = DFC_FILE_OWNER_PICC;
    at_picc->number = 0x02;
    at_picc->declared_size = 8;
    at_picc->data_offset = DFC_FILE_POOL_NONE;

    munit_assert_true(dfc_credential_delete_application_at(&c, 0));

    munit_assert_size(c.num_apps, ==, 1);
    // The application file follows its application down to index 0.
    const DfcFile* moved = dfc_credential_find_file_in_app(&c, 0, 0x01);
    munit_assert_not_null(moved);
    // The PICC file still belongs to the PICC.
    bool found_picc = false;
    for(size_t i = 0; i < c.num_files; i++) {
        if(c.files[i].number == 0x02) {
            munit_assert_size(c.files[i].app_index, ==, DFC_FILE_OWNER_PICC);
            found_picc = true;
        }
    }
    munit_assert_true(found_picc);
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/default-profile-accepts", test_default_profile_accepts, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/picc-files-unsupported", test_picc_files_unsupported, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/generation-unsupported", test_generation_unsupported, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/capacity-named", test_capacity_named, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/declared-space-capacity", test_declared_space_capacity, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/record-capacity-uses-max", test_record_capacity_uses_max, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/user-ats-length-octet", test_user_ats_length_octet, NULL, NULL, MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/random-id-with-real-uid-admitted", test_random_id_with_real_uid_admitted, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {"/delete-app-preserves-picc-owner", test_delete_app_preserves_picc_owner, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/dfc_profile",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
