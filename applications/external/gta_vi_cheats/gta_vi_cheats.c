#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>
#include <string.h>
#include "cheat_catalog.h"

typedef enum {
    ViewMenu,
    ViewText
} GtaView;
typedef enum {
    ScreenPlatform,
    ScreenCategories,
    ScreenList,
    ScreenDetail,
    ScreenGuide
} GtaScreen;
typedef enum {
    ListPublished,
    ListAll,
    ListCategory,
    ListImageClaims
} GtaListMode;
enum {
    ItemPs5,
    ItemXbox,
    ItemGuide,
    ItemPublished,
    ItemAll,
    ItemImageClaims,
    ItemEmpty,
    ItemRow = 16
};

typedef struct {
    ViewDispatcher* dispatcher;
    Submenu* menu;
    TextBox* text;
    FuriString* content;
    GtaScreen screen;
    GtaScreen guide_back;
    GtaListMode list_mode;
    bool xbox;
    size_t category;
    uint32_t category_selection;
    uint32_t list_selection;
} GtaApp;

static const char* gta_sequence(const GtaApp* app, const GtaCheat* cheat) {
    return app->xbox ? cheat->xbox : cheat->ps5;
}
static void gta_post_selection(void* context, uint32_t index) {
    GtaApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, index);
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
    submenu_reset(app->menu);
    app->screen = screen;
    char label[64];
    if(screen == ScreenPlatform) {
        submenu_set_header(app->menu, "GTA VI Cheats v0.3");
        gta_item(app, "PlayStation 5", ItemPs5);
        gta_item(app, "Xbox Series X|S", ItemXbox);
        gta_item(app, "Quick guide / warnings", ItemGuide);
        submenu_set_selected_item(app->menu, app->xbox ? ItemXbox : ItemPs5);
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
        snprintf(label, sizeof(label), "Image claims (%zu) [!]", gta_image_claim_count);
        gta_item(app, label, ItemImageClaims);
        gta_item(app, "Quick guide / warnings", ItemGuide);
        submenu_set_selected_item(app->menu, app->category_selection);
    } else if(screen == ScreenList) {
        const bool image_claims = app->list_mode == ListImageClaims;
        const char* header = "All GTA VI";
        if(app->list_mode == ListPublished)
            header = "Published inputs";
        else if(app->list_mode == ListCategory)
            header = gta_cheat_catalog[app->category].category;
        else if(image_claims)
            header = "Image claims: NOT VI";
        submenu_set_header(app->menu, header);
        bool any = false;
        if(image_claims) {
            for(size_t i = 0; i < gta_image_claim_count; i++) {
                gta_item(app, gta_image_claims[i], ItemRow + i);
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
                    snprintf(label, sizeof(label), "%s [?]", cheat->name);
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
static void gta_append_sequence(FuriString* content, const char* sequence) {
    while(*sequence) {
        const size_t length = strcspn(sequence, " ");
        furi_string_cat_printf(content, "%c", *sequence);
        sequence += length;
        while(*sequence == ' ')
            sequence++;
        if(*sequence) furi_string_cat_printf(content, " ");
    }
}
static void gta_detail(GtaApp* app, const GtaCheat* cheat) {
    text_box_reset(app->text);
    furi_string_reset(app->content);
    const char* sequence = gta_sequence(app, cheat);
    furi_string_cat_printf(
        app->content, "%s\n%s\n\n", cheat->name, app->xbox ? "Xbox Series X|S" : "PlayStation 5");
    if(sequence[0]) {
        furi_string_cat_printf(app->content, "Reported D-pad input:\n");
        gta_append_sequence(app->content, sequence);
        furi_string_cat_printf(app->content, "\nU=Up  D=Down\nL=Left R=Right\n\n");
    } else {
        furi_string_cat_printf(
            app->content,
            "Input NOT published for this platform.\nDo not use older GTA codes.\n\n");
    }
    furi_string_cat_printf(
        app->content,
        "Unlock requirement:\n%s\n\nEffect:\n%s\n\nSource / status:\n%s",
        cheat->unlock,
        cheat->effect,
        cheat->status);
    if(sequence[0])
        furi_string_cat_printf(
            app->content,
            "\n\nPress-reported, not tested by this app. Save before using: "
            "cheats block saving and trophies/achievements in that session.");
    gta_show_text(app, ScreenDetail);
}
static void gta_image_detail(GtaApp* app, const char* title) {
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "%s\n\nIMAGE CLAIM ONLY\n\n"
        "The supplied 28-row picture claims this is a GTA VI cheat, "
        "but its multi-button inputs are NOT confirmed for GTA VI.\n\n"
        "Several titles match older GTA games, including San Andreas. "
        "GTA VI guides give D-pad-only code sequences.\n\n"
        "The photographed code is deliberately excluded from the GTA VI input list. "
        "This tab indexes the image labels for auditing only.",
        title);
    gta_show_text(app, ScreenDetail);
}
static void gta_guide(GtaApp* app) {
    app->guide_back = app->screen;
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "Quick guide (v0.3)\n\n"
        "61 GTA VI effects reported by game guides, but most inputs remain unpublished. "
        "[?] marks entries without a code.\n\n"
        "18 inputs are reported by multiple sources on PS5 and Xbox. "
        "Drunk is an EXTRA PS5-only single-source Polygon report. "
        "Do not treat it as independently confirmed.\n\n"
        "To use: finish the story and the individual cheat's unlock challenge. "
        "Check Progress > Cheats. In free roam press D-pad Up to open phone. "
        "Hold L1 and R1 (PS5) or LB and RB (Xbox), then enter the D-pad code.\n\n"
        "Save before cheats: outlets report no saving or trophies/achievements "
        "once a cheat is active in that session. Story Mode only.\n\n"
        "Image claims [!] are 28 labels from the attached social-media post. "
        "The picture is NOT a credible source for GTA VI input sequences.");
    gta_show_text(app, ScreenGuide);
}
static bool gta_event(void* context, uint32_t event) {
    GtaApp* app = context;
    if(app->screen == ScreenPlatform) {
        if(event == ItemGuide) {
            gta_guide(app);
        } else if(event == ItemPs5 || event == ItemXbox) {
            app->xbox = event == ItemXbox;
            app->category_selection = ItemPublished;
            gta_show_menu(app, ScreenCategories);
        }
    } else if(app->screen == ScreenCategories) {
        app->category_selection = event;
        if(event == ItemGuide) {
            gta_guide(app);
        } else if(
            event == ItemPublished || event == ItemAll || event == ItemImageClaims ||
            (event >= ItemRow && event - ItemRow < gta_cheat_count)) {
            app->list_mode = event == ItemPublished   ? ListPublished :
                             event == ItemAll         ? ListAll :
                             event == ItemImageClaims ? ListImageClaims :
                                                        ListCategory;
            if(app->list_mode == ListCategory) app->category = event - ItemRow;
            app->list_selection = ItemEmpty;
            gta_show_menu(app, ScreenList);
        }
    } else if(app->screen == ScreenList && event >= ItemRow) {
        const size_t index = event - ItemRow;
        if(app->list_mode == ListImageClaims && index < gta_image_claim_count) {
            app->list_selection = event;
            gta_image_detail(app, gta_image_claims[index]);
        } else if(app->list_mode != ListImageClaims && index < gta_cheat_count) {
            app->list_selection = event;
            gta_detail(app, &gta_cheat_catalog[index]);
        }
    }
    return true;
}
static bool gta_back(void* context) {
    GtaApp* app = context;
    switch(app->screen) {
    case ScreenPlatform:
        view_dispatcher_stop(app->dispatcher);
        break;
    case ScreenCategories:
        gta_show_menu(app, ScreenPlatform);
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
    }
    return true;
}
int32_t gta_vi_cheats_app(void* args) {
    UNUSED(args);
    GtaApp app = {
        .dispatcher = view_dispatcher_alloc(),
        .menu = submenu_alloc(),
        .text = text_box_alloc(),
        .content = furi_string_alloc(),
    };
    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_set_event_callback_context(app.dispatcher, &app);
    view_dispatcher_set_custom_event_callback(app.dispatcher, gta_event);
    view_dispatcher_set_navigation_event_callback(app.dispatcher, gta_back);
    view_dispatcher_add_view(app.dispatcher, ViewMenu, submenu_get_view(app.menu));
    view_dispatcher_add_view(app.dispatcher, ViewText, text_box_get_view(app.text));
    view_dispatcher_attach_to_gui(app.dispatcher, gui, ViewDispatcherTypeFullscreen);
    gta_show_menu(&app, ScreenPlatform);
    view_dispatcher_run(app.dispatcher);
    view_dispatcher_remove_view(app.dispatcher, ViewText);
    view_dispatcher_remove_view(app.dispatcher, ViewMenu);
    view_dispatcher_free(app.dispatcher);
    text_box_free(app.text);
    submenu_free(app.menu);
    furi_string_free(app.content);
    furi_record_close(RECORD_GUI);
    return 0;
}
