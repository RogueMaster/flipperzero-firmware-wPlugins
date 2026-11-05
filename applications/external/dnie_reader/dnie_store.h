#pragma once

// Lecturas guardadas, cifradas con la clave unica del enclave del Flipper (ranura 11):
// solo se pueden abrir en ESTE Flipper. Formato del fichero:
//   "DNIEREC1" | IV aleatorio (16) | longitud (4) | AES-256-CBC(registro de dnie_record.h)

#include <furi.h>
#include "dnie_record.h"
#include "dnie_photo.h"

#define DNIE_SAVED_DIR DNIE_DATA_DIR "/lecturas"
#define DNIE_STORE_MAX 20

typedef struct {
    char path[128];
    char label[72]; // "NOMBRE APELLIDO" (las completas llevan "(DNI)" detras)
    uint8_t kind; // DnieRecordKind
} DnieSavedEntry;

typedef enum {
    DnieStoreOk,
    DnieStoreErrorFull,
    DnieStoreErrorCrypto,
    DnieStoreErrorFile,
    DnieStoreErrorFormat, // no se pudo descifrar/verificar (corrupto o de otro Flipper)
} DnieStoreResult;

// Guarda la lectura. Si ya hay una de la misma persona y del mismo tipo, la sustituye.
DnieStoreResult dnie_store_save(const DnieRecord* rec);

// Lista las lecturas (descifrandolas para mostrar el nombre). Devuelve cuantas hay.
size_t dnie_store_list(DnieSavedEntry* out, size_t max);

DnieStoreResult dnie_store_load(const char* path, DnieRecord* out);

bool dnie_store_delete(const char* path);

// Cambia el nombre que se muestra (alias) de una lectura guardada. "" = volver al del documento.
DnieStoreResult dnie_store_rename(const char* path, const char* alias);
