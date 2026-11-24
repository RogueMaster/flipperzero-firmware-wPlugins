#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>
#include <string.h>
#include "cheat_catalog.h"
#include "companion.h"
#include "gta_vi_companion_icons.h"
#include "mock_loaders.h"
#include "safe_codes.h"
#include "secret_missions.h"
#include "secret_progress.h"

typedef enum {
    ViewMenu,
    ViewText,
    ViewCollection,
    ViewCollectionDetail,
    ViewHome,
    ViewSecretList,
    ViewSecretDetail,
    ViewMockLoaders,
    ViewMockLoaderSettings,
} GtaView;
typedef enum {
    ScreenHome,
    ScreenStartup,
    ScreenCategories,
    ScreenList,
    ScreenDetail,
    ScreenGuide,
    ScreenMoneyGlitch,
    ScreenSecretList,
    ScreenSecretDetail,
    ScreenSafeCategories,
    ScreenSafeList,
    ScreenSafeDetail,
    ScreenSafeTips,
    ScreenCompanion,
    ScreenCollections,
    ScreenCollectionGroup,
    ScreenDispatchRegions,
    ScreenMockLoaders,
    ScreenMockLoaderSettings,
} GtaScreen;
typedef enum {
    ListPublished,
    ListAll,
    ListCategory,
    ListUnverified
} GtaListMode;
enum {
    ItemPs5,
    ItemXbox,
    ItemGuide,
    ItemPublished,
    ItemAll,
    ItemUnverified,
    ItemEmpty,
    ItemMoneyGlitch,
    ItemSafeCodes,
    ItemSecretMissions,
    ItemRow = 16,
    ItemCollection = 128,
    ItemDispatch = ItemCollection + GtaCollectionCount,
    ItemCollections,
    ItemMockLoaders,
    ItemMockLoaderSettings,
    ItemExploreRow = 256,
    ItemDispatchRegion = 0x300,
    ItemSafeAll = 0x400,
    ItemSafeMission,
    ItemSafeOperations,
    ItemSafeDoors,
    ItemSafePuzzles,
    ItemSafeTips,
    ItemSafeRow = 0x500,
    ItemSecretRow = 0x600,
    ItemSecretOpen = 0x700,
    ItemSecretToggle,
    ItemSecretToggleDetail,
};
typedef struct {
    const Icon* icon;
    uint32_t event;
    const char* title;
} GtaHomeItem;
static const GtaHomeItem gta_home_items[] = {
    {&I_menu_infinite_money_glitch_25x25, ItemMoneyGlitch, "Infinite Money Glitch"},
    {&I_menu_ps5_cheats_25x25, ItemPs5, "PS5 Cheats"},
    {&I_menu_xbox_cheats_25x25, ItemXbox, "Xbox Cheats"},
    {&I_menu_collections_25x25, ItemCollections, "Collections"},
    {&I_menu_safe_codes_25x25, ItemSafeCodes, "Safe & Door Codes"},
    {&I_menu_secret_missions_25x25, ItemSecretMissions, "Secret Missions"},
    {&I_menu_randys_wish_list_25x25, ItemCollection + GtaCollectionRandy, NULL},
    {&I_menu_moses_drop_offs_25x25, ItemCollection + GtaCollectionMoses, NULL},
    {&I_menu_wymans_classic_cars_25x25, ItemCollection + GtaCollectionWyman, NULL},
    {&I_menu_standout_vehicles_25x25, ItemCollection + GtaCollectionStandout, NULL},
    {&I_menu_garages_25x25, ItemCollection + GtaCollectionGarages, NULL},
    {&I_menu_houses_cribs_25x25, ItemCollection + GtaCollectionHouses, NULL},
    {&I_menu_dispatch_25x25, ItemDispatch, "Dispatch"},
    {&I_menu_mock_loaders_25x25, ItemMockLoaders, "Mock Loaders"},
    {&I_menu_mock_loader_settings_25x25, ItemMockLoaderSettings, "Mock Loader Settings"},
    {&I_menu_quick_guide_warnings_25x25, ItemGuide, "Quick guide / warnings"},
};
enum {
    GtaHomeColumns = 4,
    GtaHomePageSize = 2 * GtaHomeColumns,
    GtaHomeItemCount = sizeof(gta_home_items) / sizeof(gta_home_items[0]),
    GtaHomePageCount = (GtaHomeItemCount + GtaHomePageSize - 1) / GtaHomePageSize,
};
typedef struct {
    size_t selected;
    size_t completed;
    uint8_t checked[GtaSecretProgressBytes];
    bool save_error;
} GtaSecretListModel;
typedef struct {
    ViewDispatcher* dispatcher;
    View* home;
    View* secret_list;
    Widget* secret_detail;
    Storage* secret_storage;
    GtaSecretProgress secret_progress;
    size_t secret_index;
    GtaMockLoaders* mock_loaders;
    Submenu* menu;
    TextBox* text;
    FuriString* content;
    GtaCompanion* companion;
    GtaScreen screen;
    GtaScreen guide_back;
    GtaScreen companion_back;
    const GtaExploreSection* explore_group;
    GtaListMode list_mode;
    bool xbox;
    size_t category;
    uint32_t category_selection;
    uint32_t list_selection;
    uint32_t explore_root_selection;
    uint32_t explore_group_selection;
    uint32_t safe_category_selection;
    uint32_t safe_list_selection;
    uint8_t safe_category; /* 0=all; 1..4 correspond to GtaSafeKind+1 */
    GtaDispatchRegion dispatch_region;
} GtaApp;
static const char* gta_sequence(const GtaApp* app, const GtaCheat* cheat) {
    return app->xbox ? cheat->xbox : cheat->ps5;
}
/* A screenshot effect may share a name with the 61 main catalog entries.
 * This name match DOES NOT validate the screenshot's button combination. */
static const GtaScreenshotCode* gta_screenshot_match(const char* catalog_name) {
    for(size_t i = 0; i < gta_screenshot_count; i++) {
        const char* linked = gta_screenshot_codes[i].main_catalog_name;
        if(linked && strcmp(linked, catalog_name) == 0) return &gta_screenshot_codes[i];
    }
    return NULL;
}
static void gta_post_selection(void* context, uint32_t index) {
    GtaApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, index);
}
static void gta_home_draw(Canvas* canvas, void* context) {
    const size_t selected = *(const size_t*)context;
    const GtaHomeItem* item = &gta_home_items[selected];
    const char* title = item->title;
    if(!title) title = gta_collection_catalog[item->event - ItemCollection].title;
    const size_t page = selected / GtaHomePageSize;
    const size_t first = page * GtaHomePageSize;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, title);
    for(size_t slot = 0; slot < GtaHomePageSize && first + slot < GtaHomeItemCount; ++slot) {
        const int32_t x = 2 + (slot % GtaHomeColumns) * 32;
        const int32_t y = 10 + (slot / GtaHomeColumns) * 27;
        if(first + slot == selected) canvas_draw_rframe(canvas, x, y, 27, 27, 2);
        canvas_draw_icon(canvas, x + 1, y + 1, gta_home_items[first + slot].icon);
    }
    elements_scrollbar_pos(canvas, 127, 10, 54, page, GtaHomePageCount);
}
static bool gta_home_input(InputEvent* event, void* context) {
    GtaApp* app = context;
    if(event->key == InputKeyBack ||
       (event->type != InputTypeShort && event->type != InputTypeRepeat)) {
        return false;
    }
    uint32_t item_event;
    bool moved;
    with_view_model(
        app->home,
        size_t * selected,
        {
            size_t next = *selected;
            switch(event->key) {
            case InputKeyLeft:
                next = next ? next - 1 : GtaHomeItemCount - 1;
                break;
            case InputKeyRight:
                next = (next + 1) % GtaHomeItemCount;
                break;
            case InputKeyUp:
                if(next >= GtaHomeColumns) next -= GtaHomeColumns;
                break;
            case InputKeyDown:
                if(next / GtaHomeColumns < (GtaHomeItemCount - 1) / GtaHomeColumns)
                    next = MIN(next + GtaHomeColumns, GtaHomeItemCount - 1U);
                break;
            default:
                break;
            }
            moved = next != *selected;
            *selected = next;
            item_event = gta_home_items[next].event;
        },
        moved);
    if(event->key == InputKeyOk && event->type == InputTypeShort)
        gta_post_selection(app, item_event);
    return true;
}

static void gta_item(GtaApp* app, const char* label, uint32_t index) {
    submenu_add_item(app->menu, label, index, gta_post_selection, app);
}
static size_t gta_published_count(const GtaApp* app) {
    size_t count = 0;
    for(size_t i = 0; i < gta_cheat_count; i++)
        if(gta_sequence(app, &gta_cheat_catalog[i])[0]) count++;
    return count;
}
static size_t gta_safe_filter_count(uint8_t category) {
    size_t count = 0;
    for(size_t i = 0; i < gta_safe_code_count; i++)
        if(!category || gta_safe_codes[i].kind == (GtaSafeKind)(category - 1)) count++;
    return count;
}
static const char* gta_safe_category_name(uint8_t category) {
    static const char* const labels[] = {
        "All codes", "Mission safes", "Operations", "Doors / garages", "Puzzles / other"};
    return labels[category < 5 ? category : 0];
}
static void gta_secret_refresh(GtaApp* app) {
    size_t complete = 0;
    for(size_t i = 0; i < gta_secret_mission_count; ++i) {
        if(gta_secret_progress_is_checked(
               &app->secret_progress, gta_secret_missions[i].progress_id))
            ++complete;
    }
    with_view_model(
        app->secret_list,
        GtaSecretListModel * model,
        {
            model->selected = app->secret_index;
            model->completed = complete;
            memcpy(model->checked, app->secret_progress.save.checked, sizeof(model->checked));
            model->save_error = app->secret_progress.save_error;
        },
        true);
}
static void gta_secret_draw_label(Canvas* canvas, const char* name, int32_t x, int32_t y) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%.47s", name);
    size_t len = strlen(buf);
    while(len && canvas_string_width(canvas, buf) > 112) {
        buf[--len] = '\0';
    }
    canvas_draw_str(canvas, x, y, buf);
}
static void gta_secret_list_draw(Canvas* canvas, void* context) {
    const GtaSecretListModel* model = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "Secret Missions");
    char status[20];
    snprintf(
        status,
        sizeof(status),
        "%u/%u",
        (unsigned)model->completed,
        (unsigned)gta_secret_mission_count);
    canvas_draw_str_aligned(canvas, 125, 9, AlignRight, AlignBottom, status);
    canvas_draw_line(canvas, 0, 11, 127, 11);
    const size_t first = (model->selected / 4u) * 4u;
    for(size_t row = 0; row < 4 && first + row < gta_secret_mission_count; ++row) {
        const size_t i = first + row;
        const GtaSecretMission* entry = &gta_secret_missions[i];
        const uint8_t y = (uint8_t)(20u + row * 10u);
        if(i == model->selected) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_frame(canvas, 3, y - 6, 7, 7);
        const uint8_t id = entry->progress_id;
        if(model->checked[id / 8u] & (1u << (id % 8u))) {
            canvas_draw_line(canvas, 4, y - 3, 6, y - 1);
            canvas_draw_line(canvas, 6, y - 1, 8, y - 5);
        }
        char title[60];
        snprintf(title, sizeof(title), "%s %s", entry->stage, entry->title);
        gta_secret_draw_label(canvas, title, 14, y);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_draw_str(canvas, 2, 62, model->save_error ? "SD save failed" : "OK Details  > Check");
}
static bool gta_secret_list_input(InputEvent* event, void* context) {
    GtaApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    switch(event->key) {
    case InputKeyUp:
        if(app->secret_index) app->secret_index--;
        break;
    case InputKeyDown:
        if(app->secret_index + 1 < gta_secret_mission_count) app->secret_index++;
        break;
    case InputKeyRight:
        if(event->type == InputTypeShort)
            view_dispatcher_send_custom_event(app->dispatcher, ItemSecretToggle);
        return true;
    case InputKeyOk:
        if(event->type == InputTypeShort)
            view_dispatcher_send_custom_event(app->dispatcher, ItemSecretOpen);
        return true;
    default:
        return false;
    }
    gta_secret_refresh(app);
    return true;
}
static void gta_secret_button_callback(GuiButtonType button, InputType type, void* context) {
    if(button == GuiButtonTypeCenter && type == InputTypeShort) {
        GtaApp* app = context;
        view_dispatcher_send_custom_event(app->dispatcher, ItemSecretToggleDetail);
    }
}
static void gta_show_menu(GtaApp* app, GtaScreen screen) {
    app->screen = screen;
    if(screen == ScreenHome) {
        view_dispatcher_switch_to_view(app->dispatcher, ViewHome);
        return;
    }
    if(screen == ScreenSecretList) {
        gta_secret_refresh(app);
        view_dispatcher_switch_to_view(app->dispatcher, ViewSecretList);
        return;
    }
    submenu_reset(app->menu);
    char label[64];
    if(screen == ScreenSafeCategories) {
        submenu_set_header(app->menu, "Safe & Door Codes");
        for(uint8_t i = 0; i < 5; i++) {
            snprintf(
                label,
                sizeof(label),
                "%s (%zu)",
                gta_safe_category_name(i),
                gta_safe_filter_count(i));
            gta_item(app, label, ItemSafeAll + i);
        }
        gta_item(app, "Random safe tips", ItemSafeTips);
        submenu_set_selected_item(app->menu, app->safe_category_selection);
    } else if(screen == ScreenSafeList) {
        submenu_set_header(app->menu, gta_safe_category_name(app->safe_category));
        for(size_t i = 0; i < gta_safe_code_count; i++) {
            const GtaSafeCode* entry = &gta_safe_codes[i];
            if(app->safe_category && entry->kind != (GtaSafeKind)(app->safe_category - 1))
                continue;
            /* Show the combination first: a long location name would hide it
             * off-screen in the Flipper's narrow Submenu row. */
            snprintf(
                label,
                sizeof(label),
                "%s%s %s",
                entry->code,
                entry->status >= GtaSafeReported ? "?" : "",
                entry->name);
            gta_item(app, label, ItemSafeRow + i);
        }
        if(app->safe_list_selection >= ItemSafeRow &&
           app->safe_list_selection - ItemSafeRow < gta_safe_code_count)
            submenu_set_selected_item(app->menu, app->safe_list_selection);
    } else if(screen == ScreenCollections || screen == ScreenCollectionGroup) {
        const bool root = screen == ScreenCollections;
        const GtaExploreSection* items = root ? gta_explore_sections :
                                                app->explore_group->children;
        const size_t count = root ? gta_explore_section_count : app->explore_group->child_count;
        submenu_set_header(app->menu, root ? "Collections" : app->explore_group->title);
        for(size_t i = 0; i < count; ++i) {
            gta_item(app, items[i].title, ItemExploreRow + i);
        }
        submenu_set_selected_item(
            app->menu, root ? app->explore_root_selection : app->explore_group_selection);
    } else if(screen == ScreenDispatchRegions) {
        submenu_set_header(app->menu, "Dispatch Region");
        for(size_t i = 0; i < GtaDispatchRegionCount; ++i) {
            gta_item(app, gta_dispatch_regions[i], ItemDispatchRegion + i);
        }
        submenu_set_selected_item(app->menu, ItemDispatchRegion + app->dispatch_region);
    } else if(screen == ScreenCategories) {
        submenu_set_header(app->menu, app->xbox ? "Xbox Series X|S" : "PlayStation 5");
        snprintf(label, sizeof(label), "Published inputs (%zu)", gta_published_count(app));
        gta_item(app, label, ItemPublished);
        snprintf(label, sizeof(label), "All GTA VI entries (%zu)", gta_cheat_count);
        gta_item(app, label, ItemAll);
        for(size_t start = 0; start < gta_cheat_count;) {
            size_t end = start + 1;
            while(end < gta_cheat_count &&
                  strcmp(gta_cheat_catalog[start].category, gta_cheat_catalog[end].category) == 0)
                end++;
            size_t published = 0;
            for(size_t i = start; i < end; i++)
                if(gta_sequence(app, &gta_cheat_catalog[i])[0]) published++;
            snprintf(
                label,
                sizeof(label),
                "%s (%zu/%zu)",
                gta_cheat_catalog[start].category,
                published,
                end - start);
            gta_item(app, label, ItemRow + start);
            start = end;
        }
        snprintf(label, sizeof(label), "Unverified codes (%zu)", gta_unverified_count);
        gta_item(app, label, ItemUnverified);
        gta_item(app, "Quick guide / warnings", ItemGuide);
        submenu_set_selected_item(app->menu, app->category_selection);
    } else if(screen == ScreenList) {
        const bool unverified = app->list_mode == ListUnverified;
        const char* header = "All GTA VI";
        if(app->list_mode == ListPublished)
            header = "Published inputs";
        else if(app->list_mode == ListCategory)
            header = gta_cheat_catalog[app->category].category;
        else if(unverified)
            header = "Unverified codes";
        submenu_set_header(app->menu, header);
        bool any = false;
        if(unverified) {
            for(size_t i = 0; i < gta_screenshot_count; i++) {
                if(gta_screenshot_codes[i].main_catalog_name) continue;
                gta_item(app, gta_screenshot_codes[i].name, ItemRow + i);
                any = true;
            }
        } else {
            for(size_t i = 0; i < gta_cheat_count; i++) {
                const GtaCheat* cheat = &gta_cheat_catalog[i];
                const bool has_input = gta_sequence(app, cheat)[0] != '\0';
                if(app->list_mode == ListPublished && !has_input) continue;
                if(app->list_mode == ListCategory &&
                   strcmp(cheat->category, gta_cheat_catalog[app->category].category))
                    continue;
                if(has_input) {
                    gta_item(app, cheat->name, ItemRow + i);
                } else {
                    const char* marker = "[?]";
                    if(gta_reported_sequence(cheat)) {
                        marker = "[report]";
                    } else if(gta_screenshot_match(cheat->name)) {
                        marker = "[img?]";
                    }
                    snprintf(label, sizeof(label), "%s %s", cheat->name, marker);
                    gta_item(app, label, ItemRow + i);
                }
                any = true;
            }
        }
        if(!any) gta_item(app, "No reported codes", ItemEmpty);
        if(any && app->list_selection != ItemEmpty)
            submenu_set_selected_item(app->menu, app->list_selection);
    }
    view_dispatcher_switch_to_view(app->dispatcher, ViewMenu);
}
static void gta_show_text(GtaApp* app, GtaScreen screen) {
    app->screen = screen;
    text_box_set_text(app->text, furi_string_get_cstr(app->content));
    view_dispatcher_switch_to_view(app->dispatcher, ViewText);
}
/* Full controller tokens are essential: old first-letter rendering
 * collapsed R1/R2 and L1/L2 into indistinguishable R/L keys. */
static bool gta_token_is(const char* token, size_t length, const char* value) {
    return strlen(value) == length && strncmp(token, value, length) == 0;
}
static const char* gta_short_button(const char* token, size_t length, bool xbox) {
    if(gta_token_is(token, length, "Up")) return "U";
    if(gta_token_is(token, length, "Down")) return "D";
    if(gta_token_is(token, length, "Left")) return "L";
    if(gta_token_is(token, length, "Right")) return "R";
    if(gta_token_is(token, length, "R1")) return xbox ? "RB" : "R1";
    if(gta_token_is(token, length, "R2")) return xbox ? "RT" : "R2";
    if(gta_token_is(token, length, "L1")) return xbox ? "LB" : "L1";
    if(gta_token_is(token, length, "L2")) return xbox ? "LT" : "L2";
    if(gta_token_is(token, length, "Triangle")) return xbox ? "Y" : "TRI";
    if(gta_token_is(token, length, "Circle")) return xbox ? "B" : "CIR";
    if(gta_token_is(token, length, "Square")) return xbox ? "X" : "SQR";
    if(gta_token_is(token, length, "Cross")) return xbox ? "A" : "CROSS";
    return NULL;
}
static void gta_append_sequence(FuriString* content, const char* sequence, bool xbox) {
    size_t words = 0;
    while(*sequence) {
        while(*sequence == ' ')
            sequence++;
        if(!*sequence) break;
        const size_t length = strcspn(sequence, " ");
        const char* short_name = gta_short_button(sequence, length, xbox);
        if(words) furi_string_cat_printf(content, words % 4 == 0 ? "\n" : " ");
        if(short_name)
            furi_string_cat_printf(content, "%s", short_name);
        else
            furi_string_cat_printf(content, "%.*s", (int)length, sequence);
        sequence += length;
        words++;
    }
}
static void gta_detail(GtaApp* app, const GtaCheat* cheat, const GtaScreenshotCode* screenshot) {
    text_box_reset(app->text);
    furi_string_reset(app->content);
    const bool screenshot_only = cheat == NULL;
    const char* reported = screenshot_only ? NULL : gta_reported_sequence(cheat);
    const char* name = screenshot_only ? screenshot->name : cheat->name;
    furi_string_cat_printf(
        app->content, "%s\n%s\n\n", name, app->xbox ? "Xbox Series X|S" : "PlayStation 5");
    if(!screenshot_only) {
        const char* sequence = gta_sequence(app, cheat);
        if(sequence[0]) {
            furi_string_cat_printf(app->content, "Reported GTA VI D-pad:\n");
            gta_append_sequence(app->content, sequence, app->xbox);
            furi_string_cat_printf(app->content, "\n\n");
        } else if(reported) {
            furi_string_cat_printf(app->content, "Single-source D-pad (unverified)\n");
            gta_append_sequence(app->content, reported, app->xbox);
            furi_string_cat_printf(app->content, "\n\n");
        } else {
            furi_string_cat_printf(app->content, "GTA VI D-pad code: not published here.\n\n");
        }
    }
    if(screenshot) {
        furi_string_cat_printf(
            app->content, screenshot_only ? "UNVERIFIED code:\n" : "NOT TESTED:\n");
        gta_append_sequence(app->content, screenshot->ps5_sequence, app->xbox);
        furi_string_cat_printf(app->content, "\n\n");
        if(app->xbox) {
            furi_string_cat_printf(
                app->content,
                "Xbox labels mapped from PS5 controls. Xbox input NOT independently published.\n\n");
        }
        furi_string_cat_printf(app->content, "This INPUT is not verified for GTA VI.\n\n");
    }
    if(screenshot_only) {
        furi_string_cat_printf(
            app->content,
            "May be a legacy GTA code.\n"
            "PS keys: TRI=Triangle, CIR=Circle, SQR=Square; "
            "U/D/L/R = D-pad directions.");
    } else {
        furi_string_cat_printf(
            app->content,
            "Unlock requirement:\n%s\n\nEffect:\n%s\n\nSource / status:\n%s",
            cheat->unlock,
            cheat->effect,
            cheat->status);
        if(gta_sequence(app, cheat)[0] || reported) {
            furi_string_cat_printf(
                app->content,
                "\n\nGTA VI code press-reported; not tested. "
                "Save before use: cheats may block saving and achievements.");
        }
        if(screenshot) {
            furi_string_cat_printf(
                app->content,
                "\n\nThe reported effect matches this main entry, "
                "but its alternate input is NOT confirmed.\n"
                "PS keys: TRI=Triangle, CIR=Circle, SQR=Square.");
        }
    }
    gta_show_text(app, ScreenDetail);
}
static void gta_guide(GtaApp* app) {
    app->guide_back = app->screen;
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "Companion v" FAP_VERSION "\n\n"
        "Secret Missions: optional finds by reported chapter. Right toggles checkmarks in the list; "
        "OK opens details and Check/Uncheck toggles status. Progress saves on SD. "
        "Chapter gates marked ? are uncertain. "
        "Safe & Door Codes: browse published numeric codes and passwords by type. "
        "[?] marks less-corroborated, conflicting or potentially changing reports. "
        "Ordinary robbery safes often have a random code.\n\n"
        "Bridges, Cars, Stunts and Collections: each listed entry can be marked Completed. "
        "Right toggles a checkmark in its list; OK opens details, with Check/Undo at center. "
        "Progress saves to SD and reloads automatically.\n\n"
        "Under the Bridge and Stunt Jumps are grouped by county in Collections; "
        "Knife Flights and Base Jumps are there too (397 stunts total). "
        "Cars include Randy, Moses, Wyman and Standout Vehicles. "
        "Collections include cards, scans, flamingos and other finds. "
        "Any 300 stunts count toward 100%%.\n\n"
        "Cocaine Flamingos: destroy the figures with BLACK-WHITE beaks. "
        "Black-yellow beaks are decorative; collecting loose cocaine is optional. "
        "Magazine Styles: scan with the phone, switch to Jason/Lucia for character-specific "
        "unlocks, and inspect private houses when clues point inside.\n\n"
        "Diving: Island Dreams unlocks diving. Bring scuba gear, choose the metal detector "
        "from lower-right weapon wheel, follow chest markers and dig once light turns GREEN. "
        "The nearby wrecks southeast of Key Lento can be visited together.\n\n"
        "Optional Missions are a separate Collections checklist; Cal and Kirk already appear "
        "in Secret Missions. Messages in Bottles are optional reading, not part of 34 books "
        "and swaps. Extra unique weapon names are guide-reported, not game-verified.\n\n"
        "Missables: keep a manual save before Tourist Trap (C1) and Checking Out (C2). "
        "In C2 pay Linda before Slam Dunk, collect Starlet Motel cards and books, and help Wyman "
        "at his car before moving. In C3 take Gil's pistol in Safe Word and Monty's pistol in The Missing Link. "
        "Pete's rifle is a C4 mission pickup.\n\n"
        "Moses: during Take a Vehicle to Moses (C2), bring an eligible tracker-free car for one free registration. "
        "Bison has six cargo slots, Dominator two; Montagne XL waits behind Watkins Auto Parts. "
        "Confirm Register shows Free before accepting. See Thrax for reported early access.\n\n"
        "Fast Focus leveling: Rodgers Ranch Shooting Range, northeast Port Gellhorn (Kelly County). "
        "Buy the $5 waiver and choose Market Range Beginner. Hit all six targets (about 5 sec). "
        "Select Quit and back out to the range, then start again. Do NOT choose Restart on results. "
        "Each exit is reported to grant permanent Focus; gains vary. Check Jason & Lucia > character > "
        "purple eye stat until Level 4. Repeat for the other character after Coffee Break (C2). "
        "No cheat activation needed. Patch behavior may change. See Shooting Gold Medals.\n\n"
        "Dispatch: pick a region for unchecked deliveries and collectibles. "
        "OK checks or undoes a stop. Right opens the next stop. "
        "Back returns to the region menu.\n\n"
        "61 GTA VI effects reported by game guides, but most inputs remain unpublished. "
        "[?] marks entries without a code.\n\n"
        "21 inputs are corroborated in PS5 and Xbox guides, including Drunk. "
        "Pinhole [report] under Filters has a single-source input. "
        "The app labels it unverified.\n\n"
        "To use: finish the story and the individual cheat's unlock challenge. "
        "Check Progress > Cheats. In free roam press D-pad Up to open phone. "
        "Hold L1 and R1 (PS5) or LB and RB (Xbox), then enter the D-pad code.\n\n"
        "Save before cheats: outlets report no saving or trophies/achievements "
        "once a cheat is active in that session. Story Mode only.\n\n"
        "Test source: 28 PlayStation controller sequences transcribed, not GTA VI-tested. "
        "24 screenshot-only entries show their sequences in Unverified codes. "
        "Four matching effects show the test input as an alternate in the main catalog. "
        "Effect matches do NOT verify the test inputs. "
        "Xbox equivalents are button-label conversions, not separately sourced codes. "
        "TRI=Triangle CIR=Circle SQR=Square; U/D/L/R=direction.");

    size_t total_items = 0;
    size_t completed_items = 0;
    if(gta_companion_progress_summary(app->companion, &total_items, &completed_items)) {
        /* Hundredths of a percent, rounded: no floating-point printf needed. */
        const uint32_t progress =
            total_items ? (uint32_t)((completed_items * 10000u + total_items / 2u) / total_items) :
                          0u;
        furi_string_cat_printf(
            app->content,
            "\n\nTotal Collection Items: %lu\n"
            "Total Items Completed: %lu\n"
            "Progress: %lu.%02lu%%",
            (unsigned long)total_items,
            (unsigned long)completed_items,
            (unsigned long)(progress / 100u),
            (unsigned long)(progress % 100u));
    } else {
        furi_string_cat_printf(
            app->content,
            "\n\nTotal Collection Items: Unavailable\n"
            "Total Items Completed: Unavailable\n"
            "Progress: Unavailable");
    }
    gta_show_text(app, ScreenGuide);
}
static void gta_money_glitch(GtaApp* app) {
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "Cars + Cocaine Money Glitch\n\n"
        "Story Mode; reported exploit, not a cheat code. The reliable core is "
        "warehouse cocaine respawning while bags stay in vehicle storage. "
        "A two-car payout loop at Dunk's is community-reported ONLY; confirm "
        "real credits, not merely a changing counter. Back up your save.\n\n"
        "1. Prepare two vehicles\n"
        "a) Chapter 2 is safer: Chapter 1 story progress can take your cash.\n"
        "b) Register BOTH vehicles before filling: a 10-slot van AND a "
        "6-slot car. Use owned cars or register each at Pay n Spray or "
        "Moses; do not rely on unregistered stolen vehicles.\n"
        "c) Keep a separate manual backup save; do not enable cheats.\n\n"
        "2. Fill with cocaine\n"
        "a) Drive to Apollo Aggregates, red warehouse east of Hamlet.\n"
        "b) Loot cocaine from the outside teddy-bear van, the inside van, "
        "office table and safe; take the center van's cash bag.\n"
        "c) Fill and stow bags in your 10-slot van, then your second car. "
        "Leave the INSIDE warehouse van parked so its loot can reset.\n\n"
        "3. Refresh by switching characters\n"
        "a) Leave your collected cocaine stored in your vehicles. "
        "Switch to the other playable character, then switch back.\n"
        "b) Return to Apollo Aggregates and recheck the inside van and "
        "office safe for respawned cocaine. Reload your vehicles; repeat.\n"
        "c) If it does not reset, put bags in an owned vehicle, save to a "
        "separate slot, reload, then return. Do not overwrite your backup.\n\n"
        "4. Two-car Dunk's loop (UNVERIFIED)\n"
        "a) At Dunk's trailer, reverse the FULL 10-slot van to the drop point "
        "and unload its bags.\n"
        "b) Move the van out, back the full 6-slot car into place, and unload.\n"
        "c) A player claims that repeating character pickup/drop-off after "
        "the swap keeps storage from depleting (~$13,200/drop). "
        "This may be a VISUAL bug with no payout: test a small sale first.\n\n"
        "5. Check payout or sell normally\n"
        "a) Verify actual bank balance, pending payments and safehouse mail "
        "before relying on repeat sales.\n"
        "b) If the two-car claim fails, sell the filled van normally. "
        "Donnie/Dunk in early chapters or Tyrone (Ch3, best narcotics "
        "rate); Brian's Lockup uses keypad 7700 and pays later.\n"
        "c) Exact prices, vehicle slots, respawns and patch status vary.");
    gta_show_text(app, ScreenMoneyGlitch);
}
static const char* gta_safe_status_name(GtaSafeStatus status) {
    switch(status) {
    case GtaSafeTested:
        return "Guide-tested; not tested by this app";
    case GtaSafeCorroborated:
        return "Multiple published reports";
    case GtaSafeVariable:
        return "May differ by robbery/save";
    case GtaSafeConflicting:
        return "Sources disagree; check clue";
    case GtaSafeReported:
        return "Limited / single-source report";
    default:
        return "Unverified";
    }
}
static const char* gta_safe_kind_name(GtaSafeKind kind) {
    switch(kind) {
    case GtaSafeMission:
        return "Mission safe";
    case GtaSafeOperation:
        return "Gang / operation safe";
    case GtaSafeDoor:
        return "Door or garage keypad";
    case GtaSafePuzzle:
        return "Puzzle / other lock";
    default:
        return "Code";
    }
}
static void gta_safe_detail(GtaApp* app, size_t index) {
    if(index >= gta_safe_code_count) return;
    const GtaSafeCode* entry = &gta_safe_codes[index];
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "%s\n\nCODE: %s\n\nRegion: %s\nType: %s\n\nWhere:\n%s\n\nClue / notes:\n%s\n\nStatus:\n%s",
        entry->name,
        entry->code,
        entry->region,
        gta_safe_kind_name(entry->kind),
        entry->where,
        entry->notes,
        gta_safe_status_name(entry->status));
    gta_show_text(app, ScreenSafeDetail);
}
static void gta_secret_detail(GtaApp* app, size_t index) {
    if(index >= gta_secret_mission_count) return;
    const GtaSecretMission* entry = &gta_secret_missions[index];
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "%s\n\nEarliest access: %s\n\nLocation:\n%s\n\nSteps:\n%s\n\n"
        "Reward:\n%s\n\nConfidence:\n%s\n\nSources:\n%s",
        entry->title,
        entry->when,
        entry->where,
        entry->steps,
        entry->reward,
        entry->status,
        entry->sources);
    app->screen = ScreenSecretDetail;
    widget_reset(app->secret_detail);
    widget_add_text_scroll_element(
        app->secret_detail, 0, 0, 128, 52, furi_string_get_cstr(app->content));
    widget_add_button_element(
        app->secret_detail,
        GuiButtonTypeCenter,
        gta_secret_progress_is_checked(&app->secret_progress, entry->progress_id) ? "Uncheck" :
                                                                                    "Check",
        gta_secret_button_callback,
        app);
    view_dispatcher_switch_to_view(app->dispatcher, ViewSecretDetail);
}
static void gta_safe_tips(GtaApp* app) {
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "Randomized safe codes\n\n"
        "This list collects published fixed or reported combinations. "
        "Not all map safes have a reusable code. Many ordinary store and bank robbery "
        "safes change the code between runs or saves. Some mission codes also vary.\n\n"
        "Find the clue: check back-office notes, whiteboards, maps, computers or guards. "
        "Press L3 + R3 (both sticks) to activate Focus Mode. "
        "Use the Safe Code? dialogue prompt during a robbery when available. "
        "Check Notes on the keypad if you already collected the clue.\n\n"
        "If a code fails, do not assume a game bug; check your local clue. "
        "Retain every leading zero. Some keypads accept five or six digits, "
        "and some computers accept words.\n\n"
        "Sticky bombs can force open some safes but attract enemies or police. "
        "Locked mission doors and garage keypads may not respond to explosives.\n\n"
        "Entries are based on published guides through 24 Nov 2026 and have not "
        "been independently tested by this app's maintainers.");
    gta_show_text(app, ScreenSafeTips);
}
static bool gta_event(void* context, uint32_t event) {
    GtaApp* app = context;
    if(app->screen == ScreenCompanion) return gta_companion_custom_event(app->companion, event);
    if(app->screen == ScreenHome) {
        if(event == ItemMoneyGlitch) {
            gta_money_glitch(app);
        } else if(event == ItemSafeCodes) {
            gta_show_menu(app, ScreenSafeCategories);
        } else if(event == ItemSecretMissions) {
            gta_show_menu(app, ScreenSecretList);
        } else if(event == ItemGuide) {
            gta_guide(app);
        } else if(event == ItemPs5 || event == ItemXbox) {
            app->xbox = event == ItemXbox;
            app->category_selection = ItemPublished;
            gta_show_menu(app, ScreenCategories);
        } else if(event >= ItemCollection && event < ItemDispatch) {
            app->screen = ScreenCompanion;
            app->companion_back = ScreenHome;
            gta_companion_open_list(app->companion, (GtaCollectionId)(event - ItemCollection));
        } else if(event == ItemDispatch) {
            gta_show_menu(app, ScreenDispatchRegions);
        } else if(event == ItemCollections) {
            gta_show_menu(app, ScreenCollections);
        } else if(event == ItemMockLoaders) {
            app->screen = ScreenMockLoaders;
            view_dispatcher_switch_to_view(app->dispatcher, ViewMockLoaders);
        } else if(event == ItemMockLoaderSettings) {
            app->screen = ScreenMockLoaderSettings;
            view_dispatcher_switch_to_view(app->dispatcher, ViewMockLoaderSettings);
        }
    } else if(app->screen == ScreenSecretList) {
        if(event == ItemSecretOpen && app->secret_index < gta_secret_mission_count) {
            gta_secret_detail(app, app->secret_index);
        } else if(event == ItemSecretToggle && app->secret_index < gta_secret_mission_count) {
            gta_secret_progress_toggle(
                &app->secret_progress,
                app->secret_storage,
                gta_secret_missions[app->secret_index].progress_id);
            gta_secret_refresh(app);
        }
    } else if(app->screen == ScreenSecretDetail) {
        if(event == ItemSecretToggleDetail && app->secret_index < gta_secret_mission_count) {
            gta_secret_progress_toggle(
                &app->secret_progress,
                app->secret_storage,
                gta_secret_missions[app->secret_index].progress_id);
            gta_secret_refresh(app);
            gta_secret_detail(app, app->secret_index);
        }
    } else if(app->screen == ScreenSafeCategories) {
        if(event == ItemSafeTips) {
            app->safe_category_selection = event;
            gta_safe_tips(app);
        } else if(event >= ItemSafeAll && event <= ItemSafePuzzles) {
            app->safe_category_selection = event;
            app->safe_category = (uint8_t)(event - ItemSafeAll);
            app->safe_list_selection = 0;
            gta_show_menu(app, ScreenSafeList);
        }
    } else if(app->screen == ScreenSafeList) {
        if(event >= ItemSafeRow && event - ItemSafeRow < gta_safe_code_count) {
            const size_t index = event - ItemSafeRow;
            if(!app->safe_category ||
               gta_safe_codes[index].kind == (GtaSafeKind)(app->safe_category - 1)) {
                app->safe_list_selection = event;
                gta_safe_detail(app, index);
            }
        }
    } else if(app->screen == ScreenDispatchRegions) {
        if(event < ItemDispatchRegion || event - ItemDispatchRegion >= GtaDispatchRegionCount) {
            return false;
        }
        app->dispatch_region = (GtaDispatchRegion)(event - ItemDispatchRegion);
        app->companion_back = ScreenDispatchRegions;
        app->screen = ScreenCompanion;
        gta_companion_open_dispatch(app->companion, app->dispatch_region);
    } else if(app->screen == ScreenCollections || app->screen == ScreenCollectionGroup) {
        const bool root = app->screen == ScreenCollections;
        const GtaExploreSection* items = root ? gta_explore_sections :
                                                app->explore_group->children;
        const size_t count = root ? gta_explore_section_count : app->explore_group->child_count;
        if(event < ItemExploreRow || event - ItemExploreRow >= count) return false;
        const GtaExploreSection* selected = &items[event - ItemExploreRow];
        if(root) {
            app->explore_root_selection = event;
        } else {
            app->explore_group_selection = event;
        }
        if(selected->child_count) {
            app->explore_group = selected;
            app->explore_group_selection = ItemExploreRow;
            gta_show_menu(app, ScreenCollectionGroup);
        } else {
            app->companion_back = app->screen;
            app->screen = ScreenCompanion;
            gta_companion_open_exploration(app->companion, selected);
        }
    } else if(app->screen == ScreenCategories) {
        app->category_selection = event;
        if(event == ItemGuide) {
            gta_guide(app);
        } else if(
            event == ItemPublished || event == ItemAll || event == ItemUnverified ||
            (event >= ItemRow && event - ItemRow < gta_cheat_count)) {
            app->list_mode = event == ItemPublished  ? ListPublished :
                             event == ItemAll        ? ListAll :
                             event == ItemUnverified ? ListUnverified :
                                                       ListCategory;
            if(app->list_mode == ListCategory) app->category = event - ItemRow;
            app->list_selection = ItemEmpty;
            gta_show_menu(app, ScreenList);
        }
    } else if(app->screen == ScreenList && event >= ItemRow) {
        const size_t index = event - ItemRow;
        if(app->list_mode == ListUnverified && index < gta_screenshot_count &&
           !gta_screenshot_codes[index].main_catalog_name) {
            app->list_selection = event;
            gta_detail(app, NULL, &gta_screenshot_codes[index]);
        } else if(app->list_mode != ListUnverified && index < gta_cheat_count) {
            app->list_selection = event;
            gta_detail(
                app,
                &gta_cheat_catalog[index],
                gta_screenshot_match(gta_cheat_catalog[index].name));
        }
    }
    return true;
}
static bool gta_back(void* context) {
    GtaApp* app = context;
    switch(app->screen) {
    case ScreenStartup:
        break;
    case ScreenHome:
        view_dispatcher_stop(app->dispatcher);
        break;
    case ScreenCategories:
    case ScreenCollections:
    case ScreenDispatchRegions:
    case ScreenMockLoaders:
    case ScreenMockLoaderSettings:
    case ScreenMoneyGlitch:
    case ScreenSafeCategories:
    case ScreenSecretList:
        gta_show_menu(app, ScreenHome);
        break;
    case ScreenSecretDetail:
        gta_show_menu(app, ScreenSecretList);
        break;
    case ScreenSafeList:
        gta_show_menu(app, ScreenSafeCategories);
        break;
    case ScreenSafeDetail:
        gta_show_menu(app, ScreenSafeList);
        break;
    case ScreenSafeTips:
        gta_show_menu(app, ScreenSafeCategories);
        break;
    case ScreenList:
        gta_show_menu(app, ScreenCategories);
        break;
    case ScreenDetail:
        gta_show_menu(app, ScreenList);
        break;
    case ScreenGuide:
        gta_show_menu(app, app->guide_back);
        break;
    case ScreenCompanion:
        if(!gta_companion_back(app->companion)) gta_show_menu(app, app->companion_back);
        break;
    case ScreenCollectionGroup:
        gta_show_menu(app, ScreenCollections);
        break;
    }
    return true;
}
static void gta_finish_startup(void* context) {
    gta_show_menu(context, ScreenHome);
}
int32_t gta_vi_companion_app(void* args) {
    UNUSED(args);
    GtaApp app = {
        .dispatcher = view_dispatcher_alloc(),
        .home = view_alloc(),
        .menu = submenu_alloc(),
        .text = text_box_alloc(),
        .content = furi_string_alloc(),
        .explore_root_selection = ItemExploreRow,
        .safe_category_selection = ItemSafeAll,
    };
    app.secret_storage = furi_record_open(RECORD_STORAGE);
    gta_secret_progress_load(&app.secret_progress, app.secret_storage);
    app.secret_list = view_alloc();
    view_allocate_model(app.secret_list, ViewModelTypeLocking, sizeof(GtaSecretListModel));
    view_set_context(app.secret_list, &app);
    view_set_draw_callback(app.secret_list, gta_secret_list_draw);
    view_set_input_callback(app.secret_list, gta_secret_list_input);
    app.secret_detail = widget_alloc();
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_set_event_callback_context(app.dispatcher, &app);
    view_dispatcher_set_custom_event_callback(app.dispatcher, gta_event);
    view_dispatcher_set_navigation_event_callback(app.dispatcher, gta_back);
    view_set_context(app.home, &app);
    view_allocate_model(app.home, ViewModelTypeLocking, sizeof(size_t));
    with_view_model(app.home, size_t * selected, { *selected = 0; }, false);
    view_set_draw_callback(app.home, gta_home_draw);
    view_set_input_callback(app.home, gta_home_input);
    view_dispatcher_add_view(app.dispatcher, ViewHome, app.home);
    view_dispatcher_add_view(app.dispatcher, ViewSecretList, app.secret_list);
    view_dispatcher_add_view(app.dispatcher, ViewSecretDetail, widget_get_view(app.secret_detail));
    view_dispatcher_add_view(app.dispatcher, ViewMenu, submenu_get_view(app.menu));
    view_dispatcher_add_view(app.dispatcher, ViewText, text_box_get_view(app.text));
    app.mock_loaders =
        gta_mock_loaders_alloc(app.dispatcher, ViewMockLoaders, ViewMockLoaderSettings);
    app.companion = gta_companion_alloc(app.dispatcher, ViewCollection, ViewCollectionDetail);
    view_dispatcher_attach_to_gui(app.dispatcher, gui, ViewDispatcherTypeFullscreen);
    FuriEventLoopTimer* startup_timer = NULL;
    app.screen = ScreenStartup;
    if(gta_mock_loaders_show_startup(app.mock_loaders)) {
        startup_timer = furi_event_loop_timer_alloc(
            view_dispatcher_get_event_loop(app.dispatcher),
            gta_finish_startup,
            FuriEventLoopTimerTypeOnce,
            &app);
        furi_event_loop_timer_start(startup_timer, furi_ms_to_ticks(500));
    } else {
        gta_show_menu(&app, ScreenHome);
    }
    view_dispatcher_run(app.dispatcher);
    if(startup_timer) furi_event_loop_timer_free(startup_timer);
    gta_companion_free(app.companion);
    gta_mock_loaders_free(app.mock_loaders);
    view_dispatcher_remove_view(app.dispatcher, ViewText);
    view_dispatcher_remove_view(app.dispatcher, ViewMenu);
    view_dispatcher_remove_view(app.dispatcher, ViewHome);
    view_dispatcher_remove_view(app.dispatcher, ViewSecretList);
    view_dispatcher_remove_view(app.dispatcher, ViewSecretDetail);
    view_dispatcher_free(app.dispatcher);
    view_free(app.home);
    view_free(app.secret_list);
    widget_free(app.secret_detail);
    furi_record_close(RECORD_STORAGE);
    text_box_free(app.text);
    submenu_free(app.menu);
    furi_string_free(app.content);
    furi_record_close(RECORD_GUI);
    return 0;
}
