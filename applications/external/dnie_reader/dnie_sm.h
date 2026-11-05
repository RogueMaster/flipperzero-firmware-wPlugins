#pragma once

// Secure Messaging (ICAO 9303-11 sec. 9.8 / BSI TR-03110-3), en dos sabores:
//  AES  (tras PACE):  cifrado AES-CBC con IV = E(KSenc, SSC), MAC = AES-CMAC, SSC de 16 bytes
//  3DES (tras Chip Authentication v1 de este DNIe): 3DES-CBC con IV = 0, MAC "retail"
//                     ISO 9797-1 alg. 3, SSC de 8 bytes
//  - Datos del comando cifrados              -> DO87
//  - Le protegido                            -> DO97
//  - MAC(pad(SSC || pad(cabecera) || DO87 || DO97))[0..8] -> DO8E
//  - El SSC se incrementa en cada comando y en cada respuesta.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DNIE_SM_BLOCK 16

typedef enum {
    DnieSmCipherAes,
    DnieSmCipher3Des,
} DnieSmCipher;

typedef struct {
    uint8_t cipher; // DnieSmCipher
    uint8_t block; // 16 (AES) u 8 (3DES)
    uint8_t ks_enc[16];
    uint8_t ks_mac[16];
    uint8_t ssc[DNIE_SM_BLOCK]; // en 3DES solo se usan los 8 primeros bytes
} DnieSm;

typedef enum {
    DnieSmOk,
    DnieSmErrorMac, // el MAC de la respuesta no cuadra: NO fiarse de los datos
    DnieSmErrorFormat, // faltan objetos SM o estan mal formados
    DnieSmErrorPadding,
    DnieSmErrorPlainStatus, // el chip respondio solo con un SW sin proteger (p.ej. 6987/6988)
    DnieSmErrorBuffer,
    DnieSmErrorCrypto,
} DnieSmResult;

void dnie_sm_init(DnieSm* sm, const uint8_t ks_enc[16], const uint8_t ks_mac[16]); // AES, SSC=0
// 3DES con el SSC inicial dado (8 bytes; NULL = 0, que es lo que pide Chip Authentication)
void dnie_sm_init_3des(
    DnieSm* sm,
    const uint8_t ks_enc[16],
    const uint8_t ks_mac[16],
    const uint8_t* ssc8);
void dnie_sm_clear(DnieSm* sm); // borra las claves de memoria

// Protege un comando. hdr = CLA INS P1 P2 (sin proteger). data/len opcionales.
// le < 0 = sin Le; 0..256 = Le (256 se codifica 00). Devuelve la longitud o 0 si error.
size_t dnie_sm_wrap(
    DnieSm* sm,
    const uint8_t hdr[4],
    const uint8_t* data,
    size_t len,
    int le,
    uint8_t* out,
    size_t out_max);

// Verifica y descifra una respuesta protegida (resp incluye el SW1SW2 final del transporte).
// En *sw devuelve el SW protegido (DO99), o el SW plano si el chip no protegio la respuesta.
DnieSmResult dnie_sm_unwrap(
    DnieSm* sm,
    const uint8_t* resp,
    size_t resp_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    uint16_t* sw);

const char* dnie_sm_result_str(DnieSmResult result);
