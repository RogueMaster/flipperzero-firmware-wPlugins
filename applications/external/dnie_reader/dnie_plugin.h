#pragma once

// Carga bajo demanda de los plugins. Se abren justo antes de usarlos y se cierran en cuanto
// terminan, para que su codigo no ocupe RAM el resto del tiempo.
// Abrir un plugin nunca cuelga la app: si no cabe o falta el fichero, devuelve el motivo.

#include <flipper_application/plugins/plugin_manager.h>

#include "dnie_plugin_api.h"

typedef enum {
    DniePluginFoto,
    DniePluginAutenticidad,
    DniePluginHimnos,
    DniePluginEcc,
    DniePluginCount,
} DniePluginId;

typedef enum {
    DniePluginOk,
    DniePluginNoMemory,
    DniePluginMissing, // el .fal no esta o no se pudo cargar
} DniePluginStatus;

typedef struct {
    PluginManager* manager;
    const void* api; // la tabla del plugin; NULL si no esta abierto
} DniePlugin;

// Llamar desde un hilo de la app (no desde el del poller): APP_ASSETS_PATH depende del hilo.
DniePluginStatus dnie_plugin_open(DniePlugin* plugin, DniePluginId id);

// Se puede llamar aunque no este abierto
void dnie_plugin_close(DniePlugin* plugin);

// Recalcula el tramado cargando y descargando el plugin de foto. false si no se pudo.
bool dnie_foto_dither(const DniePixelSource* src, DnieDither mode, DniePixelArt* out);

// Abre el plugin de curva eliptica y le pasa las funciones de la app
DniePluginStatus dnie_plugin_open_ecc(DniePlugin* plugin);
