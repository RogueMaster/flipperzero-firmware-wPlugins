#pragma once

// Foto del titular (DG2): volcado a la SD durante la lectura y extraccion de la imagen.

#include <furi.h>
#include <storage/storage.h>

#include "dnie_lds.h"
#include "dnie_dg2.h"
#include "dnie_hash.h"

// Ruta explicita: el volcado se hace desde el hilo NFC, asi no dependemos del alias /data
#define DNIE_DATA_DIR       EXT_PATH("apps_data/dnie_reader")
#define DNIE_DG2_PATH       DNIE_DATA_DIR "/dg2.bin"
#define DNIE_PHOTO_J2K_PATH DNIE_DATA_DIR "/foto.jp2"
#define DNIE_PHOTO_JPG_PATH DNIE_DATA_DIR "/foto.jpg"

typedef void (*DniePhotoProgress)(uint8_t percent, void* context);

typedef struct {
    // Lectura (hilo NFC)
    DnieLdsStatus lds;
    size_t dg2_len;
    // Extraccion (hilo GUI)
    bool image_found;
    DnieDg2Info dg2;
    bool j2k_ok;
    DnieJ2kInfo j2k;
    bool saved;
    const char* path;
} DniePhoto;

// Lee DG2 y lo vuelca a DNIE_DG2_PATH. Dentro del callback del poller, con la app eMRTD
// ya seleccionada.
DnieLdsResult dnie_photo_read_dg2(
    DnieTransport* poller,
    DnieSm* sm,
    DniePhoto* photo,
    DniePhotoProgress progress,
    void* context,
    DnieHash* hash); // opcional: se actualiza con cada trozo (para verificar contra el SOD)

// Busca la imagen dentro de DG2, la guarda como foto.jp2 / foto.jpg y analiza su cabecera.
// Borra dg2.bin al terminar. Llamar desde el hilo de la GUI.
bool dnie_photo_extract(DniePhoto* photo);

// Borra de la SD la foto en claro (dg2.bin, foto.jp2, foto.jpg). Se llama en cuanto la foto
// se ha convertido en pixel art y al salir de la app: solo queda la copia cifrada que guardes.
void dnie_photo_wipe(void);
