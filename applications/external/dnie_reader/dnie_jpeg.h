#pragma once

// Decodificador JPEG minimo para el Flipper: SOLO luminancia y a 1/8 de resolucion.
//
// A escala 1/8 cada bloque 8x8 se convierte en un pixel cuyo valor es la media del bloque,
// que es justo el coeficiente DC. Asi no hace falta IDCT: solo decodificar Huffman
// (los AC se leen para saltarlos). Es lo mismo que hace libjpeg con scale 1/8.
//
// Soporta JPEG secuencial con Huffman (SOF0/SOF1), 8 bits, 1 o 3 componentes, cualquier
// submuestreo, intervalos de reinicio. No soporta progresivo ni aritmetico.

#include "dnie_j2k.h" // DnieGrayImage y codigos de resultado compartidos

// Fuente de bytes: devuelve cuantos bytes ha copiado en buf (0 = fin de fichero)
typedef size_t (*DnieJpegRead)(void* ctx, uint8_t* buf, size_t max);

DnieJ2kResult dnie_jpeg_decode_dc(DnieJpegRead read, void* ctx, DnieGrayImage* out);
