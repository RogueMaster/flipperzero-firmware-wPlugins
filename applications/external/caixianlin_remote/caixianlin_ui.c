#include "caixianlin_ui.h"
#include "caixianlin_storage.h"
#include "caixianlin_radio.h"
#include "caixianlin_protocol.h"

// Forward declarations for internal functions
static void handle_setup_input(CaixianlinRemoteApp* app, InputEvent* event);
static void handle_listen_input(CaixianlinRemoteApp* app, InputEvent* event);
static void handle_main_input(CaixianlinRemoteApp* app, InputEvent* event);

// Initialize UI
void caixianlin_ui_init(CaixianlinRemoteApp* app) {
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, caixianlin_ui_draw, app);
    view_port_input_callback_set(app->view_port, caixianlin_ui_input, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

// Cleanup UI
void caixianlin_ui_deinit(CaixianlinRemoteApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
}

// Helper function: Draw horizontal progress bar
static void draw_progress_bar(
    Canvas* canvas,
    int x,
    int y,
    int width,
    int height,
    uint8_t value,
    bool show_outline) {
    if(show_outline) {
        canvas_draw_frame(canvas, x, y, width, height);
    }

    // Calculate filled width (value is 0-100)
    int filled_width = (value * (width - 2)) / 100;
    if(filled_width > 0) {
        canvas_draw_box(canvas, x + 1, y + 1, filled_width, height - 2);
    }
}

// Helper function: Draw menu item box with selection state
static void draw_menu_item(Canvas* canvas, const char* text, int y, bool selected) {
    int x = 4;
    int width = 120;
    int height = 13;

    canvas_draw_rframe(canvas, x, y, width, height, 2);
    if(selected) {
        // Draw double border for selected item (bold effect)
        canvas_draw_rframe(canvas, x + 1, y + 1, width - 2, height - 2, 1);
    }

    // Draw text with padding
    canvas_draw_str(canvas, x + 4, y + 10, text);
}

// Helper function: Draw animated spinner
static void draw_spinner(Canvas* canvas, int x, int y, uint32_t frame_counter) {
    uint32_t step = (frame_counter / 2) % 4;

    char* ch;
    switch(step) {
    case 0:
        ch = "-";
        break;
    case 1:
        ch = "\\";
        break;
    case 2:
        ch = "|";
        break;
    case 3:
        ch = "/";
        break;
    default:
        ch = "?";
        break;
    }

    canvas_draw_str(canvas, x, y, ch);
}

// Draw callback
void caixianlin_ui_draw(Canvas* canvas, void* ctx) {
    CaixianlinRemoteApp* app = ctx;

    canvas_clear(canvas);

    // Increment frame counter for animations
    app->frame_counter++;

    if(app->screen == ScreenSetup) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 10, "Setup");

        canvas_set_font(canvas, FontSecondary);

        // Station ID
        char buf[32];
        if(app->editing_station_id) {
            // Draw menu item box around Station ID
            // draw_menu_item(canvas, "", 17, true);

            snprintf(buf, sizeof(buf), "Station ID:");
            canvas_draw_str(canvas, 8, 34, buf);

            // Draw each digit, highlight selected
            for(int i = 0; i < 5; i++) {
                char digit[2] = {0};
                digit[0] = '0' + caixianlin_protocol_get_station_digit(app->station_id, i);
                int x = 72 + i * 8;

                if(i == app->station_id_digit) {
                    canvas_draw_rbox(canvas, x - 2, 25, 9, 12, 2);
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_str(canvas, x, 34, digit);
                canvas_set_color(canvas, ColorBlack);
            }
        } else {
            snprintf(buf, sizeof(buf), "Station ID: %d", app->station_id);
            draw_menu_item(canvas, buf, 13, app->setup_selected == 0);

            snprintf(buf, sizeof(buf), "Channel: %d", app->channel);
            draw_menu_item(canvas, buf, 25, app->setup_selected == 1);

            draw_menu_item(canvas, "Listen for Remote", 38, app->setup_selected == 2);

            draw_menu_item(canvas, "Done", 51, app->setup_selected == 3);
        }

    } else if(app->screen == ScreenListen) {
        canvas_set_font(canvas, FontPrimary);

        // Draw animated listening indicator
        draw_spinner(canvas, 6, 12, app->frame_counter);
        canvas_draw_str(canvas, 16, 12, "Listening");

        char buf[32];
        canvas_set_font(canvas, FontSecondary);

        // Draw progress bar for buffer
        canvas_draw_str(canvas, 2, 25, "Buffer:");
        int buffer_percent = (app->rx_capture.work_buffer_len * 100) / WORK_BUFFER_SIZE;
        if(buffer_percent > 100) buffer_percent = 100;
        draw_progress_bar(canvas, 42, 18, 80, 6, buffer_percent, true);

        // Draw progress bar for queue
        canvas_draw_str(canvas, 2, 34, "Queue:");
        size_t queue_available = furi_stream_buffer_bytes_available(app->rx_capture.stream_buffer);
        queue_available /= sizeof(int32_t);
        int queue_percent = (queue_available * 100) / RX_BUFFER_SIZE;
        if(queue_percent > 100) queue_percent = 100;
        draw_progress_bar(canvas, 42, 27, 80, 6, queue_percent, true);

        // Show last decoded message if available
        if(app->rx_capture.capture_valid) {
            // Draw box around captured result
            canvas_draw_rframe(canvas, 2, 39, 124, 13, 2);

            snprintf(
                buf,
                sizeof(buf),
                "ID=%d | CH=%d",
                app->rx_capture.captured_station_id,
                app->rx_capture.captured_channel);
            canvas_draw_str(canvas, 6, 49, buf);

            canvas_draw_str(canvas, 2, 62, "OK=Apply Back=Stop V=Rst");
        } else {
            canvas_draw_str(canvas, 2, 50, "Waiting for signal...");
            canvas_draw_str(canvas, 2, 62, "Back=Stop");
        }

    } else { // ScreenMain
        canvas_set_font(canvas, FontPrimary);

        char buf[32];
        snprintf(buf, sizeof(buf), "ID:%d | CH:%d", app->station_id, app->channel);
        canvas_draw_str(canvas, 2, 12, buf);

        // Draw header separator line
        canvas_draw_line(canvas, 0, 15, 127, 15);

        // Mode
        canvas_set_font(canvas, FontPrimary);
        snprintf(buf, sizeof(buf), "%s", mode_names[app->mode]);
        int mode_width = canvas_string_width(canvas, buf);
        int mode_x = (128 - mode_width) / 2;
        canvas_draw_str(canvas, mode_x, 29, buf);

        // Draw rounded box around mode
        canvas_draw_rframe(canvas, mode_x - 4, 19, mode_width + 8, 12, 2);

        // Draw arrows
        canvas_set_font(canvas, FontSecondary);
        int arrow_width = canvas_string_width(canvas, "<");
        canvas_draw_str(canvas, mode_x - 7 - arrow_width, 29, "<");
        canvas_draw_str(canvas, mode_x + mode_width + 7, 29, ">");

        // Strength
        if(app->mode != MODE_BEEP) {
            canvas_set_font(canvas, FontSecondary);
            snprintf(buf, sizeof(buf), "Strength: %d", app->strength);
            int str_width = canvas_string_width(canvas, buf);
            canvas_draw_str(canvas, (128 - str_width) / 2, 42, buf);

            // Draw strength progress bar
            draw_progress_bar(canvas, 13, 45, 102, 6, app->strength, true);
        }

        // Status
        char* status;
        if(app->is_transmitting) {
            canvas_set_font(canvas, FontPrimary);
            status = "[ Transmitting! ]";
        } else {
            canvas_set_font(canvas, FontSecondary);
            status = "[ Hold OK to transmit ]";
        }
        int str_width = canvas_string_width(canvas, status);
        canvas_draw_str(canvas, (128 - str_width) / 2, 62, status);
    }
}

// Input callback
void caixianlin_ui_input(InputEvent* event, void* ctx) {
    CaixianlinRemoteApp* app = ctx;
    furi_message_queue_put(app->event_queue, event, FuriWaitForever);
}

// Handle setup screen input
static void handle_setup_input(CaixianlinRemoteApp* app, InputEvent* event) {
    if(app->editing_station_id) {
        // Station ID digit editing mode
        if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
            if(event->key == InputKeyUp) {
                uint8_t digit =
                    caixianlin_protocol_get_station_digit(app->station_id, app->station_id_digit);
                digit = (digit + 1) % 10;
                app->station_id = caixianlin_protocol_set_station_digit(
                    app->station_id, app->station_id_digit, digit);
            } else if(event->key == InputKeyDown) {
                uint8_t digit =
                    caixianlin_protocol_get_station_digit(app->station_id, app->station_id_digit);
                digit = (digit + 9) % 10;
                app->station_id = caixianlin_protocol_set_station_digit(
                    app->station_id, app->station_id_digit, digit);
            } else if(event->key == InputKeyRight) {
                if(app->station_id_digit < 4) app->station_id_digit++;
            } else if(event->key == InputKeyLeft) {
                if(app->station_id_digit > 0) app->station_id_digit--;
            } else if(event->key == InputKeyOk) {
                app->editing_station_id = false;
                caixianlin_storage_save(app);
            } else if(event->key == InputKeyBack) {
                app->editing_station_id = false;
            }
        }
    } else {
        // Normal setup navigation
        if(event->type == InputTypeShort) {
            if(event->key == InputKeyUp) {
                if(app->setup_selected > 0) app->setup_selected--;
            } else if(event->key == InputKeyDown) {
                if(app->setup_selected < 3) app->setup_selected++;
            } else if(event->key == InputKeyLeft) {
                if(app->setup_selected == 1 && app->channel > 0) {
                    app->channel--;
                    caixianlin_storage_save(app);
                }
            } else if(event->key == InputKeyRight) {
                if(app->setup_selected == 1 && app->channel < 3) {
                    app->channel++;
                    caixianlin_storage_save(app);
                }
            } else if(event->key == InputKeyOk) {
                if(app->setup_selected == 0) {
                    app->editing_station_id = true;
                    app->station_id_digit = 0;
                } else if(app->setup_selected == 2) {
                    app->screen = ScreenListen;
                    caixianlin_radio_start_rx(app);
                } else if(app->setup_selected == 3) {
                    app->screen = ScreenMain;
                }
            } else if(event->key == InputKeyBack) {
                app->running = false;
            }
        }
    }
}

// Handle listen screen input
static void handle_listen_input(CaixianlinRemoteApp* app, InputEvent* event) {
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            // Apply captured station ID and channel if valid
            if(app->rx_capture.capture_valid) {
                app->station_id = app->rx_capture.captured_station_id;
                app->channel = app->rx_capture.captured_channel;
                caixianlin_storage_save(app);

                // Stop listening and return to setup
                if(app->is_listening) {
                    caixianlin_radio_stop_rx(app);
                }
                app->screen = ScreenSetup;
            }
        } else if(event->key == InputKeyBack) {
            // Stop listening and return to setup
            if(app->is_listening) {
                caixianlin_radio_stop_rx(app);
            }
            app->screen = ScreenSetup;
        } else if(event->key == InputKeyDown) {
            // Reset the capture flag
            app->rx_capture.capture_valid = false;
        }
    }
}

// Handle main screen input
static void handle_main_input(CaixianlinRemoteApp* app, InputEvent* event) {
    if(event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            caixianlin_radio_start_tx(app);
        } else if(event->type == InputTypeRelease) {
            caixianlin_radio_stop_tx(app);
        }
    } else if(event->type == InputTypeShort) {
        if(event->key == InputKeyRight) {
            if(app->mode < 3) {
                app->mode++;
            } else {
                app->mode = 1;
            }
        } else if(event->key == InputKeyLeft) {
            if(app->mode > 1) {
                app->mode--;
            } else {
                app->mode = 3;
            }
        } else if(event->key == InputKeyUp && app->mode != MODE_BEEP) {
            if(app->strength < 100) app->strength++;
        } else if(event->key == InputKeyDown && app->mode != MODE_BEEP) {
            if(app->strength > 0) app->strength--;
        }
    } else if(event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp && app->mode != MODE_BEEP) {
            if(app->strength < 90) {
                app->strength += 10;
            } else {
                app->strength = 100;
            }
        } else if(event->key == InputKeyDown && app->mode != MODE_BEEP) {
            if(app->strength > 10) {
                app->strength -= 10;
            } else {
                app->strength = 0;
            }
        }
    }

    if(event->key == InputKeyBack) {
        if(event->type == InputTypeLong) {
            if(app->is_transmitting) caixianlin_radio_stop_tx(app);
            app->screen = ScreenSetup;
        } else if(event->type == InputTypeShort) {
            if(app->is_transmitting) caixianlin_radio_stop_tx(app);
            app->running = false;
        }
    }
}

// Process input event for current screen
void caixianlin_ui_handle_event(CaixianlinRemoteApp* app, InputEvent* event) {
    switch(app->screen) {
    case ScreenSetup:
        handle_setup_input(app, event);
        break;
    case ScreenListen:
        handle_listen_input(app, event);
        break;
    case ScreenMain:
        handle_main_input(app, event);
        break;
    }
}
