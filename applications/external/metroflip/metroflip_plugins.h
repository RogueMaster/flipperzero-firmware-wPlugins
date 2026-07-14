#include <furi/core/string.h>
#include "metroflip_i.h"

#define METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID      "metroflip_plugins"
#define METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION 1

typedef void (*MetrolfipSupportedCardPluginOnEnter)(Metroflip* app);
typedef bool (*MetrolfipSupportedCardPluginOnEvent)(Metroflip* app, SceneManagerEvent event);
typedef void (*MetrolfipSupportedCardPluginOnExit)(Metroflip* app);

typedef struct {
    const char* card_name;
    MetrolfipSupportedCardPluginOnEnter plugin_on_enter;
    MetrolfipSupportedCardPluginOnEvent plugin_on_event;
    MetrolfipSupportedCardPluginOnExit plugin_on_exit;
} MetroflipPlugin;
