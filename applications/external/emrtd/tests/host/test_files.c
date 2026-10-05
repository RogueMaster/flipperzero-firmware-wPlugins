/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The catalogue of elementary files, checked against ICAO Doc 9303 part 10
 * table 36. Wrong identifiers here would send every later layer to the wrong
 * file, so the whole table is walked rather than sampled.
 */

#include "emrtd_test.h"

#include "../../protocol/emrtd_files.h"

static void test_dg1(void) {
    emrtd_test_begin("DG1 carries the identifiers of the standard");

    const EmrtdFileInfo* dg1 = emrtd_file_info(EmrtdFileDg1);
    TEST_CHECK(dg1 != NULL);
    if(dg1 == NULL) {
        return;
    }
    TEST_EQ_STR(dg1->name, "EF.DG1");
    TEST_EQ_INT(dg1->fid, 0x0101);
    TEST_EQ_INT(dg1->sfi, 0x01);
    TEST_EQ_INT(dg1->tag, 0x61);
    TEST_EQ_INT(dg1->dg_number, 1);
    TEST_CHECK(!dg1->eac_protected);

    uint8_t fid[2] = {0xFF, 0xFF};
    emrtd_file_fid_bytes(dg1, fid);
    TEST_EQ_HEX(fid, sizeof(fid), "0101");

    emrtd_test_begin("EF.COM and EF.SOD sit outside the numbered groups");
    const EmrtdFileInfo* com = emrtd_file_info(EmrtdFileCom);
    const EmrtdFileInfo* sod = emrtd_file_info(EmrtdFileSod);
    TEST_CHECK(com != NULL && sod != NULL);
    if(com == NULL || sod == NULL) {
        return;
    }
    TEST_EQ_INT(com->fid, 0x011E);
    TEST_EQ_INT(com->sfi, 0x1E);
    TEST_EQ_INT(com->tag, 0x60);
    TEST_EQ_INT(com->dg_number, -1);
    TEST_EQ_INT(sod->fid, 0x011D);
    TEST_EQ_INT(sod->sfi, 0x1D);
    TEST_EQ_INT(sod->tag, 0x77);
    TEST_EQ_INT(sod->dg_number, -1);
}

static void test_lookups(void) {
    emrtd_test_begin("a file is found by its data group number");

    TEST_CHECK(emrtd_file_by_dg_number(2) == emrtd_file_info(EmrtdFileDg2));
    TEST_CHECK(emrtd_file_by_dg_number(16) == emrtd_file_info(EmrtdFileDg16));
    TEST_CHECK(emrtd_file_by_dg_number(0) == NULL);
    TEST_CHECK(emrtd_file_by_dg_number(17) == NULL);
    TEST_CHECK(emrtd_file_by_dg_number(-1) == NULL);
    TEST_CHECK(emrtd_file_by_dg_number(99) == NULL);

    emrtd_test_begin("a file is found by the template tag EF.COM announces");
    TEST_CHECK(emrtd_file_by_tag(0x75) == emrtd_file_info(EmrtdFileDg2));
    TEST_CHECK(emrtd_file_by_tag(0x60) == emrtd_file_info(EmrtdFileCom));
    TEST_CHECK(emrtd_file_by_tag(0x77) == emrtd_file_info(EmrtdFileSod));
    TEST_CHECK(emrtd_file_by_tag(0x01) == NULL);
    TEST_CHECK(emrtd_file_by_tag(0x00) == NULL);
    TEST_CHECK(emrtd_file_by_tag(0xFF) == NULL);

    emrtd_test_begin("an index outside the table is refused");
    TEST_CHECK(emrtd_file_info(EmrtdFileCount) == NULL);
    TEST_CHECK(emrtd_file_info((EmrtdFileId)1000) == NULL);
}

static void test_table_is_consistent(void) {
    emrtd_test_begin("every identifier in the table is unique");

    for(size_t i = 0; i < (size_t)EmrtdFileCount; i++) {
        const EmrtdFileInfo* a = emrtd_file_info((EmrtdFileId)i);
        TEST_CHECK(a != NULL);
        if(a == NULL) {
            continue;
        }
        /* The index and the identifier must agree, since the mask uses both. */
        TEST_EQ_INT(a->id, (int)i);

        for(size_t j = i + 1; j < (size_t)EmrtdFileCount; j++) {
            const EmrtdFileInfo* b = emrtd_file_info((EmrtdFileId)j);
            if(b == NULL) {
                continue;
            }
            TEST_CHECK(a->fid != b->fid);
            TEST_CHECK(a->tag != b->tag);
            TEST_CHECK(a->sfi != b->sfi);
        }
    }

    emrtd_test_begin("the data groups run from 1 to 16 without a gap");
    for(int number = 1; number <= 16; number++) {
        const EmrtdFileInfo* info = emrtd_file_by_dg_number(number);
        TEST_CHECK(info != NULL);
        if(info != NULL) {
            TEST_EQ_INT(info->dg_number, number);
            /* The file identifier of a group is 0x0100 plus its number. */
            TEST_EQ_INT(info->fid, 0x0100 + number);
            TEST_EQ_INT(info->sfi, number);
        }
    }

    emrtd_test_begin("every file has text for the screen");
    for(size_t i = 0; i < (size_t)EmrtdFileCount; i++) {
        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)i);
        if(info == NULL) {
            continue;
        }
        TEST_CHECK(info->name != NULL && info->name[0] != '\0');
        TEST_CHECK(info->label != NULL && info->label[0] != '\0');
        TEST_CHECK(info->description != NULL && info->description[0] != '\0');
        /* The menu is 128 pixels wide, so a long label would be cut off. */
        TEST_CHECK(strlen(info->label) <= 18);
        TEST_CHECK(strlen(info->name) <= 8);
    }
}

static void test_default_mask(void) {
    emrtd_test_begin("the biometric groups need a certificate and are left out");

    const EmrtdFileMask mask = emrtd_file_default_mask();
    TEST_CHECK((mask & EMRTD_FILE_BIT(EmrtdFileDg3)) == 0);
    TEST_CHECK((mask & EMRTD_FILE_BIT(EmrtdFileDg4)) == 0);
    TEST_CHECK(emrtd_file_info(EmrtdFileDg3)->eac_protected);
    TEST_CHECK(emrtd_file_info(EmrtdFileDg4)->eac_protected);

    emrtd_test_begin("everything else is attempted by default");
    for(size_t i = 0; i < (size_t)EmrtdFileCount; i++) {
        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)i);
        if(info == NULL) {
            continue;
        }
        const bool selected = (mask & EMRTD_FILE_BIT(info->id)) != 0;
        TEST_CHECK(selected == !info->eac_protected);
    }
}

void test_suite_files(void) {
    test_dg1();
    test_lookups();
    test_table_is_consistent();
    test_default_mask();
}
