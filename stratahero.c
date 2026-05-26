#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include <stratahero_icons.h>
#include "constants.h"
#include "settings.h"
#include "catalog.h"
#include "glyphs.h"
#include "game.h"
#include "splashscreen.h"


typedef enum {
    StrataHero_View_SplashScreen,
    StrataHero_View_MainMenu,
    StrataHero_View_Game,
    StrataHero_View_CatalogStratagemTypes,
    StrataHero_View_CatalogStratagems,
    StrataHero_View_CatalogStratagem,
    StrataHero_View_Settings,
    StrataHero_View_SaveSettingsConfirmation,
    StrataHero_ViewCount,
} StrataHeroView;

typedef enum {
    StrataHero_MainMenuEvent_Play,
    StrataHero_MainMenuEvent_Catalog,
    StrataHero_MainMenuEvent_Settings,
} StrataHeroMainMenuEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    StrataHeroSettings settings;

    StrataHeroView current_view;

    StrataHeroSplashScreen* splash_screen;
    Submenu* main_menu;

    StratagemTypesWidget* stratagem_types_widget;
    StratagemListWidget* stratagems_list_widget;
    StratagemDetailWidget* stratagem_detail_widget;

    StrataHeroGameWidget* game_widget;
    StrataHeroSettingsWidget* settings_widget;

    FuriTimer* invalid_code_timer;
} StrataHeroApp;


static void stratahero_switch_view(StrataHeroApp* app, StrataHeroView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static bool stratahero_view_dispatcher_navigation_callback(void* context) {
    furi_assert(context);

    StrataHeroApp* app = context;
    switch (app->current_view) {
        case StrataHero_View_MainMenu:
        case StrataHero_View_SplashScreen:
            view_dispatcher_stop(app->view_dispatcher);
            break;

        case StrataHero_View_Game:
        case StrataHero_View_CatalogStratagemTypes:
            stratahero_switch_view(app, StrataHero_View_MainMenu);
            break;

        case StrataHero_View_Settings:
            if (stratahero_settings_widget_has_pending_changes(app->settings_widget)) {
                stratahero_switch_view(app, StrataHero_View_SaveSettingsConfirmation);
            } else {
                stratahero_switch_view(app, StrataHero_View_MainMenu);
            }
            break;

        case StrataHero_View_SaveSettingsConfirmation:
            stratahero_switch_view(app, StrataHero_View_Settings);
            break;

        case StrataHero_View_CatalogStratagems:
            stratahero_switch_view(app, StrataHero_View_CatalogStratagemTypes);
            break;

        case StrataHero_View_CatalogStratagem:
            stratahero_switch_view(app, StrataHero_View_CatalogStratagems);
            break;

        default:
            return false;
    }

    return true;
}

static void splash_screen_advance_callback(void* context) {
    stratahero_switch_view(context, StrataHero_View_MainMenu);
}

static void game_widget_navigation_callback(StrataHeroGameWidgetNavigationEvent event, void* context) {
    switch (event) {
        case StrataHeroGameWidgetNavigationEvent_Back:
            stratahero_view_dispatcher_navigation_callback(context);
            break;
    }
}

static void settings_widget_navigation_callback(void* context) {
    stratahero_switch_view(context, StrataHero_View_MainMenu);
}

static void settings_widget_changed_callback(StrataHeroSettings *settings, void* context) {
    StrataHeroApp* app = context;
    app->settings = *settings;
    stratahero_game_widget_set_settings(app->game_widget, settings);
}

static void main_menu_event_callback(void* context, uint32_t index) {
    furi_assert(context);

    StrataHeroApp* app = context;
    switch ((StrataHeroMainMenuEvent)index) {
        case StrataHero_MainMenuEvent_Play:
            stratahero_switch_view(app, StrataHero_View_Game);
            break;
        case StrataHero_MainMenuEvent_Catalog:
            stratahero_switch_view(app, StrataHero_View_CatalogStratagemTypes);
            break;
        case StrataHero_MainMenuEvent_Settings:
            stratahero_switch_view(app, StrataHero_View_Settings);
            break;
    }
}

static void stratagem_type_selected_callback(StratagemType type, void* context) {
    StrataHeroApp* app = context;
    stratagem_list_widget_set_stratagem_type(app->stratagems_list_widget, type);
    stratahero_switch_view(app, StrataHero_View_CatalogStratagems);
}

static void stratagem_selected_callback(const Stratagem* stratagem, void* context) {
    StrataHeroApp* app = context;
    stratagem_detail_widget_set_stratagem(app->stratagem_detail_widget, stratagem);
    stratahero_switch_view(app, StrataHero_View_CatalogStratagem);
}

StrataHeroApp* stratahero_app_alloc() {
    StrataHeroApp* app = malloc(sizeof(StrataHeroApp));
    stratahero_load_settings(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);

    app->main_menu = submenu_alloc();
    submenu_add_item(app->main_menu, "Play", StrataHero_MainMenuEvent_Play, main_menu_event_callback, app);
    submenu_add_item(app->main_menu, "Stratagems", StrataHero_MainMenuEvent_Catalog, main_menu_event_callback, app);
    submenu_add_item(app->main_menu, "Settings", StrataHero_MainMenuEvent_Settings, main_menu_event_callback, app);

    app->stratagem_types_widget = stratagem_types_widget_alloc();
    stratagem_types_widget_set_selected_callback(app->stratagem_types_widget, stratagem_type_selected_callback, app);

    app->stratagems_list_widget = stratagem_list_widget_alloc();
    stratagem_list_widget_set_selected_callback(app->stratagems_list_widget, stratagem_selected_callback, app);

    app->stratagem_detail_widget = stratagem_detail_widget_alloc();

    app->game_widget = stratahero_game_widget_alloc();
    stratahero_game_widget_set_settings(app->game_widget, &app->settings);
    stratahero_game_widget_set_navigation_callback(app->game_widget, game_widget_navigation_callback, app);

    // Settings
    app->settings_widget = stratahero_settings_widget_alloc();
    stratahero_settings_widget_set_settings(app->settings_widget, &app->settings);
    stratahero_settings_widget_set_navigation_callback(app->settings_widget, settings_widget_navigation_callback, app);
    stratahero_settings_widget_set_settings_changed_callback(app->settings_widget, settings_widget_changed_callback, app);

    app->splash_screen = stratahero_splash_screen_alloc();
    stratahero_splash_screen_set_advance_callback(app->splash_screen, splash_screen_advance_callback, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_SplashScreen, stratahero_splash_screen_get_view(app->splash_screen));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_MainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_Game, stratahero_game_widget_get_view(app->game_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_CatalogStratagemTypes, stratagem_types_widget_get_view(app->stratagem_types_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_CatalogStratagems, stratagem_list_widget_get_view(app->stratagems_list_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_CatalogStratagem, stratagem_detail_widget_get_view(app->stratagem_detail_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_Settings, stratahero_settings_widget_get_view(app->settings_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_SaveSettingsConfirmation, stratahero_settings_widget_get_confirmation_view(app->settings_widget));
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, stratahero_view_dispatcher_navigation_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    stratahero_switch_view(app, StrataHero_View_SplashScreen);

    return app;
}

void stratahero_app_run(StrataHeroApp* app) {
    view_dispatcher_run(app->view_dispatcher);
}

void stratahero_app_free(StrataHeroApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_MainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_Game);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagemTypes);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagem);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagems);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_SaveSettingsConfirmation);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_Settings);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_SplashScreen);
    view_dispatcher_free(app->view_dispatcher);

    stratahero_game_widget_free(app->game_widget);
    stratahero_settings_widget_free(app->settings_widget);
    stratahero_splash_screen_free(app->splash_screen);
    stratagem_types_widget_free(app->stratagem_types_widget);
    stratagem_detail_widget_free(app->stratagem_detail_widget);
    stratagem_list_widget_free(app->stratagems_list_widget);
    submenu_free(app->main_menu);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t stratahero_main(void* p) {
    UNUSED(p);

    StrataHeroApp* app = stratahero_app_alloc();
    stratahero_app_run(app);
    stratahero_app_free(app);

    return 0;
}

