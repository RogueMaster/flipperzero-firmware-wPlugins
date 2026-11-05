#pragma once

// Cadena de confianza: comprueba que el certificado del Document Signer (el que firma el SOD)
// lo emitio la CSCA del pais. Las de Espana van dentro de la app; las de otros paises
// principales, en un fichero de la SD (assets/csca.bin) que se recorre sin cargarlo entero.
// Todas salen de la CSCA Master List del BSI aleman.

#include "dnie_der.h"
#include "dnie_hash.h"

typedef struct {
    const char* name;
    uint16_t valid_until; // año
    const uint8_t* subject; // Name DER completo
    size_t subject_len;
    const uint8_t* n;
    size_t n_len;
    uint32_t e;
} DnieCscaKey;

typedef enum {
    DnieCscaNotDone,
    DnieCscaOk,
    DnieCscaBadSignature, // emisor conocido pero la firma no cuadra: certificado falso
    DnieCscaUnknownIssuer, // no lo firma ninguna CSCA que conozcamos
    DnieCscaUnsupported, // algoritmo no implementado (ECDSA, SHA-384/512...)
    DnieCscaFormat,
    DnieCscaNoMemory,
    DnieCscaKeyNotFound, // pais conocido pero ninguna de sus claves cuadra (puede faltar alguna)
} DnieCscaResult;

typedef struct {
    DnieCscaResult result;
    char csca_name[44]; // CSCA que lo valida (o la que coincide en nombre)
} DnieCscaInfo;

// Lector de csca.bin (NULL = solo las CSCA de Espana). Devuelve bytes leidos.
typedef size_t (*DnieCscaRead)(void* ctx, uint8_t* buf, size_t len);

// cert = Certificate DER completo (SEQUENCE). Comprueba su firma con las CSCA conocidas.
DnieCscaResult
    dnie_csca_verify_cert(const DerTlv* cert, DnieCscaRead read, void* ctx, DnieCscaInfo* info);

const char* dnie_csca_result_str(DnieCscaResult r);
