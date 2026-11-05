#pragma once

// Autenticidad del documento, parte 1 (Passive Authentication sin la firma todavia):
// lee EF.SOD y DG14 por el canal seguro y comprueba que los hashes de DG1, DG2 y DG14
// coinciden con los del SOD, y que el messageDigest firmado cubre esos hashes.

#include "dnie_lds.h"
#include "dnie_sod.h"
#include "dnie_dg14.h"
#include "dnie_pa.h"
#include "dnie_csca.h"
#include "dnie_ca.h"

#define DNIE_FID_SOD  0x011D
#define DNIE_FID_DG14 0x010E
#define DNIE_SOD_MAX  6144
#define DNIE_DG14_MAX 1024

typedef enum {
    DnieCheckNotDone, // no se pudo comprobar (no leido)
    DnieCheckOk,
    DnieCheckFail, // NO coincide: datos manipulados
    DnieCheckMissing, // el SOD no incluye ese DG
    DnieCheckUnsupported, // algoritmo de hash no disponible
} DnieCheck;

typedef struct {
    DnieLdsStatus sod_status;
    DnieLdsStatus dg14_status;
    uint8_t* sod_buf; // malloc; el DnieSod apunta dentro
    size_t sod_len;
    DnieSod* sod; // malloc, NULL si no se pudo parsear
    uint8_t* dg14_buf; // malloc
    size_t dg14_len;
    bool dg14_ok;
    DnieDg14 dg14;

    DnieCheck dg1;
    DnieCheck dg2;
    DnieCheck dg14_hash;
    DnieCheck message_digest;

    // Firma del SOD (Passive Authentication) y Chip Authentication
    DniePaInfo pa;
    DnieCscaInfo csca; // certificado DS emitido por CSCA SPAIN
    DnieCaStatus ca;

    // Resumen para la pantalla (sobrevive a dnie_auth_compact)
    bool sod_parsed;
    DnieHashAlg lds_hash;
    uint8_t dg_numbers[DNIE_SOD_MAX_DG];
    uint8_t dg_count;
    char sig_alg[48];
    bool has_cert;
    char ds_subject_cn[48];
    char ds_issuer_cn[48];
} DnieAuth;

void dnie_auth_reset(DnieAuth* a); // libera y deja todo en "no comprobado"

// Lee SOD y DG14 y comprueba DG1 y DG14. Dentro del callback del poller, app eMRTD seleccionada.
void dnie_auth_read(
    DnieTransport* poller,
    DnieSm* sm,
    const uint8_t* dg1,
    size_t dg1_len,
    DnieAuth* a);

// Para hashear DG2 mientras se vuelca a la SD
bool dnie_auth_dg2_hash_start(const DnieAuth* a, DnieHash* h);
void dnie_auth_dg2_hash_done(DnieAuth* a, DnieHash* h, bool read_ok);

// Libera SOD y DG14 (unos 8 KB) conservando el resumen y la clave de DG14. Llamar al terminar.
void dnie_auth_compact(DnieAuth* a);

// Todo verificado: hashes + firma del SOD + Chip Authentication
bool dnie_auth_all_ok(const DnieAuth* a);

// Datos autenticos (hashes + firma + CSCA) aunque el chip no se haya podido comprobar
bool dnie_auth_data_ok(const DnieAuth* a);

// true si todo lo comprobado coincide (y se pudo comprobar algo)
bool dnie_auth_integrity_ok(const DnieAuth* a);
bool dnie_auth_integrity_failed(const DnieAuth* a);

const char* dnie_check_str(DnieCheck c);
