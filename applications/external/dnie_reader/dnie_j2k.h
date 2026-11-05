#pragma once

// Decodificador JPEG2000 minimo para el Flipper: SOLO luminancia y a resolucion reducida.
//
// Aprovecha que JPEG2000 permite reconstruir versiones pequenas de la imagen descartando
// los niveles de wavelet mas finos: para la pantalla de 128x64 basta con ~1/4 de resolucion,
// lo que reduce memoria y trabajo a una fraccion de un decodificador completo.
//
// Soporta lo que usan las fotos del DNIe: 1 tile, 1 capa, 1 precinto por resolucion,
// code-blocks sin modos especiales, wavelet 9/7 o 5/3, cuantizacion 0/1/2, MCT.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    DnieJ2kOk,
    DnieJ2kErrorFormat, // codestream mal formado o truncado
    DnieJ2kErrorUnsupported, // variante que este decodificador minimo no implementa
    DnieJ2kErrorMemory,
    DnieJ2kErrorTruncated, // faltan bytes: los datos de baja resolucion siguen mas adelante
} DnieJ2kResult;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t reduce; // niveles de wavelet descartados (escala 1/2^reduce)
    uint8_t* pixels; // width*height, 0 = negro, 255 = blanco
} DnieGrayImage;

// data = fichero .jp2 o codestream .j2k (o solo su principio: si no basta, devuelve
// DnieJ2kErrorTruncated y hay que volver a llamar con mas bytes).
// Elige la resolucion mas pequena con altura >= min_height.
DnieJ2kResult
    dnie_j2k_decode_luma(const uint8_t* data, size_t len, uint16_t min_height, DnieGrayImage* out);

void dnie_gray_free(DnieGrayImage* img);

const char* dnie_j2k_result_str(DnieJ2kResult result);
