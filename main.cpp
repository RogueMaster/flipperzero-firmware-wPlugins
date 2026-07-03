//
// Host bridge: the only permanently resident piece of the app. Alternates
// between the menu plugin and the game plugin, so at any moment RAM holds the
// bridge plus exactly one of the two halves (see plugin_api.h).
//
#include "plugin_api.h"

#include <furi.h>
#include <flipper_application/flipper_application.h>
#include <loader/firmware_api/firmware_api.h>
#include <gui/gui.h>
#include <storage/storage.h>

#include <string.h>

#define TAG "Flipcraft"

namespace {

// Loads a .fal, validates its descriptor and returns the API struct, or
// nullptr (with *out_app freed) on any failure.
const void* pluginLoad(
    Storage* storage,
    const char* path,
    const char* appid,
    uint32_t api_version,
    FlipperApplication** out_app) {
    FlipperApplication* app = flipper_application_alloc(storage, firmware_api_interface);
    const void* entry = nullptr;

    do {
        if(flipper_application_preload(app, path) != FlipperApplicationPreloadStatusSuccess) {
            FURI_LOG_E(TAG, "Failed to preload %s", path);
            break;
        }
        if(!flipper_application_is_plugin(app)) {
            FURI_LOG_E(TAG, "%s is not a plugin", path);
            break;
        }
        if(flipper_application_map_to_memory(app) != FlipperApplicationLoadStatusSuccess) {
            FURI_LOG_E(TAG, "Failed to map %s", path);
            break;
        }
        const FlipperAppPluginDescriptor* desc = flipper_application_plugin_get_descriptor(app);
        if(!desc || strcmp(desc->appid, appid) != 0 || desc->ep_api_version != api_version) {
            FURI_LOG_E(TAG, "Bad descriptor in %s", path);
            break;
        }
        entry = desc->entry_point;
    } while(false);

    if(!entry) {
        flipper_application_free(app);
        *out_app = nullptr;
        return nullptr;
    }
    *out_app = app;
    return entry;
}

// Full-screen "Generating world" bar shown while the worldgen plugin runs on
// this thread; the GUI thread reads `percent` through the viewport callback.
struct GenProgress {
    volatile uint8_t percent = 0;
    ViewPort* view_port = nullptr;
};

void genDraw(Canvas* canvas, void* ctx) {
    GenProgress* p = reinterpret_cast<GenProgress*>(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "Generating world");
    canvas_draw_frame(canvas, 14, 32, 100, 10);
    canvas_draw_box(canvas, 16, 34, (uint16_t)((uint32_t)p->percent * 96 / 100), 6);
}

void genOnProgress(void* ctx, uint8_t percent) {
    GenProgress* p = reinterpret_cast<GenProgress*>(ctx);
    if(p->percent == percent) return;
    p->percent = percent;
    view_port_update(p->view_port);
}

// Loads the worldgen plugin, shows the progress screen, writes the world.
bool generateWorld(Storage* storage, const char* path, uint8_t chunks, uint32_t seed) {
    FlipperApplication* plugin = nullptr;
    const FlipcraftWorldgenApi* gen = reinterpret_cast<const FlipcraftWorldgenApi*>(pluginLoad(
        storage,
        APP_ASSETS_PATH("plugins/flipcraft_worldgen.fal"),
        FLIPCRAFT_WORLDGEN_APP_ID,
        FLIPCRAFT_WORLDGEN_API_VERSION,
        &plugin));
    if(!gen) return false;

    GenProgress progress;
    Gui* gui = reinterpret_cast<Gui*>(furi_record_open(RECORD_GUI));
    progress.view_port = view_port_alloc();
    view_port_draw_callback_set(progress.view_port, genDraw, &progress);
    gui_add_view_port(gui, progress.view_port, GuiLayerFullscreen);

    bool ok = gen->generate(path, chunks, seed, genOnProgress, &progress);

    view_port_enabled_set(progress.view_port, false);
    gui_remove_view_port(gui, progress.view_port);
    view_port_free(progress.view_port);
    furi_record_close(RECORD_GUI);

    flipper_application_free(plugin); // worldgen code leaves RAM here
    return ok;
}

} // namespace

extern "C" int32_t flipcraft_app(void* p) {
    (void)p;
    Storage* storage = reinterpret_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    int32_t result = 0;
    char world_path[256];

    while(true) {
        FlipperApplication* plugin = nullptr;
        const FlipcraftMenuApi* menu = reinterpret_cast<const FlipcraftMenuApi*>(pluginLoad(
            storage,
            APP_ASSETS_PATH("plugins/flipcraft_menu.fal"),
            FLIPCRAFT_MENU_APP_ID,
            FLIPCRAFT_MENU_API_VERSION,
            &plugin));
        if(!menu) {
            result = -1;
            break;
        }
        uint8_t chunks = 16;
        uint32_t seed = 0;
        FlipcraftMenuAction action = menu->run(world_path, sizeof(world_path), &chunks, &seed);
        flipper_application_free(plugin); // menu code leaves RAM here
        if(action == FlipcraftMenuActionQuit) break;

        if(action == FlipcraftMenuActionGenerate) {
            // On failure the save was already removed; fall back to the menu.
            if(!generateWorld(storage, world_path, chunks, seed)) continue;
        }

        const FlipcraftGameApi* game = reinterpret_cast<const FlipcraftGameApi*>(pluginLoad(
            storage,
            APP_ASSETS_PATH("plugins/flipcraft_game.fal"),
            FLIPCRAFT_GAME_APP_ID,
            FLIPCRAFT_GAME_API_VERSION,
            &plugin));
        if(!game) {
            result = -1;
            break;
        }
        game->run(world_path);
        flipper_application_free(plugin); // game code leaves RAM here
    }

    furi_record_close(RECORD_STORAGE);
    return result;
}
