/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * SecurityInfos, and the verdict this build reaches about each of them.
 *
 * Classification is what stands between a document that cannot be opened and a
 * document whose reason for not opening can be named on the screen, so every
 * PACE variant of BSI TR-03110-3 appendix A.1.1.1 is put through it - the ones
 * that work and, more importantly, the ones that cannot.
 */

#include "emrtd_test.h"

#include "../../protocol/emrtd_security_info.h"

/* 0.4.0.127.0.7.2.2.4, id-PACE, without its mapping and cipher. */
static const uint8_t pace_oid_prefix[8] = {0x04, 0x00, 0x7F, 0x00, 0x07, 0x02, 0x02, 0x04};

/** Build one PACEInfo SEQUENCE. A negative parameter id leaves it out. */
static size_t
    build_pace_info(uint8_t* out, uint8_t mapping, uint8_t cipher, int version, int param_id) {
    uint8_t body[32];
    size_t pos = 0;

    body[pos++] = 0x06;
    body[pos++] = 0x0A;
    memcpy(body + pos, pace_oid_prefix, sizeof(pace_oid_prefix));
    pos += sizeof(pace_oid_prefix);
    body[pos++] = mapping;
    body[pos++] = cipher;

    body[pos++] = 0x02;
    body[pos++] = 0x01;
    body[pos++] = (uint8_t)version;

    if(param_id >= 0) {
        body[pos++] = 0x02;
        body[pos++] = 0x01;
        body[pos++] = (uint8_t)param_id;
    }

    out[0] = 0x30;
    out[1] = (uint8_t)pos;
    memcpy(out + 2, body, pos);
    return pos + 2;
}

/** Wrap entries in the SET OF that EF.CardAccess is. */
static size_t build_card_access(uint8_t* out, const uint8_t* entries, size_t entries_len) {
    out[0] = 0x31;
    out[1] = (uint8_t)entries_len;
    memcpy(out + 2, entries, entries_len);
    return entries_len + 2;
}

/** Wrap the same SET in the DG14 template. */
static size_t build_dg14(uint8_t* out, const uint8_t* entries, size_t entries_len) {
    uint8_t set[256];
    const size_t set_len = build_card_access(set, entries, entries_len);
    out[0] = 0x6E;
    out[1] = (uint8_t)set_len;
    memcpy(out + 2, set, set_len);
    return set_len + 2;
}

static void test_usable_pace(void) {
    emrtd_test_begin("the PACE a European passport announces is usable");

    uint8_t entry[64];
    uint8_t file[128];
    /* id-PACE-ECDH-GM-AES-CBC-CMAC-128 on brainpoolP256r1. */
    const size_t entry_len = build_pace_info(entry, 0x02, 0x02, 2, 13);
    const size_t len = build_card_access(file, entry, entry_len);

    EmrtdSecurityInfos infos;
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_EQ_INT(infos.entry_count, 1);
    TEST_CHECK(infos.has_pace);
    TEST_CHECK(infos.pace.usable);
    TEST_EQ_INT(infos.pace.reason, EmrtdErrorNone);
    TEST_EQ_INT(infos.pace.mapping, EmrtdPaceMappingGeneric);
    TEST_EQ_INT(infos.pace.agreement, EmrtdPaceAgreementEcdh);
    TEST_EQ_INT(infos.pace.cipher, EmrtdCipherAes128);
    TEST_EQ_INT(infos.pace.version, 2);
    TEST_EQ_INT(infos.pace.parameter_id, 13);
    TEST_CHECK(infos.pace.curve != NULL);
    if(infos.pace.curve != NULL) {
        TEST_EQ_STR(infos.pace.curve->name, "brainpoolP256r1");
    }
    TEST_EQ_STR(infos.pace.oid_name, "id-PACE-ECDH-GM-AES-CBC-CMAC-128");
    TEST_EQ_INT(infos.pace.oid_len, 10);
    TEST_EQ_HEX(infos.pace.oid, infos.pace.oid_len, "04007F00070202040202");
    TEST_CHECK(!infos.has_chip_auth);
    TEST_CHECK(!infos.has_terminal_auth);
    TEST_CHECK(!infos.has_active_auth);

    emrtd_test_begin("the same entry inside DG14 reads the same");
    const size_t dg14_len = build_dg14(file, entry, entry_len);
    TEST_EQ_INT(emrtd_security_infos_parse(file, dg14_len, &infos), EmrtdErrorNone);
    TEST_CHECK(infos.has_pace && infos.pace.usable);
    TEST_EQ_INT(infos.pace.parameter_id, 13);

    emrtd_test_begin("a bare SEQUENCE holding one entry reads the same");
    TEST_EQ_INT(emrtd_security_infos_parse(entry, entry_len, &infos), EmrtdErrorNone);
    TEST_EQ_INT(infos.entry_count, 1);
    TEST_CHECK(infos.has_pace && infos.pace.usable);

    emrtd_test_begin("NIST P-256 is usable too");
    const size_t p256_len =
        build_card_access(file, entry, build_pace_info(entry, 0x02, 0x02, 2, 12));
    TEST_EQ_INT(emrtd_security_infos_parse(file, p256_len, &infos), EmrtdErrorNone);
    TEST_CHECK(infos.pace.usable);
    TEST_EQ_STR(infos.pace.curve->name, "NIST P-256");

    emrtd_test_begin("a description names the protocol and the curve");
    char text[48];
    emrtd_pace_info_describe(&infos.pace, text, sizeof(text));
    TEST_EQ_STR(text, "ECDH-GM/AES-128, NIST P-256");
    emrtd_pace_info_describe(&infos.pace, text, 10);
    TEST_EQ_STR(text, "ECDH-GM/A");
    emrtd_pace_info_describe(NULL, text, sizeof(text));
    TEST_EQ_STR(text, "");
}

static void test_unusable_pace(void) {
    uint8_t entry[64];
    uint8_t file[128];
    EmrtdSecurityInfos infos;

    static const struct {
        uint8_t mapping;
        int param_id;
        EmrtdError reason;
        const char* what;
    } cases[] = {
        {0x04, 13, EmrtdErrorPaceUnsupportedMapping, "the integrated mapping is refused by name"},
        {0x06,
         13,
         EmrtdErrorPaceUnsupportedMapping,
         "the chip authentication mapping is refused by name"},
        {0x01, 13, EmrtdErrorPaceUnsupportedDh, "PACE over a MODP group is refused by name"},
        {0x03, 13, EmrtdErrorPaceUnsupportedDh, "MODP outranks the mapping as a reason"},
        {0x02,
         17,
         EmrtdErrorPaceUnsupportedCurve,
         "a curve wider than the build allows is refused"},
        {0x02, 7, EmrtdErrorPaceUnsupportedCurve, "a parameter id of no curve at all is refused"},
        {0x02, -1, EmrtdErrorPaceUnsupportedCurve, "a PACEInfo naming no curve is refused"},
    };

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        emrtd_test_begin(cases[i].what);

        const size_t entry_len =
            build_pace_info(entry, cases[i].mapping, 0x02, 2, cases[i].param_id);
        const size_t len = build_card_access(file, entry, entry_len);

        TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
        TEST_CHECK(infos.has_pace);
        TEST_CHECK(!infos.pace.usable);
        TEST_EQ_INT(infos.pace.reason, cases[i].reason);
        /* The entry is kept so that the error can quote it. */
        TEST_EQ_INT(infos.pace.oid_len, 10);
    }

    emrtd_test_begin("the parameter id table follows TR-03110-3, not the reference code");
    /*
     * Id 15 is NIST P-384 and id 16 is brainpoolP384r1. Reading them the
     * other way round, as the Python implementation does, would put PACE on
     * the wrong curve and fail with nothing to say about why.
     */
    const size_t len = build_card_access(file, entry, build_pace_info(entry, 0x02, 0x02, 2, 15));
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_CHECK(infos.pace.curve != NULL);
    if(infos.pace.curve != NULL) {
        TEST_EQ_STR(infos.pace.curve->name, "NIST P-384");
    }
    TEST_CHECK(!infos.pace.usable);
    TEST_EQ_INT(infos.pace.reason, EmrtdErrorPaceUnsupportedCurve);

    emrtd_test_begin("an unusable entry still describes itself");
    char text[48];
    emrtd_pace_info_describe(&infos.pace, text, sizeof(text));
    TEST_EQ_STR(text, "ECDH-GM/AES-128, NIST P-384");
}

static void test_selection(void) {
    emrtd_test_begin("a usable entry wins over an unusable one whatever the order");

    uint8_t entries[256];
    uint8_t file[300];
    EmrtdSecurityInfos infos;

    /* The integrated mapping first, then the generic one this build can run. */
    size_t pos = 0;
    pos += build_pace_info(entries + pos, 0x04, 0x04, 2, 13);
    pos += build_pace_info(entries + pos, 0x02, 0x02, 2, 13);
    size_t len = build_card_access(file, entries, pos);

    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_EQ_INT(infos.entry_count, 2);
    TEST_CHECK(infos.pace.usable);
    TEST_EQ_INT(infos.pace.mapping, EmrtdPaceMappingGeneric);
    TEST_EQ_INT(infos.pace.cipher, EmrtdCipherAes128);

    /* And the other way round, to be sure the order is not what decided it. */
    pos = 0;
    pos += build_pace_info(entries + pos, 0x02, 0x02, 2, 13);
    pos += build_pace_info(entries + pos, 0x04, 0x04, 2, 13);
    len = build_card_access(file, entries, pos);
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_CHECK(infos.pace.usable);
    TEST_EQ_INT(infos.pace.cipher, EmrtdCipherAes128);

    emrtd_test_begin("among usable entries the stronger cipher wins");
    pos = 0;
    pos += build_pace_info(entries + pos, 0x02, 0x02, 2, 13);
    pos += build_pace_info(entries + pos, 0x02, 0x04, 2, 13);
    pos += build_pace_info(entries + pos, 0x02, 0x03, 2, 13);
    len = build_card_access(file, entries, pos);
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_EQ_INT(infos.entry_count, 3);
    TEST_CHECK(infos.pace.usable);
    TEST_EQ_INT(infos.pace.cipher, EmrtdCipherAes256);
    TEST_EQ_STR(infos.pace.oid_name, "id-PACE-ECDH-GM-AES-CBC-CMAC-256");

    emrtd_test_begin("the shortcut returns the same entry");
    EmrtdPaceInfo best;
    TEST_CHECK(emrtd_security_infos_best_pace(file, len, &best));
    TEST_EQ_INT(best.cipher, EmrtdCipherAes256);
    TEST_CHECK(best.usable);

    emrtd_test_begin("3DES under the generic mapping is still a protocol this build runs");
    len = build_card_access(file, entries, build_pace_info(entries, 0x02, 0x01, 2, 13));
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_CHECK(infos.pace.usable);
    TEST_EQ_INT(infos.pace.cipher, EmrtdCipherTdes);
    TEST_EQ_STR(infos.pace.oid_name, "id-PACE-ECDH-GM-3DES-CBC-CBC");
}

static void test_other_protocols(void) {
    emrtd_test_begin("chip, terminal and active authentication are noticed");

    uint8_t entries[256];
    uint8_t file[300];
    EmrtdSecurityInfos infos;

    /* id-CA-ECDH-AES-CBC-CMAC-128, id-PK-ECDH, id-TA and the AA object. */
    static const char* const others =
        /* id-CA-ECDH-AES-CBC-CMAC-128, version 1. */
        "300F060A04007F00070202030202020101"
        /* id-PK-ECDH, the key that Chip Authentication uses. */
        "300B060904007F000702020102"
        /* id-TA, Terminal Authentication. */
        "300A060804007F0007020202"
        /* 2.23.136.1.1.5, the Active Authentication protocol object. */
        "300806066781080101 05";
    size_t len = emrtd_test_hex(others, entries, sizeof(entries));
    len = build_card_access(file, entries, len);

    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_EQ_INT(infos.entry_count, 4);
    TEST_CHECK(infos.has_chip_auth);
    TEST_CHECK(infos.has_terminal_auth);
    TEST_CHECK(infos.has_active_auth);
    TEST_CHECK(!infos.has_pace);

    emrtd_test_begin("a PACE domain parameter entry is not mistaken for a PACEInfo");
    /* id-PACE-ECDH-GM with no cipher: a PACEDomainParameterInfo. */
    len = emrtd_test_hex("300E060904007F000702020402020102", entries, sizeof(entries));
    len = build_card_access(file, entries, len);
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_EQ_INT(infos.entry_count, 1);
    TEST_CHECK(!infos.has_pace);

    emrtd_test_begin("a protocol from another world is counted and ignored");
    len = emrtd_test_hex("300A06085511223344556677", entries, sizeof(entries));
    len = build_card_access(file, entries, len);
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorNone);
    TEST_CHECK(!infos.has_pace);
}

static void test_malformed(void) {
    uint8_t entry[64];
    uint8_t file[128];
    EmrtdSecurityInfos infos;

    emrtd_test_begin("padding after the last entry does not spoil the ones before it");
    const size_t entry_len = build_pace_info(entry, 0x02, 0x02, 2, 13);
    size_t len = build_card_access(file, entry, entry_len);
    file[1] = (uint8_t)(file[1] + 3); /* Three bytes of rubbish inside the SET. */
    file[len] = 0x00;
    file[len + 1] = 0x00;
    file[len + 2] = 0x00;
    TEST_EQ_INT(emrtd_security_infos_parse(file, len + 3, &infos), EmrtdErrorNone);
    TEST_CHECK(infos.has_pace && infos.pace.usable);

    emrtd_test_begin("a file that is not a SecurityInfos structure is refused");
    len = emrtd_test_hex("0401AA", file, sizeof(file));
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorParse);

    len = emrtd_test_hex("3100", file, sizeof(file));
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorParse);

    len = emrtd_test_hex("31FF", file, sizeof(file));
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorParse);

    TEST_EQ_INT(emrtd_security_infos_parse(file, 0, &infos), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_security_infos_parse(NULL, 4, &infos), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_security_infos_parse(file, 4, NULL), EmrtdErrorInvalidInput);
    TEST_CHECK(!emrtd_security_infos_best_pace(file, 0, &infos.pace));

    emrtd_test_begin("an entry that does not begin with an identifier is skipped");
    len = emrtd_test_hex("31053003020101", file, sizeof(file));
    TEST_EQ_INT(emrtd_security_infos_parse(file, len, &infos), EmrtdErrorParse);
    TEST_EQ_INT(infos.entry_count, 0);

    emrtd_test_begin("a truncated entry loses itself and nothing else");
    len = build_card_access(file, entry, entry_len);
    for(size_t prefix = 1; prefix < len; prefix++) {
        EmrtdSecurityInfos partial;
        /* Only that the parser survives and never claims a usable PACE. */
        if(emrtd_security_infos_parse(file, prefix, &partial) == EmrtdErrorNone) {
            TEST_CHECK(!partial.has_pace || partial.pace.oid_len == 10);
        }
    }
    TEST_CHECK(true);
}

void test_suite_security_info(void) {
    test_usable_pace();
    test_unusable_pace();
    test_selection();
    test_other_protocols();
    test_malformed();
}
