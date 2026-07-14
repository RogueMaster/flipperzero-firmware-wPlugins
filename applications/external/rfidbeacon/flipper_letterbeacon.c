// CC0 1.0 Universal (CC0 1.0)
// Public Domain Dedication
// https://github.com/nmrr

#include <stdio.h>
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal_rfid.h>
#include <furi_hal_nfc.h>

typedef enum {
    EventTypeInput,
    ClockEventTypeTick,
    ClockEventTypeTickPause,
    EventStartTransmission,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} EventApp;

const char CW_char[54] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',  'M', 'N',
                          'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',  '0', '1',
                          '2', '3', '4', '5', '6', '7', '8', '9', '.', ',', '?', '\'', '!', '/',
                          '(', ')', '&', ':', ';', '=', '+', '-', '_', '"', '$', '@'};
const uint8_t CW_size[54] = {2, 4, 4, 3, 1, 4, 3, 4, 2, 4, 3, 4, 2, 2, 3, 4, 4, 3,
                             3, 1, 3, 4, 3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
                             6, 6, 6, 6, 6, 5, 5, 6, 5, 6, 6, 5, 5, 6, 6, 6, 7, 6};
const uint8_t CW_value[54] = {
    0b01000000, 0b10000000, 0b10100000, 0b10000000, 0b00000000, 0b00100000, 0b11000000, 0b00000000,
    0b00000000, 0b01110000, 0b10100000, 0b01000000, 0b11000000, 0b10000000, 0b11100000, 0b01100000,
    0b11010000, 0b01000000, 0b00000000, 0b10000000, 0b00100000, 0b00010000, 0b01100000, 0b10010000,
    0b10110000, 0b11000000, 0b11111000, 0b01111000, 0b00111000, 0b00011000, 0b00001000, 0b00000000,
    0b10000000, 0b11000000, 0b11100000, 0b11110000, 0b01010100, 0b11001100, 0b00110000, 0b01111000,
    0b10101100, 0b10010000, 0b10110000, 0b10110100, 0b01000000, 0b11100000, 0b10101000, 0b10001000,
    0b01010000, 0b10000100, 0b00110100, 0b01001000, 0b00010010, 0b01101000};

typedef struct {
    FuriMutex* mutex;
    uint8_t status;
    uint8_t enableCW_mutex;
    uint16_t clockTickValue;
    uint16_t pauseValue;
    uint8_t selectMenu;
    uint8_t transmissionMode;
} mutexStruct;

static void draw_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    mutexStruct* mutexVal = ctx;
    mutexStruct mutexDraw;
    furi_mutex_acquire(mutexVal->mutex, FuriWaitForever);
    memcpy(&mutexDraw, mutexVal, sizeof(mutexStruct));
    furi_mutex_release(mutexVal->mutex);

    canvas_set_font(canvas, FontPrimary);

    if(mutexDraw.transmissionMode == 0) {
        if(mutexDraw.enableCW_mutex == 0)
            canvas_draw_str_aligned(
                canvas, 64, 5, AlignCenter, AlignCenter, "RFID (125 kHz) - OFF");
        else
            canvas_draw_str_aligned(
                canvas, 64, 5, AlignCenter, AlignCenter, "RFID (125 kHz) - ON");
    } else {
        if(mutexDraw.enableCW_mutex == 0)
            canvas_draw_str_aligned(
                canvas, 64, 5, AlignCenter, AlignCenter, "NFC (13.56 MHz) - OFF");
        else
            canvas_draw_str_aligned(
                canvas, 64, 5, AlignCenter, AlignCenter, "NFC (13.56 MHz) - ON");
    }

    char buffer[32];
    uint8_t positionBuffer = 0;

    if(mutexDraw.selectMenu == 0) {
        buffer[positionBuffer++] = '>';
        buffer[positionBuffer++] = '>';
        buffer[positionBuffer++] = ' ';
        buffer[positionBuffer++] = ' ';
    }

    buffer[positionBuffer++] = CW_char[mutexDraw.status];
    buffer[positionBuffer++] = ' ';
    buffer[positionBuffer++] = ' ';
    buffer[positionBuffer++] = ' ';
    buffer[positionBuffer++] = ' ';

    uint8_t maskMorse = 0b10000000;
    for(uint8_t i = 0; i < CW_size[mutexDraw.status]; i++) {
        if(i != 0) {
            buffer[positionBuffer++] = ' ';
            maskMorse >>= 1;
        }

        if((CW_value[mutexDraw.status] & maskMorse) != 0) {
            buffer[positionBuffer++] = '-';
        } else {
            buffer[positionBuffer++] = '.';
        }
    }

    buffer[positionBuffer++] = '\0';

    if(mutexDraw.selectMenu == 0)
        canvas_draw_str_aligned(canvas, 0, 25, AlignLeft, AlignCenter, buffer);
    else
        canvas_draw_str_aligned(canvas, 16, 25, AlignLeft, AlignCenter, buffer);

    if(mutexDraw.selectMenu == 1) {
        snprintf(buffer, sizeof(buffer), ">>  Tick: %hu", mutexDraw.clockTickValue);
        canvas_draw_str_aligned(canvas, 0, 37, AlignLeft, AlignCenter, buffer);
    } else {
        snprintf(buffer, sizeof(buffer), "Tick: %hu", mutexDraw.clockTickValue);
        canvas_draw_str_aligned(canvas, 16, 37, AlignLeft, AlignCenter, buffer);
    }

    if(mutexDraw.selectMenu == 2) {
        snprintf(buffer, sizeof(buffer), ">>  Pause: %hu", mutexDraw.pauseValue);
        canvas_draw_str_aligned(canvas, 0, 49, AlignLeft, AlignCenter, buffer);
    } else {
        snprintf(buffer, sizeof(buffer), "Pause: %hu", mutexDraw.pauseValue);
        canvas_draw_str_aligned(canvas, 16, 49, AlignLeft, AlignCenter, buffer);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    EventApp event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void clock_tick(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    EventApp event = {.type = ClockEventTypeTick};
    furi_message_queue_put(queue, &event, 0);
}

static void clock_tickPause(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    EventApp event = {.type = ClockEventTypeTickPause};
    furi_message_queue_put(queue, &event, 0);
}

static uint8_t RFID_STATUS = 0;
void RFID_ON(NotificationApp* notification) {
    if(RFID_STATUS == 0) {
        notification_message(notification, &sequence_set_only_red_255);
        furi_hal_rfid_tim_read_start(125000, 0.5);
        RFID_STATUS = 1;
    }
}

void RFID_OFF(NotificationApp* notification) {
    if(RFID_STATUS == 1) {
        notification_message(notification, &sequence_reset_red);
        furi_hal_rfid_tim_read_stop();
        RFID_STATUS = 0;
    }
}

static uint8_t NFC_STATUS = 0;
void NFC_ON(NotificationApp* notification) {
    if(NFC_STATUS == 0) {
        notification_message(notification, &sequence_set_only_blue_255);
        furi_hal_nfc_low_power_mode_stop();
        furi_hal_nfc_poller_field_on();
        NFC_STATUS = 1;
    }
}

void NFC_OFF(NotificationApp* notification) {
    if(NFC_STATUS == 1) {
        notification_message(notification, &sequence_reset_blue);
        furi_hal_nfc_low_power_mode_start();
        NFC_STATUS = 0;
    }
}

int32_t flipper_letterbeacon_app() {
    EventApp event;
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(EventApp));

    mutexStruct mutexVal;
    mutexVal.status = 0;
    mutexVal.enableCW_mutex = 0;
    mutexVal.clockTickValue = 250;
    mutexVal.pauseValue = 3000;
    mutexVal.selectMenu = 0;
    mutexVal.transmissionMode = 0;

    mutexVal.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!mutexVal.mutex) {
        furi_message_queue_free(event_queue);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &mutexVal.mutex);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    FuriTimer* timer = furi_timer_alloc(clock_tick, FuriTimerTypePeriodic, event_queue);
    FuriTimer* timerPause = furi_timer_alloc(clock_tickPause, FuriTimerTypePeriodic, event_queue);

    uint8_t enableCW = 0;
    uint16_t letterState = 0;
    uint8_t letterPosition = 0;
    uint8_t letterChosen = 0;
    uint8_t lastTransmission = 0;

    uint8_t draw = 0;

    // 0 : RFID, 1 : NFC
    uint8_t transmissionMode = 0;
    uint8_t transmissionModeMenu = transmissionMode;

    furi_hal_nfc_acquire();

    while(1) {
        furi_check(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk);
        uint8_t screenRefresh = 0;

        if(event.type == EventTypeInput) {
            if(event.input.key == InputKeyBack && event.input.type == InputTypeLong) {
                break;
            } else if(event.input.key == InputKeyOk) {
                if(event.input.type == InputTypeLong) {
                    furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);

                    if(enableCW == 0) {
                        enableCW = 1;
                        transmissionMode = transmissionModeMenu;
                        EventApp eventTx = {.type = EventStartTransmission};
                        furi_message_queue_put(event_queue, &eventTx, 0);
                    } else {
                        if(transmissionMode == 0)
                            RFID_OFF(notification);
                        else if(transmissionMode == 1)
                            NFC_OFF(notification);
                        enableCW = 0;
                        furi_timer_stop(timer);
                        furi_timer_stop(timerPause);
                    }

                    mutexVal.enableCW_mutex = enableCW;
                    screenRefresh = 1;
                    furi_mutex_release(mutexVal.mutex);
                } else if(event.input.type == InputTypeShort) {
                    if(transmissionModeMenu == 0)
                        transmissionModeMenu = 1;
                    else
                        transmissionModeMenu = 0;

                    furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);
                    mutexVal.transmissionMode = transmissionModeMenu;
                    furi_mutex_release(mutexVal.mutex);

                    screenRefresh = 1;
                }
            } else if(event.input.key == InputKeyLeft && event.input.type == InputTypeShort) {
                furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);

                if(mutexVal.selectMenu == 0) {
                    if(mutexVal.status != 0)
                        mutexVal.status--;
                    else
                        mutexVal.status = 53;
                    screenRefresh = 1;
                } else if(mutexVal.selectMenu == 1) {
                    if(mutexVal.clockTickValue > 50) {
                        mutexVal.clockTickValue -= 10;
                        screenRefresh = 1;
                    }
                } else if(mutexVal.selectMenu == 2) {
                    if(mutexVal.pauseValue > 500) {
                        mutexVal.pauseValue -= 100;
                        screenRefresh = 1;
                    }
                }

                furi_mutex_release(mutexVal.mutex);
            } else if(event.input.key == InputKeyLeft && event.input.type == InputTypeLong) {
                furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);

                if(mutexVal.selectMenu == 0) {
                    if(mutexVal.status >= 10)
                        mutexVal.status -= 10;
                    else
                        mutexVal.status = 53 - 10 + mutexVal.status + 1;
                    screenRefresh = 1;
                } else if(mutexVal.selectMenu == 1) {
                    if(mutexVal.clockTickValue >= 150) {
                        mutexVal.clockTickValue -= 100;
                        screenRefresh = 1;
                    } else if(mutexVal.clockTickValue > 50) {
                        mutexVal.clockTickValue = 50;
                        screenRefresh = 1;
                    }
                } else if(mutexVal.selectMenu == 2) {
                    if(mutexVal.pauseValue >= 1500) {
                        mutexVal.pauseValue -= 1000;
                        screenRefresh = 1;
                    } else if(mutexVal.pauseValue > 500) {
                        mutexVal.pauseValue = 500;
                        screenRefresh = 1;
                    }
                }

                furi_mutex_release(mutexVal.mutex);
            } else if(event.input.key == InputKeyRight && event.input.type == InputTypeShort) {
                furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);

                if(mutexVal.selectMenu == 0) {
                    if(mutexVal.status != 53)
                        mutexVal.status++;
                    else
                        mutexVal.status = 0;
                    screenRefresh = 1;
                } else if(mutexVal.selectMenu == 1) {
                    if(mutexVal.clockTickValue < 1000) {
                        mutexVal.clockTickValue += 10;
                        screenRefresh = 1;
                    }
                } else if(mutexVal.selectMenu == 2) {
                    if(mutexVal.pauseValue < 10000) {
                        mutexVal.pauseValue += 100;
                        screenRefresh = 1;
                    }
                }

                furi_mutex_release(mutexVal.mutex);
            } else if(event.input.key == InputKeyRight && event.input.type == InputTypeLong) {
                furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);
                if(mutexVal.selectMenu == 0) {
                    if(mutexVal.status <= 43)
                        mutexVal.status += 10;
                    else
                        mutexVal.status = 10 - (53 - mutexVal.status) - 1;
                    screenRefresh = 1;
                }
                if(mutexVal.selectMenu == 1) {
                    if(mutexVal.clockTickValue <= 900) {
                        mutexVal.clockTickValue += 100;
                        screenRefresh = 1;
                    } else if(mutexVal.clockTickValue < 1000) {
                        mutexVal.clockTickValue = 1000;
                        screenRefresh = 1;
                    }
                } else if(mutexVal.selectMenu == 2) {
                    if(mutexVal.pauseValue <= 9000) {
                        mutexVal.pauseValue += 1000;
                        screenRefresh = 1;
                    } else if(mutexVal.pauseValue < 10000) {
                        mutexVal.pauseValue = 10000;
                        screenRefresh = 1;
                    }
                }

                furi_mutex_release(mutexVal.mutex);
            } else if(event.input.key == InputKeyUp && event.input.type == InputTypeShort) {
                furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);

                if(mutexVal.selectMenu != 0) {
                    mutexVal.selectMenu--;
                    screenRefresh = 1;
                }

                furi_mutex_release(mutexVal.mutex);

            } else if(event.input.key == InputKeyDown && event.input.type == InputTypeShort) {
                furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);
                if(mutexVal.selectMenu != 2) {
                    mutexVal.selectMenu++;
                    screenRefresh = 1;
                }
                furi_mutex_release(mutexVal.mutex);
            }
        } else if(event.type == ClockEventTypeTick || event.type == EventStartTransmission) {
            if(event.type == EventStartTransmission) {
                draw = 0;
                letterState = 0;
                letterPosition = 0;
                lastTransmission = 0;
                furi_mutex_acquire(mutexVal.mutex, FuriWaitForever);
                letterChosen = mutexVal.status;
                furi_mutex_release(mutexVal.mutex);
                furi_timer_start(timer, mutexVal.clockTickValue);
            }

            if(draw == 0) {
                if(letterPosition == CW_size[letterChosen] - 1) {
                    lastTransmission = 1;
                }

                uint8_t mask = 0b10000000;
                mask >>= letterPosition;
                if((CW_value[letterChosen] & mask) != 0)
                    draw = 2;
                else
                    draw = 1;
                letterState = 0;
                letterPosition++;
            }

            // DOT
            if(draw == 1) {
                if(letterState == 0) {
                    if(transmissionMode == 0)
                        RFID_ON(notification);
                    else if(transmissionMode == 1)
                        NFC_ON(notification);
                    letterState = 1;
                } else {
                    if(transmissionMode == 0)
                        RFID_OFF(notification);
                    else if(transmissionMode == 1)
                        NFC_OFF(notification);
                    draw = 0;
                    letterState = 0;

                    if(lastTransmission == 1) {
                        furi_timer_stop(timer);
                        furi_timer_start(timerPause, mutexVal.pauseValue);
                    }
                }
            }
            // DASH
            else if(draw == 2) {
                if(letterState == 0) {
                    if(transmissionMode == 0)
                        RFID_ON(notification);
                    else if(transmissionMode == 1)
                        NFC_ON(notification);
                    letterState = 1;
                } else {
                    if(letterState != 3)
                        letterState++;
                    else {
                        if(transmissionMode == 0)
                            RFID_OFF(notification);
                        else if(transmissionMode == 1)
                            NFC_OFF(notification);
                        draw = 0;
                        letterState = 0;

                        if(lastTransmission == 1) {
                            furi_timer_stop(timer);
                            furi_timer_start(timerPause, mutexVal.pauseValue);
                        }
                    }
                }
            }
        } else if(event.type == ClockEventTypeTickPause) {
            furi_timer_stop(timerPause);
            transmissionMode = transmissionModeMenu;
            EventApp eventTx = {.type = EventStartTransmission};
            furi_message_queue_put(event_queue, &eventTx, 0);
        }

        if(screenRefresh == 1) view_port_update(view_port);
    }

    RFID_OFF(notification);
    NFC_OFF(notification);

    furi_hal_nfc_release();

    furi_timer_free(timer);
    furi_timer_free(timerPause);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    return 0;
}
