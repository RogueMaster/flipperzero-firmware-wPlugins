#pragma once

// Chip Authentication v1 (BSI TR-03110 v1 / ICAO 9303-11 sec. 6.2), la que anuncia el DG14 de
// este DNIe (id-CA-ECDH-3DES-CBC-CBC, version 1):
//   1. Generamos un par de claves efimero en la curva de la clave estatica del chip (DG14).
//   2. Se lo mandamos con MSE:Set KAT (por el canal seguro actual).
//   3. Ambos calculan K = x(sk_efimera * PK_chip) = x(sk_chip * PK_efimera)
//      y derivan nuevas claves de sesion; el canal pasa a 3DES con SSC = 0.
//   4. Comprobacion implicita: si el siguiente comando funciona con las claves nuevas, el chip
//      tiene la clave privada de DG14 -> no es un clon (DG14 esta firmado dentro del SOD).

#include "dnie_lds.h"
#include "dnie_dg14.h"

typedef enum {
    DnieCaNotDone,
    DnieCaOk, // chip autentico
    DnieCaNotAvailable, // el DG14 no trae datos de Chip Authentication
    DnieCaUnsupported, // curva o cifrado que no implementamos
    DnieCaRejected, // el chip rechazo MSE:Set KAT
    DnieCaFailed, // el canal nuevo NO funciona: el chip no tiene la clave -> posible clon
    DnieCaTransport,
    DnieCaCrypto,
    DnieCaNoMemory, // no habia memoria para hacerla (no se hizo: no dice nada del chip)
} DnieCaResult;

typedef struct {
    DnieCaResult result;
    uint16_t sw;
    uint8_t cipher; // 1 = 3DES, 2 = AES-128
    uint32_t ecc_ms;
    DnieLdsStatus lds; // del comando de comprobacion
} DnieCaStatus;

// Ejecuta CA. Si va bien, *sm queda con las claves nuevas. dg1_head = primeros 4 bytes de DG1
// (los volvemos a leer por el canal nuevo para comprobarlo). Dentro del callback del poller.
DnieCaResult dnie_ca_run(
    DnieTransport* poller,
    DnieSm* sm,
    const DnieDg14* dg14,
    const uint8_t dg1_head[4],
    DnieCaStatus* st);

const char* dnie_ca_result_str(DnieCaResult r);
