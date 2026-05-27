#pragma once

#include "ecp.h"

int mbedtls_ecdsa_sign(mbedtls_ecp_group *grp, mbedtls_mpi *r, mbedtls_mpi *s, const mbedtls_mpi *d,
                       const unsigned char *buf, size_t blen,
                       int (*f_rng)(void *, unsigned char *, unsigned), void *p_rng);
int mbedtls_ecdsa_verify(mbedtls_ecp_group *grp, const unsigned char *buf, size_t blen,
                         const mbedtls_ecp_point *q, const mbedtls_mpi *r, const mbedtls_mpi *s);
