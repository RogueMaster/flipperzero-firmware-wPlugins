#include "catdriver.h"
#include "catdriver_uart.h"

#include <expansion/expansion.h>

void save_batch_file(Context* ctx) {
    if(ctx->batch_count == 0) {
        return;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage_common_exists(storage, FILE_PATH)) {
        storage_common_mkdir(storage, FILE_PATH);
    }

    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, ctx->file_name, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        FURI_LOG_E(appname, "Failed to open file for append: %s", ctx->file_name);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);

        furi_hal_light_set(LightRed, 255);
        furi_delay_ms(100);
        furi_hal_light_set(LightRed, 0);
        return;
    }

    // WiGLE 1.4 Header format: written once per file
    if(!ctx->file_header_written) {
        FuriString* hdr = furi_string_alloc();
        furi_string_printf(
            hdr,
            "WigleWifi-1.4,appRelease=v1.0,model=CatDriver,release=1.0,Flipper Zero,,CatDriver,ElectronicCats\r\n"
            "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\r\n");
        storage_file_write(file, furi_string_get_cstr(hdr), furi_string_size(hdr));
        furi_string_free(hdr);
        ctx->file_header_written = true;
    }

    FuriString* string = furi_string_alloc();

    for(uint16_t i = 0; i < ctx->batch_count; i++) {
        AccessPoint* ap = &ctx->batch_buffer[i];
        const char* auth = (ap->authmode[0] != '\0') ? ap->authmode : "[ESS]";
        const char* dev_type = (ap->type[0] != '\0') ? ap->type : "WIFI";

        furi_string_printf(
            string,
            "%s,%s,%s,%04d-%02d-%02d %02d:%02d:%02d,%d,%d,%.6f,%.6f,0,0,%s\r\n",
            ap->bssid,
            ap->ssid,
            auth,
            ap->datetime.year,
            ap->datetime.month,
            ap->datetime.day,
            ap->datetime.hour,
            ap->datetime.minute,
            ap->datetime.second,
            ap->channel,
            ap->rssi,
            (double)ap->latitude,
            (double)ap->longitude,
            dev_type);

        if(!storage_file_write(file, furi_string_get_cstr(string), furi_string_size(string))) {
            FURI_LOG_E(appname, "Failed to write AP to file");
        }
    }

    furi_string_free(string);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    ctx->total_saved_count += ctx->batch_count;
    ctx->batch_count = 0;

    // Flash green LED briefly to confirm batch append
    furi_hal_light_set(LightGreen, 255);
    furi_delay_ms(40);
    furi_hal_light_set(LightGreen, 0);
}

void catdriver_start_scan_session(Context* ctx, ScanMode mode) {
    // 1. If there's an ongoing batch from a previous session, flush it first
    if(ctx->batch_count > 0 && ctx->file_name[0] != '\0') {
        save_batch_file(ctx);
    }

    // 2. Clear UI buffers and counters for this fresh session
    ctx->batch_count = 0;
    ctx->recent_count = 0;
    ctx->recent_index = 0;
    memset(ctx->recent_aps, 0, sizeof(ctx->recent_aps));
    memset(&ctx->active_access_point, 0, sizeof(AccessPoint));
    ctx->pressedButton = false;
    ctx->file_header_written = false;

    // 3. Set the mode
    ctx->scan_mode = mode;

    // 4. Generate new distinct file name with mode suffix:
    // catdriver_YYYY_MM_DD_HH_MM_SS_all.csv
    // catdriver_YYYY_MM_DD_HH_MM_SS_wifi.csv
    // catdriver_YYYY_MM_DD_HH_MM_SS_ble.csv
    const char* mode_suffix = "all";
    if(mode == ScanModeWifi) {
        mode_suffix = "wifi";
    } else if(mode == ScanModeBle) {
        mode_suffix = "ble";
    }

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(
        ctx->file_name,
        sizeof(ctx->file_name),
        "%s/catdriver_%04d_%02d_%02d_%02d_%02d_%02d_%s.csv",
        FILE_PATH,
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second,
        mode_suffix);

    // 5. Send command to ESP32
    catdriver_set_scan_mode(ctx, mode);

    // 6. Switch view
    ctx->view_state = VIEW_SCAN;
}

void catdriver_stop_scan_session(Context* ctx) {
    // 1. Stop ESP32 scanning
    catdriver_stop_scan(ctx);

    // 2. Flush remaining APs to the session's CSV file
    if(ctx->batch_count > 0 && ctx->file_name[0] != '\0') {
        save_batch_file(ctx);
    }

    // 3. Clear UI buffers so next session starts 100% clean
    ctx->batch_count = 0;
    ctx->recent_count = 0;
    ctx->recent_index = 0;
    memset(ctx->recent_aps, 0, sizeof(ctx->recent_aps));
    memset(&ctx->active_access_point, 0, sizeof(AccessPoint));
    ctx->pressedButton = false;
    ctx->file_header_written = false;
    ctx->file_name[0] = '\0';

    // 4. Return to menu
    ctx->view_state = VIEW_MENU;
}

static void tick_callback(void* ctx_q) {
    furi_assert(ctx_q);
    FuriMessageQueue* queue = ctx_q;
    Event event = {.type = EventTypeTick};
    furi_message_queue_put(queue, &event, 0);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_assert(queue);
    Event event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

static const uint8_t icon_all_bits[] = {0x04, 0x08, 0x0c, 0x0c, 0x18, 0x06, 0x12, 0x12, 0x24, 0x09,
                                        0xc4, 0x08, 0xc0, 0x00, 0xc0, 0x00, 0xc4, 0x08, 0x24, 0x09,
                                        0x12, 0x12, 0x18, 0x06, 0x0c, 0x0c, 0x04, 0x08};

static const uint8_t icon_wifi_bits[] = {0x00, 0x00, 0xf8, 0x07, 0xfc, 0x0f, 0x06,
                                         0x18, 0xe0, 0x01, 0xf0, 0x03, 0x18, 0x06,
                                         0x00, 0x00, 0xc0, 0x00, 0xe0, 0x01, 0xe0,
                                         0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t icon_ble_bits[] = {0x60, 0x00, 0xe0, 0x00, 0xe0, 0x01, 0x62, 0x03, 0x64, 0x06,
                                        0x68, 0x06, 0x70, 0x03, 0x70, 0x03, 0x68, 0x06, 0x64, 0x06,
                                        0x62, 0x03, 0xe0, 0x01, 0xe0, 0x00, 0x60, 0x00};

static const uint8_t icon_gear_bits[] = {0xe0, 0x01, 0xf0, 0x03, 0xe4, 0x09, 0xcc,
                                         0x0c, 0xfe, 0x1f, 0x7e, 0x1e, 0x9e, 0x1e,
                                         0x5e, 0x1e, 0x9e, 0x1f, 0xfe, 0x1f, 0xcc,
                                         0x0c, 0xe4, 0x09, 0xf0, 0x03, 0xe0, 0x01};

static void draw_menu_screen(Canvas* canvas, Context* ctx) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignBottom, "CAT DRIVING");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 19, AlignCenter, AlignBottom, "(wardrive ESP32 + GPS)");

    // Determine carousel page:
    // Page 0: Cards 0 (ALL) and 1 (WIFI)
    // Page 1: Cards 2 (BLE) and 3 (Settings)
    uint8_t page = 0;
    if(ctx->menu_selected_button == 2 || ctx->menu_selected_button == 3 ||
       (ctx->menu_selected_button == 4 && ctx->menu_last_card >= 2)) {
        page = 1;
    }

    // Scroll indicators on left/right edges
    if(page > 0) {
        canvas_draw_str(canvas, 3, 37, "<");
    }
    if(page < 1) {
        canvas_draw_str(canvas, 121, 37, ">");
    }

    struct {
        const char* label_en;
        const char* label_es;
        const uint8_t* icon_bits;
    } card_info[4] = {
        {"ALL", "ALL", icon_all_bits},
        {"WIFI", "WIFI", icon_wifi_bits},
        {"BLE", "BLE", icon_ble_bits},
        {"Settings", "Ajustes", icon_gear_bits},
    };

    uint8_t left_card_idx = page * 2;
    uint8_t right_card_idx = page * 2 + 1;

    uint8_t card_y = 22;
    uint8_t card_w = 48;
    uint8_t card_h = 26;

    // Draw Left Card at X=13
    uint8_t left_x = 13;
    bool left_sel = (ctx->menu_selected_button == left_card_idx);
    if(left_sel) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, left_x, card_y, card_w, card_h, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, left_x, card_y, card_w, card_h, 2);
    }
    canvas_draw_xbm(
        canvas, left_x + (card_w - 14) / 2, card_y + 2, 14, 14, card_info[left_card_idx].icon_bits);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        left_x + card_w / 2,
        card_y + card_h - 2,
        AlignCenter,
        AlignBottom,
        (ctx->language == LangES) ? card_info[left_card_idx].label_es :
                                    card_info[left_card_idx].label_en);
    canvas_set_color(canvas, ColorBlack);

    // Draw Right Card at X=67
    uint8_t right_x = 67;
    bool right_sel = (ctx->menu_selected_button == right_card_idx);
    if(right_sel) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, right_x, card_y, card_w, card_h, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, right_x, card_y, card_w, card_h, 2);
    }
    canvas_draw_xbm(
        canvas,
        right_x + (card_w - 14) / 2,
        card_y + 2,
        14,
        14,
        card_info[right_card_idx].icon_bits);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        right_x + card_w / 2,
        card_y + card_h - 2,
        AlignCenter,
        AlignBottom,
        (ctx->language == LangES) ? card_info[right_card_idx].label_es :
                                    card_info[right_card_idx].label_en);
    canvas_set_color(canvas, ColorBlack);

    // Draw About at bottom (centered)
    const char* about_label = (ctx->language == LangES) ? "Acerca de" : "About";
    uint8_t about_w = (ctx->language == LangES) ? 64 : 44;
    uint8_t about_x = (128 - about_w) / 2;

    canvas_set_font(canvas, FontSecondary);
    if(ctx->menu_selected_button == 4) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, about_x, 51, about_w, 12, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, about_label);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, about_label);
    }
}

static void draw_settings_screen(Canvas* canvas, Context* ctx) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, (ctx->language == LangES) ? "Ajustes" : "Settings");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    const char* items_en[3] = {"Language", "ESP32 Info", "GPS Info"};
    const char* items_es[3] = {"Idioma", "Info de ESP32", "Info de GPS"};

    uint8_t y_start = 15;
    uint8_t row_h = 14;

    for(uint8_t i = 0; i < 3; i++) {
        uint8_t y = y_start + i * (row_h + 2);
        bool selected = (ctx->settings_selected_index == i);

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 1, y, 126, row_h, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        const char* label = (ctx->language == LangES) ? items_es[i] : items_en[i];
        canvas_draw_str(canvas, 6, y + 10, label);

        if(i == 0) {
            const char* val = (ctx->language == LangES) ? "< Espanol >" : "< English >";
            canvas_draw_str_aligned(canvas, 122, y + 10, AlignRight, AlignBottom, val);
        } else {
            canvas_draw_str_aligned(canvas, 122, y + 10, AlignRight, AlignBottom, ">");
        }
    }

    canvas_set_color(canvas, ColorBlack);
}

static void draw_esp_info_screen(Canvas* canvas, Context* ctx) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "ESP32 Info");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, "[OK: Retry]");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(ctx->esp_info_received && strlen(ctx->esp_version_info) > 0) {
        char all_lines[6][64];
        const char* info = ctx->esp_version_info;
        const char* comma = strchr(info, ',');

        if(comma) {
            size_t l1 = comma - info;
            if(l1 > 24) l1 = 24;
            snprintf(all_lines[0], sizeof(all_lines[0]), "FW: %.*s", (int)l1, info);
            while(*comma == ',' || *comma == ' ')
                comma++;
            snprintf(all_lines[1], sizeof(all_lines[1]), "   %.24s", comma);
        } else {
            snprintf(all_lines[0], sizeof(all_lines[0]), "FW: %.24s", info);
            all_lines[1][0] = '\0';
        }

        if(all_lines[1][0] == '\0') {
            snprintf(all_lines[1], sizeof(all_lines[1]), "   WiFi 2.4GHz + BLE");
        }
        snprintf(all_lines[2], sizeof(all_lines[2]), "Port: USART");
        snprintf(all_lines[3], sizeof(all_lines[3]), "Pins: 13 (TX), 14 (RX)");
        snprintf(all_lines[4], sizeof(all_lines[4]), "Speed: 115200 baud");
        snprintf(all_lines[5], sizeof(all_lines[5]), "Status: Connected [OK]");

        uint8_t total_items = 6;
        uint8_t visible_items = 4;
        uint8_t max_scroll = (total_items > visible_items) ? (total_items - visible_items) : 0;
        if(ctx->esp_scroll_offset > max_scroll) ctx->esp_scroll_offset = max_scroll;

        uint8_t y_start = 24;
        uint8_t row_h = 11;

        for(uint8_t i = 0; i < visible_items; i++) {
            uint8_t idx = ctx->esp_scroll_offset + i;
            if(idx < total_items) {
                uint8_t y = y_start + i * row_h;
                canvas_draw_str(canvas, 2, y, all_lines[idx]);
            }
        }

        if(ctx->esp_scroll_offset > 0) {
            canvas_draw_str(canvas, 122, 22, "^");
        }
        if(ctx->esp_scroll_offset < max_scroll) {
            canvas_draw_str(canvas, 122, 58, "v");
        }
    } else {
        uint32_t elapsed = furi_get_tick() - ctx->esp_query_tick;
        if(elapsed < 2500) {
            canvas_draw_str(
                canvas,
                2,
                28,
                (ctx->language == LangES) ? "Consultando modulo ESP32..." :
                                            "Querying ESP32 module...");
            canvas_draw_str(
                canvas,
                2,
                42,
                (ctx->language == LangES) ? "Esperando respuesta serial..." :
                                            "Waiting for serial reply...");
        } else {
            canvas_draw_str(
                canvas,
                2,
                24,
                (ctx->language == LangES) ? "Sin respuesta del ESP32!" : "No reply from ESP32!");
            canvas_draw_str(canvas, 2, 36, "Check Pins 13(TX)/14(RX)");
            canvas_draw_str(
                canvas,
                2,
                48,
                (ctx->language == LangES) ? "Pulsa OK para reintentar" :
                                            "Press OK to retry query");
        }
    }
}

static void draw_about_screen(Canvas* canvas, Context* ctx) {
    if(ctx->about_page == 0) {
        // Page 1: Requested Header and Author info
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "CAT DRIVING");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 22, AlignCenter, AlignBottom, "(wardrive ESP32 + GPS)");
        canvas_draw_str(canvas, 0, 34, "Hardware: ESP32 S3 + NEO 6M V2");
        canvas_draw_str(
            canvas, 0, 46, (ctx->language == LangES) ? "Desarrollador: EVL87" : "Developer: EVL87");

        elements_button_left(canvas, (ctx->language == LangES) ? "Menu" : "Menu");
        elements_button_right(canvas, ">");
    } else if(ctx->about_page == 1) {
        // Page 2: Technical Specifications in natural words
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(
            canvas,
            0,
            11,
            (ctx->language == LangES) ? "Especificaciones [2/3]" : "Specifications [2/3]");

        canvas_set_font(canvas, FontSecondary);
        if(ctx->language == LangES) {
            canvas_draw_str(canvas, 0, 21, "* Buffer RAM 30 (Sin OOM)");
            canvas_draw_str(canvas, 0, 30, "* Guardado append en .CSV");
            canvas_draw_str(canvas, 0, 39, "* Captura cifrado (WPA2..)");
            canvas_draw_str(canvas, 0, 48, "* WiGLE 1.4: WiFi + BLE");
        } else {
            canvas_draw_str(canvas, 0, 21, "* RAM Buffer 30 (No OOM)");
            canvas_draw_str(canvas, 0, 30, "* Append to CSV (No .txt)");
            canvas_draw_str(canvas, 0, 39, "* Real security auth mode");
            canvas_draw_str(canvas, 0, 48, "* WiGLE 1.4: WiFi + BLE");
        }

        elements_button_left(canvas, "<");
        elements_button_right(canvas, ">");
    } else {
        // Page 3: Credit to Sil333033
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(
            canvas, 0, 11, (ctx->language == LangES) ? "Creditos [3/3]" : "Credits [3/3]");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas,
            0,
            24,
            (ctx->language == LangES) ? "Basado en el trabajo de:" : "Based on work by:");
        canvas_draw_str(canvas, 0, 35, "Sil333033 (GitHub)");
        canvas_draw_str(canvas, 0, 46, "Repo: ll-wardrive");

        elements_button_left(canvas, "<");
        elements_button_right(canvas, (ctx->language == LangES) ? "Menu" : "Menu");
    }
}

static void draw_access_point(Canvas* canvas, Context* ctx) {
    FuriString* string = furi_string_alloc();
    AccessPoint ap = ctx->active_access_point;

    // BSSID aligned to the right (top row)
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 125, 12, AlignRight, AlignBottom, ap.bssid);

    // SSID centered in primary font
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignBottom, ap.ssid);

    canvas_set_font(canvas, FontSecondary);

    furi_string_printf(string, "Sig:%ddBm", ap.rssi);
    canvas_draw_str_aligned(canvas, 3, 35, AlignLeft, AlignBottom, furi_string_get_cstr(string));

    // Display human-readable Auth mode or BLE tag
    if(strcmp(ap.type, "BLE") == 0) {
        canvas_draw_str_aligned(canvas, 78, 35, AlignLeft, AlignBottom, "Type:BLE");
        canvas_draw_str_aligned(canvas, 3, 47, AlignLeft, AlignBottom, "CH:BLE");
    } else {
        const char* display_auth = "OPEN";
        if(strstr(ap.authmode, "WPA3") != NULL) {
            display_auth = "WPA3";
        } else if(strstr(ap.authmode, "WPA2") != NULL) {
            display_auth = "WPA2";
        } else if(strstr(ap.authmode, "WPA") != NULL) {
            display_auth = "WPA";
        } else if(strstr(ap.authmode, "WEP") != NULL) {
            display_auth = "WEP";
        } else if(strstr(ap.authmode, "OWE") != NULL) {
            display_auth = "OWE";
        }
        furi_string_printf(string, "Sec:%s", display_auth);
        canvas_draw_str_aligned(
            canvas, 78, 35, AlignLeft, AlignBottom, furi_string_get_cstr(string));

        furi_string_printf(string, "CH:%d", ap.channel);
        canvas_draw_str_aligned(
            canvas, 3, 47, AlignLeft, AlignBottom, furi_string_get_cstr(string));
    }

    furi_string_printf(string, "%d", ap.packetRxCount);
    canvas_draw_icon(canvas, 38, 39, &I_down);
    canvas_draw_str_aligned(canvas, 48, 47, AlignLeft, AlignBottom, furi_string_get_cstr(string));

    furi_string_printf(string, "%d", ap.packetTxCount);
    canvas_draw_icon(canvas, 80, 38, &I_up);
    canvas_draw_str_aligned(canvas, 90, 47, AlignLeft, AlignBottom, furi_string_get_cstr(string));

    furi_string_printf(
        string,
        (ctx->language == LangES) ? "Visto: %02d:%02d:%02d (%lds)" : "Seen: %02d:%02d:%02d (%lds)",
        ap.datetime.hour,
        ap.datetime.minute,
        ap.datetime.second,
        furi_hal_rtc_get_timestamp() - datetime_datetime_to_timestamp(&ap.datetime));
    canvas_draw_str_aligned(canvas, 3, 59, AlignLeft, AlignBottom, furi_string_get_cstr(string));

    furi_string_free(string);
}

static void draw_scan_screen(Canvas* canvas, Context* ctx) {
    if(ctx->recent_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 70, 25, "CatDriver");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 70, 40, (ctx->language == LangES) ? "Buscando..." : "Scanning...");
        canvas_draw_icon(canvas, 1, 9, &I_DolphinWait_59x54);
        return;
    }

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    FuriString* string = furi_string_alloc();
    canvas_set_font(canvas, FontSecondary);

    // Only %d/%d at top left (no mode label to prevent collision with BSSID)
    furi_string_printf(string, "%d/%d", ctx->recent_index + 1, ctx->recent_count);

    canvas_draw_str_aligned(canvas, 3, 12, AlignLeft, AlignBottom, furi_string_get_cstr(string));

    draw_access_point(canvas, ctx);

    furi_string_free(string);
}

static void draw_gps_screen(Canvas* canvas, Context* ctx) {
    FuriString* string = furi_string_alloc();
    canvas_set_font(canvas, FontPrimary);

    if(UART_CH_ESP == UART_CH_GPS) {
        canvas_draw_str(canvas, 0, 10, "GPS channel invalid!");
        canvas_draw_str(canvas, 0, 20, "UART conflict detected");
    } else {
        // Left side telemetry
        furi_string_printf(
            string, "%f", isnan(ctx->gps_data.latitude) ? 0 : (double)ctx->gps_data.latitude);
        canvas_draw_str(canvas, 0, 10, furi_string_get_cstr(string));

        furi_string_printf(
            string, "%f", isnan(ctx->gps_data.longitude) ? 0 : (double)ctx->gps_data.longitude);
        canvas_draw_str(canvas, 0, 21, furi_string_get_cstr(string));

        furi_string_printf(string, "%d sats", ctx->gps_data.satelites);
        canvas_draw_str(canvas, 0, 32, furi_string_get_cstr(string));

        furi_string_printf(
            string,
            "%02d:%02d:%02dZ",
            ctx->gps_data.hour,
            ctx->gps_data.minute,
            ctx->gps_data.second);
        canvas_draw_str(canvas, 0, 43, furi_string_get_cstr(string));

        // Right side: Header and Status badge underneath it
        canvas_draw_str_aligned(canvas, 98, 11, AlignCenter, AlignBottom, "GPS DATA");

        canvas_set_font(canvas, FontSecondary);
        bool has_fix =
            (ctx->gps_data.satelites >= 3 && !isnan(ctx->gps_data.latitude) &&
             ctx->gps_data.latitude != 0.0f);
        if(has_fix) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, 72, 15, 52, 14, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 98, 22, AlignCenter, AlignCenter, "FIX READY");
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_rframe(canvas, 72, 15, 52, 14, 2);
            canvas_draw_str_aligned(canvas, 98, 22, AlignCenter, AlignCenter, "NO FIX");
        }

        canvas_draw_str_aligned(canvas, 98, 41, AlignCenter, AlignBottom, "Pins: 15, 16");
        canvas_draw_str_aligned(canvas, 98, 52, AlignCenter, AlignBottom, "9600 baud");
    }

    furi_string_free(string);
}

static void render_callback(Canvas* canvas, void* context) {
    Context* ctx = context;
    furi_mutex_acquire(ctx->mutex, FuriWaitForever);

    switch(ctx->view_state) {
    case VIEW_MENU:
        draw_menu_screen(canvas, ctx);
        break;
    case VIEW_SCAN:
        draw_scan_screen(canvas, ctx);
        break;
    case VIEW_SETTINGS:
        draw_settings_screen(canvas, ctx);
        break;
    case VIEW_ABOUT:
        draw_about_screen(canvas, ctx);
        break;
    case VIEW_GPS:
        draw_gps_screen(canvas, ctx);
        break;
    case VIEW_ESP_INFO:
        draw_esp_info_screen(canvas, ctx);
        break;
    }

    furi_mutex_release(ctx->mutex);
}

int32_t catdriver_app() {
    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    // turn off 5v, so it gets reset on startup
    if(furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
    }

    // Enable 5v on startup
    uint8_t attempts = 0;
    while(!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    furi_delay_ms(200);

    Context* ctx = malloc(sizeof(Context));
    memset(ctx, 0, sizeof(Context));

    ctx->queue = furi_message_queue_alloc(8, sizeof(Event));
    ctx->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    ctx->batch_count = 0;
    ctx->recent_count = 0;
    ctx->recent_index = 0;
    ctx->total_scanned_count = 0;
    ctx->total_saved_count = 0;
    ctx->file_header_written = false;
    ctx->pressedButton = false;
    ctx->view_state = VIEW_MENU;
    ctx->about_page = 0;
    ctx->scan_mode = ScanModeAll;
    ctx->language = LangEN; // Default to English as requested
    ctx->menu_selected_button = 0;
    ctx->menu_last_card = 0;
    ctx->file_name[0] = '\0';

    catdriver_uart_init(ctx);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, ctx);
    view_port_input_callback_set(view_port, input_callback, ctx->queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FuriTimer* timer = furi_timer_alloc(tick_callback, FuriTimerTypePeriodic, ctx->queue);
    furi_timer_start(timer, 100);

    // application loop
    Event event;
    bool processing = true;
    do {
        if(furi_message_queue_get(ctx->queue, &event, FuriWaitForever) == FuriStatusOk) {
            furi_mutex_acquire(ctx->mutex, FuriWaitForever);
            switch(event.type) {
            case EventTypeKey:
                // Handle BACK key
                if(event.input.type == InputTypeShort && event.input.key == InputKeyBack) {
                    if(ctx->view_state == VIEW_MENU) {
                        processing = false;
                    } else if(ctx->view_state == VIEW_SCAN) {
                        catdriver_stop_scan_session(ctx);
                    } else if(ctx->view_state == VIEW_SETTINGS) {
                        ctx->view_state = VIEW_MENU;
                    } else if(ctx->view_state == VIEW_ABOUT) {
                        ctx->view_state = VIEW_MENU;
                    } else if(ctx->view_state == VIEW_GPS) {
                        ctx->view_state = ctx->return_from_gps_view ? ctx->return_from_gps_view :
                                                                      VIEW_SCAN;
                    } else if(ctx->view_state == VIEW_ESP_INFO) {
                        ctx->view_state = VIEW_SETTINGS;
                    }
                } else if(event.input.type == InputTypeLong && event.input.key == InputKeyBack) {
                    if(ctx->view_state == VIEW_MENU) {
                        processing = false;
                    } else {
                        if(ctx->view_state == VIEW_SCAN) {
                            catdriver_stop_scan_session(ctx);
                        } else {
                            ctx->view_state = VIEW_MENU;
                        }
                    }
                } else if(event.input.type == InputTypePress) {
                    // D-PAD HANDLING BASED ON CURRENT VIEW STATE
                    if(ctx->view_state == VIEW_MENU) {
                        if(event.input.key == InputKeyLeft) {
                            if(ctx->menu_selected_button == 4) {
                                ctx->menu_selected_button = (ctx->menu_last_card >= 2) ? 2 : 0;
                                ctx->menu_last_card = ctx->menu_selected_button;
                            } else {
                                if(ctx->menu_selected_button == 0) {
                                    ctx->menu_selected_button = 3;
                                } else {
                                    ctx->menu_selected_button--;
                                }
                                ctx->menu_last_card = ctx->menu_selected_button;
                            }
                        } else if(event.input.key == InputKeyRight) {
                            if(ctx->menu_selected_button == 4) {
                                ctx->menu_selected_button = (ctx->menu_last_card >= 2) ? 3 : 1;
                                ctx->menu_last_card = ctx->menu_selected_button;
                            } else {
                                if(ctx->menu_selected_button >= 3) {
                                    ctx->menu_selected_button = 0;
                                } else {
                                    ctx->menu_selected_button++;
                                }
                                ctx->menu_last_card = ctx->menu_selected_button;
                            }
                        } else if(event.input.key == InputKeyDown) {
                            if(ctx->menu_selected_button < 4) {
                                ctx->menu_last_card = ctx->menu_selected_button;
                                ctx->menu_selected_button = 4;
                            } else {
                                ctx->menu_selected_button = ctx->menu_last_card;
                            }
                        } else if(event.input.key == InputKeyUp) {
                            if(ctx->menu_selected_button == 4) {
                                ctx->menu_selected_button = ctx->menu_last_card;
                            } else {
                                ctx->menu_last_card = ctx->menu_selected_button;
                                ctx->menu_selected_button = 4;
                            }
                        } else if(event.input.key == InputKeyOk) {
                            // Button Actions
                            switch(ctx->menu_selected_button) {
                            case 0: // ALL
                                catdriver_start_scan_session(ctx, ScanModeAll);
                                break;
                            case 1: // WIFI
                                catdriver_start_scan_session(ctx, ScanModeWifi);
                                break;
                            case 2: // BLE
                                catdriver_start_scan_session(ctx, ScanModeBle);
                                break;
                            case 3: // Settings
                                ctx->view_state = VIEW_SETTINGS;
                                break;
                            case 4: // About
                                ctx->about_page = 0;
                                ctx->view_state = VIEW_ABOUT;
                                break;
                            }
                        }
                    } else if(ctx->view_state == VIEW_SETTINGS) {
                        if(event.input.key == InputKeyUp) {
                            if(ctx->settings_selected_index > 0) {
                                ctx->settings_selected_index--;
                            } else {
                                ctx->settings_selected_index = 2;
                            }
                        } else if(event.input.key == InputKeyDown) {
                            if(ctx->settings_selected_index < 2) {
                                ctx->settings_selected_index++;
                            } else {
                                ctx->settings_selected_index = 0;
                            }
                        } else if(event.input.key == InputKeyLeft || event.input.key == InputKeyRight) {
                            if(ctx->settings_selected_index == 0) {
                                ctx->language = (ctx->language == LangEN) ? LangES : LangEN;
                            } else if(event.input.key == InputKeyRight) {
                                if(ctx->settings_selected_index == 1) {
                                    ctx->esp_scroll_offset = 0;
                                    catdriver_query_esp_info(ctx);
                                    ctx->view_state = VIEW_ESP_INFO;
                                } else if(ctx->settings_selected_index == 2) {
                                    ctx->return_from_gps_view = VIEW_SETTINGS;
                                    ctx->view_state = VIEW_GPS;
                                }
                            }
                        } else if(event.input.key == InputKeyOk) {
                            if(ctx->settings_selected_index == 0) {
                                ctx->language = (ctx->language == LangEN) ? LangES : LangEN;
                            } else if(ctx->settings_selected_index == 1) {
                                catdriver_query_esp_info(ctx);
                                ctx->view_state = VIEW_ESP_INFO;
                            } else if(ctx->settings_selected_index == 2) {
                                ctx->return_from_gps_view = VIEW_SETTINGS;
                                ctx->view_state = VIEW_GPS;
                            }
                        }
                    } else if(ctx->view_state == VIEW_ESP_INFO) {
                        if(event.input.key == InputKeyDown) {
                            if(ctx->esp_scroll_offset < 2) {
                                ctx->esp_scroll_offset++;
                            }
                        } else if(event.input.key == InputKeyUp) {
                            if(ctx->esp_scroll_offset > 0) {
                                ctx->esp_scroll_offset--;
                            }
                        } else if(event.input.key == InputKeyOk || event.input.key == InputKeyRight) {
                            ctx->esp_scroll_offset = 0;
                            catdriver_query_esp_info(ctx);
                        } else if(event.input.key == InputKeyLeft) {
                            ctx->view_state = VIEW_SETTINGS;
                        }
                    } else if(ctx->view_state == VIEW_GPS) {
                        if(event.input.key == InputKeyLeft || event.input.key == InputKeyOk) {
                            ctx->view_state =
                                ctx->return_from_gps_view ? ctx->return_from_gps_view : VIEW_SCAN;
                        }
                    } else if(ctx->view_state == VIEW_ABOUT) {
                        if(event.input.key == InputKeyRight) {
                            if(ctx->about_page < 2) {
                                ctx->about_page++;
                            } else {
                                ctx->view_state = VIEW_MENU;
                            }
                        } else if(event.input.key == InputKeyLeft) {
                            if(ctx->about_page > 0) {
                                ctx->about_page--;
                            } else {
                                ctx->view_state = VIEW_MENU;
                            }
                        } else if(event.input.key == InputKeyOk) {
                            if(ctx->about_page < 2) {
                                ctx->about_page++;
                            } else {
                                ctx->view_state = VIEW_MENU;
                            }
                        }
                    } else if(ctx->view_state == VIEW_SCAN) {
                        if(event.input.key == InputKeyDown) {
                            if(ctx->recent_count > 0) {
                                ctx->recent_index--;
                                if(ctx->recent_index < 0) {
                                    ctx->recent_index = ctx->recent_count - 1;
                                }
                                ctx->active_access_point = ctx->recent_aps[ctx->recent_index];
                                ctx->pressedButton = true;
                            }
                        } else if(event.input.key == InputKeyUp) {
                            if(ctx->recent_count > 0) {
                                ctx->recent_index++;
                                if(ctx->recent_index >= ctx->recent_count) {
                                    ctx->recent_index = 0;
                                }
                                ctx->active_access_point = ctx->recent_aps[ctx->recent_index];
                                ctx->pressedButton = true;
                            }
                        } else if(event.input.key == InputKeyLeft) {
                            ctx->return_from_gps_view = VIEW_SCAN;
                            ctx->view_state = VIEW_GPS;
                        } else if(event.input.key == InputKeyOk) {
                            // Mode is NOT switched here! Reset manual scroll lock to track latest live AP
                            ctx->pressedButton = false;
                        }
                    } else if(ctx->view_state == VIEW_GPS) {
                        if(event.input.key == InputKeyLeft || event.input.key == InputKeyOk) {
                            ctx->view_state = VIEW_SCAN;
                        }
                    }
                }
                break;
            case EventTypeTick:
                if(!ctx->pressedButton && ctx->recent_count > 0) {
                    ctx->active_access_point = ctx->recent_aps[ctx->recent_index];
                }
                break;
            default:
                break;
            }
            furi_mutex_release(ctx->mutex);

            view_port_update(view_port);
        } else {
            processing = false;
        }
    } while(processing);

    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(ctx->queue);

    // Stop scan session and flush any pending writes
    catdriver_stop_scan_session(ctx);
    catdriver_uart_deinit(ctx);

    furi_mutex_free(ctx->mutex);
    free(ctx);

    // Completely stop hardware blink mode and turn off all LEDs
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightBlue, 0);

    if(furi_hal_power_is_otg_enabled()) {
        furi_hal_power_disable_otg();
    }

    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}
