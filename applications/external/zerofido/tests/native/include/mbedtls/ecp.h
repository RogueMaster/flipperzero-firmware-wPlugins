#pragma once

#include <stddef.h>

typedef struct {
    int placeholder;
} mbedtls_mpi;

typedef struct {
    mbedtls_mpi X;
    mbedtls_mpi Y;
    mbedtls_mpi Z;
} mbedtls_ecp_point;

typedef struct {
    int placeholder;
    mbedtls_ecp_point G;
} mbedtls_ecp_group;

#define MBEDTLS_ECP_DP_SECP256R1 0
#define MBEDTLS_ECP_PF_UNCOMPRESSED 0
#define MBEDTLS_PRIVATE(field) field

void mbedtls_ecp_group_init(mbedtls_ecp_group *grp);
void mbedtls_ecp_group_free(mbedtls_ecp_group *grp);
int mbedtls_ecp_group_load(mbedtls_ecp_group *grp, int id);
void mbedtls_ecp_point_init(mbedtls_ecp_point *pt);
void mbedtls_ecp_point_free(mbedtls_ecp_point *pt);
int mbedtls_ecp_mul(mbedtls_ecp_group *grp, mbedtls_ecp_point *r, const mbedtls_mpi *m,
                    const mbedtls_ecp_point *p, int (*f_rng)(), void *p_rng);
int mbedtls_ecp_check_privkey(const mbedtls_ecp_group *grp, const mbedtls_mpi *d);
int mbedtls_ecp_check_pubkey(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt);
int mbedtls_ecp_point_write_binary(const mbedtls_ecp_group *grp, const mbedtls_ecp_point *pt,
                                   int format, size_t *olen, unsigned char *buf, size_t buflen);
int mbedtls_ecp_gen_keypair(mbedtls_ecp_group *grp, mbedtls_mpi *d, mbedtls_ecp_point *q,
                            int (*f_rng)(), void *p_rng);
int mbedtls_mpi_read_binary(mbedtls_mpi *x, const unsigned char *buf, size_t buflen);
int mbedtls_mpi_write_binary(const mbedtls_mpi *x, unsigned char *buf, size_t buflen);
int mbedtls_mpi_lset(mbedtls_mpi *x, int z);
void mbedtls_mpi_init(mbedtls_mpi *x);
void mbedtls_mpi_free(mbedtls_mpi *x);
