#pragma once

// Interfaz entre la app y sus plugins (.fal). Cada plugin exporta una de estas tablas.
// Si cambia alguna, hay que subir DNIE_PLUGIN_API_VERSION.

#include "dnie_j2k.h"
#include "dnie_jpeg.h"
#include "dnie_pixelart.h"
#include "dnie_pa.h"
#include "dnie_csca.h"
#include "dnie_pace.h"
#include "dnie_ca.h"
#include "dnie_sm.h"

#define DNIE_PLUGIN_APP_ID      "dnie_reader"
#define DNIE_PLUGIN_API_VERSION 1

// Foto: decodificadores, pixel art y el retrato del modo demo
typedef struct {
    DnieJ2kResult (
        *j2k_decode_luma)(const uint8_t* data, size_t len, uint16_t min_height, DnieGrayImage* out);
    DnieJ2kResult (*jpeg_decode_dc)(DnieJpegRead read, void* ctx, DnieGrayImage* out);
    bool (*pixelart_prepare)(
        const uint8_t* gray,
        uint16_t gray_w,
        uint16_t gray_h,
        uint8_t max_w,
        uint8_t max_h,
        DniePixelSource* out);
    bool (*pixelart_dither)(const DniePixelSource* src, DnieDither mode, DniePixelArt* out);
    const uint8_t* demo_face; // 4 bits por pixel
    uint16_t demo_face_w;
    uint16_t demo_face_h;
} DnieFotoApi;

// Funciones de la app que necesita el plugin de autenticidad. Se pasan como punteros (y no
// importando el simbolo) porque el cargador solo resuelve nombres contra el firmware.
typedef struct {
    size_t (*hash_len)(DnieHashAlg alg);
    bool (*hash_supported)(DnieHashAlg alg);
    bool (*hash_start)(DnieHash* h, DnieHashAlg alg);
    void (*hash_update)(DnieHash* h, const uint8_t* data, size_t len);
    bool (*hash_finish)(DnieHash* h, uint8_t out[DNIE_HASH_MAX]);
    DnieHashAlg (*hash_from_oid)(const DerTlv* oid);
} DnieAutenticidadHost;

// Autenticidad: firma del SOD (RSA) y cadena CSCA
typedef struct {
    void (*set_host)(const DnieAutenticidadHost* host); // llamar nada mas cargarlo
    DniePaResult (*verify_sod)(const DnieSod* sod, DniePaInfo* info);
    DnieCscaResult (
        *verify_cert)(const DerTlv* cert, DnieCscaRead read, void* ctx, DnieCscaInfo* info);
} DnieAutenticidadApi;

typedef struct {
    float freq; // 0 = silencio
    uint16_t ms;
} DnieNote;

// Himnos: devuelve las notas del pais emisor (0 = no tenemos su himno)
typedef struct {
    size_t (*find)(const char* issuer, const DnieNote** notes);
} DnieHimnosApi;

// Funciones de la app que necesita el plugin de curva eliptica
typedef struct {
    bool (*card_apdu)(
        DnieTransport* poller,
        const uint8_t* apdu,
        size_t apdu_len,
        BitBuffer* tx,
        BitBuffer* rx,
        uint16_t* sw);
    bool (*lds_sm_command)(
        DnieTransport* poller,
        DnieSm* sm,
        const uint8_t hdr[4],
        const uint8_t* data,
        size_t len,
        int le,
        uint8_t* out,
        size_t out_max,
        size_t* out_len,
        DnieLdsStatus* status);
    void (*sm_init)(DnieSm* sm, const uint8_t ks_enc[16], const uint8_t ks_mac[16]);
    void (*sm_init_3des)(
        DnieSm* sm,
        const uint8_t ks_enc[16],
        const uint8_t ks_mac[16],
        const uint8_t* ssc8);
    bool (*kdf_aes128)(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]);
    bool (*kdf_3des)(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]);
    bool (*aes128_cbc_decrypt_iv0)(
        const uint8_t key[16],
        const uint8_t* in,
        size_t len,
        uint8_t* out);
    bool (*aes128_cmac)(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t out[16]);
} DnieEccHost;

// Curva eliptica: PACE y Chip Authentication
typedef struct {
    void (*set_host)(const DnieEccHost* host); // llamar nada mas cargarlo
    DniePaceResult (*pace_establish)(
        DnieTransport* poller,
        const DniePacePassword* pw,
        DniePaceState* state,
        DniePaceProgressCallback progress,
        void* progress_context);
    DnieCaResult (*ca_run)(
        DnieTransport* poller,
        DnieSm* sm,
        const DnieDg14* dg14,
        const uint8_t dg1_head[4],
        DnieCaStatus* st);
} DnieEccApi;
