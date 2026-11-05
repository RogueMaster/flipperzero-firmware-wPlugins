#pragma once

// DG2 (foto) del eMRTD:
//   75 L { 7F61 L { 02 01 n, 7F60 L { A1 L <cabecera biometrica>, 5F2E L <registro facial> } } }
// Registro facial ISO/IEC 19794-5:2005:
//   cabecera general (14) | cabecera del registro (20) | puntos (8 c/u) | info imagen (12) | imagen
// Y si la imagen es JPEG2000 analizamos la cabecera del codestream (SIZ, COD).

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DNIE_DG2_HEAD_LEN 512 // con esto basta para encontrar la imagen y su cabecera

typedef enum {
    DnieImageJpeg = 0,
    DnieImageJpeg2000 = 1,
} DnieImageType;

typedef struct {
    size_t image_offset; // desde el inicio de DG2
    size_t image_len;
    uint8_t image_type; // DnieImageType
    uint16_t width, height; // segun la cabecera ISO 19794-5
    uint16_t feature_points;
} DnieDg2Info;

typedef struct {
    bool jp2_container; // .jp2 (cajas) o codestream crudo (.j2k)
    size_t codestream_offset; // dentro de la imagen
    // SIZ
    uint32_t width, height, x_off, y_off;
    uint32_t tile_w, tile_h;
    uint16_t components;
    uint8_t depth; // bits por componente (del primero)
    bool is_signed;
    // COD
    uint8_t progression; // 0 LRCP, 1 RLCP, 2 RPCL, 3 PCRL, 4 CPRL
    uint16_t layers;
    uint8_t mct; // transformacion de color
    uint8_t levels; // niveles de descomposicion wavelet
    uint8_t cb_w, cb_h; // tamano de code-block (2^(x+2))
    uint8_t cb_style;
    uint8_t transform; // 0 = 9/7 irreversible, 1 = 5/3 reversible
    bool custom_precincts;
    bool has_siz, has_cod, has_qcd;
    uint8_t qcd_style; // 0 sin cuantizar, 1 derivada, 2 expandida
} DnieJ2kInfo;

// head = primeros bytes de DG2 (hasta DNIE_DG2_HEAD_LEN); total = tamano de DG2
bool dnie_dg2_find_image(const uint8_t* head, size_t head_len, size_t total, DnieDg2Info* out);

// data = inicio de la imagen JPEG2000 (al menos unos cientos de bytes)
bool dnie_j2k_parse_header(const uint8_t* data, size_t len, DnieJ2kInfo* out);

const char* dnie_j2k_progression_str(uint8_t p);
