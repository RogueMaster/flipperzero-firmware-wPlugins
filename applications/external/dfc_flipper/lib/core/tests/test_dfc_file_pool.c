#include "munit/munit.h"
#include "dfc_credential.h"

#include <string.h>

static MunitResult test_resize_allocate_and_release(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    memset(&credential, 0, sizeof(credential));
    dfc_credential_clear(&credential);

    const uint8_t aid[] = {0x01, 0x02, 0x03};
    munit_assert_not_null(
        dfc_credential_create_application_desfire_order(&credential, aid, 0x0F, 0x01));

    DfcFile* a = dfc_credential_create_file(&credential, 0, 0x01);
    DfcFile* b = dfc_credential_create_file(&credential, 0, 0x02);
    munit_assert_not_null(a);
    munit_assert_not_null(b);

    munit_assert_true(dfc_file_resize(&credential, a, 100));
    munit_assert_true(dfc_file_resize(&credential, b, 200));
    munit_assert_size(credential.file_pool_used, ==, 300);
    munit_assert_size(dfc_credential_file_pool_free(&credential), ==, DFC_FILE_POOL_SIZE - 300);

    uint8_t* pa = dfc_file_data(&credential, a);
    uint8_t* pb = dfc_file_data(&credential, b);
    munit_assert_not_null(pa);
    munit_assert_not_null(pb);
    memset(pa, 0xAA, 100);
    memset(pb, 0xBB, 200);

    munit_assert_true(dfc_credential_delete_file(&credential, 0, 0x01));
    munit_assert_size(credential.file_pool_used, ==, 200);
    // Surviving file keeps its payload after compact.
    pb = dfc_file_data(&credential, dfc_credential_find_file_in_app(&credential, 0, 0x02));
    munit_assert_not_null(pb);
    munit_assert_uint8(pb[0], ==, 0xBB);
    munit_assert_uint8(pb[199], ==, 0xBB);

    return MUNIT_OK;
}

static MunitResult test_pool_exhaustion(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential credential;
    memset(&credential, 0, sizeof(credential));
    dfc_credential_clear(&credential);
    const uint8_t aid[] = {0x01, 0x02, 0x03};
    munit_assert_not_null(
        dfc_credential_create_application_desfire_order(&credential, aid, 0x0F, 0x01));

    // Fill the pool with as many max-sized slices as fit (single-file cap may
    // be smaller than the pool).
    size_t chunk = DFC_MAX_FILE_DATA;
    if(chunk > DFC_FILE_POOL_SIZE) {
        chunk = DFC_FILE_POOL_SIZE;
    }
    uint8_t next_no = 0;
    while(dfc_credential_file_pool_free(&credential) >= chunk && next_no < DFC_MAX_FILES) {
        DfcFile* file = dfc_credential_create_file(&credential, 0, next_no++);
        munit_assert_not_null(file);
        munit_assert_true(dfc_file_resize(&credential, file, chunk));
    }
    // Leftover smaller than chunk, or full: one more byte must fail.
    if(dfc_credential_file_pool_free(&credential) > 0 && next_no < DFC_MAX_FILES) {
        size_t rem = dfc_credential_file_pool_free(&credential);
        DfcFile* mid = dfc_credential_create_file(&credential, 0, next_no++);
        munit_assert_not_null(mid);
        munit_assert_true(dfc_file_resize(&credential, mid, rem));
    }
    munit_assert_size(dfc_credential_file_pool_free(&credential), ==, 0);
    if(next_no < DFC_MAX_FILES) {
        DfcFile* other = dfc_credential_create_file(&credential, 0, next_no);
        munit_assert_not_null(other);
        munit_assert_false(dfc_file_resize(&credential, other, 1));
    }

    return MUNIT_OK;
}


// A copy of the model has to bring the pool with it. Copying files[] alone
// leaves every data_offset pointing into a pool that was never populated, and
// dfc_file_data then fails its bounds check and hands back NULL, which reads as
// "this file is empty" rather than as an error.
static MunitResult test_copy_model_carries_pool(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential src;
    memset(&src, 0, sizeof(src));
    dfc_credential_clear(&src);

    const uint8_t aid[] = {0x01, 0x02, 0x03};
    munit_assert_not_null(
        dfc_credential_create_application_desfire_order(&src, aid, 0x0F, 0x01));
    DfcFile* file = dfc_credential_create_file(&src, 0, 0x01);
    munit_assert_not_null(file);
    munit_assert_true(dfc_file_resize(&src, file, 8));
    uint8_t* payload = dfc_file_data(&src, file);
    munit_assert_not_null(payload);
    memcpy(payload, "\xDE\xAD\xBE\xEF\x01\x02\x03\x04", 8);

    DfcCredential dst;
    memset(&dst, 0, sizeof(dst));
    dfc_credential_clear(&dst);
    dfc_credential_copy_model(&dst, &src);

    munit_assert_size(dst.num_apps, ==, 1);
    munit_assert_size(dst.num_files, ==, 1);
    munit_assert_size(dst.file_pool_used, ==, src.file_pool_used);

    const DfcFile* copied = dfc_credential_find_file_in_app(&dst, 0, 0x01);
    munit_assert_not_null(copied);
    munit_assert_size(copied->data_len, ==, 8);
    const uint8_t* copied_payload = dfc_file_data_const(&dst, copied);
    munit_assert_not_null(copied_payload);
    munit_assert_memory_equal(8, copied_payload, payload);

    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/resize-allocate-and-release",
     test_resize_allocate_and_release,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/pool-exhaustion", test_pool_exhaustion, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/copy-model-carries-pool",
     test_copy_model_carries_pool,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/dfc_file_pool",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
