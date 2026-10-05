/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Standardized elliptic curve domain parameters for PACE and Chip
 * Authentication, addressed by the parameter id carried in PACEInfo.
 *
 * The table follows BSI TR-03110 part 3, table 4, referenced by ICAO Doc 9303
 * part 11 Appendix A. European passports overwhelmingly use id 13
 * (brainpoolP256r1); id 12 (NIST P-256) appears as well.
 *
 * mbed TLS is shipped with the Flipper firmware with only secp256r1 compiled
 * in, so the brainpool groups are not available through
 * mbedtls_ecp_group_load(). They are instead populated by hand, which mbed TLS
 * supports for short Weierstrass curves: with grp->modp left NULL the generic
 * modular reduction is used. The build time limit MBEDTLS_ECP_MAX_BITS sizes
 * internal buffers in ecp_mul_comb(), so any curve above it is rejected rather
 * than silently overflowing them.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <mbedtls/ecp.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Longest coordinate any listed curve uses (NIST P-521). */
#define EMRTD_EC_COORD_MAX 66
/** Buffer size for an uncompressed point of any listed curve. */
#define EMRTD_EC_POINT_MAX (1 + 2 * EMRTD_EC_COORD_MAX)

/** One entry of the standardized domain parameter table. */
typedef struct {
    uint8_t param_id; /**< Id as encoded in PACEInfo (TR-03110-3 table 4). */
    const char* name; /**< Curve name for the UI and the export report. */
    uint16_t bits; /**< Bit length of the prime p. */
    uint16_t size; /**< Byte length of one coordinate. */
    const uint8_t* p; /**< Field prime, big endian, @c size bytes. */
    const uint8_t* a; /**< Curve coefficient a. */
    const uint8_t* b; /**< Curve coefficient b. */
    const uint8_t* gx; /**< Generator x. */
    const uint8_t* gy; /**< Generator y. */
    const uint8_t* n; /**< Group order. */
} EmrtdCurve;

/** Look up a curve by its PACEInfo parameter id, or NULL if unknown. */
const EmrtdCurve* emrtd_ec_curve_by_param_id(uint8_t param_id);

/**
 * Whether this build can actually compute on @p curve.
 *
 * Curves are listed even when unusable so that the reader can name the one it
 * met instead of reporting a bare failure.
 */
bool emrtd_ec_curve_supported(const EmrtdCurve* curve);

/**
 * Populate @p grp with the domain parameters of @p curve.
 *
 * The group must be released with mbedtls_ecp_group_free(), which frees the
 * parameters because the cofactor field is left at zero - mbed TLS uses it to
 * tell heap allocated groups from its own static ones.
 *
 * @return 0 on success, an mbed TLS error code otherwise
 */
int emrtd_ec_group_load(mbedtls_ecp_group* grp, const EmrtdCurve* curve);

/**
 * Read an uncompressed point and verify that it lies on the curve.
 *
 * Accepting a point off the curve during PACE would hand an attacker a small
 * subgroup, so this is checked before the point is ever multiplied.
 */
int emrtd_ec_point_read(
    mbedtls_ecp_group* grp,
    mbedtls_ecp_point* point,
    const uint8_t* data,
    size_t len);

/** Write a point in the uncompressed encoding 04 || X || Y (TR-03111). */
int emrtd_ec_point_write(
    const mbedtls_ecp_group* grp,
    const mbedtls_ecp_point* point,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);

/** Copy just the x coordinate, which is the PACE shared secret. */
int emrtd_ec_point_x(
    const mbedtls_ecp_group* grp,
    const mbedtls_ecp_point* point,
    size_t coord_size,
    uint8_t* out);

/** Number of entries in the table, for the self test. */
size_t emrtd_ec_curve_count(void);

/** Table entry by index, for the self test. */
const EmrtdCurve* emrtd_ec_curve_at(size_t index);

#ifdef __cplusplus
}
#endif
