// Enhanced Battle Screen with pokeyellow integration
#include "enhanced_battle_screen.h"
#include "pokemon_enhanced.h"
#include <gui/gui.h>
#include <input/input.h>
#include <furi.h>

typedef struct {
    EnhancedBattle* battle;
    uint8_t selected_move;
    bool show_moves;
    bool battle_active;
    char status_text[128];
} EnhancedBattleState;

static EnhancedBattleState* battle_state = NULL;

static void enhanced_battle_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    if(!battle_state || !battle_state->battle) return;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);

    EnhancedPokemon* player = battle_state->battle->player_pokemon;
    EnhancedPokemon* enemy = battle_state->battle->enemy_pokemon;

    // Draw enemy Pokemon (top)
    canvas_draw_str(canvas, 5, 12, enemy->name);
    char level_str[16];
    snprintf(level_str, sizeof(level_str), "Lv.%d", enemy->level);
    canvas_draw_str_aligned(canvas, 120, 12, AlignRight, AlignTop, level_str);

    // Enemy HP bar
    int enemy_hp_width = (enemy->current_hp * 50) / enemy->max_hp;
    canvas_draw_frame(canvas, 5, 15, 52, 6);
    canvas_draw_box(canvas, 6, 16, enemy_hp_width, 4);
    char hp_str[16];
    snprintf(hp_str, sizeof(hp_str), "%d/%d", enemy->current_hp, enemy->max_hp);
    canvas_draw_str_aligned(canvas, 120, 18, AlignRight, AlignTop, hp_str);

    // Draw player Pokemon (bottom)
    canvas_draw_str(canvas, 5, 45, player->name);
    char player_level_str[16];
    snprintf(player_level_str, sizeof(player_level_str), "Lv.%d", player->level);
    canvas_draw_str_aligned(canvas, 120, 45, AlignRight, AlignTop, player_level_str);

    // Player HP bar
    int player_hp_width = (player->current_hp * 50) / player->max_hp;
    canvas_draw_frame(canvas, 5, 48, 52, 6);
    canvas_draw_box(canvas, 6, 49, player_hp_width, 4);
    char player_hp_str[16];
    snprintf(player_hp_str, sizeof(player_hp_str), "%d/%d", player->current_hp, player->max_hp);
    canvas_draw_str_aligned(canvas, 120, 51, AlignRight, AlignTop, player_hp_str);

    // Show moves or battle status
    if(battle_state->show_moves && !battle_state->battle->battle_over) {
        canvas_draw_str(canvas, 5, 62, "Select Move:");
        for(int i = 0; i < 4; i++) {
            if(player->moves[i].power > 0) {
                bool selected = (i == battle_state->selected_move);
                if(selected) {
                    canvas_draw_box(canvas, 4, 64 + i * 8, 120, 8);
                    canvas_set_color(canvas, ColorWhite);
                }

                char move_str[64];
                snprintf(
                    move_str,
                    sizeof(move_str),
                    "%s PP:%d",
                    player->moves[i].name,
                    player->move_pp[i]);
                canvas_draw_str_aligned(canvas, 8, 70 + i * 8, AlignLeft, AlignCenter, move_str);

                if(selected) {
                    canvas_set_color(canvas, ColorBlack);
                }
            }
        }
    } else {
        // Show battle log or result
        if(battle_state->battle->battle_over) {
            canvas_set_font(canvas, FontSecondary);
            if(battle_state->battle->player_won) {
                canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignTop, "You Won!");
            } else {
                canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignTop, "You Lost!");
            }
            canvas_draw_str_aligned(canvas, 64, 68, AlignCenter, AlignTop, "Press Back to exit");
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 5, 62, battle_state->status_text);
        }
    }
}

static void enhanced_battle_input_callback(InputEvent* event, void* context) {
    UNUSED(context);

    if(!battle_state || !battle_state->battle) return;

    if(event->type != InputTypePress) return;

    if(battle_state->battle->battle_over) {
        if(event->key == InputKeyBack) {
            battle_state->battle_active = false;
        }
        return;
    }

    if(battle_state->show_moves) {
        switch(event->key) {
        case InputKeyUp:
            if(battle_state->selected_move > 0) {
                battle_state->selected_move--;
            }
            break;
        case InputKeyDown:
            if(battle_state->selected_move < 3 &&
               battle_state->battle->player_pokemon->moves[battle_state->selected_move + 1].power >
                   0) {
                battle_state->selected_move++;
            }
            break;
        case InputKeyOk:
            // Execute battle turn
            if(battle_state->battle->player_pokemon->move_pp[battle_state->selected_move] > 0) {
                // Reduce PP
                battle_state->battle->player_pokemon->move_pp[battle_state->selected_move]--;

                // AI selects random move for enemy
                uint8_t enemy_move = rand() % 4;
                while(battle_state->battle->enemy_pokemon->moves[enemy_move].power == 0 ||
                      battle_state->battle->enemy_pokemon->move_pp[enemy_move] == 0) {
                    enemy_move = (enemy_move + 1) % 4;
                }
                battle_state->battle->enemy_pokemon->move_pp[enemy_move]--;

                // Execute turn
                enhanced_battle_execute_turn(
                    battle_state->battle, battle_state->selected_move, enemy_move);

                // Update status text
                strncpy(
                    battle_state->status_text,
                    enhanced_battle_get_log(battle_state->battle),
                    sizeof(battle_state->status_text) - 1);

                battle_state->show_moves = false;
            }
            break;
        case InputKeyBack:
            battle_state->show_moves = false;
            break;
        default:
            break;
        }
    } else {
        switch(event->key) {
        case InputKeyOk:
            if(!battle_state->battle->battle_over) {
                battle_state->show_moves = true;
            }
            break;
        case InputKeyBack:
            battle_state->battle_active = false;
            break;
        default:
            break;
        }
    }
}

void enhanced_battle_screen_show(EnhancedPokemon* player_pokemon, EnhancedPokemon* enemy_pokemon) {
    // Initialize battle state
    battle_state = malloc(sizeof(EnhancedBattleState));
    battle_state->battle = enhanced_battle_create(player_pokemon, enemy_pokemon);
    battle_state->selected_move = 0;
    battle_state->show_moves = false;
    battle_state->battle_active = true;
    strcpy(battle_state->status_text, "Press OK to select move");

    // Create GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();

    view_port_draw_callback_set(view_port, enhanced_battle_draw_callback, NULL);
    view_port_input_callback_set(view_port, enhanced_battle_input_callback, NULL);

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main loop
    while(battle_state->battle_active) {
        furi_delay_ms(100);
        view_port_update(view_port);
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    enhanced_battle_free(battle_state->battle);
    free(battle_state);
    battle_state = NULL;
}
