#include "laser_tag_app.h"
#include "laser_tag_view.h"
#include "infrared_controller.h"
#include "game_state.h"
#include "lfrfid_reader.h"
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>

#define TAG "LaserTagApp"

struct LaserTagApp {
    Gui* gui;
    ViewPort* view_port;
    LaserTagView* view;
    FuriMessageQueue* event_queue;
    FuriTimer* timer;
    NotificationApp* notifications;
    InfraredController* ir_controller;
    GameState* game_state;
    LaserTagState state;
    bool need_redraw;
    LFRFIDReader* reader;
};

const NotificationSequence sequence_vibro_1 = {&message_vibro_on, &message_vibro_off, NULL};
const NotificationSequence sequence_short_beep =
    {&message_note_c4, &message_delay_50, &message_sound_off, NULL};

static void laser_tag_app_timer_callback(void* context) {
    furi_assert(context);
    LaserTagApp* app = context;
    FURI_LOG_D(TAG, "Timer callback triggered");

    if(app->state == LaserTagStateSplashScreen) {
        if(game_state_get_time(app->game_state) >= 2) {
            FURI_LOG_I(TAG, "Splash screen time over, switching to TeamSelect");
            app->state = LaserTagStateTeamSelect;
            game_state_reset(app->game_state);
            FURI_LOG_D(TAG, "Game state reset after splash screen");
        } else {
            FURI_LOG_D(TAG, "Updating splash screen time");
            game_state_update_time(app->game_state, 1);
        }
    } else if(app->state == LaserTagStateGame) {
        FURI_LOG_D(TAG, "Updating game time by 1 second");
        game_state_update_time(app->game_state, 1);
    }

    if(app->view) {
        FURI_LOG_D(TAG, "Updating view with the latest game state");
        laser_tag_view_update(app->view, app->game_state);
        app->need_redraw = true;
    }
}

static void laser_tag_app_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    LaserTagApp* app = context;
    FURI_LOG_D(TAG, "Input event received: type=%d, key=%d", input_event->type, input_event->key);
    furi_message_queue_put(app->event_queue, input_event, 0);
    FURI_LOG_D(TAG, "Input event queued successfully");
}

static void laser_tag_app_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    LaserTagApp* app = context;
    FURI_LOG_D(TAG, "Entering draw callback");

    if(app->state == LaserTagStateSplashScreen) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 5, 20, "Laser Tag!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 5, 40, "https://github.com/");
        canvas_draw_str(canvas, 5, 50, "RocketGod-git/");
        canvas_draw_str(canvas, 5, 60, "Flipper-Zero-Laser-Tag");
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_draw_line(canvas, 0, 30, 127, 30);
        canvas_draw_circle(canvas, 110, 15, 12);
        canvas_draw_disc(canvas, 110, 15, 4);

    } else if(app->state == LaserTagStateTeamSelect) {
        canvas_clear(canvas);
        canvas_draw_frame(canvas, 0, 0, 128, 64);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 14, 13, "SELECT TEAM");

        canvas_draw_line(canvas, 0, 16, 127, 16);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 5, 30, "LEFT");
        canvas_draw_str(canvas, 95, 30, "RIGHT");

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 45, "RED");
        canvas_draw_str(canvas, 95, 45, "BLUE");

        // Gun icon for Red team
        canvas_draw_line(canvas, 10, 50, 25, 50);
        canvas_draw_line(canvas, 25, 50, 25, 55);
        canvas_draw_line(canvas, 10, 55, 25, 55);
        canvas_draw_line(canvas, 15, 55, 15, 60);

        // Gun icon for Blue team (facing left)
        canvas_draw_line(canvas, 95, 50, 110, 50);
        canvas_draw_line(canvas, 95, 50, 95, 55);
        canvas_draw_line(canvas, 95, 55, 110, 55);
        canvas_draw_line(canvas, 105, 55, 105, 60);

        // Laser beams
        canvas_draw_line(canvas, 25, 52, 60, 32);
        canvas_draw_line(canvas, 95, 52, 60, 32);

        // Targets where lasers hit
        canvas_draw_circle(canvas, 60, 32, 5);
        canvas_draw_circle(canvas, 60, 32, 2);

    } else if(app->state == LaserTagStateGameOver) {
        canvas_clear(canvas);

        canvas_set_font(canvas, FontPrimary);

        // Display "GAME OVER!" centered on the screen
        canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, "GAME OVER!");

        // Add a solid block border around the screen
        for(int x = 0; x < 128; x += 8) {
            canvas_draw_box(canvas, x, 0, 8, 8);
            canvas_draw_box(canvas, x, 56, 8, 8);
        }
        for(int y = 8; y < 56; y += 8) {
            canvas_draw_box(canvas, 0, y, 8, 8);
            canvas_draw_box(canvas, 120, y, 8, 8);
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "Press OK to Restart");

    } else if(app->view) {
        FURI_LOG_D(TAG, "Drawing game view");
        laser_tag_view_draw(laser_tag_view_get_view(app->view), canvas);
    }
    FURI_LOG_D(TAG, "Exiting draw callback");
}

static bool matching_team(LaserTagApp* app, uint8_t data) {
    if(data == 0) {
        return true;
    } else if(game_state_get_team(app->game_state) == TeamRed) {
        return data == 0xA1;
    } else if(game_state_get_team(app->game_state) == TeamBlue) {
        return data == 0xB2;
    }
    return false;
}

static void tag_callback(uint8_t* data, uint8_t length, void* context) {
    LaserTagApp* app = (LaserTagApp*)context;

    if(length != 5) {
        FURI_LOG_W(TAG, "Tag is not for game.  Length: %d", length);
        return;
    }

    if(data[0] != 0x13 || data[1] != 0x37) {
        FURI_LOG_D(
            TAG,
            "Tag is not for game.  Data: %02x %02x %02x %02x %02x",
            data[0],
            data[1],
            data[2],
            data[3],
            data[4]);
        return;
    }

    if(matching_team(app, data[2])) {
        if(data[3] == 0xFD) {
            uint16_t max_delta_ammo = data[4];
            uint16_t ammo = game_state_get_ammo(app->game_state);
            uint16_t delta_ammo = INITIAL_AMMO - ammo;
            if(delta_ammo > max_delta_ammo) {
                delta_ammo = max_delta_ammo;
            }
            game_state_increase_ammo(app->game_state, delta_ammo);
            FURI_LOG_D(TAG, "Increased ammo by: %d", delta_ammo);
        } else {
            FURI_LOG_W(TAG, "Tag action unknown: %02x %02x", data[3], data[4]);
        }
    } else {
        FURI_LOG_I(TAG, "Tag not for team: %02x", data[2]);
    }
}

LaserTagApp* laser_tag_app_alloc() {
    FURI_LOG_D(TAG, "Allocating Laser Tag App");
    LaserTagApp* app = malloc(sizeof(LaserTagApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate LaserTagApp");
        return NULL;
    }
    FURI_LOG_I(TAG, "LaserTagApp allocated successfully");

    memset(app, 0, sizeof(LaserTagApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->view = laser_tag_view_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->game_state = game_state_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    if(!app->gui || !app->view_port || !app->view || !app->notifications || !app->game_state ||
       !app->event_queue) {
        FURI_LOG_E(TAG, "Failed to allocate resources for LaserTagApp");
        laser_tag_app_free(app);
        return NULL;
    }

    app->state = LaserTagStateSplashScreen;
    app->need_redraw = true;
    FURI_LOG_I(TAG, "Initial state set to SplashScreen");

    view_port_draw_callback_set(app->view_port, laser_tag_app_draw_callback, app);
    view_port_input_callback_set(app->view_port, laser_tag_app_input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    FURI_LOG_D(TAG, "ViewPort callbacks set and added to GUI");

    app->timer = furi_timer_alloc(laser_tag_app_timer_callback, FuriTimerTypePeriodic, app);
    if(!app->timer) {
        FURI_LOG_E(TAG, "Failed to allocate timer");
        laser_tag_app_free(app);
        return NULL;
    }
    FURI_LOG_I(TAG, "Timer allocated");

    app->reader = lfrfid_reader_alloc();
    lfrfid_reader_set_tag_callback(app->reader, "EM4100", tag_callback, app);

    furi_timer_start(app->timer, furi_kernel_get_tick_frequency());
    FURI_LOG_D(TAG, "Timer started");

    return app;
}

void laser_tag_app_free(LaserTagApp* app) {
    FURI_LOG_D(TAG, "Freeing Laser Tag App");
    furi_assert(app);

    furi_timer_free(app->timer);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    laser_tag_view_free(app->view);
    furi_message_queue_free(app->event_queue);
    if(app->ir_controller) {
        infrared_controller_free(app->ir_controller);
    }
    if(app->reader) {
        lfrfid_reader_free(app->reader);
        app->reader = NULL;
    }
    free(app->game_state);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
    FURI_LOG_I(TAG, "Laser Tag App freed successfully");
}

void laser_tag_app_fire(LaserTagApp* app) {
    furi_assert(app);
    FURI_LOG_D(TAG, "Firing laser");

    if(!app->ir_controller) {
        FURI_LOG_E(TAG, "IR controller is NULL in laser_tag_app_fire");
        return;
    }

    if(app->ir_controller->processing_signal) {
        FURI_LOG_W(TAG, "Cannot fire, hit is being processed");
        return;
    }

    infrared_controller_send(app->ir_controller);
    FURI_LOG_D(TAG, "Laser fired, decreasing ammo by 1");
    game_state_decrease_ammo(app->game_state, 1);

    notification_message(app->notifications, &sequence_short_beep);

    if(game_state_get_team(app->game_state) == TeamBlue) {
        notification_message(app->notifications, &sequence_blink_blue_100);
        FURI_LOG_I(TAG, "Notifying user with blink blue and short beep");
    } else {
        notification_message(app->notifications, &sequence_blink_red_100);
        FURI_LOG_I(TAG, "Notifying user with blink red and short beep");
    }

    app->need_redraw = true;
}

void laser_tag_app_handle_hit(LaserTagApp* app) {
    furi_assert(app);
    FURI_LOG_D(TAG, "Handling hit, decreasing health by 10");

    game_state_decrease_health(app->game_state, 10);
    notification_message(app->notifications, &sequence_vibro_1);
    FURI_LOG_I(TAG, "Notifying user with vibration");

    if(game_state_is_game_over(app->game_state)) {
        FURI_LOG_I(TAG, "Game over, switching to Game Over screen");

        notification_message(app->notifications, &sequence_error);

        app->state = LaserTagStateGameOver;
        app->need_redraw = true;
    }
}

static bool laser_tag_app_enter_game_state(LaserTagApp* app) {
    furi_assert(app);
    FURI_LOG_I(TAG, "Entering game state");

    app->state = LaserTagStateGame;
    game_state_reset(app->game_state);
    FURI_LOG_D(TAG, "Game state reset");

    laser_tag_view_update(app->view, app->game_state);
    FURI_LOG_D(TAG, "View updated with new game state");

    if(app->ir_controller) {
        infrared_controller_free(app->ir_controller);
        app->ir_controller = NULL;
    }
    app->ir_controller = infrared_controller_alloc();
    if(!app->ir_controller) {
        FURI_LOG_E(TAG, "Failed to allocate IR controller");
        return false;
    }
    FURI_LOG_I(TAG, "IR controller allocated");

    infrared_controller_set_team(app->ir_controller, game_state_get_team(app->game_state));
    FURI_LOG_D(TAG, "IR controller team set");
    app->need_redraw = true;
    return true;
}

int32_t laser_tag_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Laser Tag app starting");

    LaserTagApp* app = laser_tag_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate application");
        return -1;
    }
    FURI_LOG_D(TAG, "LaserTagApp allocated successfully");

    InputEvent event;
    bool running = true;
    while(running) {
        FURI_LOG_D(TAG, "Start of main loop iteration");

        update_infrared_board_status(app->ir_controller);

        FuriStatus status = furi_message_queue_get(app->event_queue, &event, 100);
        if(status == FuriStatusOk) {
            FURI_LOG_D(TAG, "Received input event: type=%d, key=%d", event.type, event.key);
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if(app->state == LaserTagStateSplashScreen ||
                   app->state == LaserTagStateTeamSelect) {
                    switch(event.key) {
                    case InputKeyLeft:
                        FURI_LOG_I(TAG, "Red team selected");
                        game_state_set_team(app->game_state, TeamRed);
                        if(!laser_tag_app_enter_game_state(app)) {
                            running = false;
                        }
                        break;
                    case InputKeyRight:
                        FURI_LOG_I(TAG, "Blue team selected");
                        game_state_set_team(app->game_state, TeamBlue);
                        if(!laser_tag_app_enter_game_state(app)) {
                            running = false;
                        }
                        break;
                    case InputKeyBack:
                        FURI_LOG_I(TAG, "Back key pressed, exiting");
                        running = false;
                        break;
                    default:
                        break;
                    }
                } else if(app->state == LaserTagStateGameOver) {
                    if(event.key == InputKeyOk) {
                        FURI_LOG_I(TAG, "OK key pressed, restarting game");

                        // Restart game by resetting game state and transitioning to splash screen
                        game_state_reset(app->game_state);
                        app->state = LaserTagStateSplashScreen;
                        app->need_redraw = true;
                    }
                } else if(app->state == LaserTagStateGame) {
                    if(event.key == InputKeyDown && game_state_get_ammo(app->game_state) == 0) {
                        // Reload ammo when Down button is pressed and ammo is depleted
                        FURI_LOG_I(TAG, "Down key pressed, reloading ammo");
                        game_state_increase_ammo(app->game_state, INITIAL_AMMO);
                        app->need_redraw = true;
                    } else {
                        switch(event.key) {
                        case InputKeyBack:
                            FURI_LOG_I(TAG, "Back key pressed, exiting");
                            running = false;
                            break;
                        case InputKeyOk:
                            FURI_LOG_I(TAG, "OK key pressed, firing laser");
                            laser_tag_app_fire(app);
                            break;
                        case InputKeyUp:
                            FURI_LOG_I(TAG, "Up key pressed, scanning for ammo");
                            notification_message(app->notifications, &sequence_short_beep);
                            uint16_t ammo = game_state_get_ammo(app->game_state);
                            infrared_controller_pause(app->ir_controller);
                            lfrfid_reader_start(app->reader);
                            for(int i = 0; i < 30; i++) {
                                furi_delay_ms(100);
                                if(ammo != game_state_get_ammo(app->game_state)) {
                                    break;
                                }
                            }
                            lfrfid_reader_stop(app->reader);
                            infrared_controller_resume(app->ir_controller);
                            if(ammo != game_state_get_ammo(app->game_state)) {
                                notification_message(app->notifications, &sequence_success);
                            } else {
                                notification_message(app->notifications, &sequence_error);
                            }
                            app->need_redraw = true;
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        } else if(status == FuriStatusErrorTimeout) {
            FURI_LOG_D(TAG, "No input event, continuing");
        } else {
            FURI_LOG_E(TAG, "Failed to get input event, status: %d", status);
        }

        if(app->state == LaserTagStateGame && app->ir_controller) {
            if(infrared_controller_receive(app->ir_controller)) {
                FURI_LOG_D(TAG, "Hit received, processing");
                laser_tag_app_handle_hit(app);
            }

            if(game_state_is_game_over(app->game_state)) {
                FURI_LOG_I(TAG, "Game over, notifying user with error sequence");
                notification_message(app->notifications, &sequence_error);
                // Stop game logic after game over
                app->state = LaserTagStateGameOver;
                app->need_redraw = true;
            }
        } else if(app->state == LaserTagStateGameOver) {
            if(event.key == InputKeyOk) {
                FURI_LOG_I(TAG, "OK key pressed, restarting game");
                game_state_reset(app->game_state);
                app->state = LaserTagStateSplashScreen;
                app->need_redraw = true;
            }
        }

        if(app->need_redraw) {
            FURI_LOG_D(TAG, "Updating viewport");
            view_port_update(app->view_port);
            app->need_redraw = false;
        }

        FURI_LOG_D(TAG, "End of main loop iteration");
        furi_delay_ms(10);
    }

    FURI_LOG_I(TAG, "Laser Tag app exiting");
    laser_tag_app_free(app);
    return 0;
}
