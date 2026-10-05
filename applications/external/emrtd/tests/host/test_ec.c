/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The standardized domain parameter table and the elliptic curve arithmetic
 * PACE generic mapping needs.
 *
 * This suite is the one that proves the central architectural bet: mbed TLS as
 * shipped with the Flipper firmware has only secp256r1 compiled in, yet a
 * hand populated group lets it compute on brainpoolP256r1, which is what
 * European passports use. The vectors come from ICAO Doc 9303 part 11
 * appendix G.
 */

#include "emrtd_test.h"

#include <mbedtls/bignum.h>
#include <mbedtls/ecp.h>

#include "../../crypto/emrtd_ec.h"
#include "../../crypto/emrtd_rng.h"

static void test_table(void) {
    emrtd_test_begin("the table follows TR-03110-3 table 4");

    TEST_EQ_INT(emrtd_ec_curve_count(), 11);

    const EmrtdCurve* bp256 = emrtd_ec_curve_by_param_id(13);
    TEST_CHECK(bp256 != NULL);
    TEST_EQ_STR(bp256->name, "brainpoolP256r1");
    TEST_EQ_INT(bp256->bits, 256);
    TEST_EQ_INT(bp256->size, 32);

    const EmrtdCurve* p256 = emrtd_ec_curve_by_param_id(12);
    TEST_CHECK(p256 != NULL);
    TEST_EQ_STR(p256->name, "NIST P-256");

    /* Id 15 is NIST P-384 and id 16 is brainpoolP384r1, not the other way. */
    TEST_EQ_STR(emrtd_ec_curve_by_param_id(15)->name, "NIST P-384");
    TEST_EQ_STR(emrtd_ec_curve_by_param_id(16)->name, "brainpoolP384r1");
    TEST_EQ_STR(emrtd_ec_curve_by_param_id(17)->name, "brainpoolP512r1");
    TEST_EQ_STR(emrtd_ec_curve_by_param_id(18)->name, "NIST P-521");

    /* Reserved and unassigned ids must not resolve. */
    TEST_CHECK(emrtd_ec_curve_by_param_id(0) == NULL);
    TEST_CHECK(emrtd_ec_curve_by_param_id(7) == NULL);
    TEST_CHECK(emrtd_ec_curve_by_param_id(19) == NULL);
    TEST_CHECK(emrtd_ec_curve_by_param_id(255) == NULL);
}

static void test_supported_curves_are_bounded(void) {
    emrtd_test_begin("curves above MBEDTLS_ECP_MAX_BITS are refused, not attempted");

    for(size_t i = 0; i < emrtd_ec_curve_count(); i++) {
        const EmrtdCurve* curve = emrtd_ec_curve_at(i);
        const bool supported = emrtd_ec_curve_supported(curve);
        TEST_CHECK(supported == (curve->bits <= MBEDTLS_ECP_MAX_BITS));

        mbedtls_ecp_group grp;
        const int ret = emrtd_ec_group_load(&grp, curve);
        if(supported) {
            TEST_EQ_INT(ret, 0);
            mbedtls_ecp_group_free(&grp);
        } else {
            TEST_EQ_INT(ret, MBEDTLS_ERR_ECP_BAD_INPUT_DATA);
        }
    }
}

static void test_group_arithmetic(void) {
    emrtd_test_begin("every usable curve holds its generator and its order");

    for(size_t i = 0; i < emrtd_ec_curve_count(); i++) {
        const EmrtdCurve* curve = emrtd_ec_curve_at(i);
        if(!emrtd_ec_curve_supported(curve)) {
            continue;
        }

        mbedtls_ecp_group grp;
        TEST_EQ_INT(emrtd_ec_group_load(&grp, curve), 0);
        TEST_EQ_INT(mbedtls_ecp_get_type(&grp), MBEDTLS_ECP_TYPE_SHORT_WEIERSTRASS);
        TEST_EQ_INT(grp.pbits, curve->bits);

        /* G satisfies the curve equation. */
        TEST_EQ_INT(mbedtls_ecp_check_pubkey(&grp, &grp.G), 0);

        /*
         * n * G is the point at infinity. It is computed as (n-1)*G + 1*G
         * because mbedtls_ecp_mul() rejects a scalar that is not below the
         * group order, while mbedtls_ecp_muladd() imposes no such range.
         */
        mbedtls_ecp_point r;
        mbedtls_mpi n_minus_1, one;
        mbedtls_ecp_point_init(&r);
        mbedtls_mpi_init(&n_minus_1);
        mbedtls_mpi_init(&one);
        TEST_EQ_INT(mbedtls_mpi_lset(&one, 1), 0);
        TEST_EQ_INT(mbedtls_mpi_sub_int(&n_minus_1, &grp.N, 1), 0);
        TEST_EQ_INT(mbedtls_ecp_muladd(&grp, &r, &n_minus_1, &grp.G, &one, &grp.G), 0);
        TEST_CHECK(mbedtls_ecp_is_zero(&r));
        mbedtls_mpi_free(&n_minus_1);
        mbedtls_mpi_free(&one);
        mbedtls_ecp_point_free(&r);

        mbedtls_ecp_group_free(&grp);
    }
}

/* Read a point given as two hex coordinates, through the public API. */
static int load_point(
    mbedtls_ecp_group* grp,
    mbedtls_ecp_point* pt,
    size_t size,
    const char* x_hex,
    const char* y_hex) {
    uint8_t encoded[EMRTD_EC_POINT_MAX];
    encoded[0] = 0x04;
    TEST_EQ_INT(emrtd_test_hex(x_hex, encoded + 1, size), size);
    TEST_EQ_INT(emrtd_test_hex(y_hex, encoded + 1 + size, size), size);
    return emrtd_ec_point_read(grp, pt, encoded, 1 + 2 * size);
}

/* Compare both coordinates of a point with their expected hex values. */
static void check_point(
    const mbedtls_ecp_group* grp,
    const mbedtls_ecp_point* pt,
    size_t size,
    const char* x_hex,
    const char* y_hex) {
    uint8_t encoded[EMRTD_EC_POINT_MAX];
    size_t olen = 0;
    TEST_EQ_INT(emrtd_ec_point_write(grp, pt, encoded, sizeof(encoded), &olen), 0);
    TEST_EQ_INT(olen, 1 + 2 * size);
    TEST_EQ_INT(encoded[0], 0x04);
    TEST_EQ_HEX(encoded + 1, size, x_hex);
    if(y_hex != NULL) {
        TEST_EQ_HEX(encoded + 1 + size, size, y_hex);
    }
}

static void test_pace_generic_mapping(void) {
    emrtd_test_begin("PACE generic mapping on brainpoolP256r1, appendix G.2");

    const EmrtdCurve* curve = emrtd_ec_curve_by_param_id(13);
    TEST_CHECK(emrtd_ec_curve_supported(curve));

    mbedtls_ecp_group grp;
    TEST_EQ_INT(emrtd_ec_group_load(&grp, curve), 0);

    mbedtls_mpi s, t_priv, one;
    mbedtls_ecp_point chip_pub, h, g_hat, t_pub;
    mbedtls_mpi_init(&s);
    mbedtls_mpi_init(&t_priv);
    mbedtls_mpi_init(&one);
    mbedtls_ecp_point_init(&chip_pub);
    mbedtls_ecp_point_init(&h);
    mbedtls_ecp_point_init(&g_hat);
    mbedtls_ecp_point_init(&t_pub);

    uint8_t buf[66];
    size_t n = emrtd_test_hex("3F00C4D39D153F2B2A214A078D899B22", buf, sizeof(buf));
    mbedtls_mpi_read_binary(&s, buf, n);
    n = emrtd_test_hex(
        "7F4EF07B9EA82FD78AD689B38D0BC78CF21F249D953BC46F4C6E19259C010F99", buf, sizeof(buf));
    mbedtls_mpi_read_binary(&t_priv, buf, n);
    mbedtls_mpi_lset(&one, 1);

    /* The terminal's ephemeral public key, sent in DO 81. */
    TEST_EQ_INT(mbedtls_ecp_mul(&grp, &t_pub, &t_priv, &grp.G, emrtd_random_mbedtls, NULL), 0);
    check_point(
        &grp,
        &t_pub,
        curve->size,
        "7ACF3EFC982EC45565A4B155129EFBC74650DCBFA6362D896FC70262E0C2CC5E",
        "544552DCB6725218799115B55C9BAA6D9F6BC3A9618E70C25AF71777A9C4922D");

    /* H = t_priv * chip_pub, the shared secret of the mapping exchange. */
    TEST_EQ_INT(
        load_point(
            &grp,
            &chip_pub,
            curve->size,
            "824FBA91C9CBE26BEF53A0EBE7342A3BF178CEA9F45DE0B70AA601651FBA3F57",
            "30D8C879AAA9C9F73991E61B58F4D52EB87A0A0C709A49DC63719363CCD13C54"),
        0);
    TEST_EQ_INT(mbedtls_ecp_mul(&grp, &h, &t_priv, &chip_pub, emrtd_random_mbedtls, NULL), 0);
    check_point(
        &grp,
        &h,
        curve->size,
        "60332EF2450B5D247EF6D3868397D398852ED6E8CAF6FFEEF6BF85CA57057FD5",
        "0840CA7415BAF3E43BD414D35AA4608B93A2CAF3A4E3EA4E82C9C13D03EB7181");

    /* The mapped generator G_hat = s * G + H. */
    TEST_EQ_INT(mbedtls_ecp_muladd(&grp, &g_hat, &s, &grp.G, &one, &h), 0);
    check_point(
        &grp,
        &g_hat,
        curve->size,
        "8CED63C91426D4F0EB1435E7CB1D74A46723A0AF21C89634F65A9AE87A9265E2",
        "8C879506743F8611AC33645C5B985C80B5F09A0B83407C1B6A4D857AE76FE522");
    TEST_EQ_INT(mbedtls_ecp_check_pubkey(&grp, &g_hat), 0);

    emrtd_test_begin("PACE key agreement over the mapped generator, appendix G.3");

    mbedtls_mpi t2_priv;
    mbedtls_ecp_point t2_pub, chip_pub2, shared;
    mbedtls_mpi_init(&t2_priv);
    mbedtls_ecp_point_init(&t2_pub);
    mbedtls_ecp_point_init(&chip_pub2);
    mbedtls_ecp_point_init(&shared);

    n = emrtd_test_hex(
        "A73FB703AC1436A18E0CFA5ABB3F7BEC7A070E7A6788486BEE230C4A22762595", buf, sizeof(buf));
    mbedtls_mpi_read_binary(&t2_priv, buf, n);

    TEST_EQ_INT(mbedtls_ecp_mul(&grp, &t2_pub, &t2_priv, &g_hat, emrtd_random_mbedtls, NULL), 0);
    check_point(
        &grp,
        &t2_pub,
        curve->size,
        "2DB7A64C0355044EC9DF190514C625CBA2CEA48754887122F3A5EF0D5EDD301C",
        "3556F3B3B186DF10B857B58F6A7EB80F20BA5DC7BE1D43D9BF850149FBB36462");

    TEST_EQ_INT(
        load_point(
            &grp,
            &chip_pub2,
            curve->size,
            "9E880F842905B8B3181F7AF7CAA9F0EFB743847F44A306D2D28C1D9EC65DF6DB",
            "7764B22277A2EDDC3C265A9F018F9CB852E111B768B326904B59A0193776F094"),
        0);
    TEST_EQ_INT(
        mbedtls_ecp_mul(&grp, &shared, &t2_priv, &chip_pub2, emrtd_random_mbedtls, NULL), 0);

    /* The x coordinate of the shared point is the PACE shared secret. */
    uint8_t shared_x[EMRTD_EC_COORD_MAX];
    TEST_EQ_INT(emrtd_ec_point_x(&grp, &shared, curve->size, shared_x), 0);
    TEST_EQ_HEX(
        shared_x, curve->size, "28768D20701247DAE81804C9E780EDE582A9996DB4A315020B2733197DB84925");

    mbedtls_mpi_free(&s);
    mbedtls_mpi_free(&t_priv);
    mbedtls_mpi_free(&t2_priv);
    mbedtls_mpi_free(&one);
    mbedtls_ecp_point_free(&chip_pub);
    mbedtls_ecp_point_free(&chip_pub2);
    mbedtls_ecp_point_free(&h);
    mbedtls_ecp_point_free(&g_hat);
    mbedtls_ecp_point_free(&t_pub);
    mbedtls_ecp_point_free(&t2_pub);
    mbedtls_ecp_point_free(&shared);
    mbedtls_ecp_group_free(&grp);
}

static void test_point_encoding(void) {
    emrtd_test_begin("uncompressed point encoding, TR-03111");

    const EmrtdCurve* curve = emrtd_ec_curve_by_param_id(13);
    mbedtls_ecp_group grp;
    TEST_EQ_INT(emrtd_ec_group_load(&grp, curve), 0);

    uint8_t encoded[EMRTD_EC_POINT_MAX];
    size_t olen = 0;
    TEST_EQ_INT(emrtd_ec_point_write(&grp, &grp.G, encoded, sizeof(encoded), &olen), 0);
    TEST_EQ_INT(olen, 1 + 2 * curve->size);
    TEST_EQ_INT(encoded[0], 0x04);

    mbedtls_ecp_point back;
    mbedtls_ecp_point_init(&back);
    TEST_EQ_INT(emrtd_ec_point_read(&grp, &back, encoded, olen), 0);
    TEST_EQ_INT(mbedtls_ecp_point_cmp(&back, &grp.G), 0);

    emrtd_test_begin("a point off the curve is rejected");
    encoded[olen - 1] ^= 0x01;
    TEST_CHECK(emrtd_ec_point_read(&grp, &back, encoded, olen) != 0);

    emrtd_test_begin("a compressed point is rejected");
    encoded[olen - 1] ^= 0x01;
    encoded[0] = 0x02;
    TEST_EQ_INT(
        emrtd_ec_point_read(&grp, &back, encoded, olen), MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE);

    mbedtls_ecp_point_free(&back);
    mbedtls_ecp_group_free(&grp);
}

void test_suite_ec(void) {
    test_table();
    test_supported_curves_are_bounded();
    test_group_arithmetic();
    test_pace_generic_mapping();
    test_point_encoding();
}
