#pragma once

// EF.SOD (Document Security Object, ICAO 9303-10/11): una firma CMS (PKCS#7 SignedData)
// cuyo contenido es el LDSSecurityObject, la lista de hashes de cada DG.
//
//   77 L { ContentInfo { signedData, [0] SignedData {
//       digestAlgorithms, encapContentInfo { id-icao-ldsSecurityObject, [0] OCTET STRING {
//           LDSSecurityObject { version, hashAlgorithm, { {dgNumber, hash}... } } } },
//       [0] certificates { certificado del firmante (DS) },
//       signerInfos { { ..., digestAlgorithm, [0] signedAttrs { messageDigest... },
//                       signatureAlgorithm, signature } } } } }

#include "dnie_der.h"
#include "dnie_hash.h"

#define DNIE_SOD_MAX_DG 16

typedef struct {
    uint8_t number;
    uint8_t hash[64];
} DnieSodDg;

typedef struct {
    // LDSSecurityObject
    DnieHashAlg lds_hash;
    DnieSodDg dgs[DNIE_SOD_MAX_DG];
    uint8_t dg_count;
    DerTlv econtent; // octetos del LDSSecurityObject (lo que se firma/hashea)

    // SignerInfo
    DnieHashAlg signer_hash;
    bool has_message_digest;
    uint8_t message_digest[64];
    size_t message_digest_len;
    DerTlv signed_attrs; // [0] IMPLICIT; para verificar la firma se re-etiqueta como SET (0x31)
    char sig_alg[48]; // nombre legible
    DerTlv sig_alg_oid;
    DerTlv sig_alg_seq; // AlgorithmIdentifier completo
    DerTlv signature;

    // Certificado del firmante (Document Signer)
    bool has_cert;
    DerTlv cert;
    char ds_subject_cn[48];
    char ds_issuer_cn[48];
} DnieSod;

// Parsea el SOD completo (con la etiqueta 77). Los DerTlv apuntan dentro de data.
bool dnie_sod_parse(const uint8_t* data, size_t len, DnieSod* out);

// Busca el hash esperado de un DG. NULL si el SOD no lo incluye.
const uint8_t* dnie_sod_dg_hash(const DnieSod* sod, uint8_t dg);

// Comprueba messageDigest == hash(eContent) (primera parte de la verificacion de la firma)
bool dnie_sod_check_message_digest(const DnieSod* sod);

DnieHashAlg dnie_hash_from_oid(const DerTlv* oid);
