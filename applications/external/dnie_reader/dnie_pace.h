#pragma once

#include "dnie_card.h"
#include "dnie_pace_info.h"

#define DNIE_NONCE_LEN  16 // AES: el nonce mide un bloque
#define DNIE_PACE_STEPS 4 // nonce, mapeo, ECDH, tokens

typedef enum {
    DniePaceOk,
    DniePaceErrorUnsupported, // el DNI no ofrece ECDH-GM AES-128 brainpoolP256r1
    DniePaceErrorTransport,
    DniePaceErrorMse, // MSE:Set AT rechazado
    DniePaceErrorNonce, // GA paso 1 (nonce) rechazado
    DniePaceErrorMap, // GA paso 2 (mapeo) rechazado
    DniePaceErrorKeyAgreement, // GA paso 3 (ECDH) rechazado
    DniePaceErrorWrongCan, // GA paso 4: el chip no acepto nuestro token -> CAN incorrecto
    DniePaceErrorChipToken, // el token del chip no cuadra (no deberia pasar nunca)
    DniePaceErrorFormat, // respuesta con estructura inesperada
    DniePaceErrorBadPoint, // el chip mando un punto invalido o repetido
    DniePaceErrorCrypto,
    DniePaceErrorNoMemory, // no hay RAM para la cuenta de curva eliptica
} DniePaceResult;

typedef struct {
    DniePaceInfo info; // variante elegida
    uint8_t nonce[DNIE_NONCE_LEN]; // nonce s descifrado con K_pi
    uint8_t ks_enc[16]; // claves de sesion para Secure Messaging
    uint8_t ks_mac[16];
    uint16_t sw; // ultimo status word
    uint8_t step; // ultimo paso alcanzado (1..4)
    uint32_t ecc_ms; // tiempo total gastado en curva eliptica
} DniePaceState;

// Se llama desde el hilo del poller al empezar cada paso (1..4)
typedef void (*DniePaceProgressCallback)(uint8_t step, void* context);

// Elige la variante que implementamos: ECDH-GM + AES-128 + brainpoolP256r1
bool dnie_pace_choose(const DniePaceInfo* infos, size_t n, DniePaceInfo* chosen);

// Contrasena de PACE: el CAN (DNIe) o SHA-1 de la MRZ (pasaporte)
#define DNIE_PACE_PW_MRZ 0x01
#define DNIE_PACE_PW_CAN 0x02
typedef struct {
    uint8_t ref; // DNIE_PACE_PW_*
    uint8_t secret[20];
    size_t len;
} DniePacePassword;

// PACE completo. Al terminar OK, state->ks_enc / ks_mac abren el canal seguro.
DniePaceResult dnie_pace_establish(
    DnieTransport* poller,
    const DniePacePassword* pw,
    DniePaceState* state,
    DniePaceProgressCallback progress,
    void* progress_context);

const char* dnie_pace_result_str(DniePaceResult result);
