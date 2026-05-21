#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <input/input.h>
#include <notification/notification.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include <stratahero_icons.h>
#include "constants.h"
#include "stratagems.h"
#include "settings.h"
#include "catalog.h"
#include "glyphs.h"

typedef enum {
    StrataHero_View_Intro,
    StrataHero_View_MainMenu,
    StrataHero_View_RoundIntro,
    StrataHero_View_RoundPlay,
    StrataHero_View_RoundStats,
    StrataHero_View_GameOver,
    StrataHero_View_CatalogStratagemTypes,
    StrataHero_View_CatalogStratagems,
    StrataHero_View_Settings,
    StrataHero_View_SaveSettingsConfirmation,
    StrataHero_ViewCount,
} StrataHeroView;

typedef enum {
    StrataHero_MainMenuEvent_Play,
    StrataHero_MainMenuEvent_Catalog,
    StrataHero_MainMenuEvent_Settings,
} StrataHeroMainMenuEvent;

typedef enum {
    StrataHero_SettingsMenuEvent_Setting1,
    StrataHero_SettingsMenuEvent_Setting2,
} StrataHeroSettingsMenuEvent;

static NotificationSequence success_beep = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 400, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 100 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    NULL
};

static NotificationSequence success_vibro = {
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 100 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence success_beep_and_vibro = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 400, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 100 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};


static NotificationSequence failure_beep = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 200, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 500 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    NULL
};

static NotificationSequence failure_vibro = {
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 500 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence failure_beep_and_vibro = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 200, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 500 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    NotificationApp* notification;

    StrataHeroSettings settings;

    StrataHeroView current_view;
    int current_round;
    const Stratagem* round_stratagems[MAX_STRATAGEMS_PER_ROUND + 1];
    int round_stratagems_count;
    int current_round_stratagem;
    int current_code_length;
    int current_code_progress;
    uint32_t round_start_time;
    bool input_blocked;
    bool perfect_round;
    int score;

    Widget* intro_widget;
    Submenu* main_menu;
    Submenu* stratagem_types_menu;
    StratagemListWidget* stratagems_list;
    Widget* round_intro_widget;
    Widget* round_stats_widget;
    Widget* round_play_widget;
    Widget* game_over_widget;
    StrataHeroSettingsWidget* settings_widget;

    FuriTimer* round_intro_timer;
    FuriTimer* round_tick_timer;
    FuriTimer* round_stats_timer;
    FuriTimer* invalid_code_timer;
} StrataHeroApp;


static void stratahero_update_round_play(StrataHeroApp *app);
static void stratahero_update_timer_bar(StrataHeroApp *app);

static void stratahero_switch_view(StrataHeroApp* app, StrataHeroView view) {
    view_dispatcher_switch_to_view(app->view_dispatcher, view);
    app->current_view = view;
}

static void stratahero_next_round(StrataHeroApp *app) {
    app->current_round++;

    // int count = MIN_STRATAGEMS_PER_ROUND;
    // if (MIN_STRATAGEMS_PER_ROUND < MAX_STRATAGEMS_PER_ROUND) {
    //     count += rand() % (MAX_STRATAGEMS_PER_ROUND - MIN_STRATAGEMS_PER_ROUND);
    // }
    int count = 3;

    for (int i=0; i < count; i++) {
        for (int retry=0; retry < MAX_STRATEGEM_SHUFFLE_RETRIES; retry++) {
            app->round_stratagems[i] = stratagems[rand() % stratagems_count];

            // Ensure that stratagems do not repeat
            bool success = true;
            for (int k=0; k < i; k++) {
                if (app->round_stratagems[k] == app->round_stratagems[i]) {
                    success = false;
                    break;
                }
            }

            if (success) {
                break;
            }
        }
    }
    app->round_stratagems_count = count;
    app->current_round_stratagem = 0;
    app->perfect_round = true;

    stratahero_switch_view(app, StrataHero_View_RoundIntro);
}

static void stratahero_next_code(StrataHeroApp *app) {
    app->current_round_stratagem++;
    if (app->current_round_stratagem >= app->round_stratagems_count) {
        stratahero_switch_view(app, StrataHero_View_RoundStats);
        return;
    }
    app->current_code_progress = 0;
    app->current_code_length = strlen(app->round_stratagems[app->current_round_stratagem]->code);
    app->input_blocked = false;
    stratahero_update_round_play(app);
}

static void stratahero_round_intro_timer_callback(void* context) {
    furi_assert(context);

    StrataHeroApp* app = context;

    app->current_code_length = strlen(app->round_stratagems[0]->code);
    app->current_code_progress = 0;
    stratahero_switch_view(app, StrataHero_View_RoundPlay);
    stratahero_update_round_play(app);
}

static void stratahero_round_stats_timer_callback(void* context) {
    furi_assert(context);

    StrataHeroApp* app = context;
    stratahero_next_round(app);
}

static void stratahero_round_tick_timer_callback(void* context) {
    furi_assert(context);

    StrataHeroApp* app = context;
    uint32_t current_time = furi_get_tick();
    if (app->round_start_time + 1000 * ROUND_TIME_SECONDS <= current_time) {
        // TODO: game over
        stratahero_switch_view(app, StrataHero_View_GameOver);
    }
    stratahero_update_timer_bar(app);
}

static void stratahero_invalid_code_timer_callback(void* context) {
    furi_assert(context);

    StrataHeroApp* app = context;
    app->input_blocked = false;
    app->current_code_progress = 0;
    stratahero_update_round_play(app);
}

static bool stratahero_view_dispatcher_navigation_callback(void* context) {
    furi_assert(context);

    StrataHeroApp* app = context;
    switch (app->current_view) {
        case StrataHero_View_MainMenu:
        case StrataHero_View_Intro:
            view_dispatcher_stop(app->view_dispatcher);
            break;

        case StrataHero_View_RoundIntro:
            // furi_timer_stop(app->round_intro_timer);
            stratahero_switch_view(app, StrataHero_View_MainMenu);
            break;

        case StrataHero_View_RoundStats:
            // furi_timer_stop(app->round_stats_timer);
            stratahero_switch_view(app, StrataHero_View_MainMenu);
            break;

        case StrataHero_View_RoundPlay:
        case StrataHero_View_CatalogStratagemTypes:
            stratahero_switch_view(app, StrataHero_View_MainMenu);
            break;

        case StrataHero_View_Settings:
            stratahero_switch_view(app, StrataHero_View_MainMenu);
            break;

        case StrataHero_View_CatalogStratagems:
            stratahero_switch_view(app, StrataHero_View_CatalogStratagemTypes);
            break;

        default:
            return false;
    }

    return true;
}

static void main_menu_event_callback(void* context, uint32_t index) {
    furi_assert(context);

    StrataHeroApp* app = context;
    switch ((StrataHeroMainMenuEvent)index) {
        case StrataHero_MainMenuEvent_Play:
            app->current_round = 0;
            stratahero_next_round(app);
            break;
        case StrataHero_MainMenuEvent_Catalog:
            stratahero_switch_view(app, StrataHero_View_CatalogStratagemTypes);
            break;
        case StrataHero_MainMenuEvent_Settings:
            stratahero_switch_view(app, StrataHero_View_Settings);
            break;
    }
}

static void stratagem_types_menu_event_callback(void* context, uint32_t index) {
    furi_assert(context);

    StrataHeroApp* app = context;
    stratagem_list_widget_set_stratagem_type(app->stratagems_list, (StratagemType)index);
    stratahero_switch_view(app, StrataHero_View_CatalogStratagems);
}

static void stratahero_update_round_play(StrataHeroApp *app) {
    widget_reset(app->round_play_widget);

    uint32_t timer_bar_width = (furi_get_tick() - app->round_start_time) * SCREEN_WIDTH / 1000 / ROUND_TIME_SECONDS;
    widget_add_line_element(app->round_play_widget, 0, SCREEN_HEIGHT-1, timer_bar_width, SCREEN_HEIGHT-1);

    // Display stratagem queue
    int offset_x = 5;
    int offset_y = 5;
    for (int i=app->current_round_stratagem; i < app->round_stratagems_count; i++) {
        const Icon* icon = app->round_stratagems[i]->icon;
        if (icon == NULL) {
            icon = &I_no_icon_stratagem;
        }

        widget_add_icon_element(app->round_play_widget, offset_x, 5, icon);
        offset_x += icon_get_width(icon) + 5;
    }

    // Display current stratagem code
    const Stratagem* stratagem = app->round_stratagems[app->current_round_stratagem];

    widget_add_string_element(app->round_play_widget, SCREEN_WIDTH / 2, 32, AlignCenter, AlignTop, FontSecondary, stratagem->title);

    bool inverse = app->input_blocked;

    int code_len = strlen(stratagem->code);
    offset_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * code_len) / 2;
    if (offset_x < 0) {
        offset_x = 4;
    }
    offset_y = (code_len > 10) ? 35 : 45;
    for (int i=0; i < code_len; i++) {
        const StrataHeroCodeGlyph* glyph = stratahero_get_code_glyph(stratagem->code[i]);
        if (!glyph) {
            continue;
        }

        const Icon* icon = inverse ? glyph->inverse : ((i < app->current_code_progress) ? glyph->black : glyph->white);
        widget_add_icon_element(app->round_play_widget, offset_x, offset_y, icon);
        offset_x += CODE_GLYPH_WIDTH;
        if (offset_x > SCREEN_WIDTH - CODE_GLYPH_WIDTH) {
            offset_y += CODE_GLYPH_HEIGHT;

            offset_x = (SCREEN_WIDTH - CODE_GLYPH_WIDTH * (code_len - i - 1)) / 2;
            if (offset_x < 0) {
                offset_x = 4;
            }
        }
    }
}

static void stratahero_update_timer_bar(StrataHeroApp *app) {
    uint32_t timer_bar_width = (1000 * ROUND_TIME_SECONDS + app->round_start_time - furi_get_tick()) * SCREEN_WIDTH / 1000 / ROUND_TIME_SECONDS;
    widget_add_line_element(app->round_play_widget, 0, SCREEN_HEIGHT-1, timer_bar_width, SCREEN_HEIGHT-1);
}

static bool stratahero_intro_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    StrataHeroApp* app = context;

    if (event->type == InputTypePress) {
        switch (event->key) {
            case InputKeyLeft:
            case InputKeyRight:
            case InputKeyUp:
            case InputKeyDown:
            case InputKeyOk:
                stratahero_switch_view(app, StrataHero_View_MainMenu);
                break;

            default:
                break;
        }
        return true;
    }

    return false;
}

static void notify_success(StrataHeroApp* app) {
    if (app->settings.sound_enabled && app->settings.vibro_enabled) {
        notification_message(app->notification, &success_beep_and_vibro);
    } else if (app->settings.sound_enabled) {
        notification_message(app->notification, &success_beep);
    } else if (app->settings.vibro_enabled) {
        notification_message(app->notification, &success_vibro);
    }
}

static void notify_failure(StrataHeroApp* app) {
    if (app->settings.sound_enabled && app->settings.vibro_enabled) {
        notification_message(app->notification, &failure_beep_and_vibro);
    } else if (app->settings.sound_enabled) {
        notification_message(app->notification, &failure_beep);
    } else if (app->settings.vibro_enabled) {
        notification_message(app->notification, &failure_vibro);
    }
}

static bool stratahero_play_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    StrataHeroApp* app = context;

    if (app->input_blocked) {
        return false;
    }

    bool handled = false;
    if (event->type == InputTypePress) {
        switch (event->key) {
            case InputKeyLeft:
                if (app->round_stratagems[app->current_round_stratagem]->code[app->current_code_progress] == 'L') {
                    notify_success(app);
                    app->current_code_progress++;
                } else {
                    notify_failure(app);
                    app->input_blocked = true;
                    app->perfect_round = false;
                    furi_timer_start(app->invalid_code_timer, INVALID_CODE_DELAY);
                }
                stratahero_update_round_play(app);
                handled = true;
                break;
            case InputKeyRight:
                if (app->round_stratagems[app->current_round_stratagem]->code[app->current_code_progress] == 'R') {
                    notify_success(app);
                    app->current_code_progress++;
                } else {
                    notify_failure(app);
                    app->input_blocked = true;
                    app->perfect_round = false;
                    furi_timer_start(app->invalid_code_timer, INVALID_CODE_DELAY);
                }
                stratahero_update_round_play(app);
                handled = true;
                break;
            case InputKeyUp:
                if (app->round_stratagems[app->current_round_stratagem]->code[app->current_code_progress] == 'U') {
                    notify_success(app);
                    app->current_code_progress++;
                } else {
                    notify_failure(app);
                    app->input_blocked = true;
                    app->perfect_round = false;
                    furi_timer_start(app->invalid_code_timer, INVALID_CODE_DELAY);
                }
                stratahero_update_round_play(app);
                handled = true;
                break;
            case InputKeyDown:
                if (app->round_stratagems[app->current_round_stratagem]->code[app->current_code_progress] == 'D') {
                    notify_success(app);
                    app->current_code_progress++;
                } else {
                    notify_failure(app);
                    app->input_blocked = true;
                    app->perfect_round = false;
                    furi_timer_start(app->invalid_code_timer, INVALID_CODE_DELAY);
                }
                stratahero_update_round_play(app);
                handled = true;
                break;
            default:
                break;
        }

        if (app->current_code_progress >= app->current_code_length) {
            app->score += app->current_code_length * 5;
            stratahero_next_code(app);
        }
    }

    return handled;
}

static void stratahero_play_enter_callback(void* context) {
    StrataHeroApp* app = context;
    app->round_start_time = furi_get_tick();
    furi_timer_start(app->round_tick_timer, 1000 * ROUND_TIME_SECONDS / SCREEN_WIDTH);
}

static void stratahero_play_exit_callback(void* context) {
    StrataHeroApp* app = context;
    furi_timer_stop(app->round_tick_timer);
}

static void stratahero_round_intro_enter_callback(void* context) {
    StrataHeroApp* app = context;

    widget_reset(app->round_intro_widget);
    char buffer[32];
    snprintf(buffer, sizeof(buffer)-1, "Round %d", app->current_round);
    widget_add_string_element(app->round_intro_widget, SCREEN_WIDTH / 2, 20, AlignCenter, AlignTop, FontPrimary, buffer);
    widget_add_string_element(app->round_intro_widget, SCREEN_WIDTH / 2, 50, AlignCenter, AlignTop, FontSecondary, "Get Ready");

    furi_timer_start(app->round_intro_timer, 3000);
}

static void stratahero_round_intro_exit_callback(void* context) {
    StrataHeroApp* app = context;
    furi_timer_stop(app->round_intro_timer);
}

static void stratahero_round_stats_enter_callback(void* context) {
    StrataHeroApp* app = context;
    widget_reset(app->round_stats_widget);

    char buffer[32];
    snprintf(buffer, sizeof(buffer)-1, "Round %d Complete", app->current_round);
    widget_add_string_element(app->round_stats_widget, SCREEN_WIDTH/2, 2, AlignCenter, AlignTop, FontPrimary, buffer);

    widget_add_string_element(app->round_stats_widget, 5, 20, AlignLeft, AlignTop, FontPrimary, "Round Bonus");
    int round_bonus = (app->current_round + 2) * 25;
    snprintf(buffer, sizeof(buffer)-1, "%d", round_bonus);
    widget_add_string_element(app->round_stats_widget, SCREEN_WIDTH - 5, 20, AlignRight, AlignTop, FontPrimary, buffer);
    app->score += round_bonus;

    widget_add_string_element(app->round_stats_widget, 5, 30, AlignLeft, AlignTop, FontPrimary, "Time Bonus");
    int time_bonus = (ROUND_TIME_SECONDS * 1000 - app->round_start_time + furi_get_tick()) / 100;
    snprintf(buffer, sizeof(buffer)-1, "%d", time_bonus);
    widget_add_string_element(app->round_stats_widget, SCREEN_WIDTH - 5, 30, AlignRight, AlignTop, FontPrimary, buffer);
    app->score += time_bonus;

    widget_add_string_element(app->round_stats_widget, 5, 40, AlignLeft, AlignTop, FontPrimary, "Perfect Bonus");
    int perfect_bonus = app->perfect_round ? 100 : 0;
    snprintf(buffer, sizeof(buffer)-1, "%d", perfect_bonus);
    widget_add_string_element(app->round_stats_widget, SCREEN_WIDTH - 5, 40, AlignRight, AlignTop, FontPrimary, buffer);
    app->score += perfect_bonus;

    widget_add_string_element(app->round_stats_widget, 5, 50, AlignLeft, AlignTop, FontPrimary, "Total Score");
    snprintf(buffer, sizeof(buffer)-1, "%d", app->score);
    widget_add_string_element(app->round_stats_widget, SCREEN_WIDTH - 5, 50, AlignRight, AlignTop, FontPrimary, buffer);

    furi_timer_start(app->round_stats_timer, 5000);
}

static void stratahero_round_stats_exit_callback(void* context) {
    StrataHeroApp* app = context;
    furi_timer_stop(app->round_stats_timer);
}

StrataHeroApp* stratahero_app_alloc() {
    StrataHeroApp* app = malloc(sizeof(StrataHeroApp));
    stratahero_load_settings(&app->settings);

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    app->main_menu = submenu_alloc();
    submenu_add_item(app->main_menu, "Play", StrataHero_MainMenuEvent_Play, main_menu_event_callback, app);
    submenu_add_item(app->main_menu, "Catalog", StrataHero_MainMenuEvent_Catalog, main_menu_event_callback, app);
    submenu_add_item(app->main_menu, "Settings", StrataHero_MainMenuEvent_Settings, main_menu_event_callback, app);

    app->stratagem_types_menu = submenu_alloc();
    for (int i=0; i < StratagemTypeCount; i++) {
        submenu_add_item(app->stratagem_types_menu, get_stratagem_type_title((StratagemType)i), i, stratagem_types_menu_event_callback, app);
    }

    app->stratagems_list = stratagem_list_widget_alloc();

    app->round_intro_widget = widget_alloc();
    app->round_intro_timer = furi_timer_alloc(stratahero_round_intro_timer_callback, FuriTimerTypeOnce, app);
    view_set_context(widget_get_view(app->round_intro_widget), app);
    view_set_enter_callback(widget_get_view(app->round_intro_widget), stratahero_round_intro_enter_callback);
    view_set_exit_callback(widget_get_view(app->round_intro_widget), stratahero_round_intro_exit_callback);

    app->round_stats_widget = widget_alloc();
    widget_add_string_element(app->round_stats_widget, SCREEN_WIDTH/2, 0, AlignCenter, AlignTop, FontPrimary, "Round Stats");
    app->round_stats_timer = furi_timer_alloc(stratahero_round_stats_timer_callback, FuriTimerTypeOnce, app);
    view_set_context(widget_get_view(app->round_stats_widget), app);
    view_set_enter_callback(widget_get_view(app->round_stats_widget), stratahero_round_stats_enter_callback);
    view_set_exit_callback(widget_get_view(app->round_stats_widget), stratahero_round_stats_exit_callback);

    app->round_play_widget = widget_alloc();
    View* play_view = widget_get_view(app->round_play_widget);
    view_set_context(play_view, app);
    view_set_input_callback(play_view, stratahero_play_input_callback);
    view_set_enter_callback(play_view, stratahero_play_enter_callback);
    view_set_exit_callback(play_view, stratahero_play_exit_callback);

    app->round_tick_timer = furi_timer_alloc(stratahero_round_tick_timer_callback, FuriTimerTypePeriodic, app);
    app->invalid_code_timer = furi_timer_alloc(stratahero_invalid_code_timer_callback, FuriTimerTypeOnce, app);

    app->game_over_widget = widget_alloc();
    widget_add_string_element(app->game_over_widget, SCREEN_WIDTH/2, 0, AlignCenter, AlignTop, FontPrimary, "Game Over");

    // Settings
    app->settings_widget = stratahero_settings_widget_alloc();

    app->intro_widget = widget_alloc();
    widget_add_icon_element(app->intro_widget, 35, 5, &I_logo);
    widget_add_string_element(app->intro_widget, 64, 30, AlignCenter, AlignTop, FontPrimary, "Helldivers");
    widget_add_string_element(app->intro_widget, 64, 40, AlignCenter, AlignTop, FontPrimary, "Stratagem Hero");

    widget_add_string_element(app->intro_widget, 64, 55, AlignCenter, AlignTop, FontSecondary, "Press any key");
    View* intro_view = widget_get_view(app->intro_widget);
    view_set_context(intro_view, app);
    view_set_input_callback(intro_view, stratahero_intro_input_callback);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_Intro, widget_get_view(app->intro_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_MainMenu, submenu_get_view(app->main_menu));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_RoundIntro, widget_get_view(app->round_intro_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_RoundPlay, widget_get_view(app->round_play_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_RoundStats, widget_get_view(app->round_stats_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_GameOver, widget_get_view(app->game_over_widget));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_CatalogStratagemTypes, submenu_get_view(app->stratagem_types_menu));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_CatalogStratagems, stratagem_list_widget_get_view(app->stratagems_list));
    view_dispatcher_add_view(app->view_dispatcher, StrataHero_View_Settings, stratahero_settings_widget_get_view(app->settings_widget));
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, stratahero_view_dispatcher_navigation_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    stratahero_switch_view(app, StrataHero_View_Intro);

    return app;
}

void stratahero_app_run(StrataHeroApp* app) {
    view_dispatcher_run(app->view_dispatcher);
}

void stratahero_app_free(StrataHeroApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_MainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_RoundPlay);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_RoundIntro);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_RoundStats);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_GameOver);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagemTypes);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_CatalogStratagems);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_Settings);
    view_dispatcher_remove_view(app->view_dispatcher, StrataHero_View_Intro);
    view_dispatcher_free(app->view_dispatcher);

    widget_free(app->round_intro_widget);
    widget_free(app->round_play_widget);
    widget_free(app->round_stats_widget);
    widget_free(app->game_over_widget);
    stratahero_settings_widget_free(app->settings_widget);
    widget_free(app->intro_widget);
    submenu_free(app->stratagem_types_menu);
    stratagem_list_widget_free(app->stratagems_list);
    submenu_free(app->main_menu);
    furi_timer_free(app->round_intro_timer);
    furi_timer_free(app->round_tick_timer);
    furi_timer_free(app->round_stats_timer);
    furi_timer_free(app->invalid_code_timer);
    furi_record_close(RECORD_NOTIFICATION);
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

