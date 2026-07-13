#include "jayx.h"

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    JayxApp* app = ctx;

    switch(app->ui_state) {
    case JayxUiModeSelect:
    case JayxUiWaiting:
    case JayxUiBtDev:
        draw_connect_view(canvas, app);
        break;
    case JayxUiActive:
        if(app->page == JayxPageSystem) {
            draw_system_view(canvas, app);
        } else if(app->page == JayxPageGame) {
            draw_game_view(canvas, app);
        } else if(app->page == JayxPageSpecs) {
            draw_specs_view(canvas, app);
        } else {
            draw_about_view(canvas, app);
        }
        break;
    case JayxUiLost:
        draw_status_view(canvas, app);
        break;
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, input_event, FuriWaitForever);
}

void jayx_feed_bytes(JayxApp* app, const uint8_t* data, size_t len) {
    if(!app->rx_stream || !data || len == 0) return;
    furi_stream_buffer_send(app->rx_stream, data, len, 0);
}

void jayx_copy_field(char* dst, size_t dst_len, const char* src, size_t src_len) {
    size_t n = src_len;
    if(n + 1 > dst_len) n = dst_len - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
    for(size_t i = 0; i < n; i++) {
        if((uint8_t)dst[i] < 32 || (uint8_t)dst[i] > 126) {
            if(dst[i] == '\0') break;
            dst[i] = '?';
        }
    }
    /* Trim at first embedded NUL already handled; strip trailing spaces */
    for(int i = (int)strlen(dst) - 1; i >= 0 && dst[i] == ' '; i--) {
        dst[i] = '\0';
    }
}

void jayx_specs_clamp_scroll(JayxApp* app) {
    /* specs_scroll = active section index */
    uint8_t n = app->specs_count;
    if(n > JAYX_SPEC_SECTIONS_MAX) n = JAYX_SPEC_SECTIONS_MAX;
    if(n == 0) {
        app->specs_scroll = 0;
        return;
    }
    /* Prefer a valid section */
    if(app->specs_scroll >= n) app->specs_scroll = n - 1;
    if(!app->specs[app->specs_scroll].valid) {
        for(uint8_t i = 0; i < n; i++) {
            if(app->specs[i].valid) {
                app->specs_scroll = i;
                return;
            }
        }
        app->specs_scroll = 0;
    }
}

static void jayx_merge_live(JayxApp* app, const JayxLivePacket* pkt) {
    JayxLivePacket* d = &app->data;

    d->magic = pkt->magic;
    d->version = pkt->version;
    d->msg_type = pkt->msg_type;
    d->cpu_usage = pkt->cpu_usage;
    d->cpu_temp_c = pkt->cpu_temp_c;
    d->ram_max = pkt->ram_max;
    d->ram_usage = pkt->ram_usage;
    memcpy(d->ram_unit, pkt->ram_unit, 4);

    if(pkt->gpu_usage <= 100) {
        d->gpu_usage = pkt->gpu_usage;
        d->gpu_temp_c = pkt->gpu_temp_c;
    } else if(d->gpu_usage > 100) {
        d->gpu_usage = pkt->gpu_usage;
        d->gpu_temp_c = pkt->gpu_temp_c;
    }

    if(pkt->vram_usage <= 100) {
        d->vram_usage = pkt->vram_usage;
        d->vram_max = pkt->vram_max;
        memcpy(d->vram_unit, pkt->vram_unit, 4);
    } else if(d->vram_usage > 100) {
        d->vram_usage = pkt->vram_usage;
        d->vram_max = pkt->vram_max;
        memcpy(d->vram_unit, pkt->vram_unit, 4);
    }

    uint32_t now = furi_hal_rtc_get_timestamp();
    if(pkt->fps > 0) {
        d->fps = pkt->fps;
        app->sticky_fps = pkt->fps;
        app->sticky_fps_ts = now;
    } else if(app->sticky_fps > 0 && (now - app->sticky_fps_ts) <= JAYX_FPS_STICKY_S) {
        d->fps = app->sticky_fps;
    } else {
        d->fps = 0;
        app->sticky_fps = 0;
    }

    bool name_empty = true;
    for(size_t i = 0; i < JAYX_GAME_NAME_LEN; i++) {
        if(pkt->game_name[i] != '\0') {
            name_empty = false;
            break;
        }
    }
    if(!name_empty) {
        memcpy(d->game_name, pkt->game_name, JAYX_GAME_NAME_LEN);
    }
}

static void jayx_store_specs_section(JayxApp* app, const JayxSpecsSectionPacket* pkt) {
    uint8_t idx = pkt->section_index;
    uint8_t count = pkt->section_count;
    if(count == 0 || count > JAYX_SPEC_SECTIONS_MAX) count = JAYX_SPEC_SECTIONS_MAX;
    if(idx >= JAYX_SPEC_SECTIONS_MAX || idx >= count) return;

    app->specs_count = count;
    JayxSpecSection* s = &app->specs[idx];
    jayx_copy_field(s->title, sizeof(s->title), pkt->title, JAYX_SPEC_TITLE_LEN);
    jayx_copy_field(s->line[0], sizeof(s->line[0]), pkt->line0, JAYX_SPEC_LINE_LEN);
    jayx_copy_field(s->line[1], sizeof(s->line[1]), pkt->line1, JAYX_SPEC_LINE_LEN);
    jayx_copy_field(s->line[2], sizeof(s->line[2]), pkt->line2, JAYX_SPEC_LINE_LEN);
    s->valid = true;
    app->specs_valid = true;
    jayx_specs_clamp_scroll(app);
}

static size_t jayx_packet_size_for_type(uint8_t msg_type) {
    if(msg_type == JAYX_MSG_LIVE) return JAYX_LIVE_SIZE;
    if(msg_type == JAYX_MSG_SPECS_SECTION) return JAYX_SPECS_SECTION_SIZE;
    return 0;
}

static bool jayx_try_parse_packet(JayxApp* app) {
    while(app->rx_len >= 2) {
        uint16_t magic = (uint16_t)app->rx_scratch[0] | ((uint16_t)app->rx_scratch[1] << 8);
        if(magic != JAYX_MAGIC) {
            memmove(app->rx_scratch, app->rx_scratch + 1, app->rx_len - 1);
            app->rx_len--;
            continue;
        }
        break;
    }

    if(app->rx_len < 4) return false;

    uint8_t version = app->rx_scratch[2];
    uint8_t msg_type = app->rx_scratch[3];

    if(version != JAYX_PROTO_VER) {
        memmove(app->rx_scratch, app->rx_scratch + 1, app->rx_len - 1);
        app->rx_len--;
        return true;
    }

    size_t need = jayx_packet_size_for_type(msg_type);
    if(need == 0) {
        memmove(app->rx_scratch, app->rx_scratch + 1, app->rx_len - 1);
        app->rx_len--;
        return true;
    }

    if(app->rx_len < need) return false;

    if(msg_type == JAYX_MSG_LIVE) {
        JayxLivePacket pkt;
        memcpy(&pkt, app->rx_scratch, JAYX_LIVE_SIZE);
        jayx_merge_live(app, &pkt);
        app->last_packet_ts = furi_hal_rtc_get_timestamp();
        app->ui_state = JayxUiActive;
        notification_message(app->notification, &sequence_display_backlight_on);
    } else if(msg_type == JAYX_MSG_SPECS_SECTION) {
        JayxSpecsSectionPacket pkt;
        memcpy(&pkt, app->rx_scratch, JAYX_SPECS_SECTION_SIZE);
        jayx_store_specs_section(app, &pkt);
        app->last_packet_ts = furi_hal_rtc_get_timestamp();
    }

    memmove(app->rx_scratch, app->rx_scratch + need, app->rx_len - need);
    app->rx_len -= need;
    return true;
}

void jayx_process_rx(JayxApp* app) {
    uint8_t tmp[64];
    size_t n;
    do {
        n = furi_stream_buffer_receive(app->rx_stream, tmp, sizeof(tmp), 0);
        if(n == 0) break;

        size_t space = sizeof(app->rx_scratch) - app->rx_len;
        if(n > space) n = space;
        if(n > 0) {
            memcpy(app->rx_scratch + app->rx_len, tmp, n);
            app->rx_len += n;
        }

        while(jayx_try_parse_packet(app)) {
        }
    } while(n > 0);
}

static JayxApp* jayx_alloc(void) {
    JayxApp* app = malloc(sizeof(JayxApp));
    memset(app, 0, sizeof(JayxApp));

    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    app->rx_stream = furi_stream_buffer_alloc(JAYX_RX_STREAM_SIZE, 1);

    app->ui_state = JayxUiModeSelect;
    app->select_cursor = JayxTransportUsb;
    app->transport = JayxTransportNone;
    app->page = JayxPageSystem;
    app->data.gpu_usage = 0xFF;
    app->data.vram_usage = 0xFF;
    app->data.gpu_temp_c = 0xFF;
    app->data.cpu_temp_c = 0xFF;

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    return app;
}

static void jayx_free(JayxApp* app) {
    jayx_transport_stop(app);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_stream_buffer_free(app->rx_stream);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

static void jayx_handle_input(JayxApp* app, InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return;

    if(app->ui_state == JayxUiBtDev) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->ui_state = JayxUiModeSelect;
            view_port_update(app->view_port);
        }
        return;
    }

    if(app->ui_state == JayxUiModeSelect) {
        if(event->type != InputTypeShort) return;
        if(event->key == InputKeyUp || event->key == InputKeyDown) {
            app->select_cursor = (app->select_cursor == JayxTransportUsb) ? JayxTransportBt :
                                                                           JayxTransportUsb;
            view_port_update(app->view_port);
        } else if(event->key == InputKeyOk) {
            if(app->select_cursor == JayxTransportBt) {
                /* Bluetooth not ready — show WIP message, do not start transport */
                app->ui_state = JayxUiBtDev;
            } else {
                jayx_transport_start(app, JayxTransportUsb);
            }
            view_port_update(app->view_port);
        }
        return;
    }

    if(app->ui_state == JayxUiActive || app->ui_state == JayxUiLost) {
        /* Specs: Up/Down switch section cards */
        if(app->page == JayxPageSpecs && app->ui_state == JayxUiActive && app->specs_valid) {
            uint8_t n = app->specs_count;
            if(n > JAYX_SPEC_SECTIONS_MAX) n = JAYX_SPEC_SECTIONS_MAX;
            if(n > 0 && (event->key == InputKeyUp || event->key == InputKeyDown)) {
                if(event->key == InputKeyUp) {
                    app->specs_scroll = (app->specs_scroll == 0) ? (uint8_t)(n - 1) :
                                                                  (uint8_t)(app->specs_scroll - 1);
                } else {
                    app->specs_scroll = (uint8_t)((app->specs_scroll + 1) % n);
                }
                jayx_specs_clamp_scroll(app);
                view_port_update(app->view_port);
                return;
            }
        }

        if(event->type != InputTypeShort) return;

        if(event->key == InputKeyLeft) {
            app->page = (app->page == 0) ? (JayxPage)(JAYX_PAGE_COUNT - 1) :
                                           (JayxPage)(app->page - 1);
            view_port_update(app->view_port);
        } else if(event->key == InputKeyRight) {
            app->page = (JayxPage)((app->page + 1) % JAYX_PAGE_COUNT);
            view_port_update(app->view_port);
        }
    }
}

int32_t jayx_app(void* p) {
    UNUSED(p);

    JayxApp* app = jayx_alloc();
    FURI_LOG_I(TAG, "JAYX v%s started", JAYX_VERSION);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 50) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                /* Back from BT WIP returns to menu; elsewhere exits */
                if(app->ui_state == JayxUiBtDev) {
                    jayx_handle_input(app, &event);
                } else if(app->ui_state == JayxUiModeSelect) {
                    running = false;
                    break;
                } else {
                    running = false;
                    break;
                }
            } else {
                jayx_handle_input(app, &event);
            }
        }

        if(app->rx_pending || !furi_stream_buffer_is_empty(app->rx_stream)) {
            app->rx_pending = false;
            jayx_process_rx(app);
            view_port_update(app->view_port);
        }

        if(app->ui_state == JayxUiActive) {
            uint32_t now = furi_hal_rtc_get_timestamp();
            if(now - app->last_packet_ts > JAYX_LINK_TIMEOUT_S) {
                app->ui_state = JayxUiLost;
                view_port_update(app->view_port);
            }
        }
    }

    jayx_free(app);
    return 0;
}
