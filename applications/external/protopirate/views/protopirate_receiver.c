// views/protopirate_receiver.c
#include "protopirate_receiver.h"
#include "../protopirate_history.h"
#include "../protopirate_app_i.h"
#include <input/input.h>
#include <gui/elements.h>
#include <furi.h>
#include <math.h>

#include "proto_pirate_icons.h"

#define FRAME_HEIGHT             12
#define MAX_LEN_PX               112
#define MENU_ITEMS               4u
#define UNLOCK_CNT               3
#define SUBGHZ_RAW_THRESHOLD_MIN -90.0f

struct ProtoPirateReceiver {
    View* view;
    ProtoPirateReceiverCallback callback;
    void* context;
};

typedef struct {
    ProtoPirateHistory* history;
    uint8_t list_offset;
    uint8_t history_item;
    float rssi;
    bool auto_save;
    FuriString* frequency_str;
    FuriString* preset_str;
    FuriString* history_stat_str;
    FuriString* draw_scratch;
    bool external_radio;
    ProtoPirateLock lock;
    uint8_t lock_count;
    uint8_t animation_frame;
    bool dolphin_view;
    bool sub_decode_mode;
} ProtoPirateReceiverModel;

static size_t protopirate_view_receiver_item_count(ProtoPirateReceiverModel* model) {
    furi_check(model);
    return model->history ? protopirate_history_get_item(model->history) : 0U;
}

static void protopirate_view_rssi_draw(Canvas* canvas, ProtoPirateReceiverModel* model) {
    furi_check(model);
    uint8_t u_rssi = 0;

    if(model->rssi >= SUBGHZ_RAW_THRESHOLD_MIN) {
        float v = model->rssi - SUBGHZ_RAW_THRESHOLD_MIN;
        if(v < 0.0f) v = 0.0f;
        if(v > 67.0f) v = 67.0f;
        u_rssi = (uint8_t)v;
    }

    //Add a 1px space between the segments
    uint8_t spacer = 0;
    for(uint8_t i = 1; i < u_rssi; i++) {
        if(i % 5) {
            uint8_t j = 46 + i + spacer;
            canvas_draw_dot(canvas, j, 52);
            canvas_draw_dot(canvas, j + 1, 53);
            canvas_draw_dot(canvas, j, 54);
        } else
            spacer++;
    }
}

void protopirate_view_receiver_set_sub_decode_mode(
    ProtoPirateReceiver* receiver,
    bool sub_decode_mode) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        { model->sub_decode_mode = sub_decode_mode; },
        true);
}

void protopirate_view_receiver_set_rssi(ProtoPirateReceiver* receiver, float rssi) {
    furi_check(receiver);
    with_view_model(
        receiver->view, ProtoPirateReceiverModel * model, { model->rssi = rssi; }, true);
}

void protopirate_view_receiver_set_lock(ProtoPirateReceiver* receiver, ProtoPirateLock lock) {
    furi_check(receiver);
    with_view_model(
        receiver->view, ProtoPirateReceiverModel * model, { model->lock = lock; }, true);
}

void protopirate_view_receiver_set_autosave(ProtoPirateReceiver* receiver, bool auto_save) {
    furi_check(receiver);
    with_view_model(
        receiver->view, ProtoPirateReceiverModel * model, { model->auto_save = auto_save; }, true);
}

void protopirate_view_receiver_set_callback(
    ProtoPirateReceiver* receiver,
    ProtoPirateReceiverCallback callback,
    void* context) {
    furi_check(receiver);
    receiver->callback = callback;
    receiver->context = context;
}

static void protopirate_view_receiver_update_offset(ProtoPirateReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            size_t history_item = model->history_item;
            size_t list_offset = model->list_offset;
            size_t item_count = protopirate_view_receiver_item_count(model);

            if(history_item < list_offset) {
                model->list_offset = history_item;
            } else if(history_item >= (list_offset + MENU_ITEMS)) {
                model->list_offset = history_item - (MENU_ITEMS - 1);
            }

            if(item_count < MENU_ITEMS) {
                model->list_offset = 0;
            } else {
                size_t max_offset = item_count - MENU_ITEMS;
                if(model->list_offset > max_offset) {
                    model->list_offset = max_offset;
                }
            }
        },
        true);
}

void protopirate_view_receiver_add_data_statusbar(
    ProtoPirateReceiver* receiver,
    const char* frequency_str,
    const char* preset_str,
    const char* history_stat_str,
    bool external_radio) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            furi_string_set_str(model->frequency_str, frequency_str);
            furi_string_set_str(model->preset_str, preset_str);
            furi_string_set_str(model->history_stat_str, history_stat_str);
            model->external_radio = external_radio;
        },
        true);
}

static void protopirate_view_receiver_draw_frame(Canvas* canvas, uint16_t idx, bool scrollbar) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0 + idx * FRAME_HEIGHT, scrollbar ? 122 : 127, FRAME_HEIGHT);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_dot(canvas, 0, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 1, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 1);

    canvas_draw_dot(canvas, 0, (0 + idx * FRAME_HEIGHT) + 11);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, 0 + idx * FRAME_HEIGHT);
    canvas_draw_dot(canvas, scrollbar ? 121 : 126, (0 + idx * FRAME_HEIGHT) + 11);
}

void protopirate_view_receiver_draw(Canvas* canvas, ProtoPirateReceiverModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    // Increment animation frame
    static uint8_t animation_frame = 0;
    animation_frame = (animation_frame + 1) % 96;

    size_t item_count = protopirate_view_receiver_item_count(model);
    bool scrollbar = item_count > MENU_ITEMS;

    if(!model->sub_decode_mode) {
        //Config button. (Do it at the top so we dont get Inversion problems from the list view part.)
        elements_button_left(canvas, "Config");

        // Draw RSSI
        protopirate_view_rssi_draw(canvas, model);
    }

    //Draw To Unlock, Locked etc...
    if(model->lock_count) {
        canvas_draw_str(canvas, 44, 63, furi_string_get_cstr(model->frequency_str));
        canvas_draw_str(canvas, 79, 63, furi_string_get_cstr(model->preset_str));
        canvas_draw_str(canvas, 96, 63, furi_string_get_cstr(model->history_stat_str));
        canvas_set_font(canvas, FontSecondary);
        elements_bold_rounded_frame(canvas, 14, 8, 99, 48);
        elements_multiline_text(canvas, 65, 26, "To unlock\npress:");
        canvas_draw_icon(canvas, 65, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 80, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 95, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 16, 13, &I_WarningDolphin_45x42);
        canvas_draw_dot(canvas, 17, 61);
    } else {
        if(model->lock == ProtoPirateLockOn) {
            canvas_draw_icon(canvas, 64, 55, &I_Lock_7x8);
            canvas_draw_str(canvas, 74, 62, "Locked");
        } else {
            canvas_draw_str(canvas, 44, 63, furi_string_get_cstr(model->frequency_str));
            canvas_draw_str(canvas, 79, 63, furi_string_get_cstr(model->preset_str));
            canvas_draw_str(canvas, 96, 63, furi_string_get_cstr(model->history_stat_str));
        }
    }

    //Draw the List, or the Radar/Dolphin View.
    if(item_count > 0) {
        // Draw received items list
        size_t shift_position = model->list_offset;

        for(size_t i = 0; i < MIN(item_count, MENU_ITEMS); i++) {
            size_t idx = shift_position + i;
            protopirate_history_get_text_item_menu(model->history, model->draw_scratch, idx);
            elements_string_fit_width(
                canvas, model->draw_scratch, scrollbar ? MAX_LEN_PX - 6 : MAX_LEN_PX);

            if(model->history_item == idx) {
                protopirate_view_receiver_draw_frame(canvas, i, scrollbar);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            canvas_draw_str(
                canvas, 4, 9 + (i * FRAME_HEIGHT), furi_string_get_cstr(model->draw_scratch));
        }

        //Draw scrollbar if needed
        if(scrollbar) {
            // Calculate maximum scroll position
            size_t max_scroll = item_count > MENU_ITEMS ? item_count - MENU_ITEMS : 0;
            size_t scroll_pos = shift_position;
            if(scroll_pos > max_scroll) {
                scroll_pos = max_scroll;
            }
            size_t scrollable_total = max_scroll > 0 ? max_scroll + 1 : 1;
            elements_scrollbar_pos(canvas, 128, 0, 49, scroll_pos, scrollable_total);
        }
    } else {
        //Are we in Radar View or FLipper View Mode?
        if(!model->dolphin_view) {
            // Cool animated radar with expanding dots
            int center_x = 64;
            int center_y = 22;

            // Three waves of expanding circles with different speeds
            for(int wave = 0; wave < 3; wave++) {
                // Calculate radius for this wave with offset
                int base_radius = ((animation_frame + wave * 32) % 96) / 3;

                if(base_radius < 28) {
                    // Calculate fade based on distance from center
                    int dot_density = 24 - (base_radius / 2);

                    // Draw circle with dots
                    for(int angle = 0; angle < 360; angle += (360 / dot_density)) {
                        float rad = (angle + wave * 15) * 3.14159 / 180.0;
                        int x = center_x + base_radius * cosf(rad);
                        int y = center_y + base_radius * sinf(rad);

                        // Only draw if within bounds and create fade effect
                        if(x > 0 && x < 128 && y > 0 && y < 48) {
                            // Dots get smaller/fade as they expand
                            if(base_radius < 10) {
                                canvas_draw_dot(canvas, x, y);
                                // Double dot for inner circles for brightness
                                if(base_radius < 5) {
                                    canvas_draw_dot(canvas, x + 1, y);
                                }
                            } else if(base_radius < 20) {
                                // Skip some dots for fade effect
                                if(angle % 30 == 0) {
                                    canvas_draw_dot(canvas, x, y);
                                }
                            } else {
                                // Very sparse dots at edge
                                if(angle % 60 == 0) {
                                    canvas_draw_dot(canvas, x, y);
                                }
                            }
                        }
                    }
                }
            }

            // Static guide circles (very faint)
            for(int angle = 0; angle < 360; angle += 45) {
                float rad = angle * 3.14159f / 180.0f;
                canvas_draw_dot(canvas, center_x + 15 * cosf(rad), center_y + 15 * sinf(rad));
            }

            // Rotating sweep line with glow effect
            float sweep_angle = (animation_frame * 3.75f) * 3.14159f / 180.0f;

            // Main sweep line
            int sweep_x = center_x + 22 * cosf(sweep_angle);
            int sweep_y = center_y + 22 * sinf(sweep_angle);
            canvas_draw_line(canvas, center_x, center_y, sweep_x, sweep_y);

            // Sweep "glow" - additional lines at slight offsets
            float glow_angle1 = sweep_angle - 0.05f;
            float glow_angle2 = sweep_angle + 0.05f;
            canvas_draw_line(
                canvas,
                center_x,
                center_y,
                center_x + 20 * cosf(glow_angle1),
                center_y + 20 * sinf(glow_angle1));
            canvas_draw_line(
                canvas,
                center_x,
                center_y,
                center_x + 20 * cosf(glow_angle2),
                center_y + 20 * sinf(glow_angle2));

            // Sweep trail (fading dots)
            for(int i = 1; i <= 12; i++) {
                float trail_angle = sweep_angle - (i * 0.15f);
                int trail_radius = 22 - i;
                if(trail_radius > 0) {
                    int trail_x = center_x + trail_radius * cosf(trail_angle);
                    int trail_y = center_y + trail_radius * sinf(trail_angle);
                    // Only draw every other dot in trail for fade effect
                    if(i % 2 == 0 || i < 4) {
                        canvas_draw_dot(canvas, trail_x, trail_y);
                    }
                }
            }

            // Pulsing center
            int pulse = (animation_frame % 32);
            if(pulse < 16) {
                canvas_draw_disc(canvas, center_x, center_y, 2);
            } else {
                canvas_draw_circle(canvas, center_x, center_y, 2);
            }
            if(pulse < 8 || (pulse > 16 && pulse < 24)) {
                canvas_draw_dot(canvas, center_x, center_y);
            }
        } else {
            canvas_draw_icon(
                canvas,
                0,
                0,
                model->external_radio ? &I_PP_scanning_ext_123x52 : &I_PP_scanning_123x52);
            //canvas_set_font(canvas, FontPrimary);
            //canvas_draw_str(canvas, 63, 46, "Scanning...");
            //canvas_set_font(canvas, FontSecondary);
            //canvas_draw_str(canvas, 44, 10, model->external_radio ? "Ext" : "Int");       //FOR EXACT FLIPPER CLONE
        }

        // Draw EXT/INT indicator in upper right corner
        canvas_set_font(canvas, FontSecondary);
        if(model->external_radio) {
            canvas_draw_str_aligned(canvas, 127, 0, AlignRight, AlignTop, "Ext");
        } else {
            canvas_draw_str_aligned(canvas, 127, 0, AlignRight, AlignTop, "Int");
        }

        //Draw the Auto-save Indicator
        if(model->auto_save) {
            const char* auto_save_text = "Save";
            canvas_draw_str(
                canvas, 110 - canvas_string_width(canvas, auto_save_text), 7, auto_save_text);
        }
    }
}

bool protopirate_view_receiver_input(InputEvent* event, void* context) {
    furi_check(context);
    ProtoPirateReceiver* receiver = context;

    bool consumed = false;

    ProtoPirateLock lock;
    with_view_model(
        receiver->view, ProtoPirateReceiverModel * model, { lock = model->lock; }, false);

    if(lock == ProtoPirateLockOn) {
        bool do_unlock_cb = false;

        with_view_model(
            receiver->view,
            ProtoPirateReceiverModel * model,
            {
                if(event->key == InputKeyBack) {
                    if(event->type == InputTypeShort) {
                        model->lock_count++;
                        if(model->lock_count >= UNLOCK_CNT) {
                            model->lock = ProtoPirateLockOff;
                            model->lock_count = 0;
                            do_unlock_cb = true;
                        }
                    }
                } else if(
                    event->type == InputTypeShort || event->type == InputTypeLong ||
                    event->type == InputTypePress) {
                    model->lock_count = 0;
                }
            },
            true);

        if(do_unlock_cb && receiver->callback) {
            receiver->callback(ProtoPirateCustomEventViewReceiverUnlock, receiver->context);
        }

        consumed = true;
    } else if(
        event->type == InputTypeShort || event->type == InputTypeLong ||
        event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                receiver->view,
                ProtoPirateReceiverModel * model,
                {
                    if(model->history_item > 0) {
                        model->history_item--;
                    }
                },
                true);
            protopirate_view_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyDown:
            with_view_model(
                receiver->view,
                ProtoPirateReceiverModel * model,
                {
                    size_t item_count = protopirate_view_receiver_item_count(model);
                    if(item_count > 0 && model->history_item < item_count - 1) {
                        model->history_item++;
                    }
                },
                true);
            protopirate_view_receiver_update_offset(receiver);
            consumed = true;
            break;
        case InputKeyLeft:
            if(receiver->callback) {
                receiver->callback(ProtoPirateCustomEventViewReceiverConfig, receiver->context);
            }
            consumed = true;
            break;
        case InputKeyRight:
            if(event->type == InputTypeLong) {
                bool do_delete_cb = false;
                with_view_model(
                    receiver->view,
                    ProtoPirateReceiverModel * model,
                    { do_delete_cb = protopirate_view_receiver_item_count(model) > 0; },
                    false);
                if(do_delete_cb && receiver->callback) {
                    receiver->callback(
                        ProtoPirateCustomEventViewReceiverDeleteItem, receiver->context);
                }
            }
            consumed = true;
            break;
        case InputKeyOk:
            bool do_ok_cb = false;
            bool do_toggle = false;
            /* Read-only: do not redraw */
            with_view_model(
                receiver->view,
                ProtoPirateReceiverModel * model,
                {
                    size_t item_count = protopirate_view_receiver_item_count(model);

                    if(item_count > 0) {
                        do_ok_cb = true;
                    } else if(event->type == InputTypeLong) {
                        do_toggle = true;
                    }
                },
                false);
            /* Only redraw if we actually changed dolphin_view */
            if(do_toggle) {
                with_view_model(
                    receiver->view,
                    ProtoPirateReceiverModel * model,
                    { model->dolphin_view = !model->dolphin_view; },
                    true);
            }

            if(do_ok_cb && receiver->callback) {
                receiver->callback(ProtoPirateCustomEventViewReceiverOK, receiver->context);
            }

            consumed = true;
            break;
        case InputKeyBack:
            if(receiver->callback) {
                receiver->callback(ProtoPirateCustomEventViewReceiverBack, receiver->context);
            }
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void protopirate_view_receiver_enter(void* context) {
    furi_check(context);
    UNUSED(context);
}

void protopirate_view_receiver_exit(void* context) {
    furi_check(context);
    UNUSED(context);
}

ProtoPirateReceiver* protopirate_view_receiver_alloc(bool auto_save) {
    ProtoPirateReceiver* receiver = malloc(sizeof(ProtoPirateReceiver));

    receiver->view = view_alloc();
    view_allocate_model(receiver->view, ViewModelTypeLocking, sizeof(ProtoPirateReceiverModel));
    view_set_context(receiver->view, receiver);
    view_set_draw_callback(receiver->view, (ViewDrawCallback)protopirate_view_receiver_draw);
    view_set_input_callback(receiver->view, protopirate_view_receiver_input);
    view_set_enter_callback(receiver->view, protopirate_view_receiver_enter);
    view_set_exit_callback(receiver->view, protopirate_view_receiver_exit);

    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            model->history = NULL;
            model->frequency_str = furi_string_alloc();
            model->preset_str = furi_string_alloc();
            model->history_stat_str = furi_string_alloc();
            model->draw_scratch = furi_string_alloc();
            furi_check(model->draw_scratch);
            model->list_offset = 0;
            model->history_item = 0;
            model->rssi = -127.0f;
            model->external_radio = false;
            model->lock = ProtoPirateLockOff;
            model->lock_count = 0;
            model->auto_save = auto_save;
            model->animation_frame = 0;
            model->dolphin_view = false;
            model->sub_decode_mode = false;
        },
        true);

    return receiver;
}

void protopirate_view_receiver_free(ProtoPirateReceiver* receiver) {
    furi_check(receiver);

    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            furi_string_free(model->frequency_str);
            furi_string_free(model->preset_str);
            furi_string_free(model->history_stat_str);
            furi_string_free(model->draw_scratch);
        },
        false);

    view_free(receiver->view);
    free(receiver);
}

void protopirate_view_receiver_reset_menu(ProtoPirateReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            model->history = NULL;
            model->history_item = 0;
            model->list_offset = 0;
        },
        false);
}

void protopirate_view_receiver_sync_menu_from_history(
    ProtoPirateReceiver* receiver,
    ProtoPirateHistory* history) {
    furi_check(receiver);
    furi_check(history);

    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            model->history = history;
            size_t item_count = protopirate_view_receiver_item_count(model);
            if(item_count == 0) {
                model->history_item = 0;
                model->list_offset = 0;
            } else {
                if(model->history_item >= item_count) {
                    model->history_item = item_count - 1;
                }
                if(model->list_offset >= item_count) {
                    model->list_offset = item_count - 1;
                }
            }
        },
        true);
    protopirate_view_receiver_update_offset(receiver);
}

void protopirate_view_receiver_pop_first_menu_item(ProtoPirateReceiver* receiver) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            size_t item_count = protopirate_view_receiver_item_count(model);
            if(item_count > 0) {
                if(model->history_item > 0) {
                    model->history_item--;
                }
                if(model->list_offset > 0 && model->list_offset >= item_count) {
                    model->list_offset = item_count > 0 ? item_count - 1 : 0;
                }
            }
        },
        true);
    protopirate_view_receiver_update_offset(receiver);
}

void protopirate_view_receiver_delete_item(ProtoPirateReceiver* receiver, uint16_t idx) {
    furi_check(receiver);

    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            size_t item_count = protopirate_view_receiver_item_count(model);
            if(idx <= item_count) {
                if(item_count == 0) {
                    model->history = NULL;
                    model->history_item = 0;
                    model->list_offset = 0;
                } else {
                    if(model->history_item > idx || model->history_item >= item_count) {
                        model->history_item--;
                    }
                    if(model->history_item >= item_count) {
                        model->history_item = item_count - 1;
                    }
                    if(model->list_offset >= item_count) {
                        model->list_offset = item_count - 1;
                    }
                }
            }
        },
        true);
    protopirate_view_receiver_update_offset(receiver);
}

void protopirate_view_receiver_append_menu_row_from_history(
    ProtoPirateReceiver* receiver,
    ProtoPirateHistory* history,
    uint16_t idx) {
    furi_check(receiver);
    furi_check(history);
    UNUSED(idx);
    protopirate_view_receiver_sync_menu_from_history(receiver, history);
}

View* protopirate_view_receiver_get_view(ProtoPirateReceiver* receiver) {
    furi_check(receiver);
    return receiver->view;
}

uint16_t protopirate_view_receiver_get_idx_menu(ProtoPirateReceiver* receiver) {
    furi_check(receiver);
    uint16_t idx = 0;
    with_view_model(
        receiver->view, ProtoPirateReceiverModel * model, { idx = model->history_item; }, false);
    return idx;
}

void protopirate_view_receiver_set_idx_menu(ProtoPirateReceiver* receiver, uint16_t idx) {
    furi_check(receiver);
    with_view_model(
        receiver->view,
        ProtoPirateReceiverModel * model,
        {
            model->history_item = idx;
            size_t item_count = protopirate_view_receiver_item_count(model);
            if(model->history_item >= item_count) {
                model->history_item = item_count > 0 ? item_count - 1 : 0;
            }
        },
        true);
    protopirate_view_receiver_update_offset(receiver);
}
