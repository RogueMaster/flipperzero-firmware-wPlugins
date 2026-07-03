#include "flight_monitor.h"
#include "helpers/ble_serial.h"

static void draw_progress_bar(Canvas* canvas, uint8_t x, uint8_t y, uint8_t width, uint8_t value);

static const uint8_t image_plane_0_bits[] = {
    0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0xfc, 0xfc, 0x00, 0x00, 0xc3, 0xfc, 0x00, 0x00,
    0xc3, 0x03, 0x3f, 0xc0, 0xc0, 0x03, 0x3f, 0xc0, 0xc0, 0x0f, 0xc0, 0x33, 0x30, 0x0f, 0xc0,
    0x33, 0x30, 0xf0, 0x00, 0x0c, 0x0c, 0xf0, 0x00, 0x0c, 0x0c, 0x00, 0x0f, 0x00, 0x03, 0x00,
    0x0f, 0x00, 0x03, 0x00, 0xf0, 0xc0, 0x00, 0x00, 0xf0, 0xc0, 0x00, 0x00, 0xc0, 0x00, 0x03,
    0x00, 0xc0, 0x00, 0x03, 0x00, 0x30, 0x0f, 0x03, 0x00, 0x30, 0x0f, 0x03, 0x00, 0xcc, 0x0c,
    0x0c, 0x00, 0xcc, 0x0c, 0x0c, 0xfc, 0x33, 0x30, 0x0c, 0xfc, 0x33, 0x30, 0x0c, 0x03, 0x0f,
    0x30, 0x0c, 0x03, 0x0f, 0x30, 0x0c, 0x3c, 0x0c, 0xc0, 0x30, 0x3c, 0x0c, 0xc0, 0x30, 0xc0,
    0x0c, 0xc0, 0x30, 0xc0, 0x0c, 0xc0, 0x30, 0xc0, 0x0c, 0x00, 0x33, 0xc0, 0x0c, 0x00, 0x33,
    0x00, 0x03, 0x00, 0x0f, 0x00, 0x03, 0x00, 0x0f};

static void draw_splash_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);
    canvas_set_font(canvas, FontPrimary);

    canvas_draw_xbm(canvas, 7, 10, 32, 32, image_plane_0_bits);
    canvas_draw_str(canvas, 50, 18, "War Thunder");
    canvas_draw_str(canvas, 54, 30, "Dashboard");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 9, 58, "Dr.Mosfet");
}

static const NotificationSequence sequence_altitude_200 = {
    &message_blue_255,
    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_altitude_150 = {
    &message_red_255,
    &message_green_255,
    &message_note_a5,
    &message_delay_50,
    &message_sound_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_altitude_100 = {
    &message_red_255,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_altitude_50 = {
    &message_red_255,
    &message_vibro_on,
    &message_note_e6,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_altitude_critical = {
    &message_red_255,
    &message_vibro_on,
    &message_note_c7,
    &message_delay_25,
    &message_sound_off,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_gear_warning = {
    &message_red_255,
    &message_green_255,
    &message_note_a5,
    &message_delay_100,
    &message_sound_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_gear_retract = {
    &message_green_255,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_speed_warning = {
    &message_red_255,
    &message_green_255,
    &message_note_e5,
    &message_delay_50,
    &message_sound_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_engine_failure = {
    &message_red_255,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence sequence_gforce_vibrate = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

// === CONFIGURATION SAVE/LOAD ===

static void flight_config_set_defaults(FlightConfig* config) {
    config->altitude_alarm = 120;
    config->gear_warning_alt = 150;
    config->gear_retract_alt = 200;
    config->stall_speed = 100;
    config->overspeed = 700;
    config->enable_gear_warnings = true; // Domyślnie włączone
}

static bool flight_config_load(FlightConfig* config) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, CONFIG_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, config, sizeof(FlightConfig)) == sizeof(FlightConfig)) {
            success = true;
            FURI_LOG_I(TAG, "Config loaded successfully");
        }
        storage_file_close(file);
    } else {
        FURI_LOG_I(TAG, "Config file not found, using defaults");
        flight_config_set_defaults(config);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

static bool flight_config_save(const FlightConfig* config) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, CONFIG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        if(storage_file_write(file, config, sizeof(FlightConfig)) == sizeof(FlightConfig)) {
            success = true;
            FURI_LOG_I(TAG, "Config saved successfully");
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

static void handle_altitude_alarm(FlightMonitorApp* app) {
    FlightData* d = &app->data;
    uint32_t now = furi_get_tick();

    const NotificationSequence* seq = NULL;
    uint32_t interval = 0;

    if(d->altitude < 30 && d->altitude > 0) {
        seq = &sequence_altitude_critical;
        interval = 100;
    } else if(d->altitude < 50) {
        // 30-50m: Bardzo niskie - E6, bardzo szybko
        seq = &sequence_altitude_50;
        interval = 200; // Co 200ms = 5 beep/s
    } else if(d->altitude < 70) {
        // 50-70m: Niskie - C6, szybko
        seq = &sequence_altitude_100;
        interval = 400; // Co 400ms = 2.5 beep/s
    } else if(d->altitude < 90) {
        // 70-90m: Średnie - A5
        seq = &sequence_altitude_150;
        interval = 600; // Co 600ms
    } else if(d->altitude < 120) {
        // 90-120m: Początek alarmu - G4, spokojnie
        seq = &sequence_altitude_200;
        interval = 1000; // Co 1s
    }

    // Jeśli alarm aktywny i minął czas
    if(seq && (now - app->last_beep >= interval)) {
        notification_message(app->notification, seq);
        app->last_beep = now;
    }
}

// Obsługuje alarmy (dźwięk + LED)
static void handle_alerts(FlightMonitorApp* app) {
    FlightData* d = &app->data;
    uint32_t now = furi_get_tick();

    // PRIORYTET 1: Podwozie schowane przy niskiej wysokości (NAJWAŻNIEJSZE!)
    // Alarm tylko jeśli włączony w ustawieniach
    if(app->config.enable_gear_warnings && d->altitude < app->config.gear_warning_alt &&
       d->altitude > 0 && d->gear == 0) {
        if(now - app->last_beep > 500) {
            notification_message(app->notification, &sequence_gear_warning);
            app->last_beep = now;
        }
        return;
    }

    if(app->config.enable_gear_warnings && d->altitude > app->config.gear_retract_alt &&
       d->gear == 1 && d->speed > 150) {
        if(now - app->last_beep > 2000) {
            notification_message(app->notification, &sequence_gear_retract);
            app->last_beep = now;
        }
        return;
    }

    // Alarm wysokości - tylko jeśli OPADAMY (vertical_speed < 0)
    // Nie pika podczas startu/wznoszenia ani lotu poziomego
    if(d->altitude < app->config.altitude_alarm && d->altitude > 0 && app->data_changing &&
       d->vertical_speed < 0) {
        handle_altitude_alarm(app);
        return;
    }

    if(d->engine_power < 100 && d->vertical_speed < -50 && d->altitude > 100) {
        if(now - app->last_beep > 800) {
            notification_message(app->notification, &sequence_engine_failure);
            app->last_beep = now;
        }
        return;
    }

    int abs_pitch = d->pitch < 0 ? -d->pitch : d->pitch;
    int abs_roll = d->roll < 0 ? -d->roll : d->roll;
    if((abs_pitch > 60 || abs_roll > 70) && d->speed > 200) {
        if(now - app->last_beep > 400) {
            notification_message(app->notification, &sequence_gforce_vibrate);
            app->last_beep = now;
        }
        return;
    }

    if(d->speed < app->config.stall_speed && d->altitude > 200 && d->throttle < 80 &&
       d->gear == 0) {
        if(now - app->last_beep > 300) {
            notification_message(app->notification, &sequence_speed_warning);
            app->last_beep = now;
        }
    } else if(d->speed > app->config.overspeed) {
        // Overspeed
        if(now - app->last_beep > 300) {
            notification_message(app->notification, &sequence_speed_warning);
            app->last_beep = now;
        }
    }
}

// === WIELKIE WIDOKI (jeden parametr na cały ekran) ===

static void draw_view_altitude(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);
    char buffer[32];

    // Nazwa parametru
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "ALTITUDE");

    // Wielka wartość
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buffer, sizeof(buffer), "%d", app->data.altitude);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buffer);

    // Jednostka
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "meters");
}

static void draw_view_speed(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);
    char buffer[32];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "SPEED");

    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buffer, sizeof(buffer), "%d", app->data.speed);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buffer);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "km/h");
}

static void draw_view_vertical(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);
    char buffer[32];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "VERTICAL SPEED");

    canvas_set_font(canvas, FontBigNumbers);
    double vs = app->data.vertical_speed / 10.0;
    snprintf(buffer, sizeof(buffer), "%.1f", vs);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buffer);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "m/s");
}

static void draw_view_throttle(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);
    char buffer[32];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 3, AlignCenter, AlignTop, "THROTTLE");

    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buffer, sizeof(buffer), "%d%%", app->data.throttle);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, buffer);

    draw_progress_bar(canvas, 10, 38, 108, app->data.throttle);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, "percent");
}

static void draw_view_orientation(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);
    char buffer[32];

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "ORIENTATION");

    // Pitch
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 25, "PITCH:");
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buffer, sizeof(buffer), "%d", app->data.pitch);
    canvas_draw_str(canvas, 60, 25, buffer);

    // Roll
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 50, "ROLL:");
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buffer, sizeof(buffer), "%d", app->data.roll);
    canvas_draw_str(canvas, 60, 50, buffer);
}

// === FUNKCJE RYSOWANIA ===

// Funkcja pomocnicza do rysowania paska (progressbar)
// x, y - pozycja, width - szerokość, value - wartość 0-100
static void draw_progress_bar(Canvas* canvas, uint8_t x, uint8_t y, uint8_t width, uint8_t value) {
    // Ramka paska
    canvas_draw_frame(canvas, x, y, width, 7);

    // Wypełnienie w zależności od wartości
    uint8_t fill_width = (width - 2) * value / 100;
    if(fill_width > 0) {
        canvas_draw_box(canvas, x + 1, y + 1, fill_width, 5);
    }
}

// Rysowanie widoku z danymi lotu
void draw_flight_view(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);

    if(app->config.enable_gear_warnings && app->data.altitude > app->config.gear_retract_alt &&
       app->data.gear == 1 && app->data.speed > 150) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "RETRACT GEAR");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, "Gear is down!");

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "ALT: %d m", app->data.altitude);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, buffer);
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    char buffer[32];

    // === LINIA 1: Wysokość ===
    snprintf(buffer, sizeof(buffer), "ALT:%dm", app->data.altitude);
    canvas_draw_str(canvas, 2, 10, buffer);

    // === LINIA 2: Prędkość ===
    snprintf(buffer, sizeof(buffer), "SPD:%dkm/h", app->data.speed);
    canvas_draw_str(canvas, 2, 21, buffer);

    // === LINIA 3: Prędkość wznoszenia ===
    double vs = app->data.vertical_speed / 10.0;
    snprintf(buffer, sizeof(buffer), "V/S:%.1fm/s", vs);
    canvas_draw_str(canvas, 2, 32, buffer);

    // === LINIA 4: Przepustnica (z paskiem) ===
    snprintf(buffer, sizeof(buffer), "THR:%d%%", app->data.throttle);
    canvas_draw_str(canvas, 2, 41, buffer);
    draw_progress_bar(canvas, 50, 35, 75, app->data.throttle);

    // === LINIA 5: Klapy (z paskiem) ===
    snprintf(buffer, sizeof(buffer), "FLP:%d%%", app->data.flaps);
    canvas_draw_str(canvas, 2, 52, buffer);
    draw_progress_bar(canvas, 50, 46, 75, app->data.flaps);

    // === LINIA 6: Podwozie - WYŁĄCZONE ===
    // const char* gear_status = app->data.gear ? "DOWN" : "UP";
    // snprintf(buffer, sizeof(buffer), "GEAR:%s", gear_status);
    // canvas_draw_str(canvas, 2, 64, buffer);

    // === GÓRNY PRAWY RÓG: Orientacja ===
    canvas_set_font(canvas, FontKeyboard);
    snprintf(buffer, sizeof(buffer), "P:%d", app->data.pitch);
    canvas_draw_str(canvas, 90, 10, buffer);
    snprintf(buffer, sizeof(buffer), "R:%d", app->data.roll);
    canvas_draw_str(canvas, 90, 20, buffer);
}

// === MENU USTAWIEŃ ===

typedef enum {
    MenuItemAltitudeAlarm,
    MenuItemGearWarning,
    MenuItemGearRetract,
    MenuItemStallSpeed,
    MenuItemOverspeed,
    MenuItemGearAlarms,
    MenuItemStart,
    MenuItemCount
} MenuItem;

static void draw_menu(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 3, AlignCenter, AlignTop, "Settings");

    canvas_set_font(canvas, FontSecondary);

    // Simple vertical list, 4 items visible at once
    uint8_t visible_items = 4;
    uint8_t start_item = 0;

    if(app->scene == SceneMenu) {
        // Calculate scroll position to keep cursor visible
        uint8_t cursor = app->menu_cursor;
        if(cursor >= visible_items) {
            start_item = cursor - visible_items + 1;
        }
    }

    for(uint8_t i = 0; i < visible_items && (start_item + i) < MenuItemCount; i++) {
        uint8_t item_idx = start_item + i;
        uint8_t y_pos = 15 + (i * 12);

        // Highlight selected item
        if(app->menu_cursor == item_idx) {
            canvas_draw_box(canvas, 0, y_pos - 2, 128, 12);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 2, y_pos + 7, ">");
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        char label[48];
        switch(item_idx) {
        case MenuItemAltitudeAlarm:
            snprintf(label, sizeof(label), "Alt Alarm: %um", app->config.altitude_alarm);
            break;
        case MenuItemGearWarning:
            snprintf(label, sizeof(label), "Gear Warn: %um", app->config.gear_warning_alt);
            break;
        case MenuItemGearRetract:
            snprintf(label, sizeof(label), "Gear Retr: %um", app->config.gear_retract_alt);
            break;
        case MenuItemStallSpeed:
            snprintf(label, sizeof(label), "Stall: %ukm/h", app->config.stall_speed);
            break;
        case MenuItemOverspeed:
            snprintf(label, sizeof(label), "Overspeed: %ukm/h", app->config.overspeed);
            break;
        case MenuItemGearAlarms:
            snprintf(
                label,
                sizeof(label),
                "Gear Alarms: %s",
                app->config.enable_gear_warnings ? "ON" : "OFF");
            break;
        case MenuItemStart:
            snprintf(label, sizeof(label), "START");
            break;
        default:
            label[0] = '\0';
            break;
        }

        canvas_draw_str(canvas, 10, y_pos + 7, label);
        canvas_set_color(canvas, ColorBlack);
    }

    // Scroll bar
    if(MenuItemCount > visible_items) {
        uint8_t scroll_height = 48;
        uint8_t scroll_y = 12;
        uint8_t slider_height = (visible_items * scroll_height) / MenuItemCount;
        uint8_t max_slider_pos = scroll_height - slider_height;
        uint8_t slider_pos = (start_item * max_slider_pos) / (MenuItemCount - visible_items);

        canvas_draw_frame(canvas, 125, scroll_y, 3, scroll_height);
        canvas_draw_box(canvas, 126, scroll_y + slider_pos, 1, slider_height);
    }
}

// Widok połączenia (czekamy na dane)
void draw_connect_view(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Flight Monitor");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, "Waiting for");
    canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "flight data...");
}

// Widok statusu (błędy, brak połączenia)
void draw_status_view(Canvas* canvas, FlightMonitorApp* app) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(app->bt_state) {
    case BtStateLost:
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Waiting for data");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, "No flight data");
        break;

    case BtStateNoData:
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "No Data");
        break;

    default:
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Starting...");
        break;
    }
}

// Callback renderowania ekranu
static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    FlightMonitorApp* app = ctx;

    // Splash screen na starcie
    if(app->show_splash) {
        draw_splash_screen(canvas);
        return;
    }

    // Menu ustawień
    if(app->scene == SceneMenu) {
        draw_menu(canvas, app);
        return;
    }

    // Normal operation (Scene = Running)
    switch(app->bt_state) {
    case BtStateWaiting:
        draw_connect_view(canvas);
        break;

    case BtStateRecieving:
        // Wybór widoku zależnie od trybu
        switch(app->view_mode) {
        case ViewModeAll:
            draw_flight_view(canvas, app);
            break;
        case ViewModeAltitude:
            draw_view_altitude(canvas, app);
            break;
        case ViewModeSpeed:
            draw_view_speed(canvas, app);
            break;
        case ViewModeVertical:
            draw_view_vertical(canvas, app);
            break;
        case ViewModeThrottle:
            draw_view_throttle(canvas, app);
            break;
        case ViewModeOrientation:
            draw_view_orientation(canvas, app);
            break;
        default:
            draw_flight_view(canvas, app);
            break;
        }
        break;

    default:
        draw_status_view(canvas, app);
        break;
    }
}

// Callback obsługi przycisków
static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// Callback odbierania danych przez Bluetooth
static uint16_t bt_serial_callback(SerialServiceEvent event, void* ctx) {
    furi_assert(ctx);
    FlightMonitorApp* app = ctx;

    if(event.event == SerialServiceEventTypeDataReceived) {
        FURI_LOG_D(
            TAG, "Data received. Size: %u (expected: %u)", event.data.size, sizeof(FlightData));

        // Sprawdzamy czy otrzymaliśmy kompletną strukturę danych
        if(event.data.size == sizeof(FlightData)) {
            // Sprawdzamy czy dane się zmieniły (wykrywamy czy samolot leci czy stoi po crashu)
            FlightData new_data;
            memcpy(&new_data, event.data.buffer, sizeof(FlightData));

            // Porównaj kluczowe parametry z poprzednimi danymi
            app->data_changing =
                (new_data.altitude != app->last_data.altitude ||
                 new_data.speed != app->last_data.speed ||
                 new_data.vertical_speed != app->last_data.vertical_speed ||
                 new_data.throttle != app->last_data.throttle);

            // Zapisz poprzednie dane przed nadpisaniem
            memcpy(&app->last_data, &app->data, sizeof(FlightData));

            // Kopiujemy nowe dane do naszej struktury
            memcpy(&app->data, event.data.buffer, sizeof(FlightData));
            app->bt_state = BtStateRecieving;
            app->last_packet = furi_hal_rtc_get_timestamp();

            // Włączamy podświetlenie ekranu
            notification_message(app->notification, &sequence_display_backlight_on);

            // Migamy niebieską diodą na znak odbioru danych
            notification_message(app->notification, &sequence_blink_blue_10);
        }
    }

    return 0;
}

// Alokacja pamięci i inicjalizacja aplikacji
static FlightMonitorApp* flight_monitor_alloc() {
    FlightMonitorApp* app = malloc(sizeof(FlightMonitorApp));

    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);

    // Load configuration or set defaults
    if(!flight_config_load(&app->config)) {
        flight_config_set_defaults(&app->config);
        flight_config_save(&app->config);
    }

    // Start in menu mode
    app->scene = SceneMenu;
    app->menu_cursor = 0; // First menu item
    app->view_mode = ViewModeAll; // Default view for running mode
    app->bt_state = BtStateInactive;
    app->ble_serial_profile = NULL;

    // Inicjalizacja alarmów
    app->current_alert = AlertNone;
    app->last_beep = 0;
    app->data_changing = true;
    memset(&app->last_data, 0, sizeof(FlightData));
    app->show_splash = true;

    // Dodajemy viewport do GUI
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->event_queue);

    return app;
}

// Zwalnianie zasobów aplikacji
static void flight_monitor_free(FlightMonitorApp* app) {
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_BT);
    free(app);
}

// GŁÓWNA FUNKCJA APLIKACJI
int32_t flight_monitor_app(void* p) {
    UNUSED(p);
    FlightMonitorApp* app = flight_monitor_alloc();

    // Wyświetlamy splash screen przez 3 sekundy
    view_port_update(app->view_port);
    furi_delay_ms(3000);
    app->show_splash = false;
    view_port_update(app->view_port);

    // === GŁÓWNA PĘTLA APLIKACJI ===
    InputEvent event;
    uint32_t last_update = 0; // Timer dla odświeżania ekranu

    while(true) {
        // Sprawdzamy czy użytkownik nacisnął przycisk (timeout 20ms = 50 Hz pętla)
        if(furi_message_queue_get(app->event_queue, &event, 20) == FuriStatusOk) {
            if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                if(app->scene == SceneMenu) {
                    // === MENU MODE ===
                    if(event.key == InputKeyUp) {
                        if(app->menu_cursor > 0) {
                            app->menu_cursor--;
                        } else {
                            app->menu_cursor = MenuItemCount - 1;
                        }
                    } else if(event.key == InputKeyDown) {
                        app->menu_cursor = (app->menu_cursor + 1) % MenuItemCount;
                    } else if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                        // Adjust values
                        int8_t direction = (event.key == InputKeyRight) ? 1 : -1;
                        uint16_t step = (event.type == InputTypeRepeat) ? 10 : 5;

                        switch(app->menu_cursor) {
                        case MenuItemAltitudeAlarm:
                            app->config.altitude_alarm += direction * step;
                            if(app->config.altitude_alarm < 20) app->config.altitude_alarm = 20;
                            if(app->config.altitude_alarm > 500) app->config.altitude_alarm = 500;
                            break;
                        case MenuItemGearWarning:
                            app->config.gear_warning_alt += direction * step;
                            if(app->config.gear_warning_alt < 50)
                                app->config.gear_warning_alt = 50;
                            if(app->config.gear_warning_alt > 300)
                                app->config.gear_warning_alt = 300;
                            break;
                        case MenuItemGearRetract:
                            app->config.gear_retract_alt += direction * step;
                            if(app->config.gear_retract_alt < 100)
                                app->config.gear_retract_alt = 100;
                            if(app->config.gear_retract_alt > 500)
                                app->config.gear_retract_alt = 500;
                            break;
                        case MenuItemStallSpeed:
                            app->config.stall_speed += direction * step;
                            if(app->config.stall_speed < 50) app->config.stall_speed = 50;
                            if(app->config.stall_speed > 200) app->config.stall_speed = 200;
                            break;
                        case MenuItemOverspeed:
                            app->config.overspeed += direction * (step * 10);
                            if(app->config.overspeed < 400) app->config.overspeed = 400;
                            if(app->config.overspeed > 1200) app->config.overspeed = 1200;
                            break;
                        case MenuItemGearAlarms:
                            app->config.enable_gear_warnings = !app->config.enable_gear_warnings;
                            break;
                        }
                        flight_config_save(&app->config);
                    } else if(event.key == InputKeyOk) {
                        if(app->menu_cursor == MenuItemStart) {
                            // Start application
                            app->scene = SceneRunning;
                            app->view_mode = ViewModeAll;

                            // Hide splash screen setup
                            app->show_splash = false;

                            // Initialize BLE
                            bt_disconnect(app->bt);
                            furi_delay_ms(200);
                            bt_keys_storage_set_storage_path(
                                app->bt, APP_DATA_PATH(".bt_serial.keys"));

                            BleProfileSerialParams params = {
                                .device_name_prefix = "Flight",
                                .mac_xor = 0x0003,
                            };

                            app->ble_serial_profile =
                                bt_profile_start(app->bt, ble_profile_serial, &params);
                            furi_check(app->ble_serial_profile);

                            ble_profile_serial_set_event_callback(
                                app->ble_serial_profile,
                                BT_SERIAL_BUFFER_SIZE,
                                bt_serial_callback,
                                app);

                            furi_hal_bt_start_advertising();
                            app->bt_state = BtStateWaiting;
                            FURI_LOG_I(TAG, "Bluetooth active, waiting for flight data...");
                        }
                    } else if(event.key == InputKeyBack) {
                        break; // Exit from menu
                    }
                } else {
                    // === RUNNING MODE ===
                    // Przycisk BACK = wyjście z aplikacji
                    if(event.key == InputKeyBack) {
                        break;
                    }

                    // Tylko gdy są dane - obsługa przycisków nawigacji
                    if(app->bt_state == BtStateRecieving) {
                        // OK = następny widok (w prawo)
                        if(event.key == InputKeyOk) {
                            app->view_mode = (app->view_mode + 1) % ViewModeCount;
                        }

                        // LEFT = poprzedni widok (w lewo)
                        if(event.key == InputKeyLeft) {
                            if(app->view_mode == 0) {
                                app->view_mode = ViewModeCount - 1;
                            } else {
                                app->view_mode--;
                            }
                        }
                    }
                }
            }
        }

        // === TYLKO DLA RUNNING MODE ===
        if(app->scene == SceneRunning) {
            //Sprawdzamy czy dane wciąż napływają (timeout 4 sekundy - bezpieczny dla BLE)
            if(app->bt_state == BtStateRecieving &&
               (furi_hal_rtc_get_timestamp() - app->last_packet > 4)) {
                app->bt_state = BtStateLost;
                FURI_LOG_W(TAG, "Waiting for data - no data for 4 seconds");
            }

            uint32_t now = furi_get_tick();

            // Obsługa alarmów (tylko gdy są dane) - sprawdzamy co 50ms dla szybszej reakcji
            if(app->bt_state == BtStateRecieving && (now - last_update >= 50)) {
                handle_alerts(app);
            }
        }

        // Odświeżamy ekran co 100ms (10 FPS - wystarczające)
        uint32_t now = furi_get_tick();
        if(now - last_update >= 100) {
            view_port_update(app->view_port);
            last_update = now;
        }
    }

    // === CZYSZCZENIE PRZED WYJŚCIEM ===
    if(app->scene == SceneRunning && app->ble_serial_profile) {
        ble_profile_serial_set_event_callback(app->ble_serial_profile, 0, NULL, NULL);
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_keys_storage_set_default_path(app->bt);
        furi_check(bt_profile_restore_default(app->bt));
    }

    flight_monitor_free(app);

    FURI_LOG_I(TAG, "Flight Monitor closed");
    return 0;
}
