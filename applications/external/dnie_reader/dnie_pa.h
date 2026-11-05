#pragma once

// Passive Authentication, parte 2: verificar la firma del EF.SOD con la clave publica del
// certificado del Document Signer (DS) que viene dentro del propio SOD.
//
// Que demuestra: que la lista de hashes (y por tanto DG1, DG2, DG14...) la firmo quien tiene
// la clave privada de ese certificado DS. Para cerrar el circulo falta comprobar que el
// certificado DS lo emitio la CSCA de Espana (necesita el certificado CSCA oficial).
//
// Implementado: RSA PKCS#1 v1.5 (lo que usa el DNIe), con SHA-1/224/256.

#include "dnie_sod.h"

typedef enum {
    DniePaNotDone,
    DniePaOk,
    DniePaBadSignature, // la firma NO cuadra: SOD manipulado o certificado que no es el firmante
    DniePaNoCert, // el SOD no trae certificado DS
    DniePaUnsupported, // algoritmo que no implementamos (ECDSA, RSA-PSS, SHA-384...)
    DniePaFormat, // certificado o firma mal formados
    DniePaNoMemory, // no se pudo comprobar por falta de memoria
} DniePaResult;

typedef struct {
    DniePaResult result;
    uint16_t key_bits; // tamano de la clave RSA del DS
    DnieHashAlg hash;
} DniePaInfo;

DniePaResult dnie_pa_verify_sod(const DnieSod* sod, DniePaInfo* info);

// RSA PKCS#1 v1.5: comprueba sig^e mod n == 00 01 FF..FF 00 DigestInfo(hash)
DniePaResult dnie_pa_rsa_pkcs1_verify(
    const uint8_t* n,
    size_t n_len,
    const uint8_t* e,
    size_t e_len,
    DnieHashAlg alg,
    const uint8_t* hash,
    const uint8_t* sig,
    size_t sig_len);

// RSASSA-PSS (EMSA-PSS-VERIFY, RFC 8017 9.1.2) con MGF1 del mismo hash.
// salt_len < 0: se acepta cualquier longitud de sal.
DniePaResult dnie_pa_rsa_pss_verify(
    const uint8_t* n,
    size_t n_len,
    const uint8_t* e,
    size_t e_len,
    DnieHashAlg alg,
    const uint8_t* hash,
    const uint8_t* sig,
    size_t sig_len,
    int salt_len);

// AlgorithmIdentifier de una firma RSA -> hash y si es PSS (con su sal).
// false si no es RSA o el hash no es SHA-1/224/256/384/512.
bool dnie_pa_rsa_sig_alg(const DerTlv* alg_seq, DnieHashAlg* hash, bool* pss, int* salt_len);

const char* dnie_pa_result_str(DniePaResult r);
