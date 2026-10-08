#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <gui/icon_animation.h>
#include <gui/view_i.h>
#include <assets_icons.h>
#include <applications.h>
#include <archive/helpers/archive_favorites.h>
#include <toolbox/run_parallel.h>
#include <archive/helpers/archive_helpers_ext.h>

#include "loader.h"
#include "loader_i.h"
#include "loader_menu.h"
#include "loader_menu_storage_i.h"

#include <flipper_application/flipper_application.h>
#include <flipper_application/plugins/plugin_manager.h>
#include <loader/firmware_api/firmware_api.h>
#include <toolbox/stream/file_stream.h>
#include <gui/modules/file_browser.h>
#include <core/dangerous_defines.h>
#include <core/memmgr_heap.h>
#include <cfw/cfw.h>
#include <cfw/game_menu.h>
#include <gui/icon_i.h>
#include <m-list.h>

#define TAG                               "LoaderMenu"
#define LOADER_GAME_MENU_INITIAL_COUNT    18U
#define LOADER_GAME_MENU_PAGE_SIZE        6U
#define LOADER_GAME_MENU_SCROLL_PAGES     4U
#define LOADER_GAME_MENU_SCROLL_STEPS     2U
#define LOADER_GAME_MENU_LOAD_ALL         UINT32_MAX
// Runtime-only limits; leave space for GUI work, metadata parsing and app startup.
#define LOADER_GAME_MENU_MAX_COUNT        1024U
#define LOADER_GAME_MENU_PATH_LIMIT       1024U
#define LOADER_GAME_MENU_LABEL_LIMIT      128U
#define LOADER_GAME_MENU_CATALOG_BUDGET   (32U * 1024U)
#define LOADER_GAME_MENU_METADATA_BUDGET  (128U * 1024U)
#define LOADER_GAME_MENU_HEAP_RESERVE     (16U * 1024U)
#define LOADER_GAME_MENU_WORKING_RESERVE  (8U * 1024U)
// Conservative allowance for an animation, timer, list link and allocator overhead.
#define LOADER_GAME_MENU_ENTRY_OVERHEAD   256U
#define LOADER_GAME_MENU_CATALOG_OVERHEAD 16U

enum {
    LoaderMenuFlagClose = (1U << 0),
    LoaderMenuFlagGameLoad = (1U << 1),
};

typedef enum {
    LoaderMenuViewPrimary,
    LoaderMenuViewSettings,
} LoaderMenuView;

struct LoaderMenu {
    FuriThread* thread;
    void (*closed_cb)(void*);
    void* context;

    Loader* loader;
    FuriPubSubSubscription* subscription;

    uint32_t selected_primary;
    uint32_t selected_setting;
    LoaderMenuView current_view;
    bool settings_only;
    bool games_only;
};

static int32_t loader_menu_thread(void* p);

static void loader_pubsub_callback(const void* message, void* context) {
    const LoaderEvent* event = message;
    LoaderMenu* loader_menu = context;

    if(event->type == LoaderEventTypeApplicationBeforeLoad) {
        if(loader_menu->thread) {
            furi_thread_flags_set(furi_thread_get_id(loader_menu->thread), LoaderMenuFlagClose);
            furi_thread_join(loader_menu->thread);
            furi_thread_free(loader_menu->thread);
            loader_menu->thread = NULL;
        }
    } else if(event->type == LoaderEventTypeNoMoreAppsInQueue) {
        if(!loader_menu->thread) {
            loader_menu->thread = furi_thread_alloc_ex(TAG, 2048, loader_menu_thread, loader_menu);
            furi_thread_start(loader_menu->thread);
        }
    }
}

LoaderMenu* loader_menu_alloc(
    void (*closed_cb)(void*),
    void* context,
    bool settings_only,
    bool games_only) {
    LoaderMenu* loader_menu = malloc(sizeof(LoaderMenu));
    loader_menu->closed_cb = closed_cb;
    loader_menu->context = context;
    loader_menu->games_only = !settings_only && (games_only || cfw_settings.game_mode);
    loader_menu->selected_primary = loader_menu->games_only ? cfw_settings.game_start_point :
                                                              cfw_settings.start_point;
    loader_menu->selected_setting = 0;
    loader_menu->settings_only = settings_only;
    loader_menu->current_view = settings_only ? LoaderMenuViewSettings : LoaderMenuViewPrimary;
    loader_menu->loader = furi_record_open(RECORD_LOADER);

    view_holder_set_back_callback(loader_menu->loader->view_holder, NULL, NULL);
    view_holder_set_view(
        loader_menu->loader->view_holder, loading_get_view(loader_menu->loader->loading));

    loader_menu->subscription = furi_pubsub_subscribe(
        loader_get_pubsub(loader_menu->loader), loader_pubsub_callback, loader_menu);

    loader_menu->thread = furi_thread_alloc_ex(TAG, 2048, loader_menu_thread, loader_menu);
    furi_thread_start(loader_menu->thread);
    return loader_menu;
}

void loader_menu_free(LoaderMenu* loader_menu) {
    furi_assert(loader_menu);

    furi_pubsub_unsubscribe(loader_get_pubsub(loader_menu->loader), loader_menu->subscription);
    furi_record_close(RECORD_LOADER);

    if(loader_menu->thread) {
        furi_thread_join(loader_menu->thread);
        furi_thread_free(loader_menu->thread);
    }

    view_holder_set_view(loader_menu->loader->view_holder, NULL);

    free(loader_menu);
}

typedef struct {
    const char* name;
    const Icon* icon;
    const char* path;
    bool icon_owned;
} MenuApp;

LIST_DEF(MenuAppList, MenuApp, M_POD_OPLIST)
#define M_OPL_MenuAppList_t() LIST_OPLIST(MenuAppList)

typedef struct {
    LoaderMenu* loader_menu;
    Menu* primary_menu;
    View* game_view;
    PluginManager* style_manager;
    Submenu* settings_menu;
    MenuAppList_t apps_list;
    char** game_paths;
    size_t game_path_count;
    size_t game_catalog_bytes;
    size_t game_metadata_bytes;
    bool game_catalog_limited;
    bool game_load_stopped;
    size_t game_catalog_start;
    size_t game_loaded_count;
    size_t game_target_count;
    uint32_t game_load_index;
    FuriMessageQueue* game_load_queue;
    InputKey game_scroll_key;
    uint8_t game_scroll_pages;
    int8_t game_scroll_direction;
} LoaderMenuApp;

static void loader_menu_load_style(LoaderMenuApp* app) {
    char name[32];
    if(app->loader_menu->games_only) {
        const char* plugin = cfw_menu_style_get_plugin_name(cfw_settings.game_menu_style);
        strlcpy(name, plugin ? plugin : "", sizeof(name));
    } else {
        loader_get_menu_style_name(app->loader_menu->loader, name);
    }
    if(!name[0]) return; // List stays available without an SD card.

    PluginManager* manager = plugin_manager_alloc(
        MENU_STYLE_PLUGIN_APP_ID, MENU_STYLE_PLUGIN_API_VERSION, firmware_api_interface);
    FuriString* path = furi_string_alloc_printf("%s/%s", LOADER_MENU_STYLES_PATH, name);
    PluginManagerError error = plugin_manager_load_single(manager, furi_string_get_cstr(path));
    const MenuStylePlugin* style =
        error == PluginManagerErrorNone ? plugin_manager_get_ep(manager, 0) : NULL;
    if(style && style->draw && style->navigate) {
        menu_set_style(app->primary_menu, style);
        app->style_manager = manager;
    } else {
        FURI_LOG_W(TAG, "Style %s unavailable or invalid (%u), using List", name, error);
        plugin_manager_free(manager);
    }
    furi_string_free(path);
}

static void loader_menu_start(const char* name) {
    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_start_detached_with_gui_error(loader, name, NULL);
    furi_record_close(RECORD_LOADER);
}

static void loader_menu_apps_callback(void* context, uint32_t index) {
    LoaderMenuApp* app = context;
    if(app->loader_menu->games_only) {
        if(index >= app->game_path_count) return;
        // The path catalog is immutable while open. Background appends only mutate
        // apps_list, which the GUI must not traverse concurrently.
        loader_menu_start(app->game_paths[index]);
        return;
    }
    if(index >= MenuAppList_size(app->apps_list)) return;
    const MenuApp* menu_app = MenuAppList_get(app->apps_list, index);
    const char* name = menu_app->path ? menu_app->path : menu_app->name;

    const char* extension = menu_app->path ? strrchr(menu_app->path, '.') : NULL;
    if(menu_app->path && (!extension || strcasecmp(extension, ".fap") != 0)) {
        run_with_default_app(menu_app->path);
    } else {
        loader_menu_start(name);
    }
}

static void loader_menu_last_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    const char* path = FLIPPER_EXTERNAL_APPS[FLIPPER_EXTERNAL_APPS_COUNT - 1].name;
    loader_menu_start(path);
}

static void loader_menu_applications_callback(void* context, uint32_t index) {
    UNUSED(index);
    UNUSED(context);
    const char* name = LOADER_APPLICATIONS_NAME;
    loader_menu_start(name);
}

// Can't do this in GUI callbacks because now ViewHolder waits for ongoing
// input, and inputs are not processed because GUI is processing callbacks
static int32_t loader_menu_setting_pin_unpin_parallel(void* context) {
    const char* name = context;
    archive_favorites_handle_setting_pin_unpin(name, NULL);
    return 0;
}

static void
    loader_menu_settings_menu_callback(void* context, InputType input_type, uint32_t index) {
    UNUSED(context);
    const char* name = FLIPPER_SETTINGS_APPS[index].name;

    if(input_type == InputTypeShort) {
        // Workaround for SD format when app can't be opened
        if(!strcmp(name, "Storage")) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            FS_Error status = storage_sd_status(storage);
            furi_record_close(RECORD_STORAGE);
            // If SD card not ready, cannot be formatted, so we want loader to give
            // normal error message, with function below
            if(status != FSE_NOT_READY) {
                // Attempt to launch the app, and if failed offer to format SD card
                run_parallel(loader_menu_storage_settings, storage, 512);
                return;
            }
        }
        loader_menu_start(name);
    } else if(input_type == InputTypeLong) {
        run_parallel(loader_menu_setting_pin_unpin_parallel, (void*)name, 512);
    }
}

// Can't do this in GUI callbacks because now ViewHolder waits for ongoing
// input, and inputs are not processed because GUI is processing callbacks
static void loader_menu_set_view_pending(void* context, uint32_t arg) {
    LoaderMenuApp* app = context;
    view_holder_set_view(app->loader_menu->loader->view_holder, (View*)arg);
}

static void loader_menu_switch_to_settings(void* context, uint32_t index) {
    UNUSED(index);
    LoaderMenuApp* app = context;
    furi_timer_pending_callback(
        loader_menu_set_view_pending, app, (uint32_t)submenu_get_view(app->settings_menu));
    app->loader_menu->current_view = LoaderMenuViewSettings;
}

static void loader_menu_back(void* context) {
    LoaderMenuApp* app = context;
    if(app->loader_menu->current_view == LoaderMenuViewSettings &&
       !app->loader_menu->settings_only) {
        furi_timer_pending_callback(
            loader_menu_set_view_pending, app, (uint32_t)menu_get_view(app->primary_menu));
        app->loader_menu->current_view = LoaderMenuViewPrimary;
    } else {
        furi_thread_flags_set(furi_thread_get_id(app->loader_menu->thread), LoaderMenuFlagClose);
        if(app->loader_menu->closed_cb) {
            app->loader_menu->closed_cb(app->loader_menu->context);
        }
    }
}

static bool loader_menu_game_has_memory(size_t bytes, size_t contiguous) {
    size_t available = memmgr_get_free_heap();
    size_t largest = memmgr_heap_get_max_free_block();
    return available >= LOADER_GAME_MENU_HEAP_RESERVE &&
           bytes <= available - LOADER_GAME_MENU_HEAP_RESERVE &&
           largest >= LOADER_GAME_MENU_CATALOG_OVERHEAD &&
           contiguous <= largest - LOADER_GAME_MENU_CATALOG_OVERHEAD;
}

static bool loader_menu_game_array_bytes(size_t count, size_t* bytes) {
    if(count >= SIZE_MAX / sizeof(MenuItem)) return false;
    *bytes = (count + 1U) * sizeof(MenuItem);
    return true;
}

static size_t loader_menu_game_entry_bytes(size_t label_length) {
    return label_length + 1U + sizeof(MenuItem) + sizeof(MenuApp) + sizeof(Icon) +
           sizeof(const uint8_t*) + CUSTOM_ICON_MAX_SIZE + LOADER_GAME_MENU_ENTRY_OVERHEAD;
}

static bool loader_menu_game_can_append(LoaderMenuApp* app) {
    size_t entry = loader_menu_game_entry_bytes(LOADER_GAME_MENU_LABEL_LIMIT);
    if(app->game_metadata_bytes > LOADER_GAME_MENU_METADATA_BUDGET ||
       entry > LOADER_GAME_MENU_METADATA_BUDGET - app->game_metadata_bytes) {
        return false;
    }
    size_t bytes = 0;
    bool valid = false;
    with_view_model(
        menu_get_view(app->primary_menu),
        MenuModel * model,
        { valid = loader_menu_game_array_bytes(model->count, &bytes); },
        false);
    size_t temporary = entry + LOADER_GAME_MENU_WORKING_RESERVE;
    return valid && bytes <= SIZE_MAX - temporary &&
           loader_menu_game_has_memory(
               bytes + temporary, MAX(bytes, LOADER_GAME_MENU_WORKING_RESERVE));
}

static bool loader_menu_game_add_item(
    LoaderMenuApp* app,
    const char* label,
    const Icon* icon,
    uint32_t index,
    MenuItemCallback callback) {
    bool added = false;
    with_view_model(
        menu_get_view(app->primary_menu),
        MenuModel * model,
        {
            size_t bytes;
            if(model->count <= LOADER_GAME_MENU_MAX_COUNT &&
               loader_menu_game_array_bytes(model->count, &bytes) &&
               bytes <= SIZE_MAX - LOADER_GAME_MENU_ENTRY_OVERHEAD &&
               loader_menu_game_has_memory(bytes + LOADER_GAME_MENU_ENTRY_OVERHEAD, bytes)) {
                // RM realloc() copies the new size from the old allocation. Copy only
                // initialized items here, keeping the shared Menu and its ABI unchanged.
                MenuItem* items = malloc(bytes);
                if(items) {
                    if(model->count) memcpy(items, model->items, model->count * sizeof(MenuItem));
                    IconAnimation* animation = icon_animation_alloc(icon);
                    view_tie_icon_animation(menu_get_view(app->primary_menu), animation);
                    items[model->count] = ((MenuItem){
                        .label = label,
                        .icon = animation,
                        .index = index,
                        .callback = callback,
                        .callback_context = app,
                    });
                    free(model->items);
                    model->items = items;
                    model->count++;
                    // Initial entries precede view_enter(); later appends are nonempty.
                    added = true;
                }
            }
        },
        added);
    return added;
}

static void loader_menu_game_stop(LoaderMenuApp* app) {
    if(app->game_load_stopped) return;
    app->game_load_stopped = true;
    FURI_LOG_W(TAG, "Game Menu limited to %zu loaded games", app->game_loaded_count);
    if(!(furi_thread_flags_get() & LoaderMenuFlagClose)) {
        // Best effort: never allocate the notice by consuming the heap reserve.
        loader_menu_game_add_item(
            app, "Menu limit reached", &A_Plugins_14, LOADER_GAME_MENU_LOAD_ALL, NULL);
    }
}

static bool loader_menu_add_app_entry(
    LoaderMenuApp* app,
    const char* name,
    const Icon* icon,
    const char* path,
    bool icon_owned) {
    if(app->loader_menu->games_only) {
        size_t length = strlen(name);
        if(length > LOADER_GAME_MENU_LABEL_LIMIT) return false;
        size_t bytes = loader_menu_game_entry_bytes(length);
        if(app->game_metadata_bytes > LOADER_GAME_MENU_METADATA_BUDGET ||
           bytes > LOADER_GAME_MENU_METADATA_BUDGET - app->game_metadata_bytes ||
           !loader_menu_game_add_item(
               app, name, icon, app->game_load_index, loader_menu_apps_callback)) {
            return false;
        }
        MenuAppList_push_back(app->apps_list, (MenuApp){name, icon, path, icon_owned});
        app->game_metadata_bytes += bytes;
        return true;
    }
    MenuAppList_push_back(app->apps_list, (MenuApp){name, icon, path, icon_owned});
    size_t index = MenuAppList_size(app->apps_list) - 1;
    menu_add_item(app->primary_menu, name, icon, index, loader_menu_apps_callback, app);
    return true;
}

static const Icon* loader_menu_get_ext_icon(Storage* storage, const char* path) {
    if(storage_dir_exists(storage, path)) return &I_dir_10px;
    const char* ext = strrchr(path, '.');
    if(ext && strcasecmp(ext, ".js") == 0) return &I_js_script_10px;

    return &I_file_10px;
}

bool loader_menu_load_fap_meta(
    Storage* storage,
    FuriString* path,
    FuriString* name,
    const Icon** icon) {
    *icon = NULL;
    uint8_t* icon_buf = malloc(CUSTOM_ICON_MAX_SIZE);
    if(!flipper_application_load_name_and_icon(path, storage, &icon_buf, name)) {
        free(icon_buf);
        icon_buf = NULL;
        return false;
    }
    *icon = malloc(sizeof(Icon));
    FURI_CONST_ASSIGN((*icon)->frame_count, 1);
    FURI_CONST_ASSIGN((*icon)->frame_rate, 1);
    FURI_CONST_ASSIGN((*icon)->width, 10);
    FURI_CONST_ASSIGN((*icon)->height, 10);
    FURI_CONST_ASSIGN_PTR((*icon)->frames, malloc(sizeof(const uint8_t*)));
    FURI_CONST_ASSIGN_PTR((*icon)->frames[0], icon_buf);
    return true;
}

static bool loader_menu_find_add_app(LoaderMenuApp* app, Storage* storage, FuriString* line) {
    const char* name = NULL;
    const Icon* icon = NULL;
    const char* path = NULL;
    bool icon_owned = false;
    if(furi_string_start_with(line, "/")) {
        path = app->loader_menu->games_only ? app->game_paths[app->game_load_index] :
                                              strdup(furi_string_get_cstr(line));
        icon_owned = loader_menu_load_fap_meta(storage, line, line, &icon);
        if(!icon_owned) {
            icon = loader_menu_get_ext_icon(storage, path);
        }
        if(app->loader_menu->games_only) furi_string_left(line, LOADER_GAME_MENU_LABEL_LIMIT);
        name = strdup(furi_string_get_cstr(line));
    } else {
        for(size_t i = 0; !name && i < FLIPPER_APPS_COUNT; i++) {
            if(furi_string_equal(line, FLIPPER_APPS[i].name)) {
                name = FLIPPER_APPS[i].name;
                icon = FLIPPER_APPS[i].icon;
            }
        }
        for(size_t i = 0; !name && i < FLIPPER_EXTERNAL_APPS_COUNT; i++) {
            if(furi_string_equal(line, FLIPPER_EXTERNAL_APPS[i].name)) {
                name = FLIPPER_EXTERNAL_APPS[i].name;
                icon = FLIPPER_EXTERNAL_APPS[i].icon;
            }
        }
    }
    // Path only set for FAPs
    if(name && icon) {
        if(loader_menu_add_app_entry(app, name, icon, path, icon_owned)) return true;
        if(path) {
            free((void*)name);
            if(icon_owned) {
                free((void*)icon->frames[0]);
                free((void*)icon->frames);
                free((void*)icon);
            }
            if(!app->loader_menu->games_only) free((void*)path);
        }
    }
    return false;
}

static void loader_menu_build_menu(LoaderMenuApp* app, LoaderMenu* menu) {
    menu_add_item(
        app->primary_menu,
        LOADER_APPLICATIONS_NAME,
        &A_Plugins_14,
        LoaderMenuIndexApplications,
        loader_menu_applications_callback,
        NULL);

    MenuAppList_init(app->apps_list);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    uint32_t version;
    if(file_stream_open(stream, MAINMENU_APPS_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       stream_read_line(stream, line) &&
       sscanf(furi_string_get_cstr(line), "MenuAppList Version %lu", &version) == 1 &&
       version <= 1) {
        while(stream_read_line(stream, line)) {
            furi_string_trim(line);
            if(version == 0) {
                if(furi_string_equal(line, "RFID")) {
                    furi_string_set(line, "125 kHz RFID");
                } else if(furi_string_equal(line, "SubGHz")) {
                    furi_string_set(line, "Sub-GHz");
                } else if(furi_string_equal(line, "CFW")) {
                    furi_string_set(line, "CFW Settings");
                }
            }
            loader_menu_find_add_app(app, storage, line);
        }
    } else {
        for(size_t i = 0; i < FLIPPER_APPS_COUNT; i++) {
            loader_menu_add_app_entry(
                app, FLIPPER_APPS[i].name, FLIPPER_APPS[i].icon, NULL, false);
        }
        // Until count - 1 because last app is hardcoded below
        for(size_t i = 0; i < FLIPPER_EXTERNAL_APPS_COUNT - 1; i++) {
            loader_menu_add_app_entry(
                app, FLIPPER_EXTERNAL_APPS[i].name, FLIPPER_EXTERNAL_APPS[i].icon, NULL, false);
        }
    }
    furi_string_free(line);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);

    const FlipperExternalApplication* last =
        &FLIPPER_EXTERNAL_APPS[FLIPPER_EXTERNAL_APPS_COUNT - 1];
    menu_add_item(
        app->primary_menu,
        last->name,
        last->icon,
        LoaderMenuIndexLast,
        loader_menu_last_callback,
        NULL);
    menu_add_item(
        app->primary_menu,
        "Settings",
        &A_Settings_14,
        LoaderMenuIndexSettings,
        loader_menu_switch_to_settings,
        app);

    uint32_t selected = menu->selected_primary;
    if(selected >= MenuAppList_size(app->apps_list) && selected != LoaderMenuIndexApplications &&
       selected != LoaderMenuIndexLast && selected != LoaderMenuIndexSettings) {
        selected = LoaderMenuIndexApplications;
    }
    menu_set_selected_item(app->primary_menu, selected);
}

static void loader_menu_clear_apps(LoaderMenuApp* app) {
    for
        M_EACH(menu_app, app->apps_list, MenuAppList_t) {
            // The menu must release its icon animations before these borrowed assets.
            if(menu_app->path) {
                free((void*)menu_app->name);
                if(menu_app->icon_owned) {
                    free((void*)menu_app->icon->frames[0]);
                    free((void*)menu_app->icon->frames);
                    free((void*)menu_app->icon);
                }
                if(!app->loader_menu->games_only) free((void*)menu_app->path);
            }
        }
    MenuAppList_clear(app->apps_list);
}

static void loader_menu_request_game_load(LoaderMenuApp* app, uint32_t requested) {
    // Input dispatch is the sole producer; the loader worker is the consumer.
    // Merge a full mailbox instead of losing a held-Right request behind prefetch.
    FuriStatus status = furi_message_queue_put(app->game_load_queue, &requested, 0);
    if(status != FuriStatusOk) {
        uint32_t pending;
        if(furi_message_queue_get(app->game_load_queue, &pending, 0) == FuriStatusOk) {
            requested = MAX(requested, pending);
        }
        status = furi_message_queue_put(app->game_load_queue, &requested, 0);
    }
    if(status == FuriStatusOk) {
        furi_thread_flags_set(
            furi_thread_get_id(app->loader_menu->thread), LoaderMenuFlagGameLoad);
    }
}

static bool
    loader_menu_game_input_callback(void* context, const InputEvent* event, uint32_t index) {
    LoaderMenuApp* app = context;
    size_t count = app->game_path_count;
    if(!count) return false;

    if(event->key == InputKeyRight && event->type == InputTypeLong) {
        loader_menu_request_game_load(app, LOADER_GAME_MENU_LOAD_ALL);
        return true;
    }
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(index >= count) return false;
    if(event->key != InputKeyUp && event->key != InputKeyDown && event->key != InputKeyLeft &&
       event->key != InputKeyRight) {
        return false;
    }

    // Groups of six are the loading pages, independent of the selected renderer.
    // Keep two pages ahead: page 1 -> 18, page 2 -> 24, page 3 -> 30, etc.
    size_t position = index >= app->game_catalog_start ? index - app->game_catalog_start :
                                                         count - app->game_catalog_start + index;
    size_t page_start = position - position % LOADER_GAME_MENU_PAGE_SIZE;
    size_t target = count > LOADER_GAME_MENU_INITIAL_COUNT &&
                            page_start < count - LOADER_GAME_MENU_INITIAL_COUNT ?
                        LOADER_GAME_MENU_INITIAL_COUNT + page_start :
                        count;
    if(target > LOADER_GAME_MENU_INITIAL_COUNT) {
        loader_menu_request_game_load(app, MIN(target, count));
    }
    return false;
}

static void loader_menu_game_view_draw(Canvas* canvas, void* model) {
    LoaderMenuApp* app = *(LoaderMenuApp**)model;
    if(app) view_draw(menu_get_view(app->primary_menu), canvas);
}

static size_t loader_menu_game_scroll_page(LoaderMenuApp* app, size_t* pages) {
    size_t page;
    with_view_model(
        menu_get_view(app->primary_menu),
        MenuModel * model,
        {
            page = model->position / LOADER_GAME_MENU_PAGE_SIZE;
            *pages = model->count / LOADER_GAME_MENU_PAGE_SIZE +
                     (model->count % LOADER_GAME_MENU_PAGE_SIZE != 0);
        },
        false);
    return page;
}

static bool loader_menu_game_view_input(InputEvent* event, void* context) {
    LoaderMenuApp* app = context;
    bool direction = event->key == InputKeyUp || event->key == InputKeyDown ||
                     event->key == InputKeyLeft || event->key == InputKeyRight;
    if(!direction || event->key != app->game_scroll_key || event->type == InputTypePress ||
       event->type == InputTypeRelease || event->type == InputTypeShort) {
        app->game_scroll_pages = 0;
    }
    app->game_scroll_key = event->key;
    bool repeat = direction && event->type == InputTypeRepeat;
    // Speed up only after four consecutive six-item page advances, starting
    // with the next repeat. Taps and a new hold always use the original speed.
    bool fast = repeat && app->game_scroll_pages >= LOADER_GAME_MENU_SCROLL_PAGES;
    size_t pages = 0;
    size_t page = repeat && !fast ? loader_menu_game_scroll_page(app, &pages) : 0;
    bool consumed = false;
    for(size_t step = 0; step < (fast ? LOADER_GAME_MENU_SCROLL_STEPS : 1U); step++) {
        consumed |= view_input(menu_get_view(app->primary_menu), event);
        if(repeat && !fast) {
            size_t next = loader_menu_game_scroll_page(app, &pages);
            if(page < pages && next < pages) {
                size_t distance = page > next ? page - next : next - page;
                bool forward = next > page;
                if(distance > pages - distance) {
                    distance = pages - distance;
                    forward = !forward;
                }
                if(distance) {
                    int8_t move = forward ? 1 : -1;
                    // Column/row toggles are not continuous scrolling.
                    if(move != app->game_scroll_direction) app->game_scroll_pages = 0;
                    app->game_scroll_direction = move;
                    size_t remaining = LOADER_GAME_MENU_SCROLL_PAGES - app->game_scroll_pages;
                    app->game_scroll_pages += MIN(distance, remaining);
                }
            }
        }
        if((direction && (event->type == InputTypeShort || repeat)) ||
           (event->key == InputKeyRight && event->type == InputTypeLong)) {
            // Each move uses the original style, then prefetches outside its mutex.
            consumed |= loader_menu_game_input_callback(
                app, event, menu_get_selected_item(app->primary_menu));
        }
    }
    return consumed;
}

static void loader_menu_game_view_enter(void* context) {
    LoaderMenuApp* app = context;
    app->game_scroll_pages = 0;
    view_enter(menu_get_view(app->primary_menu));
}

static void loader_menu_game_view_exit(void* context) {
    LoaderMenuApp* app = context;
    view_exit(menu_get_view(app->primary_menu));
}

static void loader_menu_game_view_update(View* view, void* context) {
    UNUSED(view);
    LoaderMenuApp* app = context;
    with_view_model(app->game_view, LoaderMenuApp * *model, { UNUSED(model); }, true);
}

static void loader_menu_build_game_view(LoaderMenuApp* app) {
    // Only this loader instance is wrapped. Keep the shared Menu implementation,
    // header and SDK interface unchanged for JavaScript and other FAP/FAL users.
    app->game_view = view_alloc();
    view_set_context(app->game_view, app);
    view_allocate_model(app->game_view, ViewModelTypeLocking, sizeof(LoaderMenuApp*));
    with_view_model(app->game_view, LoaderMenuApp * *model, { *model = app; }, false);
    view_set_draw_callback(app->game_view, loader_menu_game_view_draw);
    view_set_input_callback(app->game_view, loader_menu_game_view_input);
    view_set_enter_callback(app->game_view, loader_menu_game_view_enter);
    view_set_exit_callback(app->game_view, loader_menu_game_view_exit);
    View* menu_view = menu_get_view(app->primary_menu);
    view_set_update_callback_context(menu_view, app);
    view_set_update_callback(menu_view, loader_menu_game_view_update);
}

static void loader_menu_append_games(LoaderMenuApp* app, size_t target) {
    size_t count = app->game_path_count;
    target = MIN(target, count);
    if(target <= app->game_loaded_count) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    while(app->game_loaded_count < target) {
        if(furi_thread_flags_get() & LoaderMenuFlagClose) break;
        if(!loader_menu_game_can_append(app)) {
            loader_menu_game_stop(app);
            break;
        }
        // Rotate the catalog at the initial 18-item block. This preserves a late
        // saved selection without reading all earlier FAPs before attachment.
        size_t remaining = count - app->game_catalog_start;
        size_t index = app->game_loaded_count < remaining ?
                           app->game_catalog_start + app->game_loaded_count :
                           app->game_loaded_count - remaining;
        app->game_load_index = index;
        FuriString* line = furi_string_alloc();
        // Reserve once from the empty string; path/name changes then need no growth.
        furi_string_reserve(line, LOADER_GAME_MENU_PATH_LIMIT + 1U);
        furi_string_set_str(line, app->game_paths[index]);
        bool added = loader_menu_find_add_app(app, storage, line);
        furi_string_free(line);
        if(!added) {
            loader_menu_game_stop(app);
            break;
        }
        app->game_loaded_count++;
    }
    furi_record_close(RECORD_STORAGE);
    if(app->game_loaded_count == count && app->game_catalog_limited) loader_menu_game_stop(app);
}

static void loader_menu_add_game(const char* path, void* context) {
    LoaderMenuApp* app = context;
    if(app->game_catalog_limited) return;
    size_t length = strnlen(path, LOADER_GAME_MENU_PATH_LIMIT + 1U);
    size_t bytes = length + 1U + LOADER_GAME_MENU_CATALOG_OVERHEAD;
    if(!app->game_paths || app->game_path_count >= LOADER_GAME_MENU_MAX_COUNT ||
       length > LOADER_GAME_MENU_PATH_LIMIT ||
       app->game_catalog_bytes > LOADER_GAME_MENU_CATALOG_BUDGET ||
       bytes > LOADER_GAME_MENU_CATALOG_BUDGET - app->game_catalog_bytes ||
       !loader_menu_game_has_memory(bytes + LOADER_GAME_MENU_WORKING_RESERVE, bytes)) {
        app->game_catalog_limited = true;
        return;
    }
    app->game_paths[app->game_path_count++] = strdup(path);
    app->game_catalog_bytes += bytes;
}

static void loader_menu_build_games(LoaderMenuApp* app, LoaderMenu* menu) {
    MenuAppList_init(app->apps_list);
    app->game_load_queue = furi_message_queue_alloc(1, sizeof(uint32_t));
    // A fixed pointer table avoids reallocating the immutable launch catalog.
    size_t bytes = LOADER_GAME_MENU_MAX_COUNT * sizeof(char*);
    if(loader_menu_game_has_memory(bytes + LOADER_GAME_MENU_WORKING_RESERVE, bytes)) {
        app->game_paths = malloc(bytes);
    }
    if(app->game_paths) {
        app->game_catalog_bytes = bytes + LOADER_GAME_MENU_CATALOG_OVERHEAD;
        Storage* storage = furi_record_open(RECORD_STORAGE);
        game_menu_load(storage, loader_menu_add_game, app);
        furi_record_close(RECORD_STORAGE);
    } else {
        app->game_catalog_limited = true;
    }

    size_t count = app->game_path_count;
    uint32_t selected = menu->selected_primary < count ? menu->selected_primary : 0;
    app->game_catalog_start =
        (selected / LOADER_GAME_MENU_INITIAL_COUNT) * LOADER_GAME_MENU_INITIAL_COUNT;
    app->game_target_count = MIN(LOADER_GAME_MENU_INITIAL_COUNT, count);
    loader_menu_append_games(app, app->game_target_count);
    if(!count) {
        if(app->game_catalog_limited) {
            loader_menu_game_stop(app);
        } else {
            loader_menu_game_add_item(app, "No games found", &A_Plugins_14, 0, NULL);
        }
    }
    menu_set_selected_item(app->primary_menu, selected);
    loader_menu_build_game_view(app);

    // A saved selection can already be on loading page 2 or 3. Start its prefetch
    // after the initial 18 are attached, through the same nonblocking mailbox.
    InputEvent initial = {.key = InputKeyDown, .type = InputTypeShort};
    loader_menu_game_input_callback(app, &initial, selected);
}

static void loader_menu_process_game_load(LoaderMenuApp* app) {
    size_t count = app->game_path_count;
    while(true) {
        uint32_t requested;
        while(furi_message_queue_get(app->game_load_queue, &requested, 0) == FuriStatusOk) {
            // The target only grows, so ordinary navigation cannot downgrade a
            // load-all request already being processed.
            app->game_target_count = MAX(app->game_target_count, MIN(requested, count));
        }
        if(app->game_load_stopped || (furi_thread_flags_get() & LoaderMenuFlagClose) ||
           app->game_loaded_count >= app->game_target_count) {
            return;
        }
        size_t end = app->game_loaded_count + MIN(LOADER_GAME_MENU_PAGE_SIZE,
                                                  app->game_target_count - app->game_loaded_count);
        loader_menu_append_games(app, end);
        // Leave the menu attached. Each addition is protected by its model mutex,
        // and the loader input handler runs outside it; no view switch waits for Right
        // to be released. Check requests/close between every six-entry batch.
        furi_thread_yield();
    }
}

static void loader_menu_build_submenu(LoaderMenuApp* app, LoaderMenu* loader_menu) {
    for(size_t i = 0; i < FLIPPER_SETTINGS_APPS_COUNT; i++) {
        submenu_add_item_ex(
            app->settings_menu,
            FLIPPER_SETTINGS_APPS[i].name,
            i,
            loader_menu_settings_menu_callback,
            NULL);
    }
    submenu_set_selected_item(app->settings_menu, loader_menu->selected_setting);
}

static LoaderMenuApp* loader_menu_app_alloc(LoaderMenu* loader_menu) {
    LoaderMenuApp* app = malloc(sizeof(LoaderMenuApp));
    app->loader_menu = loader_menu;
    app->primary_menu = NULL;
    app->game_view = NULL;
    app->settings_menu = NULL;
    app->style_manager = NULL;
    app->game_paths = NULL;
    app->game_path_count = 0;
    app->game_catalog_bytes = 0;
    app->game_metadata_bytes = 0;
    app->game_catalog_limited = false;
    app->game_load_stopped = false;
    app->game_catalog_start = 0;
    app->game_loaded_count = 0;
    app->game_target_count = 0;
    app->game_load_index = 0;
    app->game_load_queue = NULL;
    app->game_scroll_key = InputKeyBack;
    app->game_scroll_pages = 0;
    app->game_scroll_direction = 0;

    // Primary menu
    if(!app->loader_menu->settings_only) {
        app->primary_menu = menu_alloc();
        if(loader_menu->games_only) {
            loader_menu_build_games(app, loader_menu);
        } else {
            loader_menu_build_menu(app, loader_menu);
        }
        loader_menu_load_style(app);
    }

    // Settings menu
    if(!loader_menu->games_only) {
        app->settings_menu = submenu_alloc();
        loader_menu_build_submenu(app, loader_menu);
    }

    View* view = app->loader_menu->current_view == LoaderMenuViewSettings ?
                     submenu_get_view(app->settings_menu) :
                     (app->game_view ? app->game_view : menu_get_view(app->primary_menu));
    if(!(furi_thread_flags_get() & LoaderMenuFlagClose)) {
        view_holder_set_view(app->loader_menu->loader->view_holder, view);
        view_holder_set_back_callback(
            app->loader_menu->loader->view_holder, loader_menu_back, app);
    }

    return app;
}

static void loader_menu_app_free(LoaderMenuApp* app) {
    view_holder_set_back_callback(app->loader_menu->loader->view_holder, NULL, NULL);
    view_holder_set_view(
        app->loader_menu->loader->view_holder,
        loading_get_view(app->loader_menu->loader->loading));

    if(!app->loader_menu->settings_only) {
        uint32_t selected = menu_get_selected_item(app->primary_menu);
        if(app->loader_menu->games_only && selected >= app->game_path_count) {
            selected = app->game_catalog_start;
        }
        app->loader_menu->selected_primary = selected;
        if(app->game_view) {
            // Wait for the wrapper draw to finish before destroying its child.
            with_view_model(app->game_view, LoaderMenuApp * *model, { *model = NULL; }, false);
        }
        // Detach the plugin vtable under the model mutex before its image can be unmapped.
        menu_set_style(app->primary_menu, NULL);
        menu_free(app->primary_menu);
        // Keep the wrapper alive until all child menu/animation timers are freed.
        if(app->game_view) view_free(app->game_view);
        if(app->style_manager) plugin_manager_free(app->style_manager);
        loader_menu_clear_apps(app);
    }
    for(size_t i = 0; i < app->game_path_count; i++) {
        free(app->game_paths[i]);
    }
    free(app->game_paths);
    if(app->game_load_queue) furi_message_queue_free(app->game_load_queue);
    app->loader_menu->selected_setting = app->loader_menu->current_view == LoaderMenuViewSettings ?
                                             submenu_get_selected_item(app->settings_menu) :
                                             0;
    if(app->settings_menu) submenu_free(app->settings_menu);

    free(app);
}

static int32_t loader_menu_thread(void* p) {
    LoaderMenu* loader_menu = p;
    furi_assert(loader_menu);

    LoaderMenuApp* app = loader_menu_app_alloc(loader_menu);

    while(true) {
        uint32_t flags = furi_thread_flags_wait(
            LoaderMenuFlagClose | LoaderMenuFlagGameLoad, FuriFlagWaitAny, FuriWaitForever);
        if(flags & (LoaderMenuFlagClose | FuriFlagError)) break;
        if((flags & LoaderMenuFlagGameLoad) && loader_menu->games_only) {
            loader_menu_process_game_load(app);
        }
    }

    loader_menu_app_free(app);

    return 0;
}
