#pragma once

// DG14 (6E L { SET OF SecurityInfo }): protocolos de seguridad del chip (BSI TR-03110-3).
// Lo que nos interesa para Chip Authentication:
//   ChipAuthenticationInfo          { id-CA-*, version, keyId OPCIONAL }
//   ChipAuthenticationPublicKeyInfo { id-PK-*, SubjectPublicKeyInfo, keyId OPCIONAL }

#include "dnie_der.h"

#define DNIE_DG14_MAX_CA 4

typedef enum {
    DnieCurveUnknown,
    DnieCurveBrainpoolP256r1,
    DnieCurveNistP256,
    DnieCurveExplicit, // parametros explicitos (hay que compararlos con los conocidos)
} DnieCurve;

typedef struct {
    uint8_t ka; // 1 = DH, 2 = ECDH
    uint8_t cipher; // 1 = 3DES, 2 = AES-128, 3 = AES-192, 4 = AES-256
    uint32_t version;
    bool has_key_id;
    uint32_t key_id;
} DnieCaInfo;

typedef struct {
    uint8_t ka; // 1 = DH, 2 = ECDH
    DnieCurve curve;
    uint32_t param_id; // si viene como parametro estandarizado (TR-03110)
    bool has_key_id;
    uint32_t key_id;
    uint8_t public_key[133]; // punto sin comprimir 04||X||Y (hasta P-521)
    size_t public_key_len;
    DerTlv explicit_params; // si curve == DnieCurveExplicit
} DnieCaKey;

typedef struct {
    DnieCaInfo infos[DNIE_DG14_MAX_CA];
    uint8_t info_count;
    DnieCaKey keys[DNIE_DG14_MAX_CA];
    uint8_t key_count;
    bool has_pace;
    bool has_ta; // Terminal Authentication (proteje huellas, DG3)
    bool has_active_auth;
    uint8_t total_infos;
} DnieDg14;

bool dnie_dg14_parse(const uint8_t* data, size_t len, DnieDg14* out);

const char* dnie_curve_name(DnieCurve c);
