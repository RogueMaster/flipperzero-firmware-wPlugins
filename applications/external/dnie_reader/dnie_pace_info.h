#pragma once

#include <furi.h>

#define DNIE_PACE_INFO_MAX 4

// Un PACEInfo de EF.CardAccess (BSI TR-03110 parte 3):
//   PACEInfo ::= SEQUENCE { protocol OID, version INTEGER, parameterId INTEGER OPTIONAL }
typedef struct {
    uint8_t mapping; // 1=DH-GM 2=ECDH-GM 3=DH-IM 4=ECDH-IM 6=ECDH-CAM
    uint8_t cipher; // 1=3DES 2=AES-128 3=AES-192 4=AES-256
    uint32_t version;
    bool has_param_id;
    uint32_t param_id; // dominio estandarizado: 13 = brainpoolP256r1, etc.
} DniePaceInfo;

// Devuelve cuantos PACEInfo encontro (0 si ninguno o si el ASN.1 esta mal formado)
size_t dnie_pace_info_parse(const uint8_t* data, size_t len, DniePaceInfo* out, size_t max_out);

const char* dnie_pace_mapping_str(uint8_t mapping);
const char* dnie_pace_cipher_str(uint8_t cipher);
const char* dnie_pace_param_str(uint32_t param_id);
