#include "dnie_ec.h"

#include "dnie_ec_params.inc"

int dnie_ec_load_bp256r1(mbedtls_ecp_group* grp) {
    int ret;
    grp->id = MBEDTLS_ECP_DP_NONE; // no es una curva "de serie" para esta build
    if((ret = mbedtls_mpi_read_binary(&grp->P, BP256_P, sizeof(BP256_P))) != 0) return ret;
    if((ret = mbedtls_mpi_read_binary(&grp->A, BP256_A, sizeof(BP256_A))) != 0) return ret;
    if((ret = mbedtls_mpi_read_binary(&grp->B, BP256_B, sizeof(BP256_B))) != 0) return ret;
    if((ret = mbedtls_mpi_read_binary(&grp->N, BP256_N, sizeof(BP256_N))) != 0) return ret;
    if((ret = mbedtls_mpi_read_binary(&grp->G.MBEDTLS_PRIVATE(X), BP256_GX, sizeof(BP256_GX))) !=
       0)
        return ret;
    if((ret = mbedtls_mpi_read_binary(&grp->G.MBEDTLS_PRIVATE(Y), BP256_GY, sizeof(BP256_GY))) !=
       0)
        return ret;
    if((ret = mbedtls_mpi_lset(&grp->G.MBEDTLS_PRIVATE(Z), 1)) != 0) return ret;

    grp->pbits = mbedtls_mpi_bitlen(&grp->P);
    grp->nbits = mbedtls_mpi_bitlen(&grp->N);
    // h = 0: parametros en heap (no estaticos) -> mbedtls_ecp_group_free los libera.
    // A definido -> mbedtls usa las formulas genericas (a != -3 en Brainpool).
    grp->MBEDTLS_PRIVATE(h) = 0;
    return 0;
}

int dnie_ec_point_write(
    const mbedtls_ecp_group* grp,
    const mbedtls_ecp_point* pt,
    uint8_t out[DNIE_EC_POINT_LEN]) {
    size_t olen = 0;
    int ret = mbedtls_ecp_point_write_binary(
        grp, pt, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, out, DNIE_EC_POINT_LEN);
    if(ret == 0 && olen != DNIE_EC_POINT_LEN) ret = MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    return ret;
}

int dnie_ec_point_read(
    const mbedtls_ecp_group* grp,
    mbedtls_ecp_point* pt,
    const uint8_t* in,
    size_t len) {
    if(len != DNIE_EC_POINT_LEN || in[0] != 0x04) return MBEDTLS_ERR_ECP_BAD_INPUT_DATA;
    int ret = mbedtls_ecp_point_read_binary(grp, pt, in, len);
    if(ret == 0) ret = mbedtls_ecp_check_pubkey(grp, pt);
    return ret;
}
