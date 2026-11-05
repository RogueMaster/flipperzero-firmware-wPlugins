#pragma once

#include <stdint.h>
#include <stddef.h>
#include <mbedtls/ecp.h>

#define DNIE_EC_COORD_LEN 32
#define DNIE_EC_POINT_LEN (1 + 2 * DNIE_EC_COORD_LEN) // 04 || X || Y (sin comprimir)

typedef int (*DnieRng)(void* ctx, unsigned char* buf, size_t len);

// Carga brainpoolP256r1 a mano: el mbedtls del firmware la trae desactivada,
// pero su aritmetica generica funciona con cualquier curva de Weierstrass.
// El llamante hace mbedtls_ecp_group_init() antes y mbedtls_ecp_group_free() despues.
int dnie_ec_load_bp256r1(mbedtls_ecp_group* grp);

// Codifica un punto como 04||X||Y
int dnie_ec_point_write(
    const mbedtls_ecp_group* grp,
    const mbedtls_ecp_point* pt,
    uint8_t out[DNIE_EC_POINT_LEN]);

// Decodifica 04||X||Y y comprueba que el punto esta en la curva (anti puntos invalidos)
int dnie_ec_point_read(
    const mbedtls_ecp_group* grp,
    mbedtls_ecp_point* pt,
    const uint8_t* in,
    size_t len);
