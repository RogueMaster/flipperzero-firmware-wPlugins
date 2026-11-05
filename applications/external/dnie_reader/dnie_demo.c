#include "dnie_demo.h"

#include <stdlib.h>
#include <string.h>

#include "demo_face.inc"

// Retrato ficticio (4 bits por pixel) -> gris 8 bits
static void draw_face(uint8_t* img) {
    for(int i = 0; i < DEMO_FACE_W * DEMO_FACE_H; i++) {
        uint8_t b = DEMO_FACE[i / 2];
        img[i] = ((i & 1) ? (b & 15) : (b >> 4)) * 17;
    }
}

// DG1 de demo en formato TD1, con digitos de control correctos (calculados al vuelo)
static void make_dg1(DnieApp* app) {
    char m[DNIE_MRZ_LEN + 1];
    // Linea 1: tipo, emisor, n. de documento + control, DNI ficticio
    char l1[31] = "IDESPBAA000589<99999999R<<<<<<";
    char l2[31] = "800101<F3101014ESP<<<<<<<<<<<<";
    char l3[31] = "ESPANOLA<ESPANOLA<<CARMEN<<<<<";
    l1[14] = '0' + dnie_mrz_check_digit(l1 + 5, 9);
    l2[6] = '0' + dnie_mrz_check_digit(l2, 6);
    l2[14] = '0' + dnie_mrz_check_digit(l2 + 8, 6);
    char comp[25 + 7 + 7 + 11];
    memcpy(comp, l1 + 5, 25);
    memcpy(comp + 25, l2, 7);
    memcpy(comp + 32, l2 + 8, 7);
    memcpy(comp + 39, l2 + 18, 11);
    l2[29] = '0' + dnie_mrz_check_digit(comp, sizeof(comp));
    memcpy(m, l1, 30);
    memcpy(m + 30, l2, 30);
    memcpy(m + 60, l3, 30);
    uint8_t* d = app->dg1;
    d[0] = 0x61;
    d[1] = 0x5D;
    d[2] = 0x5F;
    d[3] = 0x1F;
    d[4] = 0x5A;
    memcpy(d + 5, m, DNIE_MRZ_LEN);
    app->dg1_len = 5 + DNIE_MRZ_LEN;
}

bool dnie_demo_load(DnieApp* app) {
    // Estado limpio, como una lectura nueva
    memset(app->dg1, 0, sizeof(app->dg1));
    memset(&app->photo, 0, sizeof(app->photo));
    memset(&app->pix, 0, sizeof(app->pix));
    dnie_auth_reset(&app->auth);
    app->viewing_saved = false;
    app->viewing_album = false;
    app->passport = false;
    app->foreign_id = false;
    app->used_bac = false;
    app->protocol = NfcProtocolIso14443_4b;
    app->demo = true;

    make_dg1(app);
    app->mrz_ok = dnie_mrz_parse_dg1(app->dg1, app->dg1_len, &app->mrz);
    app->read_result = DnieCardOk;
    app->pace_result = DniePaceOk;
    app->lds.result = DnieLdsOk;

    // Foto dibujada y pasada a pixel art como una de verdad
    app->photo_decode_tried = true;
    app->photo_decode_result = DnieJ2kErrorMemory;
    uint8_t* img = malloc(DEMO_FACE_W * DEMO_FACE_H);
    if(img) {
        draw_face(img);
        if(!app->pix_src) app->pix_src = malloc(sizeof(DniePixelSource));
        if(app->pix_src &&
           dnie_pixelart_prepare(img, DEMO_FACE_W, DEMO_FACE_H, 56, 64, app->pix_src) &&
           dnie_pixelart_dither(app->pix_src, app->dither, &app->pix)) {
            app->photo_decode_result = DnieJ2kOk;
            app->photo_decoded_w = DEMO_FACE_W;
            app->photo_decoded_h = DEMO_FACE_H;
            app->photo_reduce = 0;
        }
        free(img);
    }
    app->photo.lds.result = DnieLdsOk;
    app->photo.image_found = true;
    app->photo.saved = true;
    app->photo.dg2.image_type = DnieImageJpeg2000;
    app->photo.dg2.width = 307;
    app->photo.dg2.height = 378;
    app->photo.dg2.image_len = 18 * 1024;

    // Resumen de autenticidad de ejemplo (como el de un DNIe real)
    DnieAuth* a = &app->auth;
    a->sod_parsed = true;
    a->dg1 = a->dg2 = a->dg14_hash = a->message_digest = DnieCheckOk;
    a->lds_hash = DnieHashSha1;
    static const uint8_t dgs[] = {1, 2, 3, 7, 11, 13, 14};
    memcpy(a->dg_numbers, dgs, sizeof(dgs));
    a->dg_count = sizeof(dgs);
    strlcpy(a->sig_alg, "RSA (PKCS#1)", sizeof(a->sig_alg));
    a->pa.result = DniePaOk;
    a->pa.key_bits = 2048;
    a->pa.hash = DnieHashSha1;
    a->has_cert = true;
    strlcpy(a->ds_subject_cn, "DS n-eID SPAIN 2", sizeof(a->ds_subject_cn));
    strlcpy(a->ds_issuer_cn, "CSCA SPAIN", sizeof(a->ds_issuer_cn));
    a->csca.result = DnieCscaOk;
    strlcpy(a->csca.csca_name, "CSCA SPAIN 3", sizeof(a->csca.csca_name));
    a->ca.result = DnieCaOk;
    a->ca.cipher = 1;
    a->ca.ecc_ms = 9000;
    a->ca.sw = 0x9000;
    return app->mrz_ok;
}
