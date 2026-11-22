#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>
#include <string.h>
#include "cheat_catalog.h"
#include "companion.h"
#include "gta_vi_companion_icons.h"
#include "mock_loaders.h"

typedef enum {
    ViewMenu,
    ViewText,
    ViewCollection,
    ViewCollectionDetail,
    ViewHome,
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
    ItemRow = 16,
    ItemCollection = 128,
    ItemDispatch = ItemCollection + GtaCollectionCount,
    ItemCollections,
    ItemMockLoaders,
    ItemMockLoaderSettings,
    ItemExploreRow = 256,
    ItemDispatchRegion = 0x300,
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
    {&I_menu_randys_wish_list_25x25, ItemCollection + GtaCollectionRandy, NULL},
    {&I_menu_moses_drop_offs_25x25, ItemCollection + GtaCollectionMoses, NULL},
    {&I_menu_wymans_classic_cars_25x25, ItemCollection + GtaCollectionWyman, NULL},
    {&I_menu_standout_vehicles_25x25, ItemCollection + GtaCollectionStandout, NULL},
    {&I_menu_garages_25x25, ItemCollection + GtaCollectionGarages, NULL},
    {&I_menu_houses_cribs_25x25, ItemCollection + GtaCollectionHouses, NULL},
    {&I_menu_dispatch_25x25, ItemDispatch, "Dispatch"},
    {&I_menu_quick_guide_warnings_25x25, ItemGuide, "Quick guide / warnings"},
    {&I_menu_mock_loaders_25x25, ItemMockLoaders, "Mock Loaders"},
    {&I_menu_mock_loader_settings_25x25, ItemMockLoaderSettings, "Mock Loader Settings"},
};

enum {
    GtaHomeColumns = 4,
    GtaHomePageSize = 2 * GtaHomeColumns,
    GtaHomeItemCount = sizeof(gta_home_items) / sizeof(gta_home_items[0]),
    GtaHomePageCount = (GtaHomeItemCount + GtaHomePageSize - 1) / GtaHomePageSize,
};

typedef struct {
    ViewDispatcher* dispatcher;
    View* home;
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

static void gta_show_menu(GtaApp* app, GtaScreen screen) {
    app->screen = screen;
    if(screen == ScreenHome) {
        view_dispatcher_switch_to_view(app->dispatcher, ViewHome);
        return;
    }

    submenu_reset(app->menu);
    char label[64];
    if(screen == ScreenCollections || screen == ScreenCollectionGroup) {
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
        "Collections: OK opens the location. Right checks or unchecks a row. "
        "OK also toggles the check in details. Checkmarks are saved on the SD card.\n\n"
        "Collections includes Story Mode discoveries, collectibles and all 397 stunts. "
        "Choose a county for Stunt Jumps or Under the Bridge. "
        "Any 300 stunts count toward 100%%.\n\n"
        "Dispatch: pick a region for unchecked deliveries and collectibles. "
        "OK checks or undoes a stop. Right opens the next stop. "
        "Back returns to the region menu.\n\n"
        "61 GTA VI effects reported by game guides, but most inputs remain unpublished. "
        "[?] marks entries without a code.\n\n"
        "19 inputs are corroborated in PS5 and Xbox guides, including Drunk. "
        "Open Pinhole [report] under Filters for a single-source input. "
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
    gta_show_text(app, ScreenGuide);
}
static void gta_money_glitch(GtaApp* app) {
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "Infinite Money Glitch\n\n"
        "Story Mode only. Reported at launch; updates may patch the loop. "
        "The reset can restore warehouse pickups while your stowed bags stay put. Repeat and sell the loot; it is not instant cash. "
        "Keep a separate backup save.\n\n"
        "1. Find the warehouse\n"
        "Use free roam with cheats off. A personal van with space is helpful. "
        "Wait until Chapter 2, or back up your save before Tourist Trap; early earnings may be lost. "
        "Go to Apollo Aggregates, east of Hamlet and north of Key Lento. Park by the red building.\n\n"
        "2. Gather the loot\n"
        "Check the teddy-bear van outside, the van inside, and the office table, safe and cash bag. "
        "Keep track of which pickups come back.\n\n"
        "3. Pack your vehicle\n"
        "Carry loot back in bags and stow them in your personal vehicle. "
        "Check its storage before attempting a reset.\n\n"
        "4. Reset (choose one)\n"
        "a) Drive about half a mile away, then return. Some one-time loot may not respawn.\n"
        "b) After Coffee Break in Chapter 2, hold D-pad Right + R1 (PS5) or RB (Xbox) to switch. "
        "Go from Lucia to Jason while he is far away, then switch back to Lucia.\n"
        "c) After stowing the loot, make a manual save in a working slot. Reload that exact save. "
        "Verify your stash and the warehouse pickups.\n\n"
        "5. Repeat or cash out\n"
        "Repeat only if the loot respawns. Deliver it to Brian's Lockup once unlocked (door code 7700), "
        "or use a fence available in your chapter. Check the safehouse mailbox after a few in-game days "
        "for delayed payout from Brian. Amounts vary.");
    gta_show_text(app, ScreenMoneyGlitch);
}

static bool gta_event(void* context, uint32_t event) {
    GtaApp* app = context;
    if(app->screen == ScreenCompanion) return gta_companion_custom_event(app->companion, event);
    if(app->screen == ScreenHome) {
        if(event == ItemMoneyGlitch) {
            gta_money_glitch(app);
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
        gta_show_menu(app, ScreenHome);
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
    };
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
    view_dispatcher_free(app.dispatcher);
    view_free(app.home);
    text_box_free(app.text);
    submenu_free(app.menu);
    furi_string_free(app.content);
    furi_record_close(RECORD_GUI);
    return 0;
}
