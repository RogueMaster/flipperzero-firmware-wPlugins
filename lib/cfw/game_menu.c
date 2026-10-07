#include "game_menu.h"

#include <furi.h>
#include <m-array.h>
#include <toolbox/stream/file_stream.h>
#include <string.h>

#define TAG                    "GameMenu"
#define GAMEMENU_TEMP_PATH     GAMEMENU_APPS_PATH ".tmp"
#define GAMEMENU_BACKUP_PATH   GAMEMENU_APPS_PATH ".bak"
#define GAMEMENU_HEADER        "GamesMenuList Version 1\n"
#define GAMEMENU_DEFAULTS      "All Games"
#define GAMEMENU_FILENAME_SIZE 256

ARRAY_DEF(GameMenuPaths, char*)

typedef enum {
    GameMenuReadInvalid,
    GameMenuReadCustom,
    GameMenuReadDefaults,
} GameMenuReadResult;

static bool game_menu_is_fap(const char* path) {
    const char* extension = strrchr(path, '.');
    return extension && strcasecmp(extension, ".fap") == 0;
}

static bool game_menu_is_valid_path(const char* path) {
    return strncmp(path, "/ext/", 5) == 0 && game_menu_is_fap(path) && !strchr(path, '\n') &&
           !strchr(path, '\r');
}

static bool game_menu_paths_contains(const GameMenuPaths_t paths, const char* path) {
    for(size_t i = 0; i < GameMenuPaths_size(paths); i++) {
        if(strcmp(*GameMenuPaths_cget(paths, i), path) == 0) return true;
    }
    return false;
}

static void game_menu_paths_free(GameMenuPaths_t paths) {
    for(size_t i = 0; i < GameMenuPaths_size(paths); i++) {
        free(*GameMenuPaths_get(paths, i));
    }
    GameMenuPaths_clear(paths);
}

static void
    game_menu_load_defaults(Storage* storage, GameMenuEntryCallback callback, void* context) {
    GameMenuPaths_t directories;
    GameMenuPaths_t paths;
    GameMenuPaths_init(directories);
    GameMenuPaths_init(paths);
    GameMenuPaths_push_back(directories, strdup(GAMEMENU_GAMES_PATH));
    GameMenuPaths_push_back(directories, strdup(EXT_PATH("apps/GPIO/Games")));
    GameMenuPaths_push_back(directories, strdup(EXT_PATH("apps/GPIO/VGM")));

    /* Iterate instead of recursing: directory depth must not consume the loader stack. */
    File* folder = storage_file_alloc(storage);
    char* filename = malloc(GAMEMENU_FILENAME_SIZE);
    FuriString* path = furi_string_alloc();
    FileInfo info;
    while(!GameMenuPaths_empty_p(directories)) {
        char* directory;
        GameMenuPaths_pop_back(&directory, directories);
        if(storage_dir_open(folder, directory)) {
            while(storage_dir_read(folder, &info, filename, GAMEMENU_FILENAME_SIZE)) {
                if(filename[0] == '.') continue;
                furi_string_printf(path, "%s/%s", directory, filename);
                if(info.flags & FSF_DIRECTORY) {
                    if(strcmp(filename, "assets") != 0) {
                        GameMenuPaths_push_back(directories, strdup(furi_string_get_cstr(path)));
                    }
                } else if(game_menu_is_fap(filename)) {
                    GameMenuPaths_push_back(paths, strdup(furi_string_get_cstr(path)));
                }
            }
        }
        storage_dir_close(folder);
        free(directory);
    }
    furi_string_free(path);
    free(filename);
    storage_file_free(folder);
    game_menu_paths_free(directories);

    /* qsort is disabled in RM's external-app API; keep both users of this helper self-contained. */
    for(size_t i = 1; i < GameMenuPaths_size(paths); i++) {
        char* current = *GameMenuPaths_get(paths, i);
        size_t j = i;
        while(j && strcasecmp(*GameMenuPaths_get(paths, j - 1), current) > 0) {
            *GameMenuPaths_get(paths, j) = *GameMenuPaths_get(paths, j - 1);
            j--;
        }
        *GameMenuPaths_get(paths, j) = current;
    }
    for(size_t i = 0; i < GameMenuPaths_size(paths); i++) {
        callback(*GameMenuPaths_get(paths, i), context);
    }
    game_menu_paths_free(paths);
}

static GameMenuReadResult game_menu_read(
    Storage* storage,
    const char* config_path,
    GameMenuEntryCallback callback,
    void* context) {
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    GameMenuPaths_t paths;
    GameMenuPaths_init(paths);
    GameMenuReadResult result = GameMenuReadInvalid;
    do {
        if(!file_stream_open(stream, config_path, FSAM_READ, FSOM_OPEN_EXISTING)) break;
        if(!stream_read_line(stream, line)) break;
        furi_string_trim(line);
        if(!furi_string_equal(line, "GamesMenuList Version 0") &&
           !furi_string_equal(line, "GamesMenuList Version 1"))
            break;

        result = GameMenuReadCustom;
        while(stream_read_line(stream, line)) {
            furi_string_trim(line);
            if(furi_string_empty(line) || furi_string_start_with(line, "#")) continue;
            if(furi_string_equal(line, GAMEMENU_DEFAULTS) && GameMenuPaths_empty_p(paths)) {
                result = GameMenuReadDefaults;
            } else if(result == GameMenuReadDefaults) {
                result = GameMenuReadInvalid;
                break;
            } else if(
                game_menu_is_valid_path(furi_string_get_cstr(line)) &&
                !game_menu_paths_contains(paths, furi_string_get_cstr(line))) {
                GameMenuPaths_push_back(paths, strdup(furi_string_get_cstr(line)));
            }
        }
        if(file_stream_get_error(stream) != FSE_OK) result = GameMenuReadInvalid;
    } while(false);
    file_stream_close(stream);
    stream_free(stream);
    furi_string_free(line);

    if(result == GameMenuReadCustom) {
        for(size_t i = 0; i < GameMenuPaths_size(paths); i++) {
            callback(*GameMenuPaths_get(paths, i), context);
        }
    }
    game_menu_paths_free(paths);
    return result;
}

GameMenuSource game_menu_load(Storage* storage, GameMenuEntryCallback callback, void* context) {
    furi_check(storage);
    furi_check(callback);
    const bool has_custom = storage_file_exists(storage, GAMEMENU_APPS_PATH);
    const bool has_backup = !has_custom && storage_file_exists(storage, GAMEMENU_BACKUP_PATH);
    GameMenuReadResult result = game_menu_read(
        storage,
        has_custom ? GAMEMENU_APPS_PATH :
        has_backup ? GAMEMENU_BACKUP_PATH :
                     GAMEMENU_LEGACY_PATH,
        callback,
        context);
    if(result == GameMenuReadCustom) {
        return has_custom || has_backup ? GameMenuSourceCustom : GameMenuSourceLegacy;
    }
    game_menu_load_defaults(storage, callback, context);
    return GameMenuSourceDefaults;
}

static bool
    game_menu_write(Storage* storage, const char* const* paths, size_t count, bool defaults) {
    for(size_t i = 0; i < count; i++) {
        if(!paths[i] || !game_menu_is_valid_path(paths[i])) return false;
    }
    File* file = storage_file_alloc(storage);
    bool success = storage_file_open(file, GAMEMENU_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        const char* header = GAMEMENU_HEADER;
        success = storage_file_write(file, header, strlen(header)) == strlen(header);
        if(defaults && success) {
            const char* marker = GAMEMENU_DEFAULTS "\n";
            success = storage_file_write(file, marker, strlen(marker)) == strlen(marker);
        }
        for(size_t i = 0; success && i < count; i++) {
            const size_t length = strlen(paths[i]);
            success = storage_file_write(file, paths[i], length) == length &&
                      storage_file_write(file, "\n", 1) == 1;
        }
        if(success) success = storage_file_sync(file);
    }
    if(!storage_file_close(file)) success = false;
    storage_file_free(file);
    /* Storage's overwrite-rename deletes its destination first. Keep the old list
     * and use no-overwrite renames so a failed commit remains recoverable. */
    if(success && storage_file_exists(storage, GAMEMENU_APPS_PATH)) {
        FS_Error error = storage_common_remove(storage, GAMEMENU_BACKUP_PATH);
        success = error == FSE_OK || error == FSE_NOT_EXIST;
        if(success) {
            success = storage_common_rename_safe(
                          storage, GAMEMENU_APPS_PATH, GAMEMENU_BACKUP_PATH) == FSE_OK;
        }
    }
    if(success) {
        success = storage_common_rename_safe(storage, GAMEMENU_TEMP_PATH, GAMEMENU_APPS_PATH) ==
                  FSE_OK;
        if(!success && !storage_file_exists(storage, GAMEMENU_APPS_PATH)) {
            storage_common_rename_safe(storage, GAMEMENU_BACKUP_PATH, GAMEMENU_APPS_PATH);
        }
        if(success) storage_common_remove(storage, GAMEMENU_BACKUP_PATH);
    }
    if(!success) {
        storage_common_remove(storage, GAMEMENU_TEMP_PATH);
        FURI_LOG_E(TAG, "Could not save game menu");
    }
    return success;
}

bool game_menu_save(Storage* storage, const char* const* paths, size_t count) {
    furi_check(storage);
    furi_check(paths || !count);
    return game_menu_write(storage, paths, count, false);
}

bool game_menu_reset(Storage* storage) {
    furi_check(storage);
    return game_menu_write(storage, NULL, 0, true);
}
