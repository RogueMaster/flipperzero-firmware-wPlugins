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
enum {
    ItemPs5,
    ItemXbox,
    ItemGuide,
    ItemAll,
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
    bool xbox;
    bool all_cheats;
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

static void gta_show_menu(GtaApp* app, GtaScreen screen) {
    submenu_reset(app->menu);
    app->screen = screen;
    if(screen == ScreenPlatform) {
        submenu_set_header(app->menu, "GTA VI Story Mode");
        gta_item(app, "PlayStation 5", ItemPs5);
        gta_item(app, "Xbox Series X|S", ItemXbox);
        gta_item(app, "Quick guide / warnings", ItemGuide);
        submenu_set_selected_item(app->menu, app->xbox ? ItemXbox : ItemPs5);
    } else if(screen == ScreenCategories) {
        size_t known = 0;
        char label[64];
        submenu_set_header(app->menu, app->xbox ? "Xbox Series X|S" : "PlayStation 5");
        for(size_t i = 0; gta_cheat_catalog[i].name; i++)
            known += gta_sequence(app, &gta_cheat_catalog[i])[0] != '\0';
        snprintf(label, sizeof(label), "All cheats (%zu)", known);
        gta_item(app, label, ItemAll);
        for(size_t start = 0; gta_cheat_catalog[start].name;) {
            size_t end = start + 1;
            while(gta_cheat_catalog[end].name &&
                  strcmp(gta_cheat_catalog[start].category, gta_cheat_catalog[end].category) == 0)
                end++;
            size_t count = 0;
            for(size_t i = start; i < end; i++)
                count += gta_sequence(app, &gta_cheat_catalog[i])[0] != '\0';
            if(count) {
                snprintf(
                    label, sizeof(label), "%s (%zu)", gta_cheat_catalog[start].category, count);
                gta_item(app, label, ItemRow + start);
            }
            start = end;
        }
        gta_item(app, "Quick guide / warnings", ItemGuide);
        submenu_set_selected_item(app->menu, app->category_selection);
    } else {
        submenu_set_header(
            app->menu, app->all_cheats ? "All cheats" : gta_cheat_catalog[app->category].category);
        bool any = false;
        for(size_t i = 0; gta_cheat_catalog[i].name; i++) {
            const bool known = gta_sequence(app, &gta_cheat_catalog[i])[0] != '\0';
            if(!known || (!app->all_cheats && strcmp(
                                                  gta_cheat_catalog[i].category,
                                                  gta_cheat_catalog[app->category].category) != 0))
                continue;
            gta_item(app, gta_cheat_catalog[i].name, ItemRow + i);
            any = true;
        }
        if(!any) gta_item(app, "No verified codes", ItemEmpty);
        if(app->list_selection != ItemEmpty)
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
        furi_string_cat_printf(app->content, "D-pad sequence:\n");
        gta_append_sequence(app->content, sequence);
        furi_string_cat_printf(app->content, "\nU=Up  D=Down\nL=Left  R=Right\n\n");
    } else {
        furi_string_cat_printf(app->content, "No verified code for this platform.\n\n");
    }
    furi_string_cat_printf(
        app->content,
        "Unlock requirement:\n%s\n\nEffect:\n%s\n\nSource / status:\n%s",
        cheat->unlock,
        cheat->effect,
        cheat->status);
    if(sequence[0]) {
        furi_string_cat_printf(
            app->content,
            "\n\nSave first. A cheat disables saving and trophies / achievements for the rest of the session.");
    }
    gta_show_text(app, ScreenDetail);
}

static void gta_guide(GtaApp* app) {
    app->guide_back = app->screen;
    text_box_reset(app->text);
    furi_string_reset(app->content);
    furi_string_cat_printf(
        app->content,
        "Quick guide\n\nChoose your console, then All cheats or a category. Only entries with published button sequences for your console appear.\n\n"
        "Check Progress > Cheats after finishing the story to see unlock requirements. In free roam, press D-pad Up to open your phone. With the phone open, hold L1 then press R1 (PS5), or hold LB then press RB (Xbox). Enter the directions on the D-pad.\n\n"
        "Save before trying cheats. A cheat disables saving and trophies / achievements for the rest of the session. Story Mode only.\n\n"
        "Cheats can only be used after beating the story and completing their unlock requirements.");
    gta_show_text(app, ScreenGuide);
}

static bool gta_event(void* context, uint32_t event) {
    GtaApp* app = context;
    if(app->screen == ScreenPlatform) {
        if(event == ItemGuide) {
            gta_guide(app);
        } else if(event == ItemPs5 || event == ItemXbox) {
            app->xbox = event == ItemXbox;
            app->category_selection = ItemAll;
            gta_show_menu(app, ScreenCategories);
        }
    } else if(app->screen == ScreenCategories) {
        app->category_selection = event;
        if(event == ItemGuide) {
            gta_guide(app);
        } else if(event == ItemAll || (event >= ItemRow && event - ItemRow < gta_cheat_count)) {
            app->all_cheats = event == ItemAll;
            if(!app->all_cheats) app->category = event - ItemRow;
            app->list_selection = ItemEmpty;
            gta_show_menu(app, ScreenList);
        }
    } else if(app->screen == ScreenList && event >= ItemRow && event - ItemRow < gta_cheat_count) {
        app->list_selection = event;
        gta_detail(app, &gta_cheat_catalog[event - ItemRow]);
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
