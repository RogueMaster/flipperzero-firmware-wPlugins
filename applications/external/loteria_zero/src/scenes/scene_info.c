#include "scenes.h"
#include "../renderer.h"

#include <stdio.h>

static const char* k_dedication_body =
    "The inspiration behind every line of code, every idea, and every project.\n\n"
    "Because the best creations are born from what we love most.";

static void loteria_scene_our_story_button(GuiButtonType result, InputType type, void* context) {
    LoteriaApp* app = context;
    if(type != InputTypeShort) return;
    UNUSED(result);
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void loteria_scene_our_story_on_enter(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);

    widget_add_string_element(
        app->widget, 64, 1, AlignCenter, AlignTop, FontSecondary, "A special dedication");
    widget_add_string_element(app->widget, 64, 12, AlignCenter, AlignTop, FontPrimary, "For APA");
    widget_add_text_scroll_element(app->widget, 2, 24, 124, 28, k_dedication_body);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", loteria_scene_our_story_button, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewWidget);
}

bool loteria_scene_our_story_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    if(event.type == SceneManagerEventTypeCustom || event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loteria_scene_our_story_on_exit(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
}

void loteria_scene_about_on_enter(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "\e#LOTERIA ZERO\n"
        "Tradition in your hands.\n\n"
        "Digital Mexican Loteria caller for Flipper Zero.\n\n"
        "54 cards - original rhymes\n"
        "Version 1.1\n\n"
        "Controls (hold Flipper vertical):\n"
        "OK / RIGHT: next\n"
        "LEFT: history back\n"
        "RIGHT in hist: forward\n"
        "UP / DOWN: rhyme\n"
        "BACK: menu\n\n"
        "For APA");
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewWidget);
}

bool loteria_scene_about_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loteria_scene_about_on_exit(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
}

void loteria_scene_stats_on_enter(void* context) {
    LoteriaApp* app = context;
    char buf[192];
    snprintf(
        buf,
        sizeof(buf),
        "\e#Statistics\n\n"
        "Games played: %lu\n"
        "Completed: %lu\n"
        "Interrupted: %lu\n"
        "Cards called: %lu\n"
        "Last played: %lu",
        (unsigned long)app->persist.stats.games_played,
        (unsigned long)app->persist.stats.games_completed,
        (unsigned long)app->persist.stats.games_interrupted,
        (unsigned long)app->persist.stats.cards_sung,
        (unsigned long)app->persist.stats.last_played_epoch);

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, buf);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewWidget);
}

bool loteria_scene_stats_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loteria_scene_stats_on_exit(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
}

void loteria_scene_history_on_enter(void* context) {
    LoteriaApp* app = context;
    FuriString* text = furi_string_alloc_set("\e#History\n");
    uint8_t sung = loteria_game_sung_count(&app->game);

    if(sung == 0 && app->persist.has_saved_game) {
        app->game = app->persist.saved_game;
        sung = loteria_game_sung_count(&app->game);
    }

    furi_string_cat_printf(
        text, "Called: %u\nPending: %u\n\n", sung, (unsigned)(LOTERIA_CARD_COUNT - sung));

    /* Full called list (scrollable widget). */
    for(uint8_t i = 0; i < sung; i++) {
        uint8_t idx = app->game.order[i];
        if(idx >= LOTERIA_CARD_COUNT) {
            continue;
        }
        const LoteriaCard* card = &loteria_cards[idx];
        furi_string_cat_printf(text, "%02u. %s\n", i + 1, card->name);
    }

    furi_string_cat(text, "\n\e#Pending\n");
    for(uint8_t id = 1; id <= LOTERIA_CARD_COUNT; id++) {
        if(!loteria_game_is_sung(&app->game, id)) {
            furi_string_cat_printf(text, "- %s\n", loteria_cards[id - 1].name);
        }
    }

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewWidget);
}

bool loteria_scene_history_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loteria_scene_history_on_exit(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
}

static void loteria_all_cards_callback(void* context, uint32_t index) {
    LoteriaApp* app = context;
    app->all_cards_index = (uint8_t)index;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void loteria_all_cards_show_detail(LoteriaApp* app) {
    const LoteriaCard* card = &loteria_cards[app->all_cards_index];
    char title[40];
    snprintf(title, sizeof(title), "%u. %s", card->id, card->name);

    widget_reset(app->widget);
    widget_add_icon_element(app->widget, 40, 0, card->icon);
    widget_add_string_element(app->widget, 64, 50, AlignCenter, AlignTop, FontSecondary, title);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewWidget);
}

void loteria_scene_all_cards_on_enter(void* context) {
    LoteriaApp* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "54 cards");
    for(uint8_t i = 0; i < LOTERIA_CARD_COUNT; i++) {
        submenu_add_item(app->submenu, loteria_cards[i].name, i, loteria_all_cards_callback, app);
    }
    submenu_set_selected_item(app->submenu, app->all_cards_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewSubmenu);
}

bool loteria_scene_all_cards_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        loteria_all_cards_show_detail(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void loteria_scene_all_cards_on_exit(void* context) {
    LoteriaApp* app = context;
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}

static void loteria_scene_game_end_button(GuiButtonType result, InputType type, void* context) {
    LoteriaApp* app = context;
    if(type != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)result);
}

void loteria_scene_game_end_on_enter(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "GAME OVER!");
    widget_add_text_scroll_element(app->widget, 0, 20, 128, 24, "May every board find a winner!");
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Menu", loteria_scene_game_end_button, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "New", loteria_scene_game_end_button, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewWidget);
}

bool loteria_scene_game_end_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            loteria_game_start_new(&app->game);
            app->persist.stats.games_played++;
            app->persist.stats.cards_sung++;
            app->persist.saved_game = app->game;
            app->persist.has_saved_game = true;
            loteria_app_save(app);
            scene_manager_next_scene(app->scene_manager, LoteriaSceneGame);
        } else {
            scene_manager_next_scene(app->scene_manager, LoteriaSceneMainMenu);
        }
        return true;
    }
    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, LoteriaSceneMainMenu);
        return true;
    }
    return false;
}

void loteria_scene_game_end_on_exit(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
}
