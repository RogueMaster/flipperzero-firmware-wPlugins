#include "dnie_pace_math.h"
#include "dnie_crypto.h"

#include <string.h>

int dnie_pace_keypair(
    mbedtls_ecp_group* grp,
    const mbedtls_ecp_point* base,
    mbedtls_mpi* sk,
    mbedtls_ecp_point* pk,
    bool gen_sk,
    DnieRng f_rng,
    void* p_rng) {
    int ret;
    if(gen_sk && (ret = mbedtls_ecp_gen_privkey(grp, sk, f_rng, p_rng)) != 0) return ret;
    return mbedtls_ecp_mul(grp, pk, sk, base, f_rng, p_rng);
}

int dnie_pace_map_gm(
    mbedtls_ecp_group* grp,
    const uint8_t* s,
    size_t s_len,
    const mbedtls_mpi* sk_map,
    const mbedtls_ecp_point* pk_map_chip,
    mbedtls_ecp_point* g_tilde,
    DnieRng f_rng,
    void* p_rng) {
    int ret;
    mbedtls_mpi s_mpi, one;
    mbedtls_ecp_point h, s_g;
    mbedtls_mpi_init(&s_mpi);
    mbedtls_mpi_init(&one);
    mbedtls_ecp_point_init(&h);
    mbedtls_ecp_point_init(&s_g);

    do {
        // H = sk_map * PK_map_chip (secreto compartido del mapeo)
        if((ret = mbedtls_ecp_mul(grp, &h, sk_map, pk_map_chip, f_rng, p_rng)) != 0) break;
        if(mbedtls_ecp_is_zero(&h)) {
            ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
            break;
        }
        // s*G
        if((ret = mbedtls_mpi_read_binary(&s_mpi, s, s_len)) != 0) break;
        if((ret = mbedtls_ecp_mul(grp, &s_g, &s_mpi, &grp->G, f_rng, p_rng)) != 0) break;
        // G~ = 1*(s*G) + 1*H  (muladd suma puntos; los datos ya son publicos/enmascarados)
        if((ret = mbedtls_mpi_lset(&one, 1)) != 0) break;
        if((ret = mbedtls_ecp_muladd(grp, g_tilde, &one, &s_g, &one, &h)) != 0) break;
        if(mbedtls_ecp_is_zero(g_tilde)) ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    } while(0);

    mbedtls_mpi_free(&s_mpi);
    mbedtls_mpi_free(&one);
    mbedtls_ecp_point_free(&h);
    mbedtls_ecp_point_free(&s_g);
    return ret;
}

int dnie_pace_shared_x(
    mbedtls_ecp_group* grp,
    const mbedtls_mpi* sk,
    const mbedtls_ecp_point* pk_other,
    uint8_t out[DNIE_EC_COORD_LEN],
    DnieRng f_rng,
    void* p_rng) {
    int ret;
    mbedtls_ecp_point shared;
    mbedtls_ecp_point_init(&shared);
    ret = mbedtls_ecp_mul(grp, &shared, sk, pk_other, f_rng, p_rng);
    if(ret == 0 && mbedtls_ecp_is_zero(&shared)) ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    if(ret == 0)
        ret = mbedtls_mpi_write_binary(&shared.MBEDTLS_PRIVATE(X), out, DNIE_EC_COORD_LEN);
    mbedtls_ecp_point_free(&shared);
    return ret;
}

bool dnie_pace_derive_keys(
    const uint8_t k[DNIE_EC_COORD_LEN],
    uint8_t ks_enc[16],
    uint8_t ks_mac[16]) {
    return dnie_kdf_aes128(k, DNIE_EC_COORD_LEN, DNIE_KDF_ENC, ks_enc) &&
           dnie_kdf_aes128(k, DNIE_EC_COORD_LEN, DNIE_KDF_MAC, ks_mac);
}

bool dnie_pace_token(
    const uint8_t ks_mac[16],
    const uint8_t oid[DNIE_PACE_OID_LEN],
    const uint8_t pk[DNIE_EC_POINT_LEN],
    uint8_t out[DNIE_PACE_TOKEN_LEN]) {
    // 7F49 4F | 06 0A <oid> | 86 41 <04 X Y>
    uint8_t data[3 + 2 + DNIE_PACE_OID_LEN + 2 + DNIE_EC_POINT_LEN];
    size_t i = 0;
    data[i++] = 0x7F;
    data[i++] = 0x49;
    data[i++] = 2 + DNIE_PACE_OID_LEN + 2 + DNIE_EC_POINT_LEN;
    data[i++] = 0x06;
    data[i++] = DNIE_PACE_OID_LEN;
    memcpy(&data[i], oid, DNIE_PACE_OID_LEN);
    i += DNIE_PACE_OID_LEN;
    data[i++] = 0x86;
    data[i++] = DNIE_EC_POINT_LEN;
    memcpy(&data[i], pk, DNIE_EC_POINT_LEN);
    i += DNIE_EC_POINT_LEN;

    uint8_t mac[16];
    if(!dnie_aes128_cmac(ks_mac, data, i, mac)) return false;
    memcpy(out, mac, DNIE_PACE_TOKEN_LEN);
    return true;
}
