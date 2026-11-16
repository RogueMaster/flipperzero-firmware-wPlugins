// Plugin de autenticidad: firma del SOD (RSA) y cadena CSCA con sus claves.

#include <flipper_application/flipper_application.h>

#include "../dnie_plugin_api.h"

// La app nos pasa sus funciones de hash: asi no duplicamos SHA-1/SHA-256 en el plugin.
// Estas definiciones sustituyen a las de dnie_hash.c, que no se compila aqui.
static const DnieAutenticidadHost* host;

static void set_host(const DnieAutenticidadHost* h) {
    host = h;
}

size_t dnie_hash_len(DnieHashAlg alg) {
    return host->hash_len(alg);
}

bool dnie_hash_supported(DnieHashAlg alg) {
    return host->hash_supported(alg);
}

bool dnie_hash_start(DnieHash* h, DnieHashAlg alg) {
    return host->hash_start(h, alg);
}

void dnie_hash_update(DnieHash* h, const uint8_t* data, size_t len) {
    host->hash_update(h, data, len);
}

bool dnie_hash_finish(DnieHash* h, uint8_t out[DNIE_HASH_MAX]) {
    return host->hash_finish(h, out);
}

DnieHashAlg dnie_hash_from_oid(const DerTlv* oid) {
    return host->hash_from_oid(oid);
}

static const DnieAutenticidadApi api = {
    .set_host = set_host,
    .verify_sod = dnie_pa_verify_sod,
    .verify_cert = dnie_csca_verify_cert,
};

static const FlipperAppPluginDescriptor descriptor = {
    .appid = DNIE_PLUGIN_APP_ID,
    .ep_api_version = DNIE_PLUGIN_API_VERSION,
    .entry_point = &api,
};

const FlipperAppPluginDescriptor* dnie_autenticidad_ep(void) {
    return &descriptor;
}
