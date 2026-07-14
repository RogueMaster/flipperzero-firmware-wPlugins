// Enhanced Pokemon Main Application with pokeyellow integration
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#include "pokemon_integration.h"
#include "pokemon_enhanced.h"

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    bool running;
    uint8_t menu_selection;
} PokemonApp;

typedef enum {
    MENU_WILD_BATTLE,
    MENU_TRAINER_BATTLE,
    MENU_VIEW_TEAM,
    MENU_HEAL_TEAM,
    MENU_POKEDEX,
    MENU_EXIT,
    MENU_COUNT
} MainMenuOption;

static const char* menu_options[] =
    {"Wild Battle", "Trainer Battle", "View Team", "Heal Team", "Pokedex", "Exit"};

static void pokemon_app_draw_callback(Canvas* canvas, void* context) {
    PokemonApp* app = context;
    PokemonGameState* game_state = pokemon_integration_get_game_state();

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    // Title
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Pokemon Yellow+");

    // Menu options
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < MENU_COUNT; i++) {
        bool selected = (i == app->menu_selection);

        if(selected) {
            canvas_draw_box(canvas, 5, 18 + i * 10, 118, 10);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, 8, 26 + i * 10, menu_options[i]);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    // Show active Pokemon info
    EnhancedPokemon* active = pokemon_integration_get_active_pokemon();
    if(active) {
        char active_str[32];
        snprintf(active_str, sizeof(active_str), "Active: %s Lv.%d", active->name, active->level);
        canvas_draw_str_aligned(canvas, 64, 90, AlignCenter, AlignTop, active_str);

        char hp_str[32];
        snprintf(hp_str, sizeof(hp_str), "HP: %d/%d", active->current_hp, active->max_hp);
        canvas_draw_str_aligned(canvas, 64, 100, AlignCenter, AlignTop, hp_str);
    }

    // Show team count
    char team_str[16];
    snprintf(team_str, sizeof(team_str), "Team: %d/6", game_state->player_team_count);
    canvas_draw_str_aligned(canvas, 5, 110, AlignLeft, AlignTop, team_str);
}

static void pokemon_app_input_callback(InputEvent* event, void* context) {
    PokemonApp* app = context;
    furi_message_queue_put(app->event_queue, event, FuriWaitForever);
}

static void pokemon_app_show_team(PokemonApp* app) {
    UNUSED(app);
    PokemonGameState* game_state = pokemon_integration_get_game_state();

    // Simple team display - in a real app you'd create a proper screen
    for(int i = 0; i < game_state->player_team_count; i++) {
        EnhancedPokemon* pokemon = game_state->player_team[i];
        if(pokemon) {
            FURI_LOG_I(
                "Pokemon",
                "Team slot %d: %s Lv.%d HP:%d/%d",
                i + 1,
                pokemon->name,
                pokemon->level,
                pokemon->current_hp,
                pokemon->max_hp);
        }
    }
}

static void pokemon_app_show_pokedex(PokemonApp* app) {
    UNUSED(app);

    // Simple Pokedex display
    const PokemonSpecies* species_list = pokemon_get_yellow_species_list();
    int species_count = pokemon_get_yellow_species_count();

    FURI_LOG_I("Pokedex", "Available Pokemon: %d", species_count);

    // Show first 10 Pokemon as example
    for(int i = 0; i < 10 && i < species_count; i++) {
        FURI_LOG_I(
            "Pokedex",
            "#%d: %s (HP:%d ATK:%d DEF:%d SPD:%d)",
            i + 1,
            species_list[i].name,
            species_list[i].base_hp,
            species_list[i].base_attack,
            species_list[i].base_defense,
            species_list[i].base_speed);
    }
}

static bool pokemon_app_handle_input(PokemonApp* app, InputEvent* event) {
    if(event->type != InputTypePress) return true;

    switch(event->key) {
    case InputKeyUp:
        if(app->menu_selection > 0) {
            app->menu_selection--;
        }
        break;

    case InputKeyDown:
        if(app->menu_selection < MENU_COUNT - 1) {
            app->menu_selection++;
        }
        break;

    case InputKeyOk:
        switch(app->menu_selection) {
        case MENU_WILD_BATTLE:
            pokemon_integration_start_wild_battle();
            break;

        case MENU_TRAINER_BATTLE:
            pokemon_integration_start_trainer_battle(rand() % 3);
            break;

        case MENU_VIEW_TEAM:
            pokemon_app_show_team(app);
            break;

        case MENU_HEAL_TEAM:
            pokemon_integration_heal_team();
            break;

        case MENU_POKEDEX:
            pokemon_app_show_pokedex(app);
            break;

        case MENU_EXIT:
            return false;
        }
        break;

    case InputKeyBack:
        return false;

    default:
        break;
    }

    return true;
}

static PokemonApp* pokemon_app_alloc() {
    PokemonApp* app = malloc(sizeof(PokemonApp));

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->running = true;
    app->menu_selection = 0;

    view_port_draw_callback_set(app->view_port, pokemon_app_draw_callback, app);
    view_port_input_callback_set(app->view_port, pokemon_app_input_callback, app);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static void pokemon_app_free(PokemonApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    free(app);
}

int32_t pocket_battle_main(void* p) {
    UNUSED(p);

    // Initialize Pokemon integration
    pokemon_integration_init();

    PokemonApp* app = pokemon_app_alloc();

    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(!pokemon_app_handle_input(app, &event)) {
                app->running = false;
            }
        }
        view_port_update(app->view_port);
    }

    pokemon_app_free(app);
    pokemon_integration_cleanup();

    return 0;
}
