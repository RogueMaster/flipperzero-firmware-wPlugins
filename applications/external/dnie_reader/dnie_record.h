#pragma once

// Registro de una lectura guardada (texto en claro ANTES de cifrar), version 2:
//   "DNIE" | version(1)=2 | tipo(1) | fecha unix(4)
//   | tipo COMPLETO: len DG1(1) | DG1
//   | tipo ALBUM:    len nombre(1) | nombre | len apellidos(1) | apellidos | nacimiento "AAAAMMDD"
//                  | pais emisor (3, desde la version 3)
//   | desde la version 4: len alias(1) | alias (nombre puesto por el usuario)
//   | ancho(1) | alto(1) | gris(ancho*alto) | SHA-256 de todo lo anterior | relleno PKCS#7
// La version 1 (sin byte de tipo, siempre completa) se sigue pudiendo leer.
// El modo album NO guarda numero de DNI, soporte, MRZ ni caducidad: solo nombre, foto y cumple.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "dnie_pixelart.h"

#define DNIE_RECORD_DG1_MAX   128
#define DNIE_RECORD_NAME_MAX  31
#define DNIE_RECORD_ALIAS_MAX 25 // nombre que le pone el usuario ("Mama", "Juan clase"...)
#define DNIE_RECORD_VERSION   4
#define DNIE_RECORD_MAX                                                            \
    (4 + 1 + 1 + 4 + 1 + DNIE_RECORD_DG1_MAX + 3 + 1 + DNIE_RECORD_ALIAS_MAX + 2 + \
     DNIE_PIX_MAX_W * DNIE_PIX_MAX_H + 32 + 16)

typedef enum {
    DnieRecordFull = 0, // DG1 completo (todos los datos del DNI)
    DnieRecordAlbum = 1, // solo nombre, foto y fecha de nacimiento
} DnieRecordKind;

typedef struct {
    uint8_t kind; // DnieRecordKind
    uint32_t saved_at; // segundos unix
    // COMPLETO
    uint8_t dg1[DNIE_RECORD_DG1_MAX];
    uint8_t dg1_len;
    // ALBUM
    char given_names[DNIE_RECORD_NAME_MAX];
    char surnames[DNIE_RECORD_NAME_MAX];
    char birth[9]; // "AAAAMMDD"
    char issuer[4]; // pais emisor de la MRZ ("ESP", "USA", "D"...)
    // Ambos
    char alias[DNIE_RECORD_ALIAS_MAX]; // vacio = se usa el nombre del documento
    bool has_photo;
    DniePixelSource photo; // la foto ya escalada (~3 KB): no hace falta el JPEG2000
} DnieRecord;

// Serializa con SHA-256 y relleno. Devuelve la longitud (multiplo de 16) o 0 si error.
size_t dnie_record_pack(const DnieRecord* rec, uint8_t* out, size_t max);

// Comprueba relleno, formato y SHA-256. false si algo no cuadra.
bool dnie_record_unpack(const uint8_t* in, size_t len, DnieRecord* out);
