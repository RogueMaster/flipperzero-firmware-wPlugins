// Plugin de curva eliptica: PACE y Chip Authentication, con ECP y bignum de mbedtls.
// Esta cargado solo durante la sesion NFC; las lecturas guardadas y el demo no lo necesitan.

#include <flipper_application/flipper_application.h>

#include "../dnie_plugin_api.h"
#include "../dnie_crypto.h"

// La app nos pasa sus funciones (canal seguro, AES, KDF). Estas definiciones sustituyen a las
// de dnie_card.c, dnie_lds.c, dnie_sm.c y dnie_crypto.c, que no se compilan aqui.
static const DnieEccHost* host;

static void set_host(const DnieEccHost* h) {
    host = h;
}

bool dnie_card_apdu(
    DnieTransport* poller,
    const uint8_t* apdu,
    size_t apdu_len,
    BitBuffer* tx,
    BitBuffer* rx,
    uint16_t* sw) {
    return host->card_apdu(poller, apdu, apdu_len, tx, rx, sw);
}

bool dnie_lds_sm_command(
    DnieTransport* poller,
    DnieSm* sm,
    const uint8_t hdr[4],
    const uint8_t* data,
    size_t len,
    int le,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    DnieLdsStatus* status) {
    return host->lds_sm_command(poller, sm, hdr, data, len, le, out, out_max, out_len, status);
}

void dnie_sm_init(DnieSm* sm, const uint8_t ks_enc[16], const uint8_t ks_mac[16]) {
    host->sm_init(sm, ks_enc, ks_mac);
}

void dnie_sm_init_3des(
    DnieSm* sm,
    const uint8_t ks_enc[16],
    const uint8_t ks_mac[16],
    const uint8_t* ssc8) {
    host->sm_init_3des(sm, ks_enc, ks_mac, ssc8);
}

bool dnie_kdf_aes128(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]) {
    return host->kdf_aes128(secret, secret_len, counter, out);
}

bool dnie_kdf_3des(const uint8_t* secret, size_t secret_len, uint32_t counter, uint8_t out[16]) {
    return host->kdf_3des(secret, secret_len, counter, out);
}

bool dnie_aes128_cbc_decrypt_iv0(
    const uint8_t key[16],
    const uint8_t* in,
    size_t len,
    uint8_t* out) {
    return host->aes128_cbc_decrypt_iv0(key, in, len, out);
}

bool dnie_aes128_cmac(const uint8_t key[16], const uint8_t* data, size_t len, uint8_t out[16]) {
    return host->aes128_cmac(key, data, len, out);
}

static const DnieEccApi api = {
    .set_host = set_host,
    .pace_establish = dnie_pace_establish,
    .ca_run = dnie_ca_run,
};

static const FlipperAppPluginDescriptor descriptor = {
    .appid = DNIE_PLUGIN_APP_ID,
    .ep_api_version = DNIE_PLUGIN_API_VERSION,
    .entry_point = &api,
};

const FlipperAppPluginDescriptor* dnie_ecc_ep(void) {
    return &descriptor;
}
