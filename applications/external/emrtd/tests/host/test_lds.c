/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The Logical Data Structure parsers.
 *
 * EF.COM and the data groups are exercised with the examples of ICAO Doc 9303
 * and with hand built files that put one element at a time under stress. EF.SOD
 * is different: a CMS SignedData with a certificate in it cannot be summarised
 * into a hand built vector without losing exactly the structure the walk has to
 * survive, so the security object below is a real one, taken from a synthetic
 * passport for the ICAO specimen holder, certificate and signature included.
 */

#include "emrtd_test.h"

#include <stdlib.h>

#include "../../protocol/emrtd_lds.h"
#include "../../protocol/emrtd_tlv.h"

/*
 * EF.SOD of a synthetic passport: CMS SignedData over an LDSSecurityObject
 * that lists SHA-256 hashes for DG1 and DG2, signed with RSA and carrying its
 * own self signed document signer certificate. 1426 bytes.
 */
static const char* const sod_hex =
    "7782058E3082058A06092A864886F70D010702A082057B30820577020103310F"
    "300D0609608648016503040201050030700606678108010101A0660464306202"
    "0100300D06096086480165030402010500304E302502010104202BCEB0D31C89"
    "D6C4DB69764D7FCEAA21683682133C203F03DE90C010BE9F1EBC302502010204"
    "209BD2DC020C31EEE6ADA6251249F7CE740CA5342C6592288F4C75D86FE34B5D"
    "D1A082031830820314308201FCA0030201020214193AB3A4C1C89CE03DAAFDD8"
    "64168D2AF75319C8300D06092A864886F70D01010B05003044310B3009060355"
    "040613025554311D301B060355040A0C145465737420446F63756D656E742053"
    "69676E65723116301406035504030C0D53796E74686574696320445343301E17"
    "0D3236303931393133343130325A170D3336303931373133343130325A304431"
    "0B3009060355040613025554311D301B060355040A0C145465737420446F6375"
    "6D656E74205369676E65723116301406035504030C0D53796E74686574696320"
    "44534330820122300D06092A864886F70D01010105000382010F003082010A02"
    "82010100C21983FE78108C05749174B84D86F7326BB332BBB6087704B38BF15C"
    "8966F1A01FADEC9293424ACABA0272615DDE8D3CCEC0CE3FF7AE0D26B28AC37C"
    "889F152F47CD45C1571CFFAC1FEE634CDF2936B6C9BA7227A2415367B3B98582"
    "435C67E1A6484F1E29C512B1494329430536FDF0D14CB8B6B7CE74CD2C670EBC"
    "8D2D73F203CF3222C9DBD37191D2228931379D9409886FCCEA63E3FD09456FDA"
    "0AF410450114FF67484174D293E8CAA7917175930C478ED5FDB37F6E91D41F75"
    "62F2A0274E0ABEE1E41353CC3885FB51CEDA85B2E2B4FCE1C96EBE110D9069B4"
    "33A5EC759A4C28718A52D865CA4D052E58BD578631AE6A9153E6C8C5B490F620"
    "4A1A01CD0203010001300D06092A864886F70D01010B050003820101005C7006"
    "3750559E06B2232BA64F4872FB99DC01404C19C97E3BE60C87C8281838C889FC"
    "1B0ED2B46C615D84D5B83089FCCB12954C2C7E3B4E7447EC88C16A75AEE6383A"
    "393D46BF16673B27E9DBA8D5CE151023DF1EFF23EE9894ACFD42118757163C84"
    "81419B4731BD67719A08B6824C7CE5DDF5496FA1735E67DBE7FFD8FB34770593"
    "2E781E5E70752D593C75D2B4951615D557595E01A42B1727AEF9EDFF825AD820"
    "E558621FD8395594FCE458FFEABDD906DC4306F17551F6296750271F8979DEB4"
    "373DFE1028D5D9576BAC97D5B5A16F23B0584D47118555B8A9E55B1666AD0AA0"
    "583F2EDF5E9B8AD3F256503CA5360457C23018C113ADA44271DBDBF76C318201"
    "D1308201CD020101305C3044310B3009060355040613025554311D301B060355"
    "040A0C145465737420446F63756D656E74205369676E65723116301406035504"
    "030C0D53796E746865746963204453430214193AB3A4C1C89CE03DAAFDD86416"
    "8D2AF75319C8300D06096086480165030402010500A048301506092A864886F7"
    "0D01090331080606678108010101302F06092A864886F70D0109043122042082"
    "D98BE5F8C8A9F5B5A0050F5A75049ED2B132C5C9A1302FB6E5093DAC270C0A30"
    "0D06092A864886F70D0101010500048201002672C32541C21F30E8A855E4C851"
    "D8EE0AB9A65406FB26C58F33890FDCFCBB8E5545EF8ACD76D2D5998B50FEB84C"
    "9848F7911E064982E920ECEBEED44CCFE29B07017DB0215022358434DDE2CA3E"
    "1EB65F30AE837F1DA349A5B1E9148EFB0048CB93619A14871080B14A94B140DE"
    "9041D651A9B41DCF099E6C1807B674A836FD150B75E6965AFCFA0E3BF5D6A714"
    "FE48AC6CC734C2370E9D97EDE00055666308201406496A11B877BEBEFA82E864"
    "54D979399DA5FAF8093E80CDB283F66947BD6890F2586B379781AC87061BA44B"
    "7915D742E41CF92E55CE0DC5F313BA3C0D85E8554542F8F92478AC7109772E9C"
    "75E77B31B04B65635F670FC8FD629265B807";

/* The first 128 bytes of a DG2, down into the JPEG itself. */
static const char* const dg2_prefix_hex =
    "758202BC7F618202B70201017F608202AFA1005F2E8202A84641430030313000"
    "0000029E00010000029000000000000000000000000000000000000000000000"
    "00000000FFD8FFE000104A46494600010100000100010000FFDB004300080606"
    "070605080707070909080A0C140D0C0B0B0C1912130F141D1A1F1E1D1A1C1C20";

/** Build one TLV node into @p out. Returns the number of bytes written. */
static size_t
    tlv_build(uint8_t* out, size_t out_size, uint32_t tag, const uint8_t* value, size_t len) {
    uint8_t header[8];
    size_t pos = 0;

    if(tag > 0xFFFF) {
        header[pos++] = (uint8_t)(tag >> 16);
    }
    if(tag > 0xFF) {
        header[pos++] = (uint8_t)(tag >> 8);
    }
    header[pos++] = (uint8_t)tag;

    if(len < 0x80) {
        header[pos++] = (uint8_t)len;
    } else if(len <= 0xFF) {
        header[pos++] = 0x81;
        header[pos++] = (uint8_t)len;
    } else {
        header[pos++] = 0x82;
        header[pos++] = (uint8_t)(len >> 8);
        header[pos++] = (uint8_t)len;
    }

    if(pos + len > out_size) {
        return 0;
    }
    memcpy(out, header, pos);
    if(len > 0) {
        memcpy(out + pos, value, len);
    }
    return pos + len;
}

/* --- EF.COM ------------------------------------------------------------ */

static void test_ef_com(void) {
    emrtd_test_begin("the EF.COM example of ICAO 9303-11 appendix D.4");

    uint8_t data[64];
    size_t len =
        emrtd_test_hex("60145F0104303130365F36063034303030305C026175", data, sizeof(data));

    EmrtdEfCom com;
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, &com), EmrtdErrorNone);
    TEST_EQ_STR(com.lds_version, "1.6");
    TEST_EQ_STR(com.unicode_version, "4.0.0");
    TEST_EQ_INT(com.tag_count, 2);
    TEST_CHECK((com.present & EMRTD_FILE_BIT(EmrtdFileDg1)) != 0);
    TEST_CHECK((com.present & EMRTD_FILE_BIT(EmrtdFileDg2)) != 0);
    TEST_CHECK((com.present & EMRTD_FILE_BIT(EmrtdFileDg3)) == 0);
    TEST_CHECK((com.present & EMRTD_FILE_BIT(EmrtdFileCom)) == 0);

    emrtd_test_begin("a later LDS version reads as itself");
    len = emrtd_test_hex("60145F0104303130375F36063034303030305C026175", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, &com), EmrtdErrorNone);
    TEST_EQ_STR(com.lds_version, "1.7");

    emrtd_test_begin("a chip announcing every group is followed");
    len = emrtd_test_hex(
        "60195F0104303130375C10617563766566676869 6A6B6C6D6E6F70", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, &com), EmrtdErrorNone);
    TEST_EQ_INT(com.tag_count, 16);
    for(int number = 1; number <= 16; number++) {
        const EmrtdFileInfo* info = emrtd_file_by_dg_number(number);
        TEST_CHECK(info != NULL && (com.present & EMRTD_FILE_BIT(info->id)) != 0);
    }

    emrtd_test_begin("a tag no reader knows is counted but cannot be selected");
    len = emrtd_test_hex("600B5F0104303130375C026 1FF", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, &com), EmrtdErrorNone);
    TEST_EQ_INT(com.tag_count, 2);
    TEST_CHECK((com.present & EMRTD_FILE_BIT(EmrtdFileDg1)) != 0);
    TEST_EQ_INT(com.present, EMRTD_FILE_BIT(EmrtdFileDg1));

    emrtd_test_begin("EF.COM under the wrong template is refused");
    len = emrtd_test_hex("610400000000", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, &com), EmrtdErrorParse);

    emrtd_test_begin("a truncated EF.COM is refused rather than half read");
    len = emrtd_test_hex("60145F0104303130365F3606", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, &com), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_lds_parse_com(NULL, 4, &com), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, NULL), EmrtdErrorInvalidInput);

    emrtd_test_begin("an EF.COM with no elements at all still parses");
    len = emrtd_test_hex("6000", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_com(data, len, &com), EmrtdErrorNone);
    TEST_EQ_STR(com.lds_version, "");
    TEST_EQ_INT(com.present, 0);
    TEST_EQ_INT(com.tag_count, 0);
}

/* --- DG1 --------------------------------------------------------------- */

static void test_dg1(void) {
    emrtd_test_begin("DG1 holds the zone of the ICAO TD3 specimen");

    static const char* const mrz = "P<UTOERIKSSON<<ANNA<MARIA<<<<<<<<<<<<<<<<<<<"
                                   "L898902C36UTO7408122F1204159ZE184226B<<<<<10";

    uint8_t inner[128];
    uint8_t file[160];
    size_t inner_len = tlv_build(inner, sizeof(inner), 0x5F1F, (const uint8_t*)mrz, 88);
    TEST_EQ_INT(inner_len, 91);
    size_t len = tlv_build(file, sizeof(file), 0x61, inner, inner_len);
    TEST_EQ_INT(len, 93);

    EmrtdMrz parsed;
    TEST_EQ_INT(emrtd_lds_parse_dg1(file, len, &parsed), EmrtdErrorNone);
    TEST_EQ_INT(parsed.format, EmrtdMrzFormatTd3);
    TEST_EQ_STR(parsed.document_number, "L898902C3");
    TEST_EQ_STR(parsed.surname, "ERIKSSON");
    TEST_EQ_STR(parsed.given_names, "ANNA MARIA");
    TEST_CHECK(parsed.check_digits_valid);

    emrtd_test_begin("a zone whose digits do not add up still reads its fields");
    uint8_t broken[160];
    char damaged[89];
    memcpy(damaged, mrz, 88);
    damaged[87] = (char)(damaged[87] == '0' ? '1' : '0');
    inner_len = tlv_build(inner, sizeof(inner), 0x5F1F, (const uint8_t*)damaged, 88);
    len = tlv_build(broken, sizeof(broken), 0x61, inner, inner_len);
    TEST_EQ_INT(emrtd_lds_parse_dg1(broken, len, &parsed), EmrtdErrorNone);
    TEST_EQ_STR(parsed.document_number, "L898902C3");
    TEST_CHECK(!parsed.check_digits_valid);

    emrtd_test_begin("DG1 without its MRZ object is refused");
    uint8_t empty[8];
    len = tlv_build(empty, sizeof(empty), 0x61, NULL, 0);
    TEST_EQ_INT(emrtd_lds_parse_dg1(empty, len, &parsed), EmrtdErrorParse);

    emrtd_test_begin("DG1 under the wrong template is refused");
    len = tlv_build(broken, sizeof(broken), 0x75, inner, inner_len);
    TEST_EQ_INT(emrtd_lds_parse_dg1(broken, len, &parsed), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_lds_parse_dg1(NULL, 4, &parsed), EmrtdErrorInvalidInput);
}

/* --- DG2 --------------------------------------------------------------- */

static void test_dg2(void) {
    emrtd_test_begin("the facial image is found inside a real DG2");

    uint8_t data[128];
    const size_t len = emrtd_test_hex(dg2_prefix_hex, data, sizeof(data));
    TEST_EQ_INT(len, 128);

    EmrtdFaceImage image;
    TEST_CHECK(emrtd_lds_dg2_find_image(data, len, &image));
    TEST_EQ_INT(image.format, EmrtdImageJpeg);
    TEST_EQ_INT(image.offset, 68);
    TEST_EQ_STR(image.suffix, ".jpg");
    TEST_EQ_HEX(data + image.offset, 3, "FFD8FF");

    emrtd_test_begin("JPEG 2000 in either of its two wrappings is recognised");
    uint8_t jp2[32];
    size_t n = emrtd_test_hex("5F2E10 0000000C6A5020200D0A870A AABBCCDD", jp2, sizeof(jp2));
    TEST_CHECK(emrtd_lds_dg2_find_image(jp2, n, &image));
    TEST_EQ_INT(image.format, EmrtdImageJpeg2000);
    TEST_EQ_INT(image.offset, 3);
    TEST_EQ_STR(image.suffix, ".jp2");

    n = emrtd_test_hex("5F2E08 FF4FFF51 AABBCCDD", jp2, sizeof(jp2));
    TEST_CHECK(emrtd_lds_dg2_find_image(jp2, n, &image));
    TEST_EQ_INT(image.format, EmrtdImageJpeg2000);
    TEST_EQ_INT(image.offset, 3);

    emrtd_test_begin("a prefix that stops short of the image asks for more");
    /* Cut two bytes into the JPEG marker: there is no image in range yet. */
    TEST_CHECK(!emrtd_lds_dg2_find_image(data, 70, &image));
    TEST_EQ_INT(image.format, EmrtdImageUnknown);
    TEST_EQ_INT(image.offset, 0);
    TEST_CHECK(emrtd_lds_dg2_find_image(data, 71, &image));
    TEST_EQ_INT(image.offset, 68);

    emrtd_test_begin("a header with no image at all is reported as such");
    TEST_CHECK(!emrtd_lds_dg2_find_image(data, 16, &image));
    TEST_CHECK(!emrtd_lds_dg2_find_image(data, 0, &image));
    TEST_CHECK(!emrtd_lds_dg2_find_image(NULL, 16, &image));
    TEST_CHECK(!emrtd_lds_dg2_find_image(data, 16, NULL));

    emrtd_test_begin("the search never reads past the buffer it was given");
    /* An exact sized block ending one byte into a signature. */
    uint8_t* exact = malloc(4);
    TEST_CHECK(exact != NULL);
    if(exact != NULL) {
        exact[0] = 0x00;
        exact[1] = 0x00;
        exact[2] = 0xFF;
        exact[3] = 0xD8;
        TEST_CHECK(!emrtd_lds_dg2_find_image(exact, 4, &image));
        free(exact);
    }
}

/* --- DG11 and DG12 ------------------------------------------------------ */

static void test_dg11(void) {
    emrtd_test_begin("DG11 reads the additional personal details");

    uint8_t body[256];
    uint8_t file[300];
    size_t pos = 0;

    static const char* const name = "ERIKSSON<<ANNA<MARIA";
    static const char* const number = "AB1234567<<<";
    static const char* const birth = "19740812";
    static const char* const place = "UTOPIA<CAPITAL CITY";
    static const char* const address = "MAIN STREET 1<CAPITAL<UTO";
    static const char* const phone = "+420123456789";
    static const char* const job = "ENGINEER";

    pos += tlv_build(body + pos, sizeof(body) - pos, 0x5F0E, (const uint8_t*)name, strlen(name));
    pos +=
        tlv_build(body + pos, sizeof(body) - pos, 0x5F10, (const uint8_t*)number, strlen(number));
    pos += tlv_build(body + pos, sizeof(body) - pos, 0x5F2B, (const uint8_t*)birth, strlen(birth));
    pos += tlv_build(body + pos, sizeof(body) - pos, 0x5F11, (const uint8_t*)place, strlen(place));
    pos += tlv_build(
        body + pos, sizeof(body) - pos, 0x5F42, (const uint8_t*)address, strlen(address));
    pos += tlv_build(body + pos, sizeof(body) - pos, 0x5F12, (const uint8_t*)phone, strlen(phone));
    pos += tlv_build(body + pos, sizeof(body) - pos, 0x5F13, (const uint8_t*)job, strlen(job));

    const size_t len = tlv_build(file, sizeof(file), 0x6B, body, pos);
    TEST_CHECK(len > 0);

    EmrtdDg11 dg11;
    TEST_EQ_INT(emrtd_lds_parse_dg11(file, len, &dg11), EmrtdErrorNone);
    TEST_CHECK(dg11.any);
    TEST_EQ_STR(dg11.full_name, "ERIKSSON, ANNA MARIA");
    TEST_EQ_STR(dg11.personal_number, "AB1234567");
    TEST_EQ_STR(dg11.date_of_birth, "12.08.1974");
    TEST_EQ_STR(dg11.place_of_birth, "UTOPIA, CAPITAL CITY");
    TEST_EQ_STR(dg11.address, "MAIN STREET 1, CAPITAL, UTO");
    TEST_EQ_STR(dg11.telephone, "+420123456789");
    TEST_EQ_STR(dg11.profession, "ENGINEER");

    emrtd_test_begin("a date packed as BCD reads the same as one in digits");
    pos = tlv_build(body, sizeof(body), 0x5F2B, (const uint8_t*)"\x19\x74\x08\x12", 4);
    const size_t bcd_len = tlv_build(file, sizeof(file), 0x6B, body, pos);
    TEST_EQ_INT(emrtd_lds_parse_dg11(file, bcd_len, &dg11), EmrtdErrorNone);
    TEST_EQ_STR(dg11.date_of_birth, "12.08.1974");

    emrtd_test_begin("an empty DG11 reports that it holds nothing");
    const size_t empty_len = tlv_build(file, sizeof(file), 0x6B, NULL, 0);
    TEST_EQ_INT(emrtd_lds_parse_dg11(file, empty_len, &dg11), EmrtdErrorNone);
    TEST_CHECK(!dg11.any);
    TEST_EQ_STR(dg11.full_name, "");

    emrtd_test_begin("a field longer than its buffer is cut, not overflowed");
    uint8_t long_name[200];
    memset(long_name, 'A', sizeof(long_name));
    pos = tlv_build(body, sizeof(body), 0x5F0E, long_name, sizeof(long_name));
    const size_t long_len = tlv_build(file, sizeof(file), 0x6B, body, pos);
    TEST_EQ_INT(emrtd_lds_parse_dg11(file, long_len, &dg11), EmrtdErrorNone);
    TEST_EQ_INT(strlen(dg11.full_name), sizeof(dg11.full_name) - 1);

    emrtd_test_begin("DG11 under the wrong template is refused");
    const size_t wrong = tlv_build(file, sizeof(file), 0x6C, body, pos);
    TEST_EQ_INT(emrtd_lds_parse_dg11(file, wrong, &dg11), EmrtdErrorParse);
}

static void test_dg12(void) {
    emrtd_test_begin("DG12 reads the additional document details");

    uint8_t body[128];
    uint8_t file[160];
    size_t pos = 0;

    static const char* const authority = "MINISTRY OF THE INTERIOR";
    static const char* const issue = "20200101";
    static const char* const notes = "SEE PAGE 5";

    pos += tlv_build(
        body + pos, sizeof(body) - pos, 0x5F19, (const uint8_t*)authority, strlen(authority));
    pos += tlv_build(body + pos, sizeof(body) - pos, 0x5F26, (const uint8_t*)issue, strlen(issue));
    pos += tlv_build(body + pos, sizeof(body) - pos, 0x5F1B, (const uint8_t*)notes, strlen(notes));

    const size_t len = tlv_build(file, sizeof(file), 0x6C, body, pos);

    EmrtdDg12 dg12;
    TEST_EQ_INT(emrtd_lds_parse_dg12(file, len, &dg12), EmrtdErrorNone);
    TEST_CHECK(dg12.any);
    TEST_EQ_STR(dg12.issuing_authority, "MINISTRY OF THE INTERIOR");
    TEST_EQ_STR(dg12.date_of_issue, "01.01.2020");
    TEST_EQ_STR(dg12.endorsements, "SEE PAGE 5");

    emrtd_test_begin("DG12 under the wrong template is refused");
    const size_t wrong = tlv_build(file, sizeof(file), 0x6B, body, pos);
    TEST_EQ_INT(emrtd_lds_parse_dg12(file, wrong, &dg12), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_lds_parse_dg12(NULL, 4, &dg12), EmrtdErrorInvalidInput);
}

/* --- DG15 --------------------------------------------------------------- */

static void test_dg15(void) {
    emrtd_test_begin("an RSA Active Authentication key is measured");

    /*
     * SubjectPublicKeyInfo with a 2048 bit modulus: the algorithm identifier
     * of rsaEncryption, then the key itself as a BIT STRING.
     */
    uint8_t modulus[257];
    uint8_t rsa_key[512];
    uint8_t spki_body[512];
    uint8_t spki[600];
    uint8_t file[640];

    modulus[0] = 0x00; /* DER sign byte in front of a top bit that is set. */
    memset(modulus + 1, 0xC4, sizeof(modulus) - 1);

    size_t pos = 0;
    pos += tlv_build(rsa_key + pos, sizeof(rsa_key) - pos, 0x02, modulus, sizeof(modulus));
    pos +=
        tlv_build(rsa_key + pos, sizeof(rsa_key) - pos, 0x02, (const uint8_t*)"\x01\x00\x01", 3);

    uint8_t key_sequence[512];
    size_t key_len = tlv_build(key_sequence, sizeof(key_sequence), 0x30, rsa_key, pos);

    uint8_t bit_string[520];
    bit_string[0] = 0x00; /* No unused bits. */
    memcpy(bit_string + 1, key_sequence, key_len);

    uint8_t algorithm[32];
    size_t algorithm_len =
        emrtd_test_hex("300D06092A864886F70D0101010500", algorithm, sizeof(algorithm));

    pos = 0;
    memcpy(spki_body + pos, algorithm, algorithm_len);
    pos += algorithm_len;
    pos += tlv_build(spki_body + pos, sizeof(spki_body) - pos, 0x03, bit_string, key_len + 1);

    size_t spki_len = tlv_build(spki, sizeof(spki), 0x30, spki_body, pos);
    size_t len = tlv_build(file, sizeof(file), 0x6F, spki, spki_len);

    EmrtdDg15 dg15;
    TEST_EQ_INT(emrtd_lds_parse_dg15(file, len, &dg15), EmrtdErrorNone);
    TEST_EQ_STR(dg15.algorithm, "RSA");
    TEST_EQ_INT(dg15.key_bits, 2048);

    emrtd_test_begin("an elliptic curve key is measured from its point");
    uint8_t point[65];
    point[0] = 0x04;
    memset(point + 1, 0x11, sizeof(point) - 1);
    bit_string[0] = 0x00;
    memcpy(bit_string + 1, point, sizeof(point));

    algorithm_len =
        emrtd_test_hex("301306072A8648CE3D020106082A8648CE3D030107", algorithm, sizeof(algorithm));
    pos = 0;
    memcpy(spki_body + pos, algorithm, algorithm_len);
    pos += algorithm_len;
    pos +=
        tlv_build(spki_body + pos, sizeof(spki_body) - pos, 0x03, bit_string, sizeof(point) + 1);
    spki_len = tlv_build(spki, sizeof(spki), 0x30, spki_body, pos);
    len = tlv_build(file, sizeof(file), 0x6F, spki, spki_len);

    TEST_EQ_INT(emrtd_lds_parse_dg15(file, len, &dg15), EmrtdErrorNone);
    TEST_EQ_STR(dg15.algorithm, "EC");
    TEST_EQ_INT(dg15.key_bits, 256);

    emrtd_test_begin("a key this reader cannot name is still reported");
    algorithm_len = emrtd_test_hex("300B06092A864886F70D010199", algorithm, sizeof(algorithm));
    pos = 0;
    memcpy(spki_body + pos, algorithm, algorithm_len);
    pos += algorithm_len;
    pos +=
        tlv_build(spki_body + pos, sizeof(spki_body) - pos, 0x03, bit_string, sizeof(point) + 1);
    spki_len = tlv_build(spki, sizeof(spki), 0x30, spki_body, pos);
    len = tlv_build(file, sizeof(file), 0x6F, spki, spki_len);
    TEST_EQ_INT(emrtd_lds_parse_dg15(file, len, &dg15), EmrtdErrorNone);
    TEST_EQ_STR(dg15.algorithm, "unknown");
    TEST_EQ_INT(dg15.key_bits, 0);

    emrtd_test_begin("rubbish under the DG15 template is refused");
    len = emrtd_test_hex("6F04FFFFFFFF", file, sizeof(file));
    TEST_EQ_INT(emrtd_lds_parse_dg15(file, len, &dg15), EmrtdErrorParse);
    len = emrtd_test_hex("6F00", file, sizeof(file));
    TEST_EQ_INT(emrtd_lds_parse_dg15(file, len, &dg15), EmrtdErrorParse);
    TEST_EQ_INT(emrtd_lds_parse_dg15(NULL, 4, &dg15), EmrtdErrorInvalidInput);
}

/* --- EF.SOD ------------------------------------------------------------ */

static void test_sod(void) {
    emrtd_test_begin("a real security object is walked to its data group hashes");

    uint8_t* data = malloc(1500);
    TEST_CHECK(data != NULL);
    if(data == NULL) {
        return;
    }
    const size_t len = emrtd_test_hex(sod_hex, data, 1500);
    TEST_EQ_INT(len, 1426);
    /* The file announces its own length, which is how the worker sizes it. */
    TEST_EQ_INT(emrtd_tlv_total_length(data, 4), 1426);

    EmrtdEfSod sod;
    TEST_EQ_INT(emrtd_lds_parse_sod(data, len, &sod), EmrtdErrorNone);
    TEST_EQ_STR(sod.digest_algorithm, "SHA-256");
    TEST_EQ_INT(sod.digest_len, 32);
    TEST_CHECK(sod.digest_supported);
    TEST_EQ_INT(sod.hash_count, 2);
    TEST_EQ_STR(sod.signer_algorithm, "RSA PKCS#1");
    TEST_CHECK(sod.has_certificate);

    const EmrtdSodHash* dg1 = emrtd_lds_sod_hash_for(&sod, 1);
    TEST_CHECK(dg1 != NULL);
    if(dg1 != NULL) {
        TEST_EQ_INT(dg1->hash_len, 32);
        TEST_EQ_HEX(
            dg1->hash,
            dg1->hash_len,
            "2BCEB0D31C89D6C4DB69764D7FCEAA21683682133C203F03DE90C010BE9F1EBC");
    }

    const EmrtdSodHash* dg2 = emrtd_lds_sod_hash_for(&sod, 2);
    TEST_CHECK(dg2 != NULL);
    if(dg2 != NULL) {
        TEST_EQ_HEX(
            dg2->hash,
            dg2->hash_len,
            "9BD2DC020C31EEE6ADA6251249F7CE740CA5342C6592288F4C75D86FE34B5DD1");
    }

    emrtd_test_begin("a group the object does not list has no hash");
    TEST_CHECK(emrtd_lds_sod_hash_for(&sod, 3) == NULL);
    TEST_CHECK(emrtd_lds_sod_hash_for(&sod, 0) == NULL);
    TEST_CHECK(emrtd_lds_sod_hash_for(&sod, 17) == NULL);
    TEST_CHECK(emrtd_lds_sod_hash_for(NULL, 1) == NULL);

    emrtd_test_begin("the same object without its outer template parses too");
    EmrtdEfSod bare;
    TEST_EQ_INT(emrtd_lds_parse_sod(data + 4, len - 4, &bare), EmrtdErrorNone);
    TEST_EQ_INT(bare.hash_count, 2);

    emrtd_test_begin("a truncated security object is refused at every length");
    /*
     * Every prefix of the file is fed back in. None may be accepted and none
     * may read past the block, which is what the sanitizer is watching for.
     */
    for(size_t prefix = 1; prefix < len; prefix += 7) {
        uint8_t* block = malloc(prefix);
        if(block == NULL) {
            continue;
        }
        memcpy(block, data, prefix);
        EmrtdEfSod partial;
        const EmrtdError error = emrtd_lds_parse_sod(block, prefix, &partial);
        if(error == EmrtdErrorNone) {
            emrtd_test_fail(__FILE__, __LINE__, "a %zu byte prefix was accepted", prefix);
        }
        emrtd_test_checks++;
        free(block);
    }

    emrtd_test_begin("flipping a byte anywhere in the header never reads out of bounds");
    for(size_t offset = 0; offset < 40; offset++) {
        uint8_t* damaged = malloc(len);
        if(damaged == NULL) {
            continue;
        }
        memcpy(damaged, data, len);
        damaged[offset] ^= 0xFF;
        EmrtdEfSod broken;
        emrtd_lds_parse_sod(damaged, len, &broken);
        free(damaged);
    }
    emrtd_test_checks++;

    free(data);
}

static void test_sod_refusals(void) {
    emrtd_test_begin("something that is not a security object is refused");

    uint8_t data[64];
    EmrtdEfSod sod;

    size_t len = emrtd_test_hex("7702AABB", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_sod(data, len, &sod), EmrtdErrorParse);

    len = emrtd_test_hex("3003020101", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_sod(data, len, &sod), EmrtdErrorParse);

    /* The right shape but the wrong content type. */
    len = emrtd_test_hex("300D06092A864886F70D010701A000", data, sizeof(data));
    TEST_EQ_INT(emrtd_lds_parse_sod(data, len, &sod), EmrtdErrorParse);

    TEST_EQ_INT(emrtd_lds_parse_sod(NULL, 4, &sod), EmrtdErrorInvalidInput);
    TEST_EQ_INT(emrtd_lds_parse_sod(data, len, NULL), EmrtdErrorInvalidInput);

    emrtd_test_begin("an empty file is refused");
    TEST_EQ_INT(emrtd_lds_parse_sod(data, 0, &sod), EmrtdErrorParse);
}

void test_suite_lds(void) {
    test_ef_com();
    test_dg1();
    test_dg2();
    test_dg11();
    test_dg12();
    test_dg15();
    test_sod();
    test_sod_refusals();
}
