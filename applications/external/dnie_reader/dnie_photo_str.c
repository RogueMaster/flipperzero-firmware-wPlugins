// Textos y utilidades de la foto que se usan sin tener cargado el plugin de foto.

#include "dnie_j2k.h"
#include "dnie_pixelart.h"

#include <stdlib.h>

void dnie_gray_free(DnieGrayImage* img) {
    free(img->pixels);
    img->pixels = NULL;
}

const char* dnie_j2k_result_str(DnieJ2kResult result) {
    switch(result) {
    case DnieJ2kOk:
        return "OK";
    case DnieJ2kErrorFormat:
        return "JPEG2000 mal formado";
    case DnieJ2kErrorUnsupported:
        return "Variante JPEG2000\nno soportada";
    case DnieJ2kErrorMemory:
        return "Sin memoria";
    case DnieJ2kErrorTruncated:
        return "JPEG2000 incompleto";
    case DnieJ2kErrorPlugin:
        return "Falta el plugin\nde foto";
    }
    return "Unknown error";
}

const char* dnie_dither_name(DnieDither mode) {
    switch(mode) {
    case DnieDitherAtkinson:
        return "Atkinson";
    case DnieDitherFloyd:
        return "Floyd-Steinberg";
    case DnieDitherBayer:
        return "Bayer";
    default:
        return "?";
    }
}
