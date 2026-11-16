#include "dnie_plugin.h"
#include "dnie_crypto.h"

#include <furi.h>
#include <loader/firmware_api/firmware_api.h>
#include <storage/storage.h>

#define TAG "DniePlugin"

typedef struct {
    const char* path;
    size_t min_block; // mayor seccion del plugin + margen
    size_t min_free; // todo lo que ocupa cargarlo + reserva para el sistema
} DniePluginInfo;

// Tamanos medidos con arm-none-eabi-size sobre cada .fal
static const DniePluginInfo PLUGINS[DniePluginCount] = {
    [DniePluginFoto] =
        {.path = APP_ASSETS_PATH("plugins/dnie_foto.fal"),
         .min_block = 11 * 1024,
         .min_free = 20 * 1024},
    [DniePluginAutenticidad] =
        {.path = APP_ASSETS_PATH("plugins/dnie_autenticidad.fal"),
         .min_block = 10 * 1024,
         .min_free = 20 * 1024},
    [DniePluginHimnos] =
        {.path = APP_ASSETS_PATH("plugins/dnie_himnos.fal"),
         .min_block = 3 * 1024,
         .min_free = 10 * 1024},
    [DniePluginEcc] =
        {.path = APP_ASSETS_PATH("plugins/dnie_ecc.fal"),
         .min_block = 18 * 1024,
         .min_free = 44 * 1024}, // incluye la pila de 8 KB de las cuentas de curva
};

DniePluginStatus dnie_plugin_open(DniePlugin* plugin, DniePluginId id) {
    furi_check(id < DniePluginCount);
    memset(plugin, 0, sizeof(*plugin));
    const DniePluginInfo* info = &PLUGINS[id];

    // Cargar un .fal sin memoria colgaria el Flipper: mejor no intentarlo
    if(memmgr_heap_get_max_free_block() < info->min_block ||
       memmgr_get_free_heap() < info->min_free) {
        FURI_LOG_E(
            TAG,
            "Sin memoria para %s (libre %u, bloque %u)",
            info->path,
            memmgr_get_free_heap(),
            memmgr_heap_get_max_free_block());
        return DniePluginNoMemory;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool exists = storage_file_exists(storage, info->path);
    furi_record_close(RECORD_STORAGE);
    if(!exists) {
        FURI_LOG_E(TAG, "No existe %s", info->path);
        return DniePluginMissing;
    }

    plugin->manager =
        plugin_manager_alloc(DNIE_PLUGIN_APP_ID, DNIE_PLUGIN_API_VERSION, firmware_api_interface);

    PluginManagerError err = plugin_manager_load_single(plugin->manager, info->path);
    if(err != PluginManagerErrorNone) {
        FURI_LOG_E(TAG, "Error %d al cargar %s", err, info->path);
        dnie_plugin_close(plugin);
        return DniePluginMissing;
    }
    plugin->api = plugin_manager_get_ep(plugin->manager, 0);
    FURI_LOG_I(TAG, "Cargado %s, heap libre %u", info->path, memmgr_get_free_heap());
    return DniePluginOk;
}

void dnie_plugin_close(DniePlugin* plugin) {
    if(plugin->manager) plugin_manager_free(plugin->manager);
    memset(plugin, 0, sizeof(*plugin));
}

bool dnie_foto_dither(const DniePixelSource* src, DnieDither mode, DniePixelArt* out) {
    DniePlugin plugin;
    if(dnie_plugin_open(&plugin, DniePluginFoto) != DniePluginOk) return false;
    const DnieFotoApi* api = plugin.api;
    bool ok = api->pixelart_dither(src, mode, out);
    dnie_plugin_close(&plugin);
    return ok;
}

static const DnieEccHost ecc_host = {
    .card_apdu = dnie_card_apdu,
    .lds_sm_command = dnie_lds_sm_command,
    .sm_init = dnie_sm_init,
    .sm_init_3des = dnie_sm_init_3des,
    .kdf_aes128 = dnie_kdf_aes128,
    .kdf_3des = dnie_kdf_3des,
    .aes128_cbc_decrypt_iv0 = dnie_aes128_cbc_decrypt_iv0,
    .aes128_cmac = dnie_aes128_cmac,
};

DniePluginStatus dnie_plugin_open_ecc(DniePlugin* plugin) {
    DniePluginStatus st = dnie_plugin_open(plugin, DniePluginEcc);
    if(st == DniePluginOk) ((const DnieEccApi*)plugin->api)->set_host(&ecc_host);
    return st;
}
