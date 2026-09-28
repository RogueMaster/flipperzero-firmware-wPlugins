#include "munit/munit.h"

#include "dfc_credential.h"
#include "dfc_der.h"
#include "dfc_text.h"
#include "dfc_virtual_picc.h"

#include <stdio.h>
#include <string.h>

static char text_input[DFC_TEXT_MAX_SIZE];
static char text_output[DFC_TEXT_MAX_SIZE];
static uint8_t binary_input[DFC_DER_MAX_SIZE];
static uint8_t binary_output[DFC_DER_MAX_SIZE];
static DfcCredential from_text;
static DfcCredential from_binary;

static MunitResult check_example(const char* name, size_t expected_apps, size_t expected_files) {
    char path[128];
    snprintf(path, sizeof(path), "../examples/credentials/%s.dfc", name);
    FILE* file = fopen(path, "rb");
    munit_assert_not_null(file);
    size_t text_len = fread(text_input, 1, sizeof(text_input), file);
    munit_assert_int(ferror(file), ==, 0);
    munit_assert_int(fgetc(file), ==, EOF);
    munit_assert_int(fclose(file), ==, 0);
    munit_assert_size(text_len, >, 0);
    munit_assert_size(text_len, <, sizeof(text_input));
    text_input[text_len] = '\0';

    snprintf(path, sizeof(path), "../examples/credentials/%s.dfcb", name);
    file = fopen(path, "rb");
    munit_assert_not_null(file);
    size_t binary_len = fread(binary_input, 1, sizeof(binary_input), file);
    munit_assert_int(ferror(file), ==, 0);
    munit_assert_int(fgetc(file), ==, EOF);
    munit_assert_int(fclose(file), ==, 0);
    munit_assert_size(binary_len, >, 0);
    munit_assert_size(binary_len, <, sizeof(binary_input));

    memset(&from_text, 0, sizeof(from_text));
    DfcTextError detail = {0};
    DfcTextStatus text_status = dfc_text_parse(&from_text, text_input, text_len, &detail);
    if(text_status != DfcTextOk) {
        munit_logf(MUNIT_LOG_INFO, "%s line %u: %s", name, (unsigned)detail.line, detail.message);
    }
    munit_assert_int(text_status, ==, DfcTextOk);
    munit_assert_size(from_text.num_apps, ==, expected_apps);
    munit_assert_size(from_text.num_files, ==, expected_files);

    size_t written = 0;
    munit_assert_int(
        dfc_text_write(&from_text, text_output, sizeof(text_output), &written), ==, DfcTextOk);
    munit_assert_size(written, ==, text_len);
    munit_assert_memory_equal(text_len, text_output, text_input);
    munit_assert_int(
        dfc_der_encode(&from_text, binary_output, sizeof(binary_output), &written), ==, DfcDerOk);
    munit_assert_size(written, ==, binary_len);
    munit_assert_memory_equal(binary_len, binary_output, binary_input);

    memset(&from_binary, 0, sizeof(from_binary));
    munit_assert_int(dfc_der_decode(&from_binary, binary_input, binary_len), ==, DfcDerOk);
    munit_assert_int(
        dfc_der_encode(&from_binary, binary_output, sizeof(binary_output), &written), ==, DfcDerOk);
    munit_assert_size(written, ==, binary_len);
    munit_assert_memory_equal(binary_len, binary_output, binary_input);
    munit_assert_int(
        dfc_text_write(&from_binary, text_output, sizeof(text_output), &written), ==, DfcTextOk);
    munit_assert_size(written, ==, text_len);
    munit_assert_memory_equal(text_len, text_output, text_input);

    DfcVirtualPiccSession* session = dfc_virtual_picc_session_alloc(&from_binary);
    munit_assert_not_null(session);
    DfcVirtualPiccActivation activation;
    munit_assert_int(
        dfc_virtual_picc_scan_iso14443a(session, &activation), ==, DfcVirtualPiccStatusOk);
    munit_assert_size(activation.uid_len, >, 0);

    uint8_t response[128];
    size_t response_len = 0;
    const uint8_t get_version[] = {0x90, 0x60, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(session, get_version, sizeof(get_version), response,
                                           sizeof(response), &response_len),
        ==, DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, 9);
    munit_assert_memory_equal(2, response + 7, ((uint8_t[]){0x91, 0xAF}));
    if(from_binary.card.has_hardware_version) {
        munit_assert_memory_equal(7, response, from_binary.card.hardware_version);
    }
    const uint8_t next_frame[] = {0x90, 0xAF, 0x00, 0x00, 0x00};
    for(size_t frame = 0; frame < 3 && response[response_len - 1] == 0xAF; frame++) {
        munit_assert_int(
            dfc_virtual_picc_iso_dep_exchange(session, next_frame, sizeof(next_frame), response,
                                               sizeof(response), &response_len),
            ==, DfcVirtualPiccStatusOk);
    }
    munit_assert_memory_equal(2, response + response_len - 2, ((uint8_t[]){0x91, 0x00}));

    const uint8_t get_app_ids[] = {0x90, 0x6A, 0x00, 0x00, 0x00};
    munit_assert_int(
        dfc_virtual_picc_iso_dep_exchange(session, get_app_ids, sizeof(get_app_ids), response,
                                           sizeof(response), &response_len),
        ==, DfcVirtualPiccStatusOk);
    munit_assert_size(response_len, ==, expected_apps * 3 + 2);
    munit_assert_memory_equal(2, response + response_len - 2, ((uint8_t[]){0x91, 0x00}));
    for(size_t i = 0; i < expected_apps; i++) {
        for(size_t j = 0; j < 3; j++) {
            munit_assert_uint8(response[i * 3 + j], ==, from_binary.apps[i].aid[2 - j]);
        }
    }

    if(expected_apps > 0) {
        uint8_t select_app[] = {0x90, 0x5A, 0x00, 0x00, 0x03, 0, 0, 0, 0x00};
        for(size_t j = 0; j < 3; j++) select_app[5 + j] = from_binary.apps[0].aid[2 - j];
        munit_assert_int(
            dfc_virtual_picc_iso_dep_exchange(session, select_app, sizeof(select_app), response,
                                               sizeof(response), &response_len),
            ==, DfcVirtualPiccStatusOk);
        munit_assert_size(response_len, ==, 2);
        munit_assert_memory_equal(2, response, ((uint8_t[]){0x91, 0x00}));

        const uint8_t get_file_ids[] = {0x90, 0x6F, 0x00, 0x00, 0x00};
        munit_assert_int(
            dfc_virtual_picc_iso_dep_exchange(session, get_file_ids, sizeof(get_file_ids),
                                               response, sizeof(response), &response_len),
            ==, DfcVirtualPiccStatusOk);
        size_t app_files = 0;
        for(size_t i = 0; i < from_binary.num_files; i++) {
            if(from_binary.files[i].app_index != 0) continue;
            app_files++;
            bool listed = false;
            for(size_t j = 0; j + 2 < response_len; j++) {
                if(response[j] == from_binary.files[i].number) listed = true;
            }
            munit_assert_true(listed);
            uint8_t get_file_settings[] = {
                0x90, 0xF5, 0x00, 0x00, 0x01, from_binary.files[i].number, 0x00};
            size_t settings_len = 0;
            uint8_t settings[128];
            munit_assert_int(
                dfc_virtual_picc_iso_dep_exchange(session, get_file_settings,
                                                   sizeof(get_file_settings), settings,
                                                   sizeof(settings), &settings_len),
                ==, DfcVirtualPiccStatusOk);
            munit_assert_size(settings_len, >=, 3);
            munit_assert_uint8(settings[0], ==, from_binary.files[i].type);
            munit_assert_memory_equal(2, settings + settings_len - 2,
                                      ((uint8_t[]){0x91, 0x00}));
        }
        munit_assert_size(response_len, ==, app_files + 2);
        munit_assert_memory_equal(2, response + response_len - 2,
                                  ((uint8_t[]){0x91, 0x00}));
    }

    dfc_virtual_picc_session_free(session);
    return MUNIT_OK;
}

static MunitResult test_minimal(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    return check_example("v6-minimal", 0, 0);
}

static MunitResult test_auth_commands(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    return check_example("v6-auth-commands", 2, 6);
}

static MunitResult test_maximal(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    MunitResult result = check_example("v6-maximal", 4, 7);
    if(result != MUNIT_OK) return result;
    munit_assert_true(from_binary.picc_has_static_signature);
    munit_assert_true(from_binary.picc_has_proximity_key);
    munit_assert_true(from_binary.virtual_card_configured);
    munit_assert_true(from_binary.picc_has_dam_keys);
    munit_assert_size(from_binary.apps[0].num_key_sets, ==, 2);
    munit_assert_true(from_binary.apps[0].has_capability_data);
    munit_assert_true(from_binary.apps[0].delegated);
    munit_assert_true(from_binary.apps[0].has_sm_disable);
    bool found_sdm = false;
    for(size_t i = 0; i < from_binary.num_files; i++) {
        if(from_binary.files[i].app_index == 0 && from_binary.files[i].number == 0x01) {
            found_sdm = from_binary.files[i].sdm_enabled;
        }
    }
    munit_assert_true(found_sdm);
    return MUNIT_OK;
}

static MunitResult test_reordered_fields_fail(const MunitParameter params[], void* data) {
    (void)params;
    (void)data;
    static char reordered[DFC_TEXT_MAX_SIZE];
    DfcTextError detail = {0};

    munit_assert_int(check_example("v6-minimal", 0, 0), ==, MUNIT_OK);
    const char* generation = strstr(text_input, "Card Generation: EV3\n");
    const char* storage = strstr(text_input, "Card Storage: 4096\n");
    munit_assert_not_null(generation);
    munit_assert_not_null(storage);
    munit_assert_true(generation < storage);
    size_t prefix = (size_t)(generation - text_input);
    size_t generation_len = strlen("Card Generation: EV3\n");
    size_t storage_len = strlen("Card Storage: 4096\n");
    size_t original_len = strlen(text_input);
    memcpy(reordered, text_input, prefix);
    memcpy(reordered + prefix, storage, storage_len);
    memcpy(reordered + prefix + storage_len, generation, generation_len);
    memcpy(reordered + prefix + storage_len + generation_len,
           storage + storage_len, original_len - prefix - storage_len - generation_len);
    munit_assert_int(dfc_text_parse(&from_text, reordered, original_len, &detail), ==,
                     DfcTextMalformed);
    munit_assert_not_null(strstr(detail.message, "out of order"));

    munit_assert_int(check_example("v6-maximal", 4, 7), ==, MUNIT_OK);
    const char* capability = strstr(text_input, "Application 00 Capability Data:");
    const char* sm = strstr(text_input, "Application 00 SM Disable:");
    munit_assert_not_null(capability);
    munit_assert_not_null(sm);
    munit_assert_true(capability < sm);
    const char* sm_end = strchr(sm, '\n');
    munit_assert_not_null(sm_end);
    sm_end++;
    prefix = (size_t)(capability - text_input);
    size_t sm_len = (size_t)(sm_end - sm);
    original_len = strlen(text_input);
    memcpy(reordered, text_input, prefix);
    memcpy(reordered + prefix, sm, sm_len);
    memcpy(reordered + prefix + sm_len, capability, (size_t)(sm - capability));
    memcpy(reordered + prefix + sm_len + (size_t)(sm - capability), sm_end,
           original_len - (size_t)(sm_end - text_input));
    munit_assert_int(dfc_text_parse(&from_text, reordered, original_len, &detail), ==,
                     DfcTextMalformed);
    munit_assert_not_null(strstr(detail.message, "out of order"));
    return MUNIT_OK;
}

static MunitTest tests[] = {
    {"/minimal", test_minimal, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/auth-commands", test_auth_commands, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/maximal", test_maximal, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {"/reordered-fields-fail", test_reordered_fields_fail, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};

static const MunitSuite suite = {"/dfc_examples", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE};

int main(int argc, char* argv[]) {
    return munit_suite_main(&suite, NULL, argc, argv);
}
