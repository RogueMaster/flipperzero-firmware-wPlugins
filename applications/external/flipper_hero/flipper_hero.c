#include <furi.h>
#include <gui/gui.h>
#include <gui/icon.h>
#include <gui/elements.h>
#include <math.h>

#include "data/data.h"
#include "helpers/stratagem_data.h"
#include "view/arrows.h"
#include "helpers/storage.h"
#include "view/draw.h"
// I'm using the code for sound from Doom by MMX @ https://github.com/xMasterX/all-the-plugins/tree/dev/base_pack/doom
// Thanks for the permission, MMX!
#include "helpers/sound.h"

#define SCORE_MULTIPLIER          5
#define TIMER_INCREMENT_PER_ARROW 25
#define TIMER_MAX                 1000

// Durations of sounds/jingles, used to stop the music player before it loops.
#define GAME_OVER_SOUND_DUR 220
#define ROUND_END_SOUND_DUR 230

#ifdef SOUND

void stop_sound(PluginState* plugin_state) {
    FURI_LOG_I("flipper hero", "Stopping sound");
    music_player_worker_stop(plugin_state->music_instance->worker);
    plugin_state->playing_sound = false;
    plugin_state->mus_timer = 0;
}

void start_sound(PluginState* plugin_state, int selection) {
    if(!plugin_state->config->enable_sound || plugin_state->playing_sound) {
        return;
    }
    FURI_LOG_I("flipper hero", "Starting sound");
    bool s = true;
    if(!plugin_state->music_instance->model_mutex) {
        furi_mutex_acquire(plugin_state->music_instance->model_mutex, FuriWaitForever);
    }
    switch(selection) {
    case 0:
        plugin_state->mus_timer = ROUND_END_SOUND_DUR;
        music_player_worker_clear_notes(plugin_state->music_instance->worker);
        music_player_worker_load_rtttl_from_string(plugin_state->music_instance->worker, roundend);
        music_player_worker_start(plugin_state->music_instance->worker);
        break;
    case 1:
        plugin_state->mus_timer = GAME_OVER_SOUND_DUR;
        music_player_worker_clear_notes(plugin_state->music_instance->worker);
        music_player_worker_load_rtttl_from_string(plugin_state->music_instance->worker, gameover);
        music_player_worker_start(plugin_state->music_instance->worker);
        break;
    default:
        FURI_LOG_E("flipper hero", "Invalid sound ID");
        s = false;
        break;
    }
    plugin_state->playing_sound = s;
}

#endif
void fill_stratagem_queue(PluginState* plugin_state) {
    // Fill stratagem queue with invalid stratagem IDs. The renderer will know to stop counting stratagems when it reaches an invalid one.
    for(int i = 0; i < 20; i++) {
        plugin_state->stratagem_queue[i] = -1;
    }
    // Determine the amount of stratagems to put in the queue. Stratagem amount starts at round 1 with 6, and caps at 16 at round 11.
    int stratagem_amount = (plugin_state->round > 10) ? 16 : 5 + plugin_state->round;
    //Populate the queue
    for(int i = 0; i < stratagem_amount; i++) {
        // If it's a special round, (SPECIAL_ROUND_SELECTION_CHANCE) to forcibly select a stratagem in the list.
        bool force_theme = (abs(random()) % 100 < SPECIAL_ROUND_SELECTION_CHANCE);

        // Bool trickery :3
        int start_index =
            (force_theme) ? specialRoundIndexes[plugin_state->special_round]
                                               [2 - (2 * (int)plugin_state->config->unfaithful)] :
                            0;
        int count = (force_theme) ?
                        specialRoundIndexes[plugin_state->special_round]
                                           [3 - (2 * (int)plugin_state->config->unfaithful)] :
                        93;

        plugin_state->stratagem_queue[i] =
            (plugin_state->config->unfaithful) ?
                start_index + (abs(random()) % count) :
                faithfulIndexes[start_index + (abs(random()) % count)];
        /*
        if (plugin_state->config->unfaithful) {
            plugin_state->stratagem_queue[i] = abs(random() * 1000) % 93; // All stratagems
        } else {
            plugin_state->stratagem_queue[i] = faithfulIndexes[abs(random() * 1000) % 27]; // Only stratagems in actual Stratagem Hero
        }*/
    }
}

void generate_arrows(PluginState* plugin_state) {
    //Initialize an empty stratagem, then fill it with the sequence of the next stratagem in the queue.
    char current_stratagem[9] = "XXXXXXXXX";
    get_stratagem_seq(
        plugin_state->seq_buf, plugin_state->stratagem_queue[plugin_state->queue_spot]);
    for(int i = 0; i < 8; i++) {
        current_stratagem[i] = plugin_state->seq_buf[i];
    }

    // Generate arrows until an X is reached, indicating the end of the sequence.
    int i = 0;
    while(current_stratagem[i] != 'X') {
        plugin_state->arrowDirections[i] = current_stratagem[i];
        plugin_state->arrowFilled[i] = false;
        i++;
    }
    plugin_state->numArrows = i;
    plugin_state->nextArrowToFill = 0;
    plugin_state->init_marquee = true;
}

void advance_round(PluginState* plugin_state) {
    plugin_state->isRoundOver = true;
    plugin_state->bonuses[1] = plugin_state->timer;
    plugin_state->timer = TIMER_MAX;
    plugin_state->do_marquee = false;
}

void update_score_and_timer(PluginState* plugin_state) {
    plugin_state->timer += TIMER_INCREMENT_PER_ARROW * plugin_state->numArrows;
    plugin_state->score += SCORE_MULTIPLIER * plugin_state->numArrows;

    plugin_state->queue_spot++;
    // If there's a -1  where a stratagem number should be, end the round.
    if(plugin_state->stratagem_queue[plugin_state->queue_spot] == -1) {
        advance_round(plugin_state);
    }
}

void end_game(PluginState* plugin_state) {
    bool shouldSave = false;
    if(plugin_state->score > plugin_state->record->score) {
        plugin_state->record->score = plugin_state->score;
        shouldSave = true;
    }
    if(plugin_state->round > plugin_state->record->round) {
        plugin_state->record->round = plugin_state->round;
        shouldSave = true;
    }
    if(shouldSave) save_game_records(plugin_state->record);
    plugin_state->isGameOver = true;
    plugin_state->isRoundOver = false;
    plugin_state->isPreRound = false;
    plugin_state->isScoreScreen = false;
}
void start_round(PluginState* plugin_state) {
    plugin_state->isPreRound = false;
    plugin_state->queue_spot = 0;
    fill_stratagem_queue(plugin_state);
    plugin_state->timer = TIMER_MAX;
    plugin_state->perfect = true;
    generate_arrows(plugin_state);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_callback(void* context) {
    PluginState* plugin_state = (PluginState*)context;
    if(!plugin_state) return;
#ifdef SOUND
    // Decrement music timer if a sound is currently playing
    if(plugin_state->playing_sound) {
        plugin_state->mus_timer--;

        // If the timer has run out, stop playback
        if(plugin_state->mus_timer <= 0) {
            stop_sound(plugin_state);
        }
    }
#endif

    if(plugin_state->isGameOver) {
#ifdef SOUND
        // Play the game over jingle if it hasn't been played already, and if there isn't already a sound playing.
        if(!plugin_state->gameOverSoundPlayed) {
            start_sound(plugin_state, 1);
            plugin_state->gameOverSoundPlayed = true;
        }
#endif
        return;
    }
    if(plugin_state->isGamePaused) {
        return;
    }

    // Decrement the timer
    plugin_state->timer--;

    if(plugin_state->do_marquee) update_marquee_data(plugin_state);

    if(plugin_state->isRoundOver) {
#ifdef SOUND
        start_sound(plugin_state, 0);
#endif
        plugin_state->isRoundOver = false;
        plugin_state->timer = 250;
        plugin_state->isScoreScreen = true;
        plugin_state->bonuses[0] = plugin_state->round * 25;
        plugin_state->bonuses[2] = (plugin_state->perfect) ? 100 : 0;

        plugin_state->score +=
            plugin_state->bonuses[0] + plugin_state->bonuses[1] + plugin_state->bonuses[2];
        plugin_state->special_round = ((plugin_state->round + 1) % 3 == 0) ? 1 + rand() % 4 :
                                                                             SpecialRoundNone;
    }

    if(plugin_state->timer <= 0) { // Check if the game should end or change state
        if(plugin_state->isScoreScreen) {
            // switch to pre-round state from round score screen
            plugin_state->timer = 250;
            plugin_state->isPreRound = true;
            plugin_state->isScoreScreen = false;
            plugin_state->round++;

        } else if(plugin_state->isPreRound) {
            // Start a new round from pre-round state
            plugin_state->isPreRound = false;
            start_round(plugin_state);
        } else {
            end_game(plugin_state);
        }
    }
}

void init_game_state(PluginState* plugin_state) {
    plugin_state->special_round = SpecialRoundNone;
    plugin_state->isGameStarted = false;
    plugin_state->isGameOver = false;
    plugin_state->isRoundOver = false;
    plugin_state->isScoreScreen = false;
    plugin_state->isPreRound = false;
    plugin_state->gameOverSoundPlayed = false;
    plugin_state->score = 0;
    plugin_state->round = 1;
    plugin_state->perfect = true;
    plugin_state->queue_spot = 0;
    plugin_state->timer = TIMER_MAX;
    fill_stratagem_queue(plugin_state);
}

void start_game(PluginState* plugin_state, FuriTimer* timer) {
    init_game_state(plugin_state);
    plugin_state->isGameStarted = true;
    generate_arrows(plugin_state);
    furi_timer_start(timer, 10);
}

bool handle_game_input(
    PluginState* plugin_state,
    FuriTimer* timer,
    FuriStatus event_status,
    PluginEvent event) {
    bool processing = true;
    furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);
    if(event_status != FuriStatusOk) {
        FURI_LOG_D("flipper_hero", "FuriMessageQueue: event timeout");
        return processing;
    }
    if(plugin_state->isGamePaused) {
        switch(event.input.type) {
        case InputTypePress:
            if(!plugin_state->allowMenuInput) break;
            switch(event.input.key) {
            case InputKeyUp:
                plugin_state->menu_spot = ((plugin_state->menu_spot == 0) ?
                                               MENU_OPTION_COUNT :
                                               plugin_state->menu_spot) -
                                          1;
                break;
            case InputKeyDown:
                plugin_state->menu_spot = (plugin_state->menu_spot + 1) % MENU_OPTION_COUNT;
                break;
            case InputKeyLeft:
            case InputKeyRight:
                switch(plugin_state->menu_spot) {
                case 0:
                    plugin_state->config->unfaithful = !plugin_state->config->unfaithful;
                    break;
                case 1:
                    plugin_state->config->enable_sound = !plugin_state->config->enable_sound;
                    if(!plugin_state->config->enable_sound && plugin_state->playing_sound) {
                        stop_sound(plugin_state);
                    }
                    break;
                case 2:
                    plugin_state->config->center_queue = !plugin_state->config->center_queue;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
            plugin_state->allowMenuInput = false;
            break;
        case InputTypeRelease:
            plugin_state->allowMenuInput = true;
            break;
        default:
            break;
        }
    } else {
        // Pass the event to the new function for processing
        // handle_arrow_input(plugin_state, event.input);
        // if(event.input.type == InputTypePress) return processing;
        if(event.input.type == InputTypePress) {
            char expectedDirection = plugin_state->arrowDirections[plugin_state->nextArrowToFill];
            bool isCorrect = false;
            switch(event.input.key) {
            case InputKeyUp:
                isCorrect = expectedDirection == 'U';
                break;
            case InputKeyDown:
                isCorrect = expectedDirection == 'D';
                break;
            case InputKeyLeft:
                isCorrect = expectedDirection == 'L';
                break;
            case InputKeyRight:
                isCorrect = expectedDirection == 'R';
                break;
            default:
                break;
            }
            if(isCorrect) {
                plugin_state->arrowFilled[plugin_state->nextArrowToFill++] = true;
            } else if(event.input.key != InputKeyOk && event.input.key != InputKeyBack) {
                plugin_state->perfect = false;
                memset(plugin_state->arrowFilled, false, sizeof(plugin_state->arrowFilled));
                plugin_state->nextArrowToFill = 0;
            }
        }
    }
    if(event.input.key == InputKeyOk) {
        switch(event.input.type) {
        case InputTypePress:
            if(plugin_state->allowOkInput) {
                plugin_state->allowOkInput = false;
                if(plugin_state->isGamePaused) {
                    plugin_state->isGamePaused = false;
                    save_game_config(plugin_state->config);
                } else if(!plugin_state->isGameStarted || plugin_state->isGameOver) {
                    start_game(plugin_state, timer);
                }
            }
            break;
        case InputTypeRelease:
            if(!plugin_state->allowOkInput) plugin_state->allowOkInput = true;
            break;
        default:
            break;
        }
    }
    if(event.input.key == InputKeyBack) {
        switch(event.input.type) {
        case InputTypeLong: // If the back button is held, quit the game
            // Stop any sound that might be playing
            if(plugin_state->playing_sound) stop_sound(plugin_state);

            // Quit game
            // stop timers
            furi_timer_stop(timer);
            processing = false;
            break;
        case InputTypePress: // If the back button is clicked, close pause menu without saving.
            if(plugin_state->allowPauseInput) {
                // Stop sound if enable_sound is on. This allows the user to quickly stop any unwanted sounds
                if(plugin_state->playing_sound && plugin_state->config->enable_sound)
                    stop_sound(plugin_state);
                plugin_state->isGamePaused = !plugin_state->isGamePaused;
                plugin_state->allowPauseInput = false;
                plugin_state->allowMenuInput = true;
            }
            break;
        case InputTypeRelease:
            if(!plugin_state->allowPauseInput) plugin_state->allowPauseInput = true;
            break;
        default:
            break;
        }
    }
    return processing;
}

static void render_callback(Canvas* const canvas, void* ctx) {
    PluginState* plugin_state = (PluginState*)ctx;
    furi_mutex_acquire(plugin_state->mutex, FuriWaitForever);

    if(plugin_state->init_marquee) {
        init_marquee_data(canvas, plugin_state);
        plugin_state->init_marquee = false;
    }
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(!plugin_state->isGameStarted) {
        draw_start_screen(canvas);
    } else if(plugin_state->isGameOver) {
        draw_game_over_screen(canvas, plugin_state);
    } else if(plugin_state->isScoreScreen) {
        draw_round_score(plugin_state, canvas);
    } else if(plugin_state->isPreRound) {
        draw_pre_round(plugin_state, canvas);
    } else if(!plugin_state->isRoundOver) {
        draw_queue(canvas, plugin_state);
        draw_game_ui(canvas, plugin_state->round, plugin_state->score);
        draw_name(canvas, plugin_state);
        draw_arrows(canvas, plugin_state);
        draw_progress_box(canvas, plugin_state->timer);
    }
    if(plugin_state->isGamePaused) {
        draw_pause_menu(canvas, plugin_state);
    }
    furi_mutex_release(plugin_state->mutex);
}
int32_t flipper_hero_app() {
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));
    PluginState* plugin_state = malloc(sizeof(PluginState));

    FuriTimer* timer =
        furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, (void*)plugin_state);
    plugin_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    if(!plugin_state->mutex) {
        FURI_LOG_E("flipper_hero", "cannot create mutex\r\n");
        free(plugin_state);
        return 255;
    }
    plugin_state->record = malloc(sizeof(Record));
    plugin_state->config = malloc(sizeof(Config));
    load_game_records(plugin_state->record);
    load_game_config(plugin_state->config);
    init_game_state(plugin_state);
    plugin_state->allowPauseInput = true;
    plugin_state->allowOkInput = true;
#ifdef SOUND
    plugin_state->playing_sound = false;
    plugin_state->mus_timer = 0;
    plugin_state->music_instance = malloc(sizeof(MusicPlayer));
    plugin_state->music_instance->model = malloc(sizeof(MusicPlayerModel));
    plugin_state->music_instance->model->volume = 2;

    plugin_state->music_instance->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!plugin_state->music_instance->model_mutex) {
        FURI_LOG_E("flipper_hero", "cannot create music mutex\r\n");
        free(plugin_state->music_instance);
        return 255;
    }
    plugin_state->music_instance->worker = music_player_worker_alloc();
    music_player_worker_set_volume(
        plugin_state->music_instance->worker,
        MUSIC_PLAYER_VOLUMES[plugin_state->music_instance->model->volume]);
#endif

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, plugin_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    PluginEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        processing = handle_game_input(plugin_state, timer, event_status, event);

        // Check if all arrows are filled, then regenerate
        if(plugin_state->nextArrowToFill >= plugin_state->numArrows &&
           !plugin_state->isGamePaused) {
            update_score_and_timer(plugin_state);
            generate_arrows(plugin_state); // Re-initialize arrow states
        }
        view_port_update(view_port);
        furi_mutex_release(plugin_state->mutex);
    }
#ifdef SOUND
    furi_mutex_release(plugin_state->music_instance->model_mutex);
    music_player_worker_free(plugin_state->music_instance->worker);
    furi_mutex_free(plugin_state->music_instance->model_mutex);
    free(plugin_state->music_instance->model);
    free(plugin_state->music_instance);
#endif
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close("gui");
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(plugin_state->mutex);
    return 0;
}
