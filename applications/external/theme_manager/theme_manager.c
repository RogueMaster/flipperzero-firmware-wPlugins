#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/loading.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <toolbox/compress.h>

#define TAG "ThemeManager"

/* Paths — override at compile time for custom firmwares:
 *   ufbt CFLAGS='-DCUSTOM_ANIMATION_PACKS_PATH=EXT_PATH("my_anims")' */
#ifndef CUSTOM_ANIMATION_PACKS_PATH
#define ANIMATION_PACKS_PATH EXT_PATH("animation_packs")
#else
#define ANIMATION_PACKS_PATH CUSTOM_ANIMATION_PACKS_PATH
#endif

#ifndef CUSTOM_DOLPHIN_PATH
#define DOLPHIN_PATH EXT_PATH("dolphin")
#else
#define DOLPHIN_PATH CUSTOM_DOLPHIN_PATH
#endif

#define MANIFEST_FILENAME   "manifest.txt"
#define META_FILENAME       "meta.txt"
#define ANIMS_DIRNAME       "Anims"
#define DOLPHIN_MANIFEST    DOLPHIN_PATH "/" MANIFEST_FILENAME
#define DOLPHIN_BACKUP_PATH EXT_PATH("dolphin_backup")
#define MANIFEST_HEADER     "Filetype: Flipper Animation Manifest"

#define MAX_THEMES    128
#define MAX_NAME_LEN  64
#define MAX_LABEL_LEN 32

#define MENU_INDEX_RESTORE (MAX_THEMES + 1)

#define PREVIEW_MAX_BM_SIZE 2048 /* max .bm file size (compressed or raw) */
#define PREVIEW_DRAW_X      2
#define PREVIEW_DRAW_Y      2
#define PREVIEW_DRAW_W      48
#define PREVIEW_DRAW_H      32

/* Y-offsets for info text (relative to PREVIEW_DRAW_Y) */
#define INFO_TEXT_Y_NAME  8
#define INFO_TEXT_Y_TYPE  18
#define INFO_TEXT_Y_ANIMS 27
#define INFO_TEXT_Y_SIZE  36
#define INFO_TEXT_Y_BTN   63

#define INFO_NAME_MAX_LEN      13 /* max visible chars for theme name in Info view */
#define MENU_LABEL_MAX_VISIBLE 26 /* max visible chars in submenu label */

typedef enum {
    ThemeTypePack,
    ThemeTypeAnimsPack,
    ThemeTypeSingle,
} ThemeType;

typedef enum {
    ThemeManagerViewSubmenu,
    ThemeManagerViewInfo,
    ThemeManagerViewConfirm,
    ThemeManagerViewReboot,
    ThemeManagerViewDeleteConfirm,
    ThemeManagerViewPopup,
    ThemeManagerViewLoading,
} ThemeManagerView;

typedef struct {
    char name[MAX_NAME_LEN];
    char type_label[16];
    uint32_t anim_count;
    char size_str[16];

    uint8_t* frame_data;
    uint32_t frame_size;
    uint8_t frame_w;
    uint8_t frame_h;
    bool preview_loaded;
} InfoViewModel;

typedef struct {
    char name[MAX_NAME_LEN];
    char label[MAX_LABEL_LEN];
    ThemeType type;
    uint32_t anim_count;
    uint32_t cached_size;
    bool meta_cached;
} ThemeEntry;

typedef struct {
    Storage* storage;
    Gui* gui;

    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    View* info_view;
    DialogEx* confirm_dialog;
    DialogEx* reboot_dialog;
    DialogEx* delete_dialog;
    Popup* popup;
    Loading* loading;

    ThemeEntry themes[MAX_THEMES];
    uint32_t theme_count;
    uint32_t selected_index;
    bool has_backup;
    char submenu_header[32];

    FuriString* dialog_text;
} ThemeManagerApp;

static void theme_manager_scan_themes(ThemeManagerApp* app);
static bool theme_manager_apply_pack(ThemeManagerApp* app, const char* merge_src_dir);
static bool theme_manager_apply_single(ThemeManagerApp* app, const char* theme_name);
static bool theme_manager_restore_backup(ThemeManagerApp* app);
static bool theme_manager_backup_dolphin(ThemeManagerApp* app);
static bool
    theme_manager_parse_manifest(ThemeManagerApp* app, const char* path, uint32_t* out_count);

static void theme_manager_submenu_callback(void* context, uint32_t index);
static void theme_manager_confirm_callback(DialogExResult result, void* context);
static void theme_manager_reboot_callback(DialogExResult result, void* context);
static void theme_manager_delete_callback(DialogExResult result, void* context);
static void theme_manager_popup_callback(void* context);
static void theme_manager_show_error(ThemeManagerApp* app, const char* message);
static void theme_manager_show_info(ThemeManagerApp* app, uint32_t index);
static bool theme_manager_delete_theme(ThemeManagerApp* app, uint32_t index);
static void theme_manager_populate_submenu(ThemeManagerApp* app);

static void theme_manager_info_draw(Canvas* canvas, void* model);
static bool theme_manager_info_input(InputEvent* event, void* context);

static uint32_t theme_manager_nav_exit(void* context);
static uint32_t theme_manager_nav_submenu(void* context);

// -------------------------------------------------------------------
// Read entire file into FuriString. Returns NULL on error.
// Caller must free the returned string with furi_string_free().
// -------------------------------------------------------------------
static FuriString* theme_manager_read_file(ThemeManagerApp* app, const char* path) {
    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return NULL;
    }

    FuriString* accum = furi_string_alloc();
    char buf[128];
    size_t bytes_read;

    while((bytes_read = storage_file_read(file, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes_read] = '\0';
        furi_string_cat_str(accum, buf);
    }

    storage_file_close(file);
    storage_file_free(file);
    return accum;
}

// -------------------------------------------------------------------
// Parse manifest.txt — validate header and count "Name:" entries
// -------------------------------------------------------------------
static bool
    theme_manager_parse_manifest(ThemeManagerApp* app, const char* path, uint32_t* out_count) {
    *out_count = 0;

    FuriString* content = theme_manager_read_file(app, path);
    if(!content) return false;

    const char* str = furi_string_get_cstr(content);

    if(strstr(str, MANIFEST_HEADER) == NULL) {
        furi_string_free(content);
        return false;
    }

    const char* ptr = str;
    while((ptr = strstr(ptr, "Name:")) != NULL) {
        if(ptr == str || *(ptr - 1) == '\n') {
            (*out_count)++;
        }
        ptr += 5;
    }

    furi_string_free(content);
    return true;
}

// -------------------------------------------------------------------
// Parse meta.txt — extract Width and Height values
// -------------------------------------------------------------------
static bool theme_manager_parse_meta_dimensions(
    ThemeManagerApp* app,
    const char* path,
    uint8_t* out_w,
    uint8_t* out_h) {
    *out_w = 0;
    *out_h = 0;

    FuriString* content = theme_manager_read_file(app, path);
    if(!content) return false;

    const char* text = furi_string_get_cstr(content);
    bool found_w = false;
    bool found_h = false;

    const char* w_ptr = strstr(text, "Width:");
    if(w_ptr) {
        uint32_t val = 0;
        if(sscanf(w_ptr, "Width: %lu", &val) == 1 && val > 0 && val <= 128) {
            *out_w = (uint8_t)val;
            found_w = true;
        }
    }

    const char* h_ptr = strstr(text, "Height:");
    if(h_ptr) {
        uint32_t val = 0;
        if(sscanf(h_ptr, "Height: %lu", &val) == 1 && val > 0 && val <= 64) {
            *out_h = (uint8_t)val;
            found_h = true;
        }
    }

    furi_string_free(content);
    return found_w && found_h;
}

// -------------------------------------------------------------------
// Get the first animation name from manifest.txt
// -------------------------------------------------------------------
static bool theme_manager_get_first_anim_name(
    ThemeManagerApp* app,
    const char* manifest_path,
    char* out_name,
    size_t out_name_size) {
    FuriString* content = theme_manager_read_file(app, manifest_path);
    if(!content) return false;

    const char* text = furi_string_get_cstr(content);
    const char* name_ptr = strstr(text, "Name:");
    bool found = false;

    if(name_ptr) {
        name_ptr += 5; /* skip "Name:" */
        while(*name_ptr == ' ')
            name_ptr++; /* skip spaces */

        size_t i = 0;
        while(name_ptr[i] != '\0' && name_ptr[i] != '\n' && name_ptr[i] != '\r' &&
              i < out_name_size - 1) {
            out_name[i] = name_ptr[i];
            i++;
        }
        out_name[i] = '\0';
        if(i > 0) found = true;
    }

    furi_string_free(content);
    return found;
}

// -------------------------------------------------------------------
// Load preview frame (frame_0.bm) for a theme
// Determines path based on theme type, loads raw XBM data
// -------------------------------------------------------------------
static void theme_manager_load_preview(ThemeManagerApp* app, uint32_t index) {
    with_view_model(
        app->info_view,
        InfoViewModel * model,
        {
            if(model->frame_data) {
                free(model->frame_data);
                model->frame_data = NULL;
            }
            model->preview_loaded = false;
            model->frame_w = 0;
            model->frame_h = 0;
            model->frame_size = 0;
        },
        false);

    if(index >= app->theme_count) return;

    const char* name = app->themes[index].name;
    ThemeType type = app->themes[index].type;

    FuriString* meta_path = furi_string_alloc();
    FuriString* frame_path = furi_string_alloc();
    uint8_t* raw = NULL;
    uint8_t* xbm_data = NULL;
    CompressIcon* compress = NULL;

    switch(type) {
    case ThemeTypeSingle:
        furi_string_printf(meta_path, "%s/%s/%s", ANIMATION_PACKS_PATH, name, META_FILENAME);
        furi_string_printf(frame_path, "%s/%s/frame_0.bm", ANIMATION_PACKS_PATH, name);
        break;

    case ThemeTypePack: {
        FuriString* manifest =
            furi_string_alloc_printf("%s/%s/%s", ANIMATION_PACKS_PATH, name, MANIFEST_FILENAME);
        char first_anim[MAX_NAME_LEN];
        if(theme_manager_get_first_anim_name(
               app, furi_string_get_cstr(manifest), first_anim, sizeof(first_anim))) {
            furi_string_printf(
                meta_path, "%s/%s/%s/%s", ANIMATION_PACKS_PATH, name, first_anim, META_FILENAME);
            furi_string_printf(
                frame_path, "%s/%s/%s/frame_0.bm", ANIMATION_PACKS_PATH, name, first_anim);
        }
        furi_string_free(manifest);
        break;
    }

    case ThemeTypeAnimsPack: {
        FuriString* manifest = furi_string_alloc_printf(
            "%s/%s/%s/%s", ANIMATION_PACKS_PATH, name, ANIMS_DIRNAME, MANIFEST_FILENAME);
        char first_anim[MAX_NAME_LEN];
        if(theme_manager_get_first_anim_name(
               app, furi_string_get_cstr(manifest), first_anim, sizeof(first_anim))) {
            furi_string_printf(
                meta_path,
                "%s/%s/%s/%s/%s",
                ANIMATION_PACKS_PATH,
                name,
                ANIMS_DIRNAME,
                first_anim,
                META_FILENAME);
            furi_string_printf(
                frame_path,
                "%s/%s/%s/%s/frame_0.bm",
                ANIMATION_PACKS_PATH,
                name,
                ANIMS_DIRNAME,
                first_anim);
        }
        furi_string_free(manifest);
        break;
    }
    }

    uint8_t w = 0, h = 0;
    if(furi_string_size(meta_path) == 0 ||
       !theme_manager_parse_meta_dimensions(app, furi_string_get_cstr(meta_path), &w, &h)) {
        FURI_LOG_W(TAG, "Preview: can't parse meta for %s", name);
        goto cleanup;
    }

    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, furi_string_get_cstr(frame_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_W(TAG, "Preview: can't open %s", furi_string_get_cstr(frame_path));
        storage_file_free(file);
        goto cleanup;
    }

    FileInfo file_info;
    storage_common_stat(app->storage, furi_string_get_cstr(frame_path), &file_info);

    if(file_info.size > PREVIEW_MAX_BM_SIZE || file_info.size < 2) {
        FURI_LOG_W(TAG, "Preview: bad size %llu", file_info.size);
        storage_file_close(file);
        storage_file_free(file);
        goto cleanup;
    }

    raw = malloc(file_info.size);
    size_t read_bytes = storage_file_read(file, raw, file_info.size);
    storage_file_close(file);
    storage_file_free(file);

    if(read_bytes != file_info.size) {
        FURI_LOG_E(TAG, "Preview: read failed");
        goto cleanup;
    }

    uint32_t decoded_size = ((uint32_t)((w + 7) / 8)) * h;
    compress = compress_icon_alloc(decoded_size);

    uint8_t* decoded = NULL;
    compress_icon_decode(compress, raw, &decoded);
    free(raw);
    raw = NULL;

    if(!decoded) {
        FURI_LOG_W(TAG, "Preview: decompress failed for %s", name);
        goto cleanup;
    }

    xbm_data = malloc(decoded_size);
    memcpy(xbm_data, decoded, decoded_size);

    with_view_model(
        app->info_view,
        InfoViewModel * model,
        {
            model->frame_data = xbm_data;
            model->frame_size = decoded_size;
            model->frame_w = w;
            model->frame_h = h;
            model->preview_loaded = true;
        },
        false);

    xbm_data = NULL; /* ownership transferred to model */
    FURI_LOG_I(TAG, "Preview loaded: %s (%ux%u, %lu bytes)", name, w, h, decoded_size);

cleanup:
    if(raw) free(raw);
    if(xbm_data) free(xbm_data);
    if(compress) compress_icon_free(compress);
    furi_string_free(meta_path);
    furi_string_free(frame_path);
}

// -------------------------------------------------------------------
// Calculate total size of a directory (recursive)
// -------------------------------------------------------------------
static uint64_t theme_manager_get_dir_size(ThemeManagerApp* app, const char* path) {
    uint64_t total = 0;
    File* dir = storage_file_alloc(app->storage);

    if(!storage_dir_open(dir, path)) {
        storage_file_free(dir);
        return 0;
    }

    FileInfo file_info;
    char name[MAX_NAME_LEN];
    FuriString* child_path = furi_string_alloc();

    while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
        furi_string_printf(child_path, "%s/%s", path, name);
        if(file_info.flags & FSF_DIRECTORY) {
            total += theme_manager_get_dir_size(app, furi_string_get_cstr(child_path));
        } else {
            total += file_info.size;
        }
    }

    furi_string_free(child_path);
    storage_dir_close(dir);
    storage_file_free(dir);

    return total;
}

// -------------------------------------------------------------------
// Scan /ext/animation_packs/ for all 3 formats
// -------------------------------------------------------------------
/* Case-insensitive string compare (ASCII only) */
static int theme_name_cmp(const char* a, const char* b) {
    while(*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if(ca != cb) return ca - cb;
        a++;
        b++;
    }
    return *a - *b;
}

/* Insertion sort for ThemeEntry array (qsort unavailable in Flipper API) */
static void theme_entries_sort(ThemeEntry* arr, uint32_t count) {
    for(uint32_t i = 1; i < count; i++) {
        ThemeEntry tmp = arr[i];
        uint32_t j = i;
        while(j > 0 && theme_name_cmp(arr[j - 1].name, tmp.name) > 0) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = tmp;
    }
}

static void theme_manager_scan_themes(ThemeManagerApp* app) {
    app->theme_count = 0;
    app->has_backup = storage_dir_exists(app->storage, DOLPHIN_BACKUP_PATH);

    if(!storage_dir_exists(app->storage, ANIMATION_PACKS_PATH)) {
        FURI_LOG_W(TAG, "Directory %s not found", ANIMATION_PACKS_PATH);
        return;
    }

    File* dir = storage_file_alloc(app->storage);

    if(!storage_dir_open(dir, ANIMATION_PACKS_PATH)) {
        FURI_LOG_E(TAG, "Failed to open %s", ANIMATION_PACKS_PATH);
        storage_file_free(dir);
        return;
    }

    FileInfo file_info;
    char name[MAX_NAME_LEN];

    FuriString* check_path = furi_string_alloc();

    while(app->theme_count < MAX_THEMES && storage_dir_read(dir, &file_info, name, sizeof(name))) {
        if(!(file_info.flags & FSF_DIRECTORY)) continue;

        ThemeType detected_type;
        bool found = false;

        furi_string_printf(check_path, "%s/%s/%s", ANIMATION_PACKS_PATH, name, MANIFEST_FILENAME);
        if(storage_file_exists(app->storage, furi_string_get_cstr(check_path))) {
            detected_type = ThemeTypePack;
            found = true;
            FURI_LOG_I(TAG, "[Pack] %s", name);
        }

        if(!found) {
            furi_string_printf(
                check_path,
                "%s/%s/%s/%s",
                ANIMATION_PACKS_PATH,
                name,
                ANIMS_DIRNAME,
                MANIFEST_FILENAME);
            if(storage_file_exists(app->storage, furi_string_get_cstr(check_path))) {
                detected_type = ThemeTypeAnimsPack;
                found = true;
                FURI_LOG_I(TAG, "[AnimsPack] %s", name);
            }
        }

        if(!found) {
            furi_string_printf(check_path, "%s/%s/%s", ANIMATION_PACKS_PATH, name, META_FILENAME);
            if(storage_file_exists(app->storage, furi_string_get_cstr(check_path))) {
                detected_type = ThemeTypeSingle;
                found = true;
                FURI_LOG_I(TAG, "[Single] %s", name);
            }
        }

        if(found) {
            ThemeEntry* entry = &app->themes[app->theme_count];
            strncpy(entry->name, name, MAX_NAME_LEN - 1);
            entry->name[MAX_NAME_LEN - 1] = '\0';
            entry->type = detected_type;
            entry->meta_cached = false;
            entry->anim_count = 0;
            entry->cached_size = 0;
            app->theme_count++;
        } else {
            FURI_LOG_W(TAG, "Skipping %s (unknown format)", name);
        }
    }

    furi_string_free(check_path);
    storage_dir_close(dir);
    storage_file_free(dir);

    /* Sort alphabetically by name */
    if(app->theme_count > 1) {
        theme_entries_sort(app->themes, app->theme_count);
    }

    FURI_LOG_I(
        TAG, "Total: %lu themes, backup: %s", app->theme_count, app->has_backup ? "yes" : "no");
}

// -------------------------------------------------------------------
// Backup entire /ext/dolphin/ → /ext/dolphin_backup/
// Uses rename (fast on FAT32 — just a metadata change)
// -------------------------------------------------------------------
static bool theme_manager_backup_dolphin(ThemeManagerApp* app) {
    if(!storage_dir_exists(app->storage, DOLPHIN_PATH)) {
        return true;
    }

    if(storage_dir_exists(app->storage, DOLPHIN_BACKUP_PATH)) {
        storage_simply_remove_recursive(app->storage, DOLPHIN_BACKUP_PATH);
    }

    FS_Error err = storage_common_rename(app->storage, DOLPHIN_PATH, DOLPHIN_BACKUP_PATH);
    if(err != FSE_OK) {
        FURI_LOG_E(TAG, "Backup rename failed (err %d)", err);
        return false;
    }

    app->has_backup = true;
    FURI_LOG_I(TAG, "Backed up /ext/dolphin/ -> /ext/dolphin_backup/");
    return true;
}

// -------------------------------------------------------------------
// Apply Pack theme (format A or B): merge directory into /ext/dolphin/
// -------------------------------------------------------------------
static bool theme_manager_apply_pack(ThemeManagerApp* app, const char* merge_src_dir) {
    FS_Error err = storage_common_merge(app->storage, merge_src_dir, DOLPHIN_PATH);

    if(err != FSE_OK) {
        FURI_LOG_E(TAG, "Merge failed: %s -> %s (err %d)", merge_src_dir, DOLPHIN_PATH, err);
        return false;
    }

    FURI_LOG_I(TAG, "Merged: %s -> %s", merge_src_dir, DOLPHIN_PATH);
    return true;
}

// -------------------------------------------------------------------
// Apply Single animation (format C):
//   1. Copy animation folder to /ext/dolphin/<name>/
//   2. Generate manifest.txt with single Name: entry
// -------------------------------------------------------------------
static bool theme_manager_apply_single(ThemeManagerApp* app, const char* theme_name) {
    FuriString* src_dir = furi_string_alloc_printf("%s/%s", ANIMATION_PACKS_PATH, theme_name);
    FuriString* dst_dir = furi_string_alloc_printf("%s/%s", DOLPHIN_PATH, theme_name);

    FS_Error mkdir_err = storage_common_mkdir(app->storage, furi_string_get_cstr(dst_dir));
    if(mkdir_err != FSE_OK && mkdir_err != FSE_EXIST) {
        FURI_LOG_E(TAG, "mkdir failed for %s (err %d)", furi_string_get_cstr(dst_dir), mkdir_err);
    }

    FS_Error err = storage_common_merge(
        app->storage, furi_string_get_cstr(src_dir), furi_string_get_cstr(dst_dir));

    furi_string_free(src_dir);
    furi_string_free(dst_dir);

    if(err != FSE_OK) {
        FURI_LOG_E(TAG, "Copy single anim failed (err %d)", err);
        return false;
    }

    File* manifest = storage_file_alloc(app->storage);
    if(!storage_file_open(manifest, DOLPHIN_MANIFEST, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Failed to create manifest");
        storage_file_free(manifest);
        return false;
    }

    FuriString* content = furi_string_alloc_printf(
        "Filetype: Flipper Animation Manifest\n"
        "Version: 1\n"
        "\n"
        "Name: %s\n"
        "Min butthurt: 0\n"
        "Max butthurt: 14\n"
        "Min level: 1\n"
        "Max level: 30\n"
        "Weight: 5\n",
        theme_name);

    const char* str = furi_string_get_cstr(content);
    size_t len = strlen(str);
    size_t written = storage_file_write(manifest, str, len);

    if(written != len) {
        FURI_LOG_E(TAG, "Manifest write incomplete (%zu/%zu bytes)", written, len);
        furi_string_free(content);
        storage_file_close(manifest);
        storage_file_free(manifest);
        return false;
    }

    furi_string_free(content);
    storage_file_close(manifest);
    storage_file_free(manifest);

    FURI_LOG_I(TAG, "Applied single animation: %s (manifest generated)", theme_name);
    return true;
}

// -------------------------------------------------------------------
// Main apply dispatcher — routes to correct handler based on type
// -------------------------------------------------------------------
static bool theme_manager_apply_theme(ThemeManagerApp* app, uint32_t index) {
    if(index >= app->theme_count) return false;

    if(!theme_manager_backup_dolphin(app)) {
        FURI_LOG_E(TAG, "Backup failed, aborting apply");
        return false;
    }

    FS_Error mkdir_err = storage_common_mkdir(app->storage, DOLPHIN_PATH);
    if(mkdir_err != FSE_OK && mkdir_err != FSE_EXIST) {
        FURI_LOG_E(TAG, "mkdir failed for dolphin dir (err %d)", mkdir_err);
    }

    const char* name = app->themes[index].name;
    ThemeType type = app->themes[index].type;

    FuriString* src = furi_string_alloc();
    bool success = false;

    switch(type) {
    case ThemeTypePack:
        furi_string_printf(src, "%s/%s", ANIMATION_PACKS_PATH, name);
        success = theme_manager_apply_pack(app, furi_string_get_cstr(src));
        break;

    case ThemeTypeAnimsPack:
        furi_string_printf(src, "%s/%s/%s", ANIMATION_PACKS_PATH, name, ANIMS_DIRNAME);
        success = theme_manager_apply_pack(app, furi_string_get_cstr(src));
        break;

    case ThemeTypeSingle:
        success = theme_manager_apply_single(app, name);
        break;
    }

    furi_string_free(src);
    return success;
}

// -------------------------------------------------------------------
// Restore backup: swap /ext/dolphin_backup/ → /ext/dolphin/
// -------------------------------------------------------------------
static bool theme_manager_restore_backup(ThemeManagerApp* app) {
    if(!storage_dir_exists(app->storage, DOLPHIN_BACKUP_PATH)) {
        return false;
    }

    if(storage_dir_exists(app->storage, DOLPHIN_PATH)) {
        storage_simply_remove_recursive(app->storage, DOLPHIN_PATH);
    }

    FS_Error err = storage_common_rename(app->storage, DOLPHIN_BACKUP_PATH, DOLPHIN_PATH);
    if(err != FSE_OK) {
        FURI_LOG_E(TAG, "Restore rename failed (err %d)", err);
        return false;
    }

    app->has_backup = false;
    FURI_LOG_I(TAG, "Restored /ext/dolphin_backup/ -> /ext/dolphin/");
    return true;
}

// -------------------------------------------------------------------
// Delete theme from SD card
// -------------------------------------------------------------------
static bool theme_manager_delete_theme(ThemeManagerApp* app, uint32_t index) {
    if(index >= app->theme_count) return false;

    FuriString* theme_path =
        furi_string_alloc_printf("%s/%s", ANIMATION_PACKS_PATH, app->themes[index].name);

    bool success = storage_simply_remove_recursive(app->storage, furi_string_get_cstr(theme_path));

    if(success) {
        FURI_LOG_I(TAG, "Deleted theme: %s", app->themes[index].name);
    } else {
        FURI_LOG_E(TAG, "Failed to delete: %s", app->themes[index].name);
    }

    furi_string_free(theme_path);
    return success;
}

// -------------------------------------------------------------------
// Custom Info View — draw callback
// Renders preview thumbnail (left) + theme info text (right) + buttons
// -------------------------------------------------------------------
static void theme_manager_info_draw(Canvas* canvas, void* _model) {
    InfoViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_draw_frame(
        canvas, PREVIEW_DRAW_X - 1, PREVIEW_DRAW_Y - 1, PREVIEW_DRAW_W + 2, PREVIEW_DRAW_H + 2);

    if(model->preview_loaded && model->frame_data) {
        uint8_t src_w = model->frame_w;
        uint8_t src_h = model->frame_h;
        uint8_t src_row_bytes = (src_w + 7) / 8;

        uint8_t x_offset = (src_w < PREVIEW_DRAW_W) ? (PREVIEW_DRAW_W - src_w) / 2 : 0;
        uint8_t y_offset = (src_h < PREVIEW_DRAW_H) ? (PREVIEW_DRAW_H - src_h) / 2 : 0;

        uint8_t draw_w = (src_w < PREVIEW_DRAW_W) ? src_w : PREVIEW_DRAW_W;
        uint8_t draw_h = (src_h < PREVIEW_DRAW_H) ? src_h : PREVIEW_DRAW_H;

        for(uint8_t py = 0; py < draw_h; py++) {
            uint8_t sy = (src_h > PREVIEW_DRAW_H) ? (uint8_t)(py * src_h / PREVIEW_DRAW_H) : py;

            for(uint8_t px = 0; px < draw_w; px++) {
                uint8_t sx = (src_w > PREVIEW_DRAW_W) ? (uint8_t)(px * src_w / PREVIEW_DRAW_W) :
                                                        px;

                uint32_t byte_idx = (uint32_t)sy * src_row_bytes + sx / 8;
                if(byte_idx < model->frame_size) {
                    if(model->frame_data[byte_idx] & (1 << (sx % 8))) {
                        canvas_draw_dot(
                            canvas,
                            PREVIEW_DRAW_X + x_offset + px,
                            PREVIEW_DRAW_Y + y_offset + py);
                    }
                }
            }
        }
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            PREVIEW_DRAW_X + PREVIEW_DRAW_W / 2,
            PREVIEW_DRAW_Y + PREVIEW_DRAW_H / 2,
            AlignCenter,
            AlignCenter,
            "No preview");
    }

    uint8_t text_x = PREVIEW_DRAW_X + PREVIEW_DRAW_W + 4;

    canvas_set_font(canvas, FontPrimary);
    char display_name[INFO_NAME_MAX_LEN];
    strncpy(display_name, model->name, sizeof(display_name) - 1);
    display_name[sizeof(display_name) - 1] = '\0';
    if(strlen(model->name) >= sizeof(display_name)) {
        /* Name was truncated — add ellipsis */
        display_name[sizeof(display_name) - 3] = '.';
        display_name[sizeof(display_name) - 2] = '.';
    }
    canvas_draw_str(canvas, text_x, PREVIEW_DRAW_Y + INFO_TEXT_Y_NAME, display_name);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, text_x, PREVIEW_DRAW_Y + INFO_TEXT_Y_TYPE, model->type_label);

    char anim_str[20];
    snprintf(anim_str, sizeof(anim_str), "Anims: %lu", model->anim_count);
    canvas_draw_str(canvas, text_x, PREVIEW_DRAW_Y + INFO_TEXT_Y_ANIMS, anim_str);

    char size_line[24];
    snprintf(size_line, sizeof(size_line), "Size: %s", model->size_str);
    canvas_draw_str(canvas, text_x, PREVIEW_DRAW_Y + INFO_TEXT_Y_SIZE, size_line);

    /* Bottom buttons */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 2, INFO_TEXT_Y_BTN, AlignLeft, AlignBottom, "<Back");
    canvas_draw_str_aligned(canvas, 64, INFO_TEXT_Y_BTN, AlignCenter, AlignBottom, "Del[OK]");
    canvas_draw_str_aligned(canvas, 126, INFO_TEXT_Y_BTN, AlignRight, AlignBottom, "Apply>");
}

// -------------------------------------------------------------------
// Custom Info View — input callback
// Handles Back (left), Apply (right), Delete (OK)
// -------------------------------------------------------------------
static bool theme_manager_info_input(InputEvent* event, void* context) {
    ThemeManagerApp* app = context;

    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyLeft) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewSubmenu);
        return true;

    } else if(event->key == InputKeyRight) {
        uint32_t index = app->selected_index;
        if(index >= app->theme_count) return true;

        dialog_ex_set_header(
            app->confirm_dialog, app->themes[index].name, 64, 0, AlignCenter, AlignTop);

        furi_string_printf(app->dialog_text, "Apply this theme?\nBackup will be created.");
        dialog_ex_set_text(
            app->confirm_dialog,
            furi_string_get_cstr(app->dialog_text),
            64,
            26,
            AlignCenter,
            AlignTop);

        dialog_ex_set_left_button_text(app->confirm_dialog, "Back");
        dialog_ex_set_right_button_text(app->confirm_dialog, "Apply");

        view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewConfirm);
        return true;

    } else if(event->key == InputKeyOk) {
        uint32_t index = app->selected_index;
        if(index >= app->theme_count) return true;

        dialog_ex_set_header(app->delete_dialog, "Delete Theme?", 64, 0, AlignCenter, AlignTop);

        furi_string_printf(
            app->dialog_text, "%s\nThis cannot be undone!", app->themes[index].name);
        dialog_ex_set_text(
            app->delete_dialog,
            furi_string_get_cstr(app->dialog_text),
            64,
            26,
            AlignCenter,
            AlignTop);

        dialog_ex_set_left_button_text(app->delete_dialog, "Cancel");
        dialog_ex_set_right_button_text(app->delete_dialog, "Delete");

        view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewDeleteConfirm);
        return true;
    }

    return false;
}

// -------------------------------------------------------------------
// Show theme info screen (custom View with preview)
// -------------------------------------------------------------------
static void theme_manager_show_info(ThemeManagerApp* app, uint32_t index) {
    if(index >= app->theme_count) return;

    app->selected_index = index;

    ThemeEntry* entry = &app->themes[index];
    const char* name = entry->name;
    ThemeType type = entry->type;

    const char* type_label;
    uint32_t anim_count = 0;

    /* Use cached metadata if available */
    if(entry->meta_cached) {
        anim_count = entry->anim_count;
    } else {
        switch(type) {
        case ThemeTypePack: {
            FuriString* mpath = furi_string_alloc_printf(
                "%s/%s/%s", ANIMATION_PACKS_PATH, name, MANIFEST_FILENAME);
            theme_manager_parse_manifest(app, furi_string_get_cstr(mpath), &anim_count);
            furi_string_free(mpath);
            break;
        }
        case ThemeTypeAnimsPack: {
            FuriString* mpath = furi_string_alloc_printf(
                "%s/%s/%s/%s", ANIMATION_PACKS_PATH, name, ANIMS_DIRNAME, MANIFEST_FILENAME);
            theme_manager_parse_manifest(app, furi_string_get_cstr(mpath), &anim_count);
            furi_string_free(mpath);
            break;
        }
        case ThemeTypeSingle:
            anim_count = 1;
            break;
        default:
            break;
        }
        entry->anim_count = anim_count;
    }

    switch(type) {
    case ThemeTypePack:
        type_label = "Pack";
        break;
    case ThemeTypeAnimsPack:
        type_label = "Anim Pack";
        break;
    case ThemeTypeSingle:
        type_label = "Single";
        break;
    default:
        type_label = "Unknown";
        break;
    }

    /* Use cached size or calculate and cache */
    uint64_t size_bytes;
    if(entry->meta_cached) {
        size_bytes = entry->cached_size;
    } else {
        FuriString* theme_dir = furi_string_alloc_printf("%s/%s", ANIMATION_PACKS_PATH, name);
        size_bytes = theme_manager_get_dir_size(app, furi_string_get_cstr(theme_dir));
        furi_string_free(theme_dir);
        entry->cached_size = (uint32_t)size_bytes;
        entry->meta_cached = true;
    }

    char size_str[16];
    if(size_bytes >= 1024 * 1024) {
        snprintf(
            size_str,
            sizeof(size_str),
            "%lu.%lu MB",
            (uint32_t)(size_bytes / (1024 * 1024)),
            (uint32_t)((size_bytes % (1024 * 1024)) * 10 / (1024 * 1024)));
    } else if(size_bytes >= 1024) {
        snprintf(size_str, sizeof(size_str), "%lu KB", (uint32_t)(size_bytes / 1024));
    } else {
        snprintf(size_str, sizeof(size_str), "%lu B", (uint32_t)size_bytes);
    }

    with_view_model(
        app->info_view,
        InfoViewModel * model,
        {
            strncpy(model->name, name, MAX_NAME_LEN - 1);
            model->name[MAX_NAME_LEN - 1] = '\0';
            snprintf(model->type_label, sizeof(model->type_label), "Type: %s", type_label);
            model->anim_count = anim_count;
            strncpy(model->size_str, size_str, sizeof(model->size_str) - 1);
            model->size_str[sizeof(model->size_str) - 1] = '\0';
        },
        false);

    theme_manager_load_preview(app, index);

    view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewInfo);
}

// -------------------------------------------------------------------
// Submenu callback
// -------------------------------------------------------------------
static void theme_manager_submenu_callback(void* context, uint32_t index) {
    ThemeManagerApp* app = context;

    if(index == MENU_INDEX_RESTORE) {
        if(theme_manager_restore_backup(app)) {
            dialog_ex_set_header(
                app->reboot_dialog, "Backup Restored!", 64, 0, AlignCenter, AlignTop);
            dialog_ex_set_text(
                app->reboot_dialog,
                "Previous theme restored.\nReboot now?",
                64,
                26,
                AlignCenter,
                AlignTop);
            dialog_ex_set_left_button_text(app->reboot_dialog, "Later");
            dialog_ex_set_right_button_text(app->reboot_dialog, "Reboot");
            view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewReboot);
        } else {
            theme_manager_show_error(app, "No backup found!");
        }
        return;
    }

    if(index >= app->theme_count) return;

    theme_manager_show_info(app, index);
}

// -------------------------------------------------------------------
// Confirm callback
// -------------------------------------------------------------------
static void theme_manager_confirm_callback(DialogExResult result, void* context) {
    ThemeManagerApp* app = context;

    if(result == DialogExResultRight) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewLoading);

        if(theme_manager_apply_theme(app, app->selected_index)) {
            const char* type_str = "";
            switch(app->themes[app->selected_index].type) {
            case ThemeTypePack:
                type_str = "Pack merged";
                break;
            case ThemeTypeAnimsPack:
                type_str = "Anims merged";
                break;
            case ThemeTypeSingle:
                type_str = "Anim + manifest";
                break;
            }

            dialog_ex_set_header(
                app->reboot_dialog, "Theme Applied!", 64, 0, AlignCenter, AlignTop);

            furi_string_printf(
                app->dialog_text,
                "%s\n%s. Reboot now?",
                app->themes[app->selected_index].name,
                type_str);
            dialog_ex_set_text(
                app->reboot_dialog,
                furi_string_get_cstr(app->dialog_text),
                64,
                26,
                AlignCenter,
                AlignTop);

            dialog_ex_set_left_button_text(app->reboot_dialog, "Later");
            dialog_ex_set_right_button_text(app->reboot_dialog, "Reboot");
            view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewReboot);
        } else {
            theme_manager_show_error(app, "Apply failed!\nCheck SD card.");
        }
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewInfo);
    }
}

// -------------------------------------------------------------------
// Reboot callback
// -------------------------------------------------------------------
static void theme_manager_reboot_callback(DialogExResult result, void* context) {
    ThemeManagerApp* app = context;

    if(result == DialogExResultRight) {
        furi_hal_power_reset();
    } else {
        theme_manager_scan_themes(app);
        theme_manager_populate_submenu(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewSubmenu);
    }
}

// -------------------------------------------------------------------
// Delete confirmation callback
// -------------------------------------------------------------------
static void theme_manager_delete_callback(DialogExResult result, void* context) {
    ThemeManagerApp* app = context;

    if(result == DialogExResultRight) {
        if(theme_manager_delete_theme(app, app->selected_index)) {
            theme_manager_scan_themes(app);
            theme_manager_populate_submenu(app);

            popup_set_header(app->popup, "Deleted!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Theme removed from SD", 64, 32, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 2000);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, theme_manager_popup_callback);
            popup_set_context(app->popup, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewPopup);
        } else {
            theme_manager_show_error(app, "Delete failed!\nCheck SD card.");
        }
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewInfo);
    }
}

// -------------------------------------------------------------------
// Popup timeout callback — return to submenu
// -------------------------------------------------------------------
static void theme_manager_popup_callback(void* context) {
    ThemeManagerApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewSubmenu);
}

// -------------------------------------------------------------------
// Error popup
// -------------------------------------------------------------------
static void theme_manager_show_error(ThemeManagerApp* app, const char* message) {
    popup_set_header(app->popup, "Error", 64, 0, AlignCenter, AlignTop);
    popup_set_text(app->popup, message, 64, 32, AlignCenter, AlignCenter);
    popup_set_timeout(app->popup, 3000);
    popup_enable_timeout(app->popup);
    popup_set_callback(app->popup, theme_manager_popup_callback);
    popup_set_context(app->popup, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewPopup);
}

// -------------------------------------------------------------------
// Populate submenu with type labels
// -------------------------------------------------------------------
static void theme_manager_populate_submenu(ThemeManagerApp* app) {
    submenu_reset(app->submenu);

    /* Submenu header with theme count */
    snprintf(app->submenu_header, sizeof(app->submenu_header), "Themes (%lu)", app->theme_count);
    submenu_set_header(app->submenu, app->submenu_header);

    if(app->theme_count == 0) {
        if(!storage_dir_exists(app->storage, ANIMATION_PACKS_PATH)) {
            submenu_add_item(app->submenu, "[No SD / No folder]", 0, NULL, NULL);
        } else {
            submenu_add_item(app->submenu, "[No themes found]", 0, NULL, NULL);
        }
    } else {
        for(uint32_t i = 0; i < app->theme_count; i++) {
            ThemeEntry* entry = &app->themes[i];
            const char* prefix;
            switch(entry->type) {
            case ThemeTypePack:
                prefix = "[P] ";
                break;
            case ThemeTypeAnimsPack:
                prefix = "[A] ";
                break;
            case ThemeTypeSingle:
                prefix = "[S] ";
                break;
            default:
                prefix = "";
                break;
            }

            snprintf(entry->label, MAX_LABEL_LEN, "%s%s", prefix, entry->name);

            if(strlen(entry->label) > MENU_LABEL_MAX_VISIBLE) {
                entry->label[MENU_LABEL_MAX_VISIBLE - 3] = '.';
                entry->label[MENU_LABEL_MAX_VISIBLE - 2] = '.';
                entry->label[MENU_LABEL_MAX_VISIBLE - 1] = '.';
                entry->label[MENU_LABEL_MAX_VISIBLE] = '\0';
            }

            submenu_add_item(app->submenu, entry->label, i, theme_manager_submenu_callback, app);
        }
    }

    if(app->has_backup) {
        submenu_add_item(
            app->submenu,
            ">> Restore Previous <<",
            MENU_INDEX_RESTORE,
            theme_manager_submenu_callback,
            app);
    }
}

// -------------------------------------------------------------------
// Navigation
// -------------------------------------------------------------------
static uint32_t theme_manager_nav_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t theme_manager_nav_submenu(void* context) {
    UNUSED(context);
    return ThemeManagerViewSubmenu;
}

// ===================================================================
// Entry point
// ===================================================================
int32_t theme_manager_app(void* p) {
    UNUSED(p);

    ThemeManagerApp* app = malloc(sizeof(ThemeManagerApp));
    memset(app, 0, sizeof(ThemeManagerApp));
    app->dialog_text = furi_string_alloc();

    app->storage = furi_record_open(RECORD_STORAGE);
    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->submenu), theme_manager_nav_exit);
    view_dispatcher_add_view(
        app->view_dispatcher, ThemeManagerViewSubmenu, submenu_get_view(app->submenu));

    /* Custom Info view with preview */
    app->info_view = view_alloc();
    view_allocate_model(app->info_view, ViewModelTypeLocking, sizeof(InfoViewModel));
    view_set_draw_callback(app->info_view, theme_manager_info_draw);
    view_set_input_callback(app->info_view, theme_manager_info_input);
    view_set_context(app->info_view, app);
    view_set_previous_callback(app->info_view, theme_manager_nav_submenu);
    view_dispatcher_add_view(app->view_dispatcher, ThemeManagerViewInfo, app->info_view);

    /* Initialize model */
    with_view_model(
        app->info_view,
        InfoViewModel * model,
        {
            model->frame_data = NULL;
            model->preview_loaded = false;
        },
        false);

    app->confirm_dialog = dialog_ex_alloc();
    dialog_ex_set_result_callback(app->confirm_dialog, theme_manager_confirm_callback);
    dialog_ex_set_context(app->confirm_dialog, app);
    view_set_previous_callback(dialog_ex_get_view(app->confirm_dialog), theme_manager_nav_submenu);
    view_dispatcher_add_view(
        app->view_dispatcher, ThemeManagerViewConfirm, dialog_ex_get_view(app->confirm_dialog));

    app->reboot_dialog = dialog_ex_alloc();
    dialog_ex_set_result_callback(app->reboot_dialog, theme_manager_reboot_callback);
    dialog_ex_set_context(app->reboot_dialog, app);
    view_set_previous_callback(dialog_ex_get_view(app->reboot_dialog), theme_manager_nav_submenu);
    view_dispatcher_add_view(
        app->view_dispatcher, ThemeManagerViewReboot, dialog_ex_get_view(app->reboot_dialog));

    app->delete_dialog = dialog_ex_alloc();
    dialog_ex_set_result_callback(app->delete_dialog, theme_manager_delete_callback);
    dialog_ex_set_context(app->delete_dialog, app);
    view_set_previous_callback(dialog_ex_get_view(app->delete_dialog), theme_manager_nav_submenu);
    view_dispatcher_add_view(
        app->view_dispatcher,
        ThemeManagerViewDeleteConfirm,
        dialog_ex_get_view(app->delete_dialog));

    app->popup = popup_alloc();
    view_set_previous_callback(popup_get_view(app->popup), theme_manager_nav_submenu);
    view_dispatcher_add_view(
        app->view_dispatcher, ThemeManagerViewPopup, popup_get_view(app->popup));

    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ThemeManagerViewLoading, loading_get_view(app->loading));

    theme_manager_scan_themes(app);
    theme_manager_populate_submenu(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ThemeManagerViewSubmenu);
    view_dispatcher_run(app->view_dispatcher);

    /* Cleanup: free preview data */
    with_view_model(
        app->info_view,
        InfoViewModel * model,
        {
            if(model->frame_data) {
                free(model->frame_data);
                model->frame_data = NULL;
            }
        },
        false);

    view_dispatcher_remove_view(app->view_dispatcher, ThemeManagerViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, ThemeManagerViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, ThemeManagerViewDeleteConfirm);
    view_dispatcher_remove_view(app->view_dispatcher, ThemeManagerViewReboot);
    view_dispatcher_remove_view(app->view_dispatcher, ThemeManagerViewConfirm);
    view_dispatcher_remove_view(app->view_dispatcher, ThemeManagerViewInfo);
    view_dispatcher_remove_view(app->view_dispatcher, ThemeManagerViewSubmenu);

    loading_free(app->loading);
    popup_free(app->popup);
    dialog_ex_free(app->delete_dialog);
    dialog_ex_free(app->reboot_dialog);
    dialog_ex_free(app->confirm_dialog);
    view_free(app->info_view);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    furi_string_free(app->dialog_text);
    free(app);

    return 0;
}
