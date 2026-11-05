#pragma once

// Basic Access Control (ICAO 9303-11, 4.3): el acceso clasico de los pasaportes.
// La clave sale de la MRZ: numero de documento + nacimiento + caducidad (con sus digitos
// de control). Tambien da la contrasena de PACE-MRZ (SHA-1 de lo mismo).

#include "dnie_card.h"
#include "dnie_sm.h"
#include "dnie_pace.h"

typedef enum {
    DnieBacOk,
    DnieBacErrorTransport,
    DnieBacErrorChallenge, // GET CHALLENGE rechazado
    DnieBacErrorAuth, // MUTUAL AUTHENTICATE rechazado: datos de la MRZ incorrectos
    DnieBacErrorChip, // la respuesta del chip no cuadra
    DnieBacErrorCrypto,
} DnieBacResult;

// "MRZ information" = n_doc (9, relleno con '<') + cd + nacimiento AAMMDD + cd + caducidad + cd
// doc_number admite hasta 9 caracteres A-Z/0-9.
void dnie_bac_mrz_info(const char* doc_number, const char* birth, const char* expiry, char out[25]);

// Contrasena de PACE con MRZ: SHA-1(MRZ information)
void dnie_bac_pace_password(const char* mrz_info, DniePacePassword* pw);

// BAC completo: al terminar OK, sm es un canal 3DES listo para leer.
DnieBacResult
    dnie_bac_establish(DnieTransport* poller, const char* mrz_info, DnieSm* sm, uint16_t* sw);

const char* dnie_bac_result_str(DnieBacResult r);
