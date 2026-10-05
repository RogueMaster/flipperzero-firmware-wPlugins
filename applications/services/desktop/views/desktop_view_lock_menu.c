#include <furi.h>
#include <dolphin/dolphin.h>
#include <gui/elements.h>
#include <assets_icons.h>
#include <cfw/cfw.h>
#include <furi_hal_rtc.h>

#include "../desktop_i.h"
#include "desktop_view_lock_menu.h"

static const NotificationSequence sequence_note_c = {
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

typedef enum {
    DesktopLockMenuIndexLefthandedMode,
    DesktopLockMenuIndexGameMode,
    DesktopLockMenuIndexDarkMode,
    DesktopLockMenuIndexLock,
    DesktopLockMenuIndexBluetooth,
    DesktopLockMenuIndexCFW,
    DesktopLockMenuIndexBrightness,
    DesktopLockMenuIndexVolume,

    DesktopLockMenuIndexTotalCount
} DesktopLockMenuIndex;

static void desktop_lock_menu_draw_list(Canvas* canvas, DesktopLockMenuViewModel* m) {
    static const char* const labels[DesktopLockMenuIndexTotalCount] = {
        "Left-handed",
        "Game Mode",
        "Dark Mode",
        "Lock",
        "Bluetooth",
        "CFW Settings",
        "Brightness",
        "Volume",
    };
    canvas_set_font(canvas, FontSecondary);
    size_t first = (m->idx / 4) * 4;
    for(size_t row = 0; row < 4; row++) {
        size_t i = first + row;
        if(i >= DesktopLockMenuIndexTotalCount) break;
        uint8_t y = row * 16;
        if(i == m->idx) {
            canvas_draw_rbox(canvas, 0, y, 123, 16, 3);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y + 12, labels[i]);
        const char* state = NULL;
        char percent[8];
        switch(i) {
        case DesktopLockMenuIndexLefthandedMode:
            state = furi_hal_rtc_is_flag_set(FuriHalRtcFlagHandOrient) ? "ON" : "OFF";
            break;
        case DesktopLockMenuIndexGameMode:
            state = cfw_settings.game_mode ? "ON" : "OFF";
            break;
        case DesktopLockMenuIndexDarkMode:
            state = cfw_settings.dark_mode ? "ON" : "OFF";
            break;
        case DesktopLockMenuIndexBluetooth:
            state = m->lock_menu->bt->bt_settings.enabled ? "ON" : "OFF";
            break;
        case DesktopLockMenuIndexBrightness:
        case DesktopLockMenuIndexVolume:
            snprintf(
                percent,
                sizeof(percent),
                "%u%%",
                (unsigned)((i == DesktopLockMenuIndexBrightness ?
                                m->lock_menu->notification->settings.display_brightness :
                                m->lock_menu->notification->settings.speaker_volume) *
                               100.0f +
                           0.5f));
            state = i == DesktopLockMenuIndexVolume && m->stealth_mode ? "Mute" : percent;
            break;
        default:
            break;
        }
        if(state) canvas_draw_str_aligned(canvas, 119, y + 12, AlignRight, AlignBottom, state);
        canvas_set_color(canvas, ColorBlack);
    }
    elements_scrollbar(canvas, m->idx, DesktopLockMenuIndexTotalCount);
}

void desktop_lock_menu_set_callback(
    DesktopLockMenuView* lock_menu,
    DesktopLockMenuViewCallback callback,
    void* context) {
    furi_assert(lock_menu);
    furi_assert(callback);
    lock_menu->callback = callback;
    lock_menu->context = context;
}

void desktop_lock_menu_set_pin_state(DesktopLockMenuView* lock_menu, bool pin_is_set) {
    with_view_model(
        lock_menu->view,
        DesktopLockMenuViewModel * model,
        {
            model->pin_is_set = pin_is_set;
            model->lock_popup_index = pin_is_set; // Select with PIN by default if set
        },
        true);
}

void desktop_lock_menu_set_stealth_mode_state(DesktopLockMenuView* lock_menu, bool stealth_mode) {
    with_view_model(
        lock_menu->view,
        DesktopLockMenuViewModel * model,
        { model->stealth_mode = stealth_mode; },
        true);
}

void desktop_lock_menu_set_idx(DesktopLockMenuView* lock_menu, uint8_t idx) {
    furi_assert(idx < DesktopLockMenuIndexTotalCount);
    with_view_model(
        lock_menu->view, DesktopLockMenuViewModel * model, { model->idx = idx; }, true);
}

void desktop_lock_menu_draw_callback(Canvas* canvas, void* model) {
    DesktopLockMenuViewModel* m = model;

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontBatteryPercent);

    int8_t x, y, w, h;
    bool selected, toggle;
    bool enabled = false;
    uint8_t value = 0;
    int8_t total = 58;
    const Icon* icon = NULL;
    if(!cfw_settings.lock_menu_type) {
        desktop_lock_menu_draw_list(canvas, m);
    } else
        for(size_t i = 0; i < DesktopLockMenuIndexTotalCount; ++i) {
            selected = m->idx == i;
            toggle = i < 6;
            if(toggle) {
                x = 2 + 32 * (i / 2);
                y = 2 + 32 * (i % 2);
                w = 28;
                h = 28;
                enabled = false;
            } else {
                x = 98 + 16 * (i % 2);
                y = 2;
                w = 12;
                h = 60;
                value = 0;
            }

            switch(i) {
            case DesktopLockMenuIndexLefthandedMode:
                icon = &I_CC_LefthandedMode_16x16;
                enabled = furi_hal_rtc_is_flag_set(FuriHalRtcFlagHandOrient);
                break;
            case DesktopLockMenuIndexGameMode:
                icon = &I_CC_GameMode_16x16;
                enabled = cfw_settings.game_mode;
                break;
            case DesktopLockMenuIndexDarkMode:
                icon = &I_CC_DarkMode_16x16;
                enabled = cfw_settings.dark_mode;
                break;
            case DesktopLockMenuIndexLock:
                icon = &I_CC_Lock_16x16;
                break;
            case DesktopLockMenuIndexBluetooth:
                icon = &I_CC_Bluetooth_16x16;
                enabled = m->lock_menu->bt->bt_settings.enabled;
                break;
            case DesktopLockMenuIndexCFW:
                icon = &I_CC_CFW_16x16;
                break;
            case DesktopLockMenuIndexBrightness:
                icon = &I_Pin_star_7x7;
                value = total - m->lock_menu->notification->settings.display_brightness * total;
                break;
            case DesktopLockMenuIndexVolume:
                icon = m->stealth_mode ? &I_Muted_8x8 : &I_Volup_8x6;
                value = total - m->lock_menu->notification->settings.speaker_volume * total;
                break;
            default:
                break;
            }

            if(selected) {
                elements_bold_rounded_frame(canvas, x - 1, y - 1, w + 1, h + 1);
            } else {
                canvas_draw_rframe(canvas, x, y, w, h, 5);
            }

            if(toggle) {
                if(enabled) {
                    canvas_draw_rbox(canvas, x, y, w, h, 5);
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_icon(
                    canvas,
                    x + (w - icon_get_width(icon)) / 2,
                    y + (h - icon_get_height(icon)) / 2,
                    icon);
                if(enabled) {
                    canvas_set_color(canvas, ColorBlack);
                }
            } else {
                canvas_draw_icon(
                    canvas,
                    x + (w - icon_get_width(icon)) / 2,
                    y + (h - icon_get_height(icon)) / 2,
                    icon);
                canvas_set_color(canvas, ColorXOR);
                canvas_draw_box(canvas, x + 1, y + 1 + value, w - 2, h - 2 - value);
                if(selected) {
                    canvas_set_color(canvas, ColorBlack);
                } else {
                    canvas_set_color(canvas, ColorWhite);
                }
                canvas_draw_dot(canvas, x + 1, y + 1);
                canvas_draw_dot(canvas, x + 1, y + h - 2);
                canvas_draw_dot(canvas, x + w - 2, y + 1);
                canvas_draw_dot(canvas, x + w - 2, y + h - 2);
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_rframe(canvas, x, y, w, h, 5);
            }
        }

    if(m->show_lock_popup) {
        if(cfw_settings.popup_overlay) {
            canvas_draw_overlay(canvas);
        }
        canvas_set_font(canvas, FontSecondary);
        elements_bold_rounded_frame(canvas, 24, 4, 80, 56);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Keypad Lock");
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "PIN Code Lock");
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "PIN Lock + OFF");
        elements_frame(canvas, 28, 8 + m->lock_popup_index * 16, 72, 15);
    }
}

View* desktop_lock_menu_get_view(DesktopLockMenuView* lock_menu) {
    furi_assert(lock_menu);
    return lock_menu->view;
}

bool desktop_lock_menu_input_callback(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    DesktopLockMenuView* lock_menu = context;
    uint8_t idx = 0;
    bool show_lock_popup = false;
    DesktopLockMenuPopupIndex lock_popup_index = 0;
    bool stealth_mode = false;
    bool consumed = true;

    with_view_model(
        lock_menu->view,
        DesktopLockMenuViewModel * model,
        {
            show_lock_popup = model->show_lock_popup;
            stealth_mode = model->stealth_mode;
            if((event->type == InputTypeShort) || (event->type == InputTypeRepeat)) {
                if(model->show_lock_popup) {
                    if(event->key == InputKeyUp) {
                        if(model->lock_popup_index == 0) {
                            model->lock_popup_index = DesktopLockMenuPopupIndexMAX - 1;
                        } else {
                            model->lock_popup_index--;
                        }
                    } else if(event->key == InputKeyDown) {
                        if(model->lock_popup_index == DesktopLockMenuPopupIndexMAX - 1) {
                            model->lock_popup_index = 0;
                        } else {
                            model->lock_popup_index++;
                        }
                    } else if(event->key == InputKeyBack || event->key == InputKeyOk) {
                        model->show_lock_popup = false;
                    }
                } else {
                    if(model->idx == DesktopLockMenuIndexLock && event->key == InputKeyOk) {
                        model->show_lock_popup = true;
                    } else if(!cfw_settings.lock_menu_type) {
                        if(event->key == InputKeyUp) {
                            model->idx = model->idx ? model->idx - 1 :
                                                      DesktopLockMenuIndexTotalCount - 1;
                        } else if(event->key == InputKeyDown) {
                            model->idx = (model->idx + 1) % DesktopLockMenuIndexTotalCount;
                        }
                    } else if(model->idx < 6) {
                        if(event->key == InputKeyUp || event->key == InputKeyDown) {
                            if(model->idx % 2) {
                                model->idx--;
                            } else {
                                model->idx++;
                            }
                        } else if(event->key == InputKeyLeft) {
                            if(model->idx < 2) {
                                model->idx = 7;
                            } else {
                                model->idx -= 2;
                            }
                        } else if(event->key == InputKeyRight) {
                            if(model->idx >= 4) {
                                model->idx = 6;
                            } else {
                                model->idx += 2;
                            }
                        }
                    } else {
                        if(event->key == InputKeyLeft) {
                            model->idx--;
                        } else if(event->key == InputKeyRight) {
                            if(model->idx >= 7) {
                                model->idx = 1;
                            } else {
                                model->idx++;
                            }
                        }
                    }
                }
            }
            idx = model->idx;
            lock_popup_index = model->lock_popup_index;
        },
        true);

    DesktopEvent desktop_event = 0;
    if(show_lock_popup) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            switch(lock_popup_index) {
            case DesktopLockMenuPopupIndexKeypad:
                desktop_event = DesktopLockMenuEventLockKeypad;
                break;
            case DesktopLockMenuPopupIndexPinCode:
                desktop_event = DesktopLockMenuEventLockPinCode;
                break;
            case DesktopLockMenuPopupIndexPinOff:
                desktop_event = DesktopLockMenuEventLockPinOff;
                break;
            default:
                break;
            }
        }
    } else {
        if(event->key == InputKeyBack) {
            consumed = false;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            switch(idx) {
            case DesktopLockMenuIndexLefthandedMode:
                if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagHandOrient)) {
                    furi_hal_rtc_reset_flag(FuriHalRtcFlagHandOrient);
                } else {
                    furi_hal_rtc_set_flag(FuriHalRtcFlagHandOrient);
                }
                break;
            case DesktopLockMenuIndexGameMode:
                if(!cfw_settings.game_mode) {
                    dolphin_deed(getRandomDeed());
                    cfw_settings.game_mode = true;
                    lock_menu->save_cfw = true;
                }
                break;
            case DesktopLockMenuIndexDarkMode:
                cfw_settings.dark_mode = !cfw_settings.dark_mode;
                lock_menu->save_cfw = true;
                break;
            case DesktopLockMenuIndexBluetooth:
                lock_menu->bt->bt_settings.enabled = !lock_menu->bt->bt_settings.enabled;
                if(lock_menu->bt->bt_settings.enabled) {
                    furi_hal_bt_start_advertising();
                } else {
                    furi_hal_bt_stop_advertising();
                }
                lock_menu->save_bt = true;
                break;
            case DesktopLockMenuIndexCFW:
                desktop_event = DesktopLockMenuEventCFW;
                break;
            case DesktopLockMenuIndexBrightness:
                desktop_event = DesktopLockMenuEventScreenSettings;
                break;
            case DesktopLockMenuIndexVolume:
                desktop_event = stealth_mode ? DesktopLockMenuEventStealthModeOff :
                                               DesktopLockMenuEventStealthModeOn;
                break;
            default:
                break;
            }
        } else if(idx >= 6 && (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
            int8_t offset = 0;
            if(event->key == (cfw_settings.lock_menu_type ? InputKeyUp : InputKeyRight)) {
                offset = 1;
            } else if(event->key == (cfw_settings.lock_menu_type ? InputKeyDown : InputKeyLeft)) {
                offset = -1;
            }
            if(offset) {
                float value;
                switch(idx) {
                case DesktopLockMenuIndexBrightness:
                    value = lock_menu->notification->settings.display_brightness + 0.05 * offset;
                    lock_menu->notification->settings.display_brightness =
                        value < 0.00f ? 0.00f : (value > 1.00f ? 1.00f : value);
                    lock_menu->save_notification = true;
                    notification_message(lock_menu->notification, &sequence_display_backlight_on);
                    break;
                case DesktopLockMenuIndexVolume:
                    value = lock_menu->notification->settings.speaker_volume + 0.05 * offset;
                    lock_menu->notification->settings.speaker_volume =
                        value < 0.00f ? 0.00f : (value > 1.00f ? 1.00f : value);
                    lock_menu->save_notification = true;
                    notification_message(lock_menu->notification, &sequence_note_c);
                    break;
                default:
                    break;
                }
            }
        }
    }
    if(desktop_event) {
        lock_menu->callback(desktop_event, lock_menu->context);
    }

    // RM uses short left/right to move between its existing tiles. Hold right
    // to open the additional brightness/volume/vibro page without changing that navigation.
    if(!show_lock_popup && event->key == InputKeyRight && event->type == InputTypeLong) {
        lock_menu->callback(DesktopLockMenuEventOpenQuickSettings, lock_menu->context);
        consumed = true;
    }

    return consumed;
}

DesktopLockMenuView* desktop_lock_menu_alloc(void) {
    DesktopLockMenuView* lock_menu = malloc(sizeof(DesktopLockMenuView));
    lock_menu->bt = furi_record_open(RECORD_BT);
    lock_menu->notification = furi_record_open(RECORD_NOTIFICATION);
    lock_menu->view = view_alloc();
    view_allocate_model(lock_menu->view, ViewModelTypeLocking, sizeof(DesktopLockMenuViewModel));
    with_view_model(
        lock_menu->view,
        DesktopLockMenuViewModel * model,
        { model->lock_menu = lock_menu; },
        false);
    view_set_context(lock_menu->view, lock_menu);
    view_set_draw_callback(lock_menu->view, (ViewDrawCallback)desktop_lock_menu_draw_callback);
    view_set_input_callback(lock_menu->view, desktop_lock_menu_input_callback);

    return lock_menu;
}

void desktop_lock_menu_free(DesktopLockMenuView* lock_menu_view) {
    furi_assert(lock_menu_view);

    view_free(lock_menu_view->view);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_BT);
    free(lock_menu_view);
}
