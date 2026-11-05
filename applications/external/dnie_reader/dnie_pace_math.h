#pragma once

// Matematica de PACE ECDH-GM separada de la radio, para poder probarla en el PC
// contra el ejemplo de ICAO 9303-11 (apendice G.1).

#include <stdbool.h>
#include "dnie_ec.h"

#define DNIE_PACE_OID_LEN   10 // 04 00 7F 00 07 02 02 04 <mapping> <cipher>
#define DNIE_PACE_TOKEN_LEN 8

// pk = sk * base. Si gen_sk, genera antes un sk aleatorio en [1, n-1].
int dnie_pace_keypair(
    mbedtls_ecp_group* grp,
    const mbedtls_ecp_point* base,
    mbedtls_mpi* sk,
    mbedtls_ecp_point* pk,
    bool gen_sk,
    DnieRng f_rng,
    void* p_rng);

// Mapeo generico: H = sk_map * PK_map_chip ;  G~ = s*G + H
int dnie_pace_map_gm(
    mbedtls_ecp_group* grp,
    const uint8_t* s,
    size_t s_len,
    const mbedtls_mpi* sk_map,
    const mbedtls_ecp_point* pk_map_chip,
    mbedtls_ecp_point* g_tilde,
    DnieRng f_rng,
    void* p_rng);

// K = coordenada X de sk * pk_other
int dnie_pace_shared_x(
    mbedtls_ecp_group* grp,
    const mbedtls_mpi* sk,
    const mbedtls_ecp_point* pk_other,
    uint8_t out[DNIE_EC_COORD_LEN],
    DnieRng f_rng,
    void* p_rng);

// KSenc = KDF(K,1), KSmac = KDF(K,2)
bool dnie_pace_derive_keys(
    const uint8_t k[DNIE_EC_COORD_LEN],
    uint8_t ks_enc[16],
    uint8_t ks_mac[16]);

// T = CMAC(KSmac, 7F49 { 06 OID, 86 PK })[0..8]
bool dnie_pace_token(
    const uint8_t ks_mac[16],
    const uint8_t oid[DNIE_PACE_OID_LEN],
    const uint8_t pk[DNIE_EC_POINT_LEN],
    uint8_t out[DNIE_PACE_TOKEN_LEN]);
