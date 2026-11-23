#pragma once

#include <gui/view_dispatcher.h>

typedef struct GtaMockLoaders GtaMockLoaders;

GtaMockLoaders*
    gta_mock_loaders_alloc(ViewDispatcher* dispatcher, uint32_t viewer_id, uint32_t settings_id);
bool gta_mock_loaders_show_startup(GtaMockLoaders* loaders);
void gta_mock_loaders_free(GtaMockLoaders* loaders);
