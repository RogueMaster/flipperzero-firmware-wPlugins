/*
 * i2c_tools_cli — Flipper Zero I2C tools (GUI + serial CLI).
 *
 * Forked from NaejEL/flipperzero-i2ctools (https://github.com/NaejEL/flipperzero-i2ctools)
 * Copyright (C) 2023 NaejEL — original GUI / scanner / sender / sniffer code.
 * This fork adds:
 *   - N-byte read on the sender view (Long Left/Right adjust Len in READ mode);
 *   - WRITE mode toggled with Long OK (Long Left/Right then adjusts the byte to write);
 *   - HEX/ASCII display toggle with Long Back, paginated by Long Up/Down once a read
 *     result is shown;
 *   - serial CLI `i2c <scan|probe|read|write>` (see i2c_cli.c).
 *
 * Distributed under the GNU General Public License v3.0 — see LICENSE.
 */

#include "i2c_tools_cli_i.h"
#include "i2c_cli.h"

#define SCROLL_STEP 4 // bytes per long Up/Down step while viewing a read result

void i2ctools_draw_callback(Canvas* canvas, void* ctx) {
    i2cTools* i2ctools = ctx;
    if(furi_mutex_acquire(i2ctools->mutex, 200) != FuriStatusOk) {
        return;
    }

    switch(i2ctools->main_view->current_view) {
    case MAIN_VIEW:
        draw_main_view(canvas, i2ctools->main_view);
        break;

    case SCAN_VIEW:
        draw_scanner_view(canvas, i2ctools->scanner);
        break;

    case SNIFF_VIEW:
        draw_sniffer_view(canvas, i2ctools->sniffer);
        break;

    case SEND_VIEW:
        draw_sender_view(canvas, i2ctools->sender);
        break;

    case INFOS_VIEW:
        draw_infos_view(canvas, i2ctools->infos);
        break;

    default:
        break;
    }
    furi_mutex_release(i2ctools->mutex);
}

void i2ctools_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t i2c_tools_cli_app(void* p) {
    UNUSED(p);

    i2c_cli_register();

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    i2cTools* i2ctools = malloc(sizeof(i2cTools));
    i2ctools->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    i2ctools->view_port = view_port_alloc();
    view_port_draw_callback_set(i2ctools->view_port, i2ctools_draw_callback, i2ctools);
    view_port_input_callback_set(i2ctools->view_port, i2ctools_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, i2ctools->view_port, GuiLayerFullscreen);

    InputEvent event;

    i2ctools->main_view = i2c_main_view_alloc();
    i2ctools->sniffer = i2c_sniffer_alloc();
    i2ctools->sniffer->menu_index = 0;
    i2ctools->scanner = i2c_scanner_alloc();
    i2ctools->sender = i2c_sender_alloc();
    i2ctools->sender->scanner = i2ctools->scanner;
    i2ctools->infos = i2c_infos_alloc();

    while(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
        i2cToolsViews v = i2ctools->main_view->current_view;
        bool is_long = (event.type == InputTypeLong || event.type == InputTypeRepeat);

        // Back: short -> return to main menu; long (sender only) -> toggle HEX/ASCII
        if(event.key == InputKeyBack) {
            if(event.type == InputTypeShort) {
                if(v == MAIN_VIEW) {
                    break;
                } else {
                    if(v == SNIFF_VIEW) {
                        stop_interrupts();
                        i2ctools->sniffer->started = false;
                        i2ctools->sniffer->state = I2C_BUS_FREE;
                    }
                    i2ctools->main_view->current_view = MAIN_VIEW;
                }
            } else if(event.type == InputTypeLong && v == SEND_VIEW) {
                i2ctools->sender->display =
                    (i2ctools->sender->display == DISPLAY_HEX) ? DISPLAY_ASCII : DISPLAY_HEX;
            }
        }
        // Up
        else if(event.key == InputKeyUp && event.type == InputTypeRelease) {
            if(v == MAIN_VIEW) {
                if(i2ctools->main_view->menu_index > SCAN_VIEW) {
                    i2ctools->main_view->menu_index--;
                }
            } else if(v == SCAN_VIEW) {
                if(i2ctools->scanner->menu_index > 0) {
                    i2ctools->scanner->menu_index--;
                }
            } else if(v == SNIFF_VIEW) {
                if(i2ctools->sniffer->row_index > 0) {
                    i2ctools->sniffer->row_index--;
                }
            } else if(v == SEND_VIEW) {
                if(i2ctools->sender->value < 0xFF) {
                    i2ctools->sender->value++;
                    i2ctools->sender->sended = false;
                }
            }
        } else if(event.key == InputKeyUp && is_long) {
            if(v == SCAN_VIEW) {
                if(i2ctools->scanner->menu_index > 5) {
                    i2ctools->scanner->menu_index -= 5;
                }
            } else if(v == SNIFF_VIEW) {
                if(i2ctools->sniffer->row_index > 5) {
                    i2ctools->sniffer->row_index -= 5;
                } else {
                    i2ctools->sniffer->row_index = 0;
                }
            } else if(v == SEND_VIEW) {
                if(i2ctools->sender->sended && i2ctools->sender->mode == SENDER_MODE_READ &&
                   !i2ctools->sender->error) {
                    if(i2ctools->sender->recv_scroll >= SCROLL_STEP) {
                        i2ctools->sender->recv_scroll -= SCROLL_STEP;
                    } else {
                        i2ctools->sender->recv_scroll = 0;
                    }
                } else {
                    if(i2ctools->sender->value < 0xF9) {
                        i2ctools->sender->value += 5;
                        i2ctools->sender->sended = false;
                    }
                }
            }
        }
        // Down
        else if(event.key == InputKeyDown && event.type == InputTypeRelease) {
            if(v == MAIN_VIEW) {
                if(i2ctools->main_view->menu_index < MENU_SIZE - 1) {
                    i2ctools->main_view->menu_index++;
                }
            } else if(v == SCAN_VIEW) {
                if(i2ctools->scanner->menu_index < ((int)i2ctools->scanner->nb_found / 3)) {
                    i2ctools->scanner->menu_index++;
                }
            } else if(v == SNIFF_VIEW) {
                if((i2ctools->sniffer->row_index + 3) <
                   (int)i2ctools->sniffer->frames[i2ctools->sniffer->menu_index].data_index) {
                    i2ctools->sniffer->row_index++;
                }
            } else if(v == SEND_VIEW) {
                if(i2ctools->sender->value > 0x00) {
                    i2ctools->sender->value--;
                    i2ctools->sender->sended = false;
                }
            }
        } else if(event.key == InputKeyDown && is_long) {
            if(v == SNIFF_VIEW) {
                if((i2ctools->sniffer->row_index + 8) <
                   (int)i2ctools->sniffer->frames[i2ctools->sniffer->menu_index].data_index) {
                    i2ctools->sniffer->row_index += 5;
                }
            } else if(v == SEND_VIEW) {
                if(i2ctools->sender->sended && i2ctools->sender->mode == SENDER_MODE_READ &&
                   !i2ctools->sender->error) {
                    uint8_t len = i2ctools->sender->read_len;
                    uint8_t max_scroll = (len > SCROLL_STEP * 2) ? (len - SCROLL_STEP * 2) : 0;
                    if(i2ctools->sender->recv_scroll + SCROLL_STEP <= max_scroll) {
                        i2ctools->sender->recv_scroll += SCROLL_STEP;
                    } else {
                        i2ctools->sender->recv_scroll = max_scroll;
                    }
                } else {
                    if(i2ctools->sender->value > 0x05) {
                        i2ctools->sender->value -= 5;
                        i2ctools->sender->sended = false;
                    } else {
                        i2ctools->sender->value = 0;
                        i2ctools->sender->sended = false;
                    }
                }
            }
        }
        // OK
        else if(event.key == InputKeyOk && event.type == InputTypeRelease) {
            if(v == MAIN_VIEW) {
                i2ctools->main_view->current_view = i2ctools->main_view->menu_index;
            } else if(v == SCAN_VIEW) {
                scan_i2c_bus(i2ctools->scanner);
            } else if(v == SEND_VIEW) {
                i2ctools->sender->must_send = true;
            } else if(v == SNIFF_VIEW) {
                if(i2ctools->sniffer->started) {
                    stop_interrupts();
                    i2ctools->sniffer->started = false;
                    i2ctools->sniffer->state = I2C_BUS_FREE;
                } else {
                    start_interrupts(i2ctools->sniffer);
                    i2ctools->sniffer->started = true;
                    i2ctools->sniffer->state = I2C_BUS_FREE;
                }
            }
        } else if(event.key == InputKeyOk && event.type == InputTypeLong) {
            if(v == SEND_VIEW) {
                i2ctools->sender->mode = (i2ctools->sender->mode == SENDER_MODE_READ) ?
                                             SENDER_MODE_WRITE :
                                             SENDER_MODE_READ;
                i2ctools->sender->sended = false;
                i2ctools->sender->recv_scroll = 0;
            }
        }
        // Right
        else if(event.key == InputKeyRight && event.type == InputTypeRelease) {
            if(v == SEND_VIEW) {
                if(i2ctools->sender->address_idx < (i2ctools->scanner->nb_found - 1)) {
                    i2ctools->sender->address_idx++;
                    i2ctools->sender->sended = false;
                }
            } else if(v == SNIFF_VIEW) {
                if(i2ctools->sniffer->menu_index < i2ctools->sniffer->frame_index) {
                    i2ctools->sniffer->menu_index++;
                    i2ctools->sniffer->row_index = 0;
                }
            } else if(v == INFOS_VIEW) {
                if(i2ctools->infos->page + 1 < INFOS_PAGES_COUNT) {
                    i2ctools->infos->page++;
                }
            }
        } else if(event.key == InputKeyRight && is_long) {
            if(v == SEND_VIEW) {
                if(i2ctools->sender->mode == SENDER_MODE_READ) {
                    if(i2ctools->sender->read_len < I2C_MAX_READ_LEN) {
                        i2ctools->sender->read_len++;
                        i2ctools->sender->sended = false;
                    }
                } else {
                    if(i2ctools->sender->write_data < 0xFF) {
                        i2ctools->sender->write_data++;
                        i2ctools->sender->sended = false;
                    }
                }
            }
        }
        // Left
        else if(event.key == InputKeyLeft && event.type == InputTypeRelease) {
            if(v == SEND_VIEW) {
                if(i2ctools->sender->address_idx > 0) {
                    i2ctools->sender->address_idx--;
                    i2ctools->sender->sended = false;
                }
            } else if(v == SNIFF_VIEW) {
                if(i2ctools->sniffer->menu_index > 0) {
                    i2ctools->sniffer->menu_index--;
                    i2ctools->sniffer->row_index = 0;
                }
            } else if(v == INFOS_VIEW) {
                if(i2ctools->infos->page > 0) {
                    i2ctools->infos->page--;
                }
            }
        } else if(event.key == InputKeyLeft && is_long) {
            if(v == SEND_VIEW) {
                if(i2ctools->sender->mode == SENDER_MODE_READ) {
                    if(i2ctools->sender->read_len > I2C_MIN_READ_LEN) {
                        i2ctools->sender->read_len--;
                        i2ctools->sender->sended = false;
                    }
                } else {
                    if(i2ctools->sender->write_data > 0x00) {
                        i2ctools->sender->write_data--;
                        i2ctools->sender->sended = false;
                    }
                }
            }
        }
        view_port_update(i2ctools->view_port);
    }
    gui_remove_view_port(gui, i2ctools->view_port);
    view_port_free(i2ctools->view_port);
    furi_message_queue_free(event_queue);
    i2c_sniffer_free(i2ctools->sniffer);
    i2c_scanner_free(i2ctools->scanner);
    i2c_sender_free(i2ctools->sender);
    i2c_infos_free(i2ctools->infos);
    i2c_main_view_free(i2ctools->main_view);
    furi_mutex_free(i2ctools->mutex);
    free(i2ctools);
    furi_record_close(RECORD_GUI);

    i2c_cli_unregister();

    return 0;
}
