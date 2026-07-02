#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_input.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include <stratahero_icons.h>
#include "constants.h"
#include "settings.h"
#include "catalog.h"
#include "glyphs.h"
#include "game.h"
#include "quiz.h"
#include "splashscreen.h"
#include "highscores.h"

#define GAME_SCORES_PATH APP_DATA_PATH("game_scores.txt")
#define QUIZ_SCORES_PATH APP_DATA_PATH("quiz_scores.txt")

typedef enum {
    StrataHero_View_SplashScreen,
    StrataHero_View_MainMenu,
    StrataHero_View_PlayMenu,
    StrataHero_View_QuizMenu,
    StrataHero_View_Game,
    StrataHero_View_CatalogStratagemTypes,
    StrataHero_View_CatalogStratagems,
    StrataHero_View_CatalogStratagem,
    StrataHero_View_CatalogStratagemTrain,
    StrataHero_View_QuizSettings,
    StrataHero_View_Quiz,
    StrataHero_View_Settings,
    StrataHero_View_SaveSettingsConfirmation,
    StrataHero_View_GameHighscores,
    StrataHero_View_QuizHighscores,
    StrataHero_View_GameAbout,
    StrataHero_View_QuizAbout,
    StrataHero_View_NameEntry,
    StrataHero_ViewCount,
} StrataHeroView;

typedef enum {
    StrataHero_MainMenuEvent_Play,
    StrataHero_MainMenuEvent_Quiz,
    StrataHero_MainMenuEvent_Catalog,
    StrataHero_MainMenuEvent_Settings,
} StrataHeroMainMenuEvent;

typedef enum {
    PlayMenu_Start,
    PlayMenu_Highscores,
    PlayMenu_About,
} PlayMenuEvent;

typedef enum {
    QuizMenu_Start,
    QuizMenu_Highscores,
    QuizMenu_About,
} QuizMenuEvent;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    StrataHeroSettings settings;
    StrataHeroView current_view;

    StrataHeroSplashScreen* splash_screen;
    Submenu* main_menu;
    Submenu* play_menu;
    Submenu* quiz_menu;

    StratagemTypesWidget* stratagem_types_widget;
    StratagemListWidget* stratagems_list_widget;
    StratagemDetailWidget* stratagem_detail_widget;
    StratagemTrainWidget* stratagem_train_widget;

    StrataHeroGameWidget* game_widget;
    QuizWidget* quiz_widget;
    StrataHeroSettingsWidget* settings_widget;

    Widget* game_highscores_widget;
    Widget* quiz_highscores_widget;
    Widget* game_about_widget;
    Widget* quiz_about_widget;

    TextInput* name_entry;
    char name_entry_buffer[HIGHSCORE_NAME_LEN + 1];
    int pending_score;
    bool pending_score_is_quiz;

    HighScoreList game_scores;
    HighScoreList quiz_scores;

    // Pre-formatted strings — Widget stores pointers, not copies
    char game_hs_name_lines[HIGHSCORES_COUNT][32];
    char game_hs_score_lines[HIGHSCORES_COUNT][12];
    char quiz_hs_name_lines[HIGHSCORES_COUNT][32];
    char quiz_hs_score_lines[HIGHSCORES_COUNT][12];
} StrataHeroApp;

static void stratahero_switch_view(StrataHeroApp* app, StrataHeroView view) {
    app->current_view = view;
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
}

static void stratahero_populate_highscores_widget(StrataHeroApp* app, bool is_quiz) {
    HighScoreList* scores = is_quiz ? &app->quiz_scores : &app->game_scores;
    Widget* widget = is_quiz ? app->quiz_highscores_widget : app->game_highscores_widget;
    char(*name_lines)[32] = is_quiz ? app->quiz_hs_name_lines : app->game_hs_name_lines;
    char(*score_lines)[12] = is_quiz ? app->quiz_hs_score_lines : app->game_hs_score_lines;

    widget_reset(widget);
    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Highscores");

    if(scores->count == 0) {
        widget_add_string_element(
            widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, "No scores yet");
    } else {
        for(int i = 0; i < scores->count; i++) {
            snprintf(name_lines[i], 32, "%d. %.16s", i + 1, scores->entries[i].name);
            snprintf(score_lines[i], 12, "%d", scores->entries[i].score);
            widget_add_string_element(
                widget, 4, 14 + i * 10, AlignLeft, AlignTop, FontSecondary, name_lines[i]);
            widget_add_string_element(
                widget, 124, 14 + i * 10, AlignRight, AlignTop, FontSecondary, score_lines[i]);
        }
    }
}

static void stratahero_show_highscores(StrataHeroApp* app, bool is_quiz) {
    const char* path = is_quiz ? QUIZ_SCORES_PATH : GAME_SCORES_PATH;
    HighScoreList* scores = is_quiz ? &app->quiz_scores : &app->game_scores;
    StrataHeroView view = is_quiz ? StrataHero_View_QuizHighscores :
                                    StrataHero_View_GameHighscores;

    highscores_load(scores, path);
    stratahero_populate_highscores_widget(app, is_quiz);
    stratahero_switch_view(app, view);
}

static void name_entry_callback(void* context);

static void stratahero_start_name_entry(StrataHeroApp* app, int score, bool is_quiz) {
    app->pending_score = score;
    app->pending_score_is_quiz = is_quiz;
    memset(app->name_entry_buffer, 0, sizeof(app->name_entry_buffer));
    text_input_set_result_callback(
        app->name_entry,
        name_entry_callback,
        app,
        app->name_entry_buffer,
        sizeof(app->name_entry_buffer),
        true);
    stratahero_switch_view(app, StrataHero_View_NameEntry);
}

static void name_entry_callback(void* context) {
    StrataHeroApp* app = context;
    bool is_quiz = app->pending_score_is_quiz;
    const char* path = is_quiz ? QUIZ_SCORES_PATH : GAME_SCORES_PATH;
    HighScoreList* scores = is_quiz ? &app->quiz_scores : &app->game_scores;

    highscores_insert(scores, app->name_entry_buffer, app->pending_score);
    highscores_save(scores, path);

    stratahero_populate_highscores_widget(app, is_quiz);
    stratahero_switch_view(
        app, is_quiz ? StrataHero_View_QuizHighscores : StrataHero_View_GameHighscores);
}

static void stratahero_handle_game_end(StrataHeroApp* app) {
    int score = stratahero_game_widget_get_score(app->game_widget);
    highscores_load(&app->game_scores, GAME_SCORES_PATH);
    if(highscores_qualifies(&app->game_scores, score)) {
        stratahero_start_name_entry(app, score, false);
    } else {
        stratahero_switch_view(app, StrataHero_View_PlayMenu);
    }
}

static void stratahero_handle_quiz_end(StrataHeroApp* app) {
    int score = quiz_widget_get_score(app->quiz_widget);
    highscores_load(&app->quiz_scores, QUIZ_SCORES_PATH);
    if(highscores_qualifies(&app->quiz_scores, score)) {
        stratahero_start_name_entry(app, score, true);
    } else {
        stratahero_switch_view(app, StrataHero_View_QuizMenu);
    }
}

static bool stratahero_view_dispatcher_navigation_callback(void* context) {
    furi_assert(context);
    StrataHeroApp* app = context;

    switch(app->current_view) {
    case StrataHero_View_MainMenu:
    case StrataHero_View_SplashScreen:
        view_dispatcher_stop(app->view_dispatcher);
        break;

    case StrataHero_View_PlayMenu:
    case StrataHero_View_QuizMenu:
        stratahero_switch_view(app, StrataHero_View_MainMenu);
        break;

    case StrataHero_View_Game:
        stratahero_handle_game_end(app);
        break;

    case StrataHero_View_CatalogStratagemTypes:
        stratahero_switch_view(app, StrataHero_View_MainMenu);
        break;

    case StrataHero_View_Settings:
        if(stratahero_settings_widget_has_pending_changes(app->settings_widget)) {
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

    case StrataHero_View_CatalogStratagemTrain:
        stratahero_switch_view(app, StrataHero_View_CatalogStratagem);
        break;

    case StrataHero_View_QuizSettings:
        stratahero_switch_view(app, StrataHero_View_QuizMenu);
        break;

    case StrataHero_View_Quiz:
        stratahero_handle_quiz_end(app);
        break;

    case StrataHero_View_GameHighscores:
        stratahero_switch_view(app, StrataHero_View_PlayMenu);
        break;

    case StrataHero_View_QuizHighscores:
        stratahero_switch_view(app, StrataHero_View_QuizMenu);
        break;

    case StrataHero_View_GameAbout:
        stratahero_switch_view(app, StrataHero_View_PlayMenu);
        break;

    case StrataHero_View_QuizAbout:
        stratahero_switch_view(app, StrataHero_View_QuizMenu);
        break;

    case StrataHero_View_NameEntry:
        if(app->pending_score_is_quiz) {
            stratahero_switch_view(app, StrataHero_View_QuizMenu);
        } else {
            stratahero_switch_view(app, StrataHero_View_PlayMenu);
        }
        break;

    default:
        return false;
    }

    return true;
}

static void splash_screen_advance_callback(void* context) {
    stratahero_switch_view(context, StrataHero_View_MainMenu);
}

static void
    game_widget_navigation_callback(StrataHeroGameWidgetNavigationEvent event, void* context) {
    StrataHeroApp* app = context;
    switch(event) {
    case StrataHeroGameWidgetNavigationEvent_Back:
        stratahero_handle_game_end(app);
        break;
    }
}

static void settings_widget_navigation_callback(void* context) {
    stratahero_switch_view(context, StrataHero_View_MainMenu);
}

static void settings_widget_changed_callback(StrataHeroSettings* settings, void* context) {
    StrataHeroApp* app = context;
    app->settings = *settings;
    stratahero_game_widget_set_settings(app->game_widget, settings);
    stratagem_train_widget_set_settings(app->stratagem_train_widget, settings);
    quiz_widget_set_settings(app->quiz_widget, settings);
}

static void main_menu_event_callback(void* context, uint32_t index) {
    furi_assert(context);
    StrataHeroApp* app = context;
    switch((StrataHeroMainMenuEvent)index) {
    case StrataHero_MainMenuEvent_Play:
        stratahero_switch_view(app, StrataHero_View_PlayMenu);
        break;
    case StrataHero_MainMenuEvent_Quiz:
        stratahero_switch_view(app, StrataHero_View_QuizMenu);
        break;
    case StrataHero_MainMenuEvent_Catalog:
        stratahero_switch_view(app, StrataHero_View_CatalogStratagemTypes);
        break;
    case StrataHero_MainMenuEvent_Settings:
        stratahero_switch_view(app, StrataHero_View_Settings);
        break;
    }
}

static void play_menu_event_callback(void* context, uint32_t index) {
    furi_assert(context);
    StrataHeroApp* app = context;
    switch((PlayMenuEvent)index) {
    case PlayMenu_Start:
        stratahero_switch_view(app, StrataHero_View_Game);
        break;
    case PlayMenu_Highscores:
        stratahero_show_highscores(app, false);
        break;
    case PlayMenu_About:
        stratahero_switch_view(app, StrataHero_View_GameAbout);
        break;
    }
}

static void quiz_menu_event_callback(void* context, uint32_t index) {
    furi_assert(context);
    StrataHeroApp* app = context;
    switch((QuizMenuEvent)index) {
    case QuizMenu_Start:
        stratahero_switch_view(app, StrataHero_View_QuizSettings);
        break;
    case QuizMenu_Highscores:
        stratahero_show_highscores(app, true);
        break;
    case QuizMenu_About:
        stratahero_switch_view(app, StrataHero_View_QuizAbout);
        break;
    }
}

static void quiz_start_callback(void* context) {
    stratahero_switch_view(context, StrataHero_View_Quiz);
}

static void quiz_navigation_callback(void* context) {
    stratahero_handle_quiz_end(context);
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

static void stratagem_train_callback(const Stratagem* stratagem, void* context) {
    StrataHeroApp* app = context;
    stratagem_train_widget_set_stratagem(app->stratagem_train_widget, stratagem);
    stratahero_switch_view(app, StrataHero_View_CatalogStratagemTrain);
}

StrataHeroApp* stratahero_app_alloc() {
    StrataHeroApp* app = malloc(sizeof(StrataHeroApp));
    stratahero_load_settings(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);

    app->main_menu = submenu_alloc();
    submenu_add_item(
        app->main_menu, "Play", StrataHero_MainMenuEvent_Play, main_menu_event_callback, app);
    submenu_add_item(
        app->main_menu, "Quiz", StrataHero_MainMenuEvent_Quiz, main_menu_event_callback, app);
    submenu_add_item(
        app->main_menu,
        "Stratagems",
        StrataHero_MainMenuEvent_Catalog,
        main_menu_event_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Settings",
        StrataHero_MainMenuEvent_Settings,
        main_menu_event_callback,
        app);

    app->play_menu = submenu_alloc();
    submenu_set_header(app->play_menu, "Play");
    submenu_add_item(app->play_menu, "Start", PlayMenu_Start, play_menu_event_callback, app);
    submenu_add_item(
        app->play_menu, "Highscores", PlayMenu_Highscores, play_menu_event_callback, app);
    submenu_add_item(app->play_menu, "About", PlayMenu_About, play_menu_event_callback, app);

    app->quiz_menu = submenu_alloc();
    submenu_set_header(app->quiz_menu, "Quiz");
    submenu_add_item(app->quiz_menu, "Start", QuizMenu_Start, quiz_menu_event_callback, app);
    submenu_add_item(
        app->quiz_menu, "Highscores", QuizMenu_Highscores, quiz_menu_event_callback, app);
    submenu_add_item(app->quiz_menu, "About", QuizMenu_About, quiz_menu_event_callback, app);

    app->stratagem_types_widget = stratagem_types_widget_alloc();
    stratagem_types_widget_set_selected_callback(
        app->stratagem_types_widget, stratagem_type_selected_callback, app);

    app->stratagems_list_widget = stratagem_list_widget_alloc();
    stratagem_list_widget_set_selected_callback(
        app->stratagems_list_widget, stratagem_selected_callback, app);

    app->stratagem_detail_widget = stratagem_detail_widget_alloc();
    stratagem_detail_widget_set_train_callback(
        app->stratagem_detail_widget, stratagem_train_callback, app);

    app->stratagem_train_widget = stratagem_train_widget_alloc();
    stratagem_train_widget_set_settings(app->stratagem_train_widget, &app->settings);

    app->quiz_widget = quiz_widget_alloc();
    quiz_widget_set_settings(app->quiz_widget, &app->settings);
    quiz_widget_set_start_callback(app->quiz_widget, quiz_start_callback, app);
    quiz_widget_set_navigation_callback(app->quiz_widget, quiz_navigation_callback, app);

    app->game_widget = stratahero_game_widget_alloc();
    stratahero_game_widget_set_settings(app->game_widget, &app->settings);
    stratahero_game_widget_set_navigation_callback(
        app->game_widget, game_widget_navigation_callback, app);

    app->settings_widget = stratahero_settings_widget_alloc();
    stratahero_settings_widget_set_settings(app->settings_widget, &app->settings);
    stratahero_settings_widget_set_navigation_callback(
        app->settings_widget, settings_widget_navigation_callback, app);
    stratahero_settings_widget_set_settings_changed_callback(
        app->settings_widget, settings_widget_changed_callback, app);

    app->splash_screen = stratahero_splash_screen_alloc();
    stratahero_splash_screen_set_advance_callback(
        app->splash_screen, splash_screen_advance_callback, app);

    app->game_highscores_widget = widget_alloc();
    app->quiz_highscores_widget = widget_alloc();

    app->game_about_widget = widget_alloc();
    widget_add_string_element(
        app->game_about_widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Play Mode");
    widget_add_string_multiline_element(
        app->game_about_widget,
        0,
        16,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "Enter codes for each Stratagem\n"
        "before the timer runs out.\n"
        "More rounds = more Stratagems.\n"
        "Earn time & perfect bonuses!");

    app->quiz_about_widget = widget_alloc();
    widget_add_string_element(
        app->quiz_about_widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Quiz Mode");
    widget_add_string_multiline_element(
        app->quiz_about_widget,
        0,
        16,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "A Stratagem code is shown -\n"
        "pick the correct name from\n"
        "4 options! Race the clock\n"
        "and avoid mistakes.");

    app->name_entry = text_input_alloc();
    text_input_set_header_text(app->name_entry, "New High Score!");
    text_input_set_minimum_length(app->name_entry, 1);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_SplashScreen,
        stratahero_splash_screen_get_view(app->splash_screen));
    view_dispatcher_add_view(
        app->view_dispatcher, StrataHero_View_MainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, StrataHero_View_PlayMenu, submenu_get_view(app->play_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, StrataHero_View_QuizMenu, submenu_get_view(app->quiz_menu));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_Game,
        stratahero_game_widget_get_view(app->game_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_CatalogStratagemTypes,
        stratagem_types_widget_get_view(app->stratagem_types_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_CatalogStratagems,
        stratagem_list_widget_get_view(app->stratagems_list_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_CatalogStratagem,
        stratagem_detail_widget_get_view(app->stratagem_detail_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_CatalogStratagemTrain,
        stratagem_train_widget_get_view(app->stratagem_train_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_QuizSettings,
        quiz_widget_get_settings_view(app->quiz_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, StrataHero_View_Quiz, quiz_widget_get_view(app->quiz_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_Settings,
        stratahero_settings_widget_get_view(app->settings_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_SaveSettingsConfirmation,
        stratahero_settings_widget_get_confirmation_view(app->settings_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_GameHighscores,
        widget_get_view(app->game_highscores_widget));
    view_dispatcher_add_view(
        app->view_dispatcher,
        StrataHero_View_QuizHighscores,
        widget_get_view(app->quiz_highscores_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, StrataHero_View_GameAbout, widget_get_view(app->game_about_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, StrataHero_View_QuizAbout, widget_get_view(app->quiz_about_widget));
    view_dispatcher_add_view(
        app->view_dispatcher, StrataHero_View_NameEntry, text_input_get_view(app->name_entry));

    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, stratahero_view_dispatcher_navigation_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    stratahero_switch_view(app, StrataHero_View_SplashScreen);

    return app;
}

void stratahero_app_run(StrataHeroApp* app) {
    view_dispatcher_run(app->view_dispatcher);
}

void stratahero_app_free(StrataHeroApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_SplashScreen);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_MainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_PlayMenu);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_QuizMenu);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_Game);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagemTypes);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagems);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagem);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagemTrain);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_QuizSettings);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_Quiz);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_Settings);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_SaveSettingsConfirmation);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_GameHighscores);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_QuizHighscores);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_GameAbout);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_QuizAbout);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_NameEntry);
    view_dispatcher_free(app->view_dispatcher);

    stratahero_game_widget_free(app->game_widget);
    quiz_widget_free(app->quiz_widget);
    stratahero_settings_widget_free(app->settings_widget);
    stratahero_splash_screen_free(app->splash_screen);
    stratagem_types_widget_free(app->stratagem_types_widget);
    stratagem_train_widget_free(app->stratagem_train_widget);
    stratagem_detail_widget_free(app->stratagem_detail_widget);
    stratagem_list_widget_free(app->stratagems_list_widget);

    submenu_free(app->main_menu);
    submenu_free(app->play_menu);
    submenu_free(app->quiz_menu);

    widget_free(app->game_highscores_widget);
    widget_free(app->quiz_highscores_widget);
    widget_free(app->game_about_widget);
    widget_free(app->quiz_about_widget);

    text_input_free(app->name_entry);

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
