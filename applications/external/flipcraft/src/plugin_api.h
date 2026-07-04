#pragma once
//
// Contract between the tiny host FAP and the two loadable halves of the app.
//
// The host (main.cpp) is the only permanently resident code. The menu and the
// game are built as .fal plugins, embedded into the FAP's assets, unpacked to
// /ext/apps_assets/flipcraft/plugins/ on first launch and mapped into RAM only
// while they run. While the menu is open the whole game (world, renderer,
// framebuffers) does not exist in memory, and vice versa.
//
// Plugins execute on the host's thread, so the storage aliases "/data" and
// "/assets" keep resolving to the flipcraft app folders.

#include <stdint.h>
#include <stddef.h>

#define FLIPCRAFT_MENU_APP_ID      "flipcraft_menu"
#define FLIPCRAFT_MENU_API_VERSION 3u

typedef enum {
    FlipcraftMenuActionQuit = 0, // leave the app
    FlipcraftMenuActionLaunch = 1, // out_path is an existing save to play
    FlipcraftMenuActionGenerate = 2, // generate out_chunks^2 world at out_path, then play it
} FlipcraftMenuAction;

typedef struct {
    // Runs the world-selector UI and reports what the player chose. For
    // Generate, out_seed is the seed the player accepted or edited.
    FlipcraftMenuAction (
        *run)(char* out_path, size_t out_size, uint8_t* out_chunks, uint32_t* out_seed);
} FlipcraftMenuApi;

#define FLIPCRAFT_GAME_APP_ID      "flipcraft_game"
#define FLIPCRAFT_GAME_API_VERSION 1u

typedef struct {
    // Runs one game session for the save at world_path; returns when the
    // player quits back to the menu.
    int32_t (*run)(const char* world_path);
} FlipcraftGameApi;

#define FLIPCRAFT_WORLDGEN_APP_ID      "flipcraft_worldgen"
#define FLIPCRAFT_WORLDGEN_API_VERSION 1u

typedef void (*FlipcraftGenProgress)(void* ctx, uint8_t percent);

typedef struct {
    // Writes a freshly generated chunks x chunks world (16..128 per side) to
    // path using exactly the given seed. Returns false and removes the file
    // on failure.
    bool (*generate)(
        const char* path,
        uint8_t chunks,
        uint32_t seed,
        FlipcraftGenProgress progress,
        void* progress_ctx);
} FlipcraftWorldgenApi;
