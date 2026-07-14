#include <furi.h>
#include <storage/storage.h>
#include <gui/modules/submenu.h>

#include "save_toypad.h"
#include "../views/EmulateToyPad_scene.h"
#include "../minifigures.h"
#include "../debug.h"

int favorite_ids[MAX_FAVORITES];
int num_favorites = 0;

// Utility: Alloc file and storage
static File* alloc_file(Storage** storage) {
    *storage = furi_record_open(RECORD_STORAGE);
    return storage_file_alloc(*storage);
}

// Utility: Open file with mode
static bool open_file(File* file, const char* filename, bool write_mode) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    bool success = storage_file_open(
        file,
        filename,
        write_mode ? FSAM_WRITE : FSAM_READ,
        write_mode ? FSOM_CREATE_ALWAYS : FSOM_OPEN_EXISTING);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Utility: Close + free
static void file_close_and_free(File* file) {
    storage_file_close(file);
    storage_file_free(file);
}

// Utility: Binary write
static bool save_binary_to_file(const void* data, size_t size, const char* filename) {
    Storage* storage;
    File* file = alloc_file(&storage);

    if(!open_file(file, filename, true)) {
        set_debug_text("Fail open file writing");
        file_close_and_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool success = storage_file_write(file, data, size);
    file_close_and_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) set_debug_text("Fail write file");
    return success;
}

// Utility: Binary read
static bool load_binary_from_file(void* data, size_t size, const char* filename) {
    Storage* storage;
    File* file = alloc_file(&storage);

    if(!open_file(file, filename, false)) {
        set_debug_text("Fail open file reading");
        file_close_and_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    bool success = storage_file_read(file, data, size);
    file_close_and_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) set_debug_text("Fail read file");
    return success;
}

void fill_favorites_submenu(LDToyPadApp* app) {
    submenu_reset(app->submenu_favorites_selection);
    load_favorites();

    for(int i = 0; i < num_favorites; i++) {
        if(favorite_ids[i] != 0) {
            submenu_add_item(
                app->submenu_favorites_selection,
                get_minifigure_name(favorite_ids[i]),
                favorite_ids[i],
                minifigures_submenu_callback,
                app);
        }
    }

    submenu_set_header(app->submenu_favorites_selection, "Select a favorite");
}

// Token Directory
static bool make_token_dir(Storage* storage) {
    return storage_simply_mkdir(storage, APP_DATA_PATH(TOKENS_DIR));
}

// Saved Token Submenu
void fill_saved_submenu(LDToyPadApp* app) {
    submenu_reset(app->submenu_saved_selection);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    uint8_t token_count = 0;

    make_token_dir(storage);

    if(!storage_dir_open(dir, APP_DATA_PATH(TOKENS_DIR))) {
        set_debug_text("Fail open token dir");
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FileInfo file_info;
    char file_name[FILEPATH_SIZE];

    while(storage_dir_read(dir, &file_info, file_name, sizeof(file_name))) {
        if(file_info.flags & FSF_DIRECTORY) continue;
        if(!strstr(file_name, TOKEN_FILE_EXTENSION)) continue;

        char file_path[FILE_NAME_LEN_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", APP_DATA_PATH(TOKENS_DIR), file_name);

        Token* token = malloc(sizeof(Token));
        if(!load_binary_from_file(token, sizeof(Token), file_path)) {
            free(token);
            continue;
        }

        if(app->saved_token_count < MAX_SAVED_TOKENS) {
            FuriString* furi_filepath = furi_string_alloc();
            furi_string_printf(furi_filepath, "%s", file_path);
            app->saved_token_paths[app->saved_token_count++] = furi_filepath;

            submenu_add_item(
                app->submenu_saved_selection,
                token->name,
                0,
                saved_token_submenu_callback,
                furi_filepath);
        }

        free(token);
        token_count++;
    }

    storage_dir_close(dir);
    file_close_and_free(dir);
    furi_record_close(RECORD_STORAGE);

    submenu_set_header(
        app->submenu_saved_selection,
        token_count > 0 ? "Select a saved vehicle" : "No saved vehicles");
}

// Token Saving
bool save_token(Token* token) {
    if(token == NULL) {
        return false;
    }
    char uid[13] = {0};
    snprintf(
        uid,
        sizeof(uid),
        "%02x%02x%02x%02x%02x%02x",
        (uint8_t)token->uid[0],
        (uint8_t)token->uid[1],
        (uint8_t)token->uid[2],
        (uint8_t)token->uid[3],
        (uint8_t)token->uid[4],
        (uint8_t)token->uid[5]);

    char name[16] = {0};
    for(unsigned int i = 0; i < sizeof(token->name); i++) {
        if(token->name[i] != ' ' && token->name[i] != '*') {
            name[i] = token->name[i];
        }
    }

    FuriString* path = furi_string_alloc();
    furi_string_printf(
        path, "%s/%s-%s%s", APP_DATA_PATH(TOKENS_DIR), name, uid, TOKEN_FILE_EXTENSION);

    bool result = save_binary_to_file(token, sizeof(Token), furi_string_get_cstr(path));
    furi_string_free(path);
    return result;
}

Token* load_saved_token(char* filepath) {
    Token* token = malloc(sizeof(Token));
    if(!load_binary_from_file(token, sizeof(Token), filepath)) {
        free(token);
        return NULL;
    }
    return token;
}

// filename to toypads path
static void fn_to_toypads_fp(char* filename, char* fullpath) {
    snprintf(
        fullpath,
        FILEPATH_SIZE,
        "%s/%s%s",
        APP_DATA_PATH(TOYPADS_DIR),
        filename,
        TOYPADS_FILE_EXTENSION);
}

static void mkdir_toypads(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(TOYPADS_DIR));
    furi_record_close(RECORD_STORAGE);
}

// Toypad DTO (Data Transfer Object)
typedef struct {
    Token tokens[MAX_TOKENS];
    BoxInfo boxes[NUM_BOXES];
} ToyPadSaveState;

// Toypad Save/Load
bool save_toypad(Token tokens[MAX_TOKENS], BoxInfo boxes[NUM_BOXES], char* filename) {
    ToyPadSaveState state;
    memcpy(state.tokens, tokens, sizeof(Token) * MAX_TOKENS);
    memcpy(state.boxes, boxes, sizeof(BoxInfo) * NUM_BOXES);

    char fullpath[FILEPATH_SIZE];
    fn_to_toypads_fp(filename, fullpath);

    if(save_binary_to_file(&state, sizeof(ToyPadSaveState), fullpath)) {
        furi_delay_ms(100); // Ensure the save is complete
        return true;
    }
    return false;
}
bool load_saved_toypad(Token* tokens[MAX_TOKENS], BoxInfo boxes[NUM_BOXES], char* filename) {
    ToyPadSaveState state;

    char fullpath[FILEPATH_SIZE];
    fn_to_toypads_fp(filename, fullpath);

    mkdir_toypads();

    if(load_binary_from_file(&state, sizeof(ToyPadSaveState), fullpath)) {
        // set the tokens to tokens and allocate them with the loaded data
        for(int i = 0; i < MAX_TOKENS; i++) {
            if(state.tokens[i].name[0] != '\0' && state.tokens[i].uid[0] != 0) {
                tokens[i] = malloc(sizeof(Token));
                memcpy(tokens[i], &state.tokens[i], sizeof(Token));
            } else {
                tokens[i] = NULL; // No token in this slot
            }
        }
        memcpy(boxes, state.boxes, sizeof(BoxInfo) * NUM_BOXES);
        return true;
    }
    return false;
}

static File* open_favorites_file(Storage** storage, bool write_mode) {
    *storage = furi_record_open(RECORD_STORAGE);
    if(!*storage) return NULL;

    File* file = storage_file_alloc(*storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    if(!storage_file_open(
           file,
           APP_DATA_PATH(FILE_NAME_FAVORITES),
           write_mode ? FSAM_WRITE : FSAM_READ,
           write_mode ? FSOM_CREATE_ALWAYS : FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    return file;
}

bool save_favorites(void) {
    Storage* storage;
    File* file = open_favorites_file(&storage, true);
    if(!file) {
        set_debug_text("Fail open favorites file for writing");
        return false;
    }

    bool success = storage_file_write(file, &num_favorites, sizeof(int)) &&
                   storage_file_write(file, favorite_ids, sizeof(int) * num_favorites);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) set_debug_text("Fail write favorites file");
    return success;
}

static void cleanup_favorites(void) {
    if(num_favorites == 0) return;

    int write_index = 0;
    for(int read_index = 0; read_index < num_favorites; read_index++) {
        int id = favorite_ids[read_index];
        if(id > 0 && id <= 999 && strcmp(get_minifigure_name(id), "?") != 0) {
            favorite_ids[write_index++] = id;
        }
    }

    if(write_index != num_favorites) {
        num_favorites = write_index;
        save_favorites();
    }
}

void load_favorites(void) {
    num_favorites = 0;

    Storage* storage;
    File* file = open_favorites_file(&storage, false);
    if(!file) return;

    // Read num_favorites
    if(storage_file_read(file, &num_favorites, sizeof(int))) {
        if(num_favorites > MAX_FAVORITES) num_favorites = MAX_FAVORITES;

        // Read the favorite_ids array
        storage_file_read(file, favorite_ids, sizeof(int) * num_favorites);
    } else {
        num_favorites = 0; // File may be empty/corrupt
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    cleanup_favorites();
}

bool favorite(int id, LDToyPadApp* app) {
    if(num_favorites >= MAX_FAVORITES || is_favorite(id)) return false;

    favorite_ids[num_favorites++] = id;
    submenu_add_item(
        app->submenu_favorites_selection,
        get_minifigure_name(id),
        id,
        minifigures_submenu_callback,
        app);
    return save_favorites();
}

bool unfavorite(int id, LDToyPadApp* app) {
    for(int i = 0; i < num_favorites; i++) {
        if(favorite_ids[i] == id) {
            for(int j = i; j < num_favorites - 1; j++)
                favorite_ids[j] = favorite_ids[j + 1];

            num_favorites--;
            save_favorites();
            fill_favorites_submenu(app);
            return true;
        }
    }
    return false;
}

bool is_favorite(int id) {
    for(int i = 0; i < num_favorites; i++) {
        if(favorite_ids[i] == id) return true;
    }
    return false;
}
