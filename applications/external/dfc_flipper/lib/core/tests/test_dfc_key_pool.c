#include "munit/munit.h"
#include "dfc_credential.h"

#include <string.h>

static DfcApplication* add_app(DfcCredential* c, uint8_t aid0, uint8_t key_settings_2) {
    const uint8_t aid[] = {aid0, 0x02, 0x03};
    return dfc_credential_create_application_desfire_order(c, aid, 0x0F, key_settings_2);
}

static void fill_keys(DfcCredential* c, DfcApplication* app, uint8_t pattern) {
    size_t n = app ? app->num_keys : c->picc_num_keys;
    size_t len = app ? app->key_len : c->picc_key_len;
    for(size_t i = 0; i < n; i++) {
        uint8_t* key = dfc_credential_key(c, app, i);
        munit_assert_not_null(key);
        memset(key, (int)(pattern + i), dfc_credential_stored_key_length(len));
    }
}

static MunitResult test_resize_allocate_and_release(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    dfc_credential_clear(&c);

    // A cleared credential already holds the PICC master key.
    munit_assert_size(c.picc_num_keys, ==, 1);
    munit_assert_size(c.key_pool_used, ==, 16);

    DfcApplication* a = add_app(&c, 0x01, DFC_KEY_TYPE_AES | 3);
    munit_assert_not_null(a);
    munit_assert_size(a->num_keys, ==, 3);
    munit_assert_size(c.key_pool_used, ==, 16 + 3 * 16);

    DfcApplication* b = add_app(&c, 0x04, DFC_KEY_TYPE_3K3DES | 2);
    munit_assert_not_null(b);
    munit_assert_size(c.key_pool_used, ==, 16 + 3 * 16 + 2 * 24);

    fill_keys(&c, NULL, 0x10);
    fill_keys(&c, a, 0xA0);
    fill_keys(&c, b, 0xB0);

    // Slices do not overlap: each key reads back exactly what was written.
    munit_assert_uint8(dfc_credential_key(&c, NULL, 0)[0], ==, 0x10);
    munit_assert_uint8(dfc_credential_key(&c, a, 2)[15], ==, 0xA2);
    munit_assert_uint8(dfc_credential_key(&c, b, 1)[23], ==, 0xB1);

    // Deleting the first application compacts the pool and leaves the survivors
    // pointing at their own material.
    munit_assert_true(dfc_credential_delete_application_at(&c, 0));
    munit_assert_size(c.num_apps, ==, 1);
    munit_assert_size(c.key_pool_used, ==, 16 + 2 * 24);
    munit_assert_uint8(dfc_credential_key(&c, NULL, 0)[0], ==, 0x10);
    DfcApplication* survivor = dfc_credential_get_application(&c, 0);
    munit_assert_not_null(survivor);
    munit_assert_uint8(dfc_credential_key(&c, survivor, 0)[0], ==, 0xB0);
    munit_assert_uint8(dfc_credential_key(&c, survivor, 1)[23], ==, 0xB1);

    // Out-of-range slots are refused rather than reaching into a neighbour.
    munit_assert_null(dfc_credential_key(&c, survivor, 2));
    munit_assert_null(dfc_credential_key(&c, NULL, 1));

    return MUNIT_OK;
}

static MunitResult test_format_picc_keeps_picc_keys(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    dfc_credential_clear(&c);

    fill_keys(&c, NULL, 0x77);
    DfcApplication* a = add_app(&c, 0x01, DFC_KEY_TYPE_AES | 4);
    munit_assert_not_null(a);
    fill_keys(&c, a, 0xC0);

    dfc_credential_format_picc(&c);
    munit_assert_size(c.num_apps, ==, 0);
    // The PICC record survives a format, so its slice has to survive with it.
    munit_assert_size(c.key_pool_used, ==, 16);
    const uint8_t* picc_key = dfc_credential_key_const(&c, NULL, 0);
    munit_assert_not_null(picc_key);
    munit_assert_uint8(picc_key[0], ==, 0x77);

    return MUNIT_OK;
}

static MunitResult test_pool_exhaustion(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    dfc_credential_clear(&c);

    size_t created = 0;
    for(size_t i = 0; i < DFC_MAX_APPS; i++) {
        DfcApplication* app = add_app(&c, (uint8_t)(0x10 + i), DFC_KEY_TYPE_3K3DES | DFC_MAX_KEYS);
        if(!app) break;
        created++;
    }
    // Either the application limit or the pool stops it, and whichever it was,
    // the refusal is clean: nothing was half-created.
    munit_assert_size(c.num_apps, ==, created);
    munit_assert_size(c.key_pool_used, <=, DFC_KEY_POOL_SIZE);
    for(size_t i = 0; i < c.num_apps; i++) {
        munit_assert_not_null(dfc_credential_key(&c, &c.apps[i], 0));
        munit_assert_not_null(dfc_credential_key(&c, &c.apps[i], DFC_MAX_KEYS - 1));
    }

    return MUNIT_OK;
}

// A copy of the model has to bring the key pool with it, for the same reason the
// file pool does: applications hold offsets, not keys.
static MunitResult test_copy_model_carries_pool(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential src;
    memset(&src, 0, sizeof(src));
    dfc_credential_clear(&src);

    fill_keys(&src, NULL, 0x55);
    DfcApplication* app = add_app(&src, 0x01, DFC_KEY_TYPE_AES | 2);
    munit_assert_not_null(app);
    fill_keys(&src, app, 0xD0);

    DfcCredential dst;
    memset(&dst, 0, sizeof(dst));
    dfc_credential_clear(&dst);
    dfc_credential_copy_model(&dst, &src);

    munit_assert_size(dst.num_apps, ==, 1);
    munit_assert_size(dst.key_pool_used, ==, src.key_pool_used);
    munit_assert_size(dst.picc_key_offset, ==, src.picc_key_offset);

    const uint8_t* copied_picc = dfc_credential_key_const(&dst, NULL, 0);
    munit_assert_not_null(copied_picc);
    munit_assert_memory_equal(16, copied_picc, dfc_credential_key_const(&src, NULL, 0));

    for(size_t i = 0; i < 2; i++) {
        const uint8_t* copied = dfc_credential_key_const(&dst, &dst.apps[0], i);
        munit_assert_not_null(copied);
        munit_assert_memory_equal(16, copied, dfc_credential_key_const(&src, &src.apps[0], i));
    }

    return MUNIT_OK;
}

// Growing a slice in place has to keep the records that follow it addressable,
// and shrinking has to give the space back.
static MunitResult test_resize_moves_slice(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    DfcCredential c;
    memset(&c, 0, sizeof(c));
    dfc_credential_clear(&c);

    DfcApplication* a = add_app(&c, 0x01, DFC_KEY_TYPE_AES | 1);
    DfcApplication* b = add_app(&c, 0x04, DFC_KEY_TYPE_AES | 1);
    munit_assert_not_null(a);
    munit_assert_not_null(b);
    fill_keys(&c, b, 0xB0);
    size_t used = c.key_pool_used;

    munit_assert_true(dfc_credential_keys_resize(&c, a, 5, 16));
    munit_assert_size(a->num_keys, ==, 5);
    munit_assert_size(c.key_pool_used, ==, used - 16 + 5 * 16);
    munit_assert_uint8(dfc_credential_key(&c, b, 0)[0], ==, 0xB0);

    munit_assert_true(dfc_credential_keys_resize(&c, a, 1, 16));
    munit_assert_size(c.key_pool_used, ==, used);
    munit_assert_uint8(dfc_credential_key(&c, b, 0)[0], ==, 0xB0);

    dfc_credential_keys_release(&c, a);
    munit_assert_null(dfc_credential_key(&c, a, 0));
    munit_assert_uint8(dfc_credential_key(&c, b, 0)[0], ==, 0xB0);

    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/resize-allocate-and-release",
     test_resize_allocate_and_release,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {"/format-picc-keeps-picc-keys",
     test_format_picc_keeps_picc_keys,
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
    {"/resize-moves-slice", test_resize_moves_slice, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {
    "/dfc_key_pool",
    tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
