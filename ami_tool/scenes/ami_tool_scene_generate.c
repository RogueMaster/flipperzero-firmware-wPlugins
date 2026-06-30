#include "ami_tool_i.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define AMI_TOOL_GENERATE_READ_BUFFER 64
#define AMI_TOOL_GENERATE_MENU_INDEX_PREV_PAGE UINT32_C(0xFFFFFFFE)
#define AMI_TOOL_GENERATE_MENU_INDEX_NEXT_PAGE UINT32_C(0xFFFFFFFD)

typedef enum {
    AmiToolGenerateRootMenuIndexByName,
    AmiToolGenerateRootMenuIndexByGames,
} AmiToolGenerateRootMenuIndex;

typedef bool (*AmiToolGenerateLineCallback)(void* context, size_t index, FuriString* line);

static bool ami_tool_scene_generate_iterate_lines(
    AmiToolApp* app,
    const char* path,
    bool skip_header,
    AmiToolGenerateLineCallback callback,
    void* context);
static bool ami_tool_scene_generate_iterate_games(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    AmiToolGenerateLineCallback callback,
    void* context);
static bool ami_tool_scene_generate_find_mapping_for_game(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    const FuriString* game_name,
    FuriString* out_ids);
static bool ami_tool_scene_generate_fill_page_names(AmiToolApp* app);
static bool ami_tool_scene_generate_load_name_page(AmiToolApp* app);
static bool ami_tool_scene_generate_load_game_page(AmiToolApp* app);
static bool ami_tool_scene_generate_load_page_entries(AmiToolApp* app);
static void ami_tool_scene_generate_show_amiibo_menu(AmiToolApp* app, size_t game_index);
static bool ami_tool_scene_generate_show_cached_amiibo_menu(AmiToolApp* app);
static void ami_tool_scene_generate_change_page(AmiToolApp* app, int direction);
static void ami_tool_scene_generate_show_name_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_amiibo_placeholder(AmiToolApp* app, size_t amiibo_index);
static void ami_tool_scene_generate_clear_selected_game(AmiToolApp* app);
static int8_t ami_tool_scene_generate_hex_value(char ch);
static bool ami_tool_scene_generate_parse_uuid(const char* hex, uint8_t* out, size_t out_len);
static bool ami_tool_scene_generate_prepare_dump(AmiToolApp* app, const char* id_hex);

static void ami_tool_scene_generate_clear_selected_game(AmiToolApp* app) {
    if(app->generate_selected_game) {
        furi_string_reset(app->generate_selected_game);
    }
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;
    app->generate_list_source = AmiToolGenerateListSourceGame;
}

void ami_tool_generate_clear_amiibo_cache(AmiToolApp* app) {
    if(!app) return;

    for(size_t i = 0; i < AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS; i++) {
        if(app->generate_page_names[i]) {
            furi_string_reset(app->generate_page_names[i]);
        }
        if(app->generate_page_ids[i]) {
            furi_string_reset(app->generate_page_ids[i]);
        }
    }

    app->generate_page_entry_count = 0;
    app->generate_amiibo_count = 0;
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;
}

static int8_t ami_tool_scene_generate_hex_value(char ch) {
    if(ch >= '0' && ch <= '9') {
        return (int8_t)(ch - '0');
    }
    ch = (char)tolower((unsigned char)ch);
    if(ch >= 'a' && ch <= 'f') {
        return (int8_t)(10 + (ch - 'a'));
    }
    return -1;
}

static bool ami_tool_scene_generate_parse_uuid(const char* hex, uint8_t* out, size_t out_len) {
    if(!hex || !out || out_len == 0) {
        return false;
    }
    size_t required = out_len * 2;
    if(strlen(hex) < required) {
        return false;
    }
    for(size_t i = 0; i < out_len; i++) {
        char hi_char = hex[i * 2];
        char lo_char = hex[i * 2 + 1];
        if(hi_char == '\0' || lo_char == '\0') {
            return false;
        }
        int8_t hi = ami_tool_scene_generate_hex_value(hi_char);
        int8_t lo = ami_tool_scene_generate_hex_value(lo_char);
        if(hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static bool ami_tool_scene_generate_prepare_dump(AmiToolApp* app, const char* id_hex) {
    if(!app || !app->tag_data || !id_hex) {
        return false;
    }

    uint8_t uuid[8];
    if(!ami_tool_scene_generate_parse_uuid(id_hex, uuid, sizeof(uuid))) {
        return false;
    }

    ami_tool_clear_cached_tag(app);

    Ntag21xMetadataHeader header;
    RfidxStatus status = amiibo_generate(uuid, app->tag_data, &header);
    if(status != RFIDX_OK) {
        return false;
    }

    if(!ami_tool_has_retail_key(app)) {
        return false;
    }

    const DumpedKeys* keys = (const DumpedKeys*)app->retail_key;
    DerivedKey data_key = {0};
    DerivedKey tag_key = {0};

    status = amiibo_derive_key(&keys->data, app->tag_data, &data_key);
    if(status != RFIDX_OK) {
        return false;
    }
    status = amiibo_derive_key(&keys->tag, app->tag_data, &tag_key);
    if(status != RFIDX_OK) {
        return false;
    }

    status = amiibo_sign_payload(&tag_key, &data_key, app->tag_data);
    if(status != RFIDX_OK) {
        return false;
    }

    status = amiibo_cipher(&data_key, app->tag_data);
    if(status != RFIDX_OK) {
        return false;
    }

    if(app->tag_data->pages_total < 2) {
        return false;
    }

    uint8_t uid[7];
    memcpy(uid, app->tag_data->page[0].data, 3);
    memcpy(uid + 3, app->tag_data->page[1].data, 4);
    ami_tool_store_uid(app, uid, sizeof(uid));
    if(ami_tool_compute_password_from_uid(uid, sizeof(uid), &app->tag_password)) {
        app->tag_password_valid = true;
    } else {
        app->tag_password_valid = false;
        memset(&app->tag_password, 0, sizeof(app->tag_password));
    }
    static const uint8_t default_pack[4] = {0x80, 0x80, 0x00, 0x00};
    memcpy(app->tag_pack, default_pack, sizeof(default_pack));
    app->tag_pack_valid = true;

    app->tag_data_valid = true;
    return true;
}

static void ami_tool_scene_generate_submenu_callback(void* context, uint32_t index);
static void ami_tool_scene_generate_show_root_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_platform_menu(AmiToolApp* app);
static void ami_tool_scene_generate_show_games_menu(AmiToolApp* app);
static void ami_tool_scene_generate_commit_text_view(
    AmiToolApp* app,
    AmiToolGenerateState state,
    AmiToolGenerateState return_state);
static bool ami_tool_scene_generate_get_game_name(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    size_t target_index,
    FuriString* out);
static const char* ami_tool_scene_generate_platform_label(AmiToolGeneratePlatform platform);
static const char* ami_tool_scene_generate_platform_mapping_path(AmiToolGeneratePlatform platform);
static void ami_tool_scene_generate_return_to_state(AmiToolApp* app, AmiToolGenerateState state);

typedef struct {
    AmiToolApp* app;
    size_t count;
} AmiToolGenerateListBuildContext;

static bool ami_tool_scene_generate_add_game_callback(
    void* context,
    size_t index,
    FuriString* name) {
    AmiToolGenerateListBuildContext* ctx = context;
    submenu_add_item(
        ctx->app->submenu,
        furi_string_get_cstr(name),
        index,
        ami_tool_scene_generate_submenu_callback,
        ctx->app);
    ctx->count = index + 1;
    return true;
}

typedef struct {
    size_t target_index;
    FuriString* result;
    bool found;
} AmiToolGenerateFindGameContext;

static bool ami_tool_scene_generate_find_game_callback(
    void* context,
    size_t index,
    FuriString* name) {
    AmiToolGenerateFindGameContext* find_ctx = context;
    if(index == find_ctx->target_index) {
        furi_string_set(find_ctx->result, name);
        find_ctx->found = true;
        return false;
    }
    return true;
}

static void ami_tool_scene_generate_show_root_menu(AmiToolApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Generate Amiibo");
    submenu_add_item(
        app->submenu,
        "Select by Name",
        AmiToolGenerateRootMenuIndexByName,
        ami_tool_scene_generate_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Select by Games",
        AmiToolGenerateRootMenuIndexByGames,
        ami_tool_scene_generate_submenu_callback,
        app);
    ami_tool_generate_clear_amiibo_cache(app);
    ami_tool_scene_generate_clear_selected_game(app);
    app->generate_state = AmiToolGenerateStateRootMenu;
    app->generate_return_state = AmiToolGenerateStateRootMenu;
    app->generate_game_count = 0;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

static void ami_tool_scene_generate_show_platform_menu(AmiToolApp* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Choose Platform");
    for(size_t platform = 0; platform < AmiToolGeneratePlatformCount; platform++) {
        submenu_add_item(
            app->submenu,
            ami_tool_scene_generate_platform_label((AmiToolGeneratePlatform)platform),
            platform,
            ami_tool_scene_generate_submenu_callback,
            app);
    }
    app->generate_state = AmiToolGenerateStatePlatformMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

static void ami_tool_scene_generate_show_games_menu(AmiToolApp* app) {
    submenu_reset(app->submenu);
    ami_tool_generate_clear_amiibo_cache(app);
    ami_tool_scene_generate_clear_selected_game(app);
    furi_string_printf(
        app->text_box_store, "%s Games", ami_tool_scene_generate_platform_label(app->generate_platform));
    submenu_set_header(app->submenu, furi_string_get_cstr(app->text_box_store));

    AmiToolGenerateListBuildContext ctx = {
        .app = app,
        .count = 0,
    };

    bool file_ok = ami_tool_scene_generate_iterate_games(
        app, app->generate_platform, ami_tool_scene_generate_add_game_callback, &ctx);

    if(!file_ok) {
        furi_string_printf(
            app->text_box_store,
            "Unable to read the %s game list.\n\nEnsure the assets folder is installed.",
            ami_tool_scene_generate_platform_label(app->generate_platform));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStatePlatformMenu);
        return;
    }

    app->generate_game_count = ctx.count;
    if(app->generate_game_count == 0) {
        furi_string_printf(
            app->text_box_store,
            "No games found for %s.\n\nUpdate or regenerate your assets.",
            ami_tool_scene_generate_platform_label(app->generate_platform));
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStatePlatformMenu);
        return;
    }

    app->generate_state = AmiToolGenerateStateGameList;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
}

typedef struct {
    AmiToolApp* app;
    size_t start;
    size_t end;
    size_t total;
} AmiToolGenerateNamePageContext;

static bool ami_tool_scene_generate_name_page_callback(
    void* context,
    size_t index,
    FuriString* line) {
    AmiToolGenerateNamePageContext* ctx = context;
    if(!ctx || furi_string_empty(line)) {
        return true;
    }

    const char* raw = furi_string_get_cstr(line);
    const char* colon = strchr(raw, ':');
    if(!colon) {
        return true;
    }

    size_t name_len = (size_t)(colon - raw);
    while(name_len > 0 && raw[name_len - 1] == ' ') {
        name_len--;
    }
    const char* value = colon + 1;
    while(*value == ' ' || *value == '\t') {
        value++;
    }

    ctx->total++;

    if(index < ctx->start || index >= ctx->end) {
        return true;
    }

    AmiToolApp* app = ctx->app;
    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;
    if(app->generate_page_entry_count >= page_size) {
        return false;
    }

    size_t slot = ctx->app->generate_page_entry_count;
    FuriString* name_slot = ctx->app->generate_page_names[slot];
    FuriString* id_slot = ctx->app->generate_page_ids[slot];
    if(!name_slot || !id_slot) {
        return false;
    }

    ctx->app->generate_page_entry_count++;
    furi_string_set_strn(name_slot, raw, name_len);
    furi_string_set(id_slot, value);
    furi_string_trim(id_slot);
    return true;
}

static bool ami_tool_scene_generate_load_name_page(AmiToolApp* app) {
    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;
    size_t start = app->generate_page_offset;
    size_t end = start + page_size;

    app->generate_page_entry_count = 0;
    AmiToolGenerateNamePageContext context = {
        .app = app,
        .start = start,
        .end = end,
        .total = 0,
    };

    bool ok = ami_tool_scene_generate_iterate_lines(
        app,
        APP_ASSETS_PATH("amiibo_name.dat"),
        true,
        ami_tool_scene_generate_name_page_callback,
        &context);

    app->generate_amiibo_count = context.total;
    return ok && context.total > 0;
}

typedef struct {
    AmiToolApp* app;
    size_t remaining;
} AmiToolGenerateNameFillContext;

static bool ami_tool_scene_generate_fill_names_callback(
    void* context,
    size_t index,
    FuriString* line) {
    UNUSED(index);
    AmiToolGenerateNameFillContext* ctx = context;
    const char* raw = furi_string_get_cstr(line);
    const char* colon = strchr(raw, ':');
    if(!colon) {
        return true;
    }

    size_t id_len = (size_t)(colon - raw);
    AmiToolApp* app = ctx->app;

    for(size_t i = 0; i < app->generate_page_entry_count; i++) {
        if(!app->generate_page_ids[i]) continue;
        const char* target = furi_string_get_cstr(app->generate_page_ids[i]);
        if(!target || target[0] == '\0') continue;
        if(strlen(target) != id_len) continue;
        if(strncmp(raw, target, id_len) != 0) continue;

        const char* value = colon + 1;
        while(*value == ' ' || *value == '\t') {
            value++;
        }
        const char* pipe = strchr(value, '|');
        size_t name_len = pipe ? (size_t)(pipe - value) : strlen(value);
        if(app->generate_page_names[i]) {
            furi_string_set_strn(app->generate_page_names[i], value, name_len);
        }
        ctx->remaining--;
        if(ctx->remaining == 0) {
            return false;
        }
        break;
    }

    return true;
}

static bool ami_tool_scene_generate_fill_page_names(AmiToolApp* app) {
    if(!app || app->generate_page_entry_count == 0) {
        return true;
    }

    AmiToolGenerateNameFillContext context = {
        .app = app,
        .remaining = app->generate_page_entry_count,
    };

    bool ok = ami_tool_scene_generate_iterate_lines(
        app,
        APP_ASSETS_PATH("amiibo.dat"),
        true,
        ami_tool_scene_generate_fill_names_callback,
        &context);

    for(size_t i = 0; i < app->generate_page_entry_count; i++) {
        if(!app->generate_page_names[i]) continue;
        if(furi_string_empty(app->generate_page_names[i])) {
            const char* fallback =
                app->generate_page_ids[i] ? furi_string_get_cstr(app->generate_page_ids[i]) : NULL;
            if(fallback && fallback[0] != '\0') {
                furi_string_set(app->generate_page_names[i], fallback);
            } else {
                furi_string_set(app->generate_page_names[i], "Unknown Amiibo");
            }
        }
    }

    return ok;
}

static bool ami_tool_scene_generate_load_game_page(AmiToolApp* app) {
    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;
    size_t start = app->generate_page_offset;
    size_t end = start + page_size;

    app->generate_page_entry_count = 0;

    FuriString* ids_line = furi_string_alloc();
    bool found = ami_tool_scene_generate_find_mapping_for_game(
        app, app->generate_platform, app->generate_selected_game, ids_line);
    if(!found) {
        furi_string_free(ids_line);
        return false;
    }

    const char* raw = furi_string_get_cstr(ids_line);
    size_t token_start = 0;
    size_t index = 0;

    while(raw[token_start] != '\0') {
        size_t token_end = token_start;
        while(raw[token_end] != '\0' && raw[token_end] != '|') {
            token_end++;
        }
        size_t token_len = token_end - token_start;
        while(token_len > 0 && isspace((unsigned char)raw[token_start])) {
            token_start++;
            token_len--;
        }
        while(token_len > 0 && isspace((unsigned char)raw[token_start + token_len - 1])) {
            token_len--;
        }

        if(token_len > 0) {
            if(index >= start && index < end && app->generate_page_entry_count < page_size) {
                size_t slot = app->generate_page_entry_count;
                FuriString* id_slot = app->generate_page_ids[slot];
                FuriString* name_slot = app->generate_page_names[slot];
                if(!id_slot || !name_slot) {
                    furi_string_free(ids_line);
                    app->generate_page_entry_count = slot;
                    return false;
                }
                app->generate_page_entry_count++;
                furi_string_set_strn(id_slot, raw + token_start, token_len);
                furi_string_trim(id_slot);
                furi_string_set(name_slot, furi_string_get_cstr(id_slot));
            }
            index++;
        }

        if(raw[token_end] == '\0') {
            break;
        }
        token_start = token_end + 1;
    }

    furi_string_free(ids_line);
    app->generate_amiibo_count = index;
    if(index == 0) {
        return false;
    }

    ami_tool_scene_generate_fill_page_names(app);
    return true;
}

static bool ami_tool_scene_generate_load_page_entries(AmiToolApp* app) {
    if(!app) {
        return false;
    }

    bool ok = false;
    switch(app->generate_list_source) {
    case AmiToolGenerateListSourceName:
        ok = ami_tool_scene_generate_load_name_page(app);
        break;
    case AmiToolGenerateListSourceGame:
        ok = ami_tool_scene_generate_load_game_page(app);
        break;
    default:
        return false;
    }

    if(!ok) {
        return false;
    }

    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;
    if(app->generate_amiibo_count == 0) {
        return false;
    }

    if(app->generate_page_offset >= app->generate_amiibo_count) {
        size_t max_page = (app->generate_amiibo_count - 1) / page_size;
        app->generate_page_offset = max_page * page_size;
        if(app->generate_list_source == AmiToolGenerateListSourceName) {
            ok = ami_tool_scene_generate_load_name_page(app);
        } else {
            ok = ami_tool_scene_generate_load_game_page(app);
        }
    }

    return ok;
}

static void ami_tool_scene_generate_show_name_menu(AmiToolApp* app) {
    furi_string_set_str(app->generate_selected_game, "Amiibo by Name");
    app->generate_list_source = AmiToolGenerateListSourceName;
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;
    ami_tool_generate_clear_amiibo_cache(app);

    if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
        furi_string_set(
            app->text_box_store,
            "No Amiibo names available.\n\nUpdate amiibo_name.dat and try again.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateRootMenu);
    }
}

static bool ami_tool_scene_generate_show_cached_amiibo_menu(AmiToolApp* app) {
    if(!app || !app->generate_selected_game || furi_string_empty(app->generate_selected_game)) {
        return false;
    }

    if(!ami_tool_scene_generate_load_page_entries(app)) {
        return false;
    }

    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;
    if(app->generate_amiibo_count == 0) {
        return false;
    }

    if(app->generate_selected_index >= app->generate_amiibo_count) {
        app->generate_selected_index = app->generate_amiibo_count - 1;
    }

    app->generate_page_offset = (app->generate_selected_index / page_size) * page_size;
    size_t remaining = app->generate_amiibo_count - app->generate_page_offset;
    size_t page_entries = remaining < page_size ? remaining : page_size;
    size_t available_entries = app->generate_page_entry_count;
    if(page_entries > available_entries) {
        page_entries = available_entries;
    }
    size_t total_pages = (app->generate_amiibo_count + page_size - 1) / page_size;
    size_t current_page = (app->generate_page_offset / page_size) + 1;

    submenu_reset(app->submenu);
    furi_string_printf(
        app->text_box_store,
        "%s (%u/%u)",
        furi_string_get_cstr(app->generate_selected_game),
        (unsigned int)current_page,
        (unsigned int)((total_pages > 0) ? total_pages : 1));
    submenu_set_header(app->submenu, furi_string_get_cstr(app->text_box_store));

    if(app->generate_page_offset > 0) {
        submenu_add_item(
            app->submenu,
            "< Previous Page",
            AMI_TOOL_GENERATE_MENU_INDEX_PREV_PAGE,
            ami_tool_scene_generate_submenu_callback,
            app);
    }

    for(size_t i = 0; i < page_entries; i++) {
        size_t entry_index = app->generate_page_offset + i;
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(app->generate_page_names[i]),
            entry_index,
            ami_tool_scene_generate_submenu_callback,
            app);
    }

    if((app->generate_page_offset + page_entries) < app->generate_amiibo_count) {
        submenu_add_item(
            app->submenu,
            "Next Page >",
            AMI_TOOL_GENERATE_MENU_INDEX_NEXT_PAGE,
            ami_tool_scene_generate_submenu_callback,
            app);
    }

    if(page_entries > 0) {
        if(app->generate_selected_index < app->generate_page_offset ||
           app->generate_selected_index >= app->generate_page_offset + page_entries) {
            app->generate_selected_index = app->generate_page_offset;
        }
        submenu_set_selected_item(app->submenu, app->generate_selected_index);
    }

    app->generate_state = AmiToolGenerateStateAmiiboList;
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewMenu);
    return true;
}

static void ami_tool_scene_generate_change_page(AmiToolApp* app, int direction) {
    if(app->generate_amiibo_count == 0) {
        return;
    }

    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;

    if(direction > 0) {
        size_t new_offset = app->generate_page_offset + page_size;
        if(new_offset >= app->generate_amiibo_count) {
            size_t last_index = app->generate_amiibo_count - 1;
            new_offset = (last_index / page_size) * page_size;
        }
        if(new_offset != app->generate_page_offset) {
            app->generate_page_offset = new_offset;
            app->generate_selected_index = new_offset;
            ami_tool_scene_generate_show_cached_amiibo_menu(app);
        }
    } else if(direction < 0) {
        size_t new_offset =
            (app->generate_page_offset >= page_size) ? app->generate_page_offset - page_size : 0;
        if(new_offset != app->generate_page_offset) {
            app->generate_page_offset = new_offset;
            app->generate_selected_index = new_offset;
            ami_tool_scene_generate_show_cached_amiibo_menu(app);
        }
    }
}

static void ami_tool_scene_generate_show_amiibo_menu(AmiToolApp* app, size_t game_index) {
    FuriString* game_name = furi_string_alloc();
    bool found = ami_tool_scene_generate_get_game_name(
        app, app->generate_platform, game_index, game_name);

    if(!found) {
        furi_string_printf(
            app->text_box_store,
            "Unable to find the selected game.\n\nReturn to the list and try again.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateGameList);
        furi_string_free(game_name);
        return;
    }

    furi_string_set(app->generate_selected_game, game_name);
    app->generate_list_source = AmiToolGenerateListSourceGame;
    app->generate_page_offset = 0;
    app->generate_selected_index = 0;
    ami_tool_generate_clear_amiibo_cache(app);

    if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
        furi_string_set(
            app->text_box_store,
            "Unable to load Amiibo list for the selected game.\n\nUpdate your assets.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateGameList);
        furi_string_free(game_name);
        return;
    }

    furi_string_free(game_name);
}

typedef struct {
    const FuriString* game_name;
    FuriString* result_ids;
    bool found;
} AmiToolGenerateMappingContext;

static bool ami_tool_scene_generate_mapping_callback(
    void* context,
    size_t index,
    FuriString* line) {
    UNUSED(index);
    AmiToolGenerateMappingContext* ctx = context;
    const char* raw = furi_string_get_cstr(line);
    const char* target = furi_string_get_cstr(ctx->game_name);
    size_t target_len = furi_string_size(ctx->game_name);

    if(strncmp(raw, target, target_len) == 0 && raw[target_len] == ':') {
        const char* value = raw + target_len + 1;
        while(*value == ' ' || *value == '\t') {
            value++;
        }
        furi_string_set(ctx->result_ids, value);
        ctx->found = true;
        return false;
    }

    return true;
}

static bool ami_tool_scene_generate_find_mapping_for_game(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    const FuriString* game_name,
    FuriString* out_ids) {
    const char* path = ami_tool_scene_generate_platform_mapping_path(platform);
    if(!path) {
        return false;
    }

    AmiToolGenerateMappingContext context = {
        .game_name = game_name,
        .result_ids = out_ids,
        .found = false,
    };

    furi_string_reset(out_ids);
    bool ok = ami_tool_scene_generate_iterate_lines(
        app, path, true, ami_tool_scene_generate_mapping_callback, &context);
    return ok && context.found;
}

static void ami_tool_scene_generate_show_amiibo_placeholder(AmiToolApp* app, size_t amiibo_index) {
    if(amiibo_index >= app->generate_amiibo_count) {
        furi_string_printf(
            app->text_box_store,
            "Unknown Amiibo selection.\n\nReturn to the previous menu.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateAmiiboList);
        return;
    }

    app->generate_selected_index = amiibo_index;

    const size_t page_size =
        AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS ? AMI_TOOL_GENERATE_MAX_AMIIBO_PAGE_ITEMS : 1;
    size_t target_offset = (amiibo_index / page_size) * page_size;
    if(target_offset != app->generate_page_offset ||
       amiibo_index < app->generate_page_offset ||
       amiibo_index >= (app->generate_page_offset + app->generate_page_entry_count)) {
        app->generate_page_offset = target_offset;
        if(!ami_tool_scene_generate_load_page_entries(app)) {
            furi_string_set(
                app->text_box_store,
                "Unable to load Amiibo data.\n\nReturn to the previous menu.");
            ami_tool_scene_generate_commit_text_view(
                app, AmiToolGenerateStateMessage, AmiToolGenerateStateAmiiboList);
            return;
        }
    }

    size_t local_index = amiibo_index - app->generate_page_offset;
    if(local_index >= app->generate_page_entry_count) {
        furi_string_set(
            app->text_box_store,
            "Unable to load Amiibo data.\n\nReturn to the previous menu.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateAmiiboList);
        return;
    }

    FuriString* id_str = app->generate_page_ids[local_index];
    if(!id_str || furi_string_empty(id_str)) {
        furi_string_set(
            app->text_box_store, "No Amiibo ID available.\n\nReturn to the previous menu.");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateAmiiboList);
        return;
    }

    char id_hex[33] = {0};
    size_t id_len = furi_string_size(id_str);
    if(id_len >= sizeof(id_hex)) {
        id_len = sizeof(id_hex) - 1;
    }
    memcpy(id_hex, furi_string_get_cstr(id_str), id_len);
    id_hex[id_len] = '\0';

    if(!ami_tool_scene_generate_prepare_dump(app, id_hex)) {
        furi_string_printf(
            app->text_box_store,
            "Unable to generate Amiibo dump.\n\nID: %s",
            id_hex[0] ? id_hex : "Unknown");
        ami_tool_scene_generate_commit_text_view(
            app, AmiToolGenerateStateMessage, AmiToolGenerateStateAmiiboList);
        return;
    }
    ami_tool_info_stop_emulation(app);
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
    ami_tool_info_show_page(app, id_hex, false);
    app->generate_state = AmiToolGenerateStateAmiiboPlaceholder;
    app->generate_return_state = AmiToolGenerateStateAmiiboList;
}

static void ami_tool_scene_generate_commit_text_view(
    AmiToolApp* app,
    AmiToolGenerateState state,
    AmiToolGenerateState return_state) {
    app->generate_state = state;
    app->generate_return_state = return_state;
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
}

static bool ami_tool_scene_generate_iterate_lines(
    AmiToolApp* app,
    const char* path,
    bool skip_header,
    AmiToolGenerateLineCallback callback,
    void* context) {
    if(!app->storage || !path || !callback) {
        return false;
    }

    File* file = storage_file_alloc(app->storage);
    if(!file) {
        return false;
    }

    FuriString* line = furi_string_alloc();
    bool success = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bool in_data_section = !skip_header;
        size_t line_index = 0;
        bool keep_reading = true;
        uint8_t buffer[AMI_TOOL_GENERATE_READ_BUFFER];

        while(keep_reading) {
            size_t read = storage_file_read(file, buffer, sizeof(buffer));
            if(read == 0) break;

            for(size_t i = 0; i < read; i++) {
                char ch = (char)buffer[i];
                if(ch == '\r') {
                    continue;
                }
                if(ch == '\n') {
                    if(in_data_section) {
                        if(!furi_string_empty(line)) {
                            keep_reading = callback(context, line_index, line);
                            line_index++;
                        }
                    } else if(furi_string_empty(line)) {
                        in_data_section = true;
                    }
                    furi_string_reset(line);
                    if(!keep_reading) break;
                } else {
                    furi_string_push_back(line, ch);
                }
            }

            if(!keep_reading) break;
        }

        if(keep_reading && in_data_section && !furi_string_empty(line)) {
            callback(context, line_index, line);
        }

        success = true;
        storage_file_close(file);
    }

    furi_string_free(line);
    storage_file_free(file);
    return success;
}

static bool ami_tool_scene_generate_iterate_games(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    AmiToolGenerateLineCallback callback,
    void* context) {
    const char* path = NULL;
    switch(platform) {
    case AmiToolGeneratePlatform3DS:
        path = APP_ASSETS_PATH("game_3ds.dat");
        break;
    case AmiToolGeneratePlatformWiiU:
        path = APP_ASSETS_PATH("game_wiiu.dat");
        break;
    case AmiToolGeneratePlatformSwitch:
        path = APP_ASSETS_PATH("game_switch.dat");
        break;
    case AmiToolGeneratePlatformSwitch2:
        path = APP_ASSETS_PATH("game_switch2.dat");
        break;
    default:
        break;
    }

    if(!path) {
        return false;
    }

    return ami_tool_scene_generate_iterate_lines(app, path, true, callback, context);
}

static bool ami_tool_scene_generate_get_game_name(
    AmiToolApp* app,
    AmiToolGeneratePlatform platform,
    size_t target_index,
    FuriString* out) {
    AmiToolGenerateFindGameContext context = {
        .target_index = target_index,
        .result = out,
        .found = false,
    };

    furi_string_reset(out);
    bool file_ok = ami_tool_scene_generate_iterate_games(
        app, platform, ami_tool_scene_generate_find_game_callback, &context);

    return file_ok && context.found;
}

static const char* ami_tool_scene_generate_platform_label(AmiToolGeneratePlatform platform) {
    switch(platform) {
    case AmiToolGeneratePlatform3DS:
        return "3DS";
    case AmiToolGeneratePlatformWiiU:
        return "Wii U";
    case AmiToolGeneratePlatformSwitch:
        return "Switch";
    case AmiToolGeneratePlatformSwitch2:
        return "Switch 2";
    default:
        return "Unknown";
    }
}

static const char* ami_tool_scene_generate_platform_mapping_path(AmiToolGeneratePlatform platform) {
    switch(platform) {
    case AmiToolGeneratePlatform3DS:
        return APP_ASSETS_PATH("game_3ds_mapping.dat");
    case AmiToolGeneratePlatformWiiU:
        return APP_ASSETS_PATH("game_wiiu_mapping.dat");
    case AmiToolGeneratePlatformSwitch:
        return APP_ASSETS_PATH("game_switch_mapping.dat");
    case AmiToolGeneratePlatformSwitch2:
        return APP_ASSETS_PATH("game_switch2_mapping.dat");
    default:
        return NULL;
    }
}

static void ami_tool_scene_generate_submenu_callback(void* context, uint32_t index) {
    AmiToolApp* app = context;

    switch(app->generate_state) {
    case AmiToolGenerateStateRootMenu:
        if(index == AmiToolGenerateRootMenuIndexByName) {
            ami_tool_scene_generate_show_name_menu(app);
        } else if(index == AmiToolGenerateRootMenuIndexByGames) {
            ami_tool_scene_generate_show_platform_menu(app);
        }
        break;
    case AmiToolGenerateStatePlatformMenu:
        if(index < AmiToolGeneratePlatformCount) {
            app->generate_platform = (AmiToolGeneratePlatform)index;
            ami_tool_scene_generate_show_games_menu(app);
        }
        break;
    case AmiToolGenerateStateGameList:
        if(index < app->generate_game_count) {
            ami_tool_scene_generate_show_amiibo_menu(app, index);
        }
        break;
    case AmiToolGenerateStateAmiiboList:
        if(index == AMI_TOOL_GENERATE_MENU_INDEX_PREV_PAGE) {
            ami_tool_scene_generate_change_page(app, -1);
        } else if(index == AMI_TOOL_GENERATE_MENU_INDEX_NEXT_PAGE) {
            ami_tool_scene_generate_change_page(app, 1);
        } else if(index < app->generate_amiibo_count) {
            ami_tool_scene_generate_show_amiibo_placeholder(app, index);
        }
        break;
    default:
        break;
    }
}

static void ami_tool_scene_generate_return_to_state(AmiToolApp* app, AmiToolGenerateState state) {
    switch(state) {
    case AmiToolGenerateStateRootMenu:
        ami_tool_scene_generate_show_root_menu(app);
        break;
    case AmiToolGenerateStatePlatformMenu:
        ami_tool_scene_generate_show_platform_menu(app);
        break;
    case AmiToolGenerateStateGameList:
        ami_tool_scene_generate_show_games_menu(app);
        break;
    case AmiToolGenerateStateAmiiboList:
        if(app->generate_list_source == AmiToolGenerateListSourceName) {
            if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
                ami_tool_scene_generate_show_root_menu(app);
            }
        } else {
            if(!ami_tool_scene_generate_show_cached_amiibo_menu(app)) {
                if(app->generate_game_count > 0) {
                    ami_tool_scene_generate_show_games_menu(app);
                } else {
                    ami_tool_scene_generate_show_platform_menu(app);
                }
            }
        }
        break;
    default:
        ami_tool_scene_generate_show_root_menu(app);
        break;
    }
}

void ami_tool_scene_generate_on_enter(void* context) {
    AmiToolApp* app = context;
    ami_tool_scene_generate_show_root_menu(app);
}

bool ami_tool_scene_generate_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case AmiToolEventInfoWriteStarted:
        case AmiToolEventInfoWriteSuccess:
        case AmiToolEventInfoWriteFailed:
        case AmiToolEventInfoWriteCancelled:
            ami_tool_info_handle_write_event(app, event.event);
            return true;
        case AmiToolEventInfoShowActions:
            ami_tool_info_show_actions_menu(app);
            return true;
        case AmiToolEventInfoActionEmulate:
            if(!ami_tool_info_start_emulation(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to start emulation.\nGenerate or read an Amiibo first.");
            }
            return true;
        case AmiToolEventInfoActionUsage:
            ami_tool_info_show_usage(app);
            return true;
        case AmiToolEventInfoActionChangeUid:
            if(!ami_tool_info_change_uid(app)) {
                ami_tool_info_show_action_message(
                    app, "Unable to change UID.\nInstall key_retail.bin and try again.");
            }
            return true;
        case AmiToolEventInfoActionWriteTag:
            if(!ami_tool_info_write_to_tag(app)) {
                ami_tool_info_show_action_message(
                    app,
                    "Unable to write tag.\nUse a blank NTAG215 and make\nsure key_retail.bin is installed.");
            }
            return true;
        case AmiToolEventInfoActionSaveToStorage:
            if(!ami_tool_info_save_to_storage(app)) {
                ami_tool_info_show_action_message(
                    app,
                    "Unable to save Amiibo.\nGenerate or read one first\nand check storage access.");
            }
            return true;
        case AmiToolEventUsagePrevPage:
            ami_tool_info_navigate_usage(app, -1);
            return true;
        case AmiToolEventUsageNextPage:
            ami_tool_info_navigate_usage(app, 1);
            return true;
        default:
            break;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(app->write_in_progress) {
            if(app->write_waiting_for_tag) {
                ami_tool_info_request_write_cancel(app);
            }
            return true;
        }
        if(app->info_emulation_active) {
            ami_tool_info_stop_emulation(app);
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->usage_info_visible) {
            app->usage_info_visible = false;
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_action_message_visible) {
            ami_tool_info_show_actions_menu(app);
            return true;
        }
        if(app->info_actions_visible) {
            app->info_actions_visible = false;
            ami_tool_info_refresh_current_page(app);
            return true;
        }
        switch(app->generate_state) {
        case AmiToolGenerateStateRootMenu:
            return false;
        case AmiToolGenerateStatePlatformMenu:
            ami_tool_scene_generate_show_root_menu(app);
            return true;
    case AmiToolGenerateStateGameList:
        ami_tool_scene_generate_show_platform_menu(app);
        return true;
    case AmiToolGenerateStateAmiiboList:
        if(app->generate_list_source == AmiToolGenerateListSourceName) {
            ami_tool_scene_generate_show_root_menu(app);
        } else {
            ami_tool_scene_generate_show_games_menu(app);
        }
        return true;
        case AmiToolGenerateStateAmiiboPlaceholder:
        case AmiToolGenerateStateMessage:
            ami_tool_scene_generate_return_to_state(app, app->generate_return_state);
            return true;
        default:
            break;
        }
    }

    return false;
}

void ami_tool_scene_generate_on_exit(void* context) {
    AmiToolApp* app = context;
    app->generate_state = AmiToolGenerateStateRootMenu;
    app->generate_return_state = AmiToolGenerateStateRootMenu;
    app->generate_game_count = 0;
    ami_tool_generate_clear_amiibo_cache(app);
    ami_tool_scene_generate_clear_selected_game(app);
    ami_tool_info_stop_emulation(app);
    ami_tool_info_abort_write(app);
    app->info_actions_visible = false;
    app->info_action_message_visible = false;
}
