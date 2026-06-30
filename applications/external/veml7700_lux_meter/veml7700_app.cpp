#include "furi.h"
#include "furi_hal.h"
#include "gui/gui.h"
#include "gui/canvas.h"
#include <furi_hal_i2c.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_bus.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

// VEML7700 sensor I2C address (7-bit)
#define VEML7700_I2C_ADDR 0x10

// VEML7700 registers
#define ALS_CONF_REG          0x00
#define ALS_MEAS_RESULT_REG   0x04
#define WHITE_MEAS_RESULT_REG 0x05 // <-- added white-channel register

// Gain
#define ALS_GAIN_1_8_VAL          0x02
#define ALS_GAIN_1_4_VAL          0x01
#define ALS_GAIN_1_VAL            0x00
#define ALS_GAIN_2_VAL            0x03
// Integration Time
#define ALS_IT_100MS_VAL          0x00
// Power saving mode
#define ALS_POWER_SAVE_MODE_1_VAL 0x00

// I2C operation timeout
#define VEML7700_I2C_TIMEOUT 100

// Enumeration for managing application states (screens)
typedef enum {
    AppState_Main,
    AppState_Settings,
    AppState_About,
    AppState_StartConfirm, // Nowy stan dla ekranu potwierdzenia startu
} AppState;

// lista opcji w menu - kazda to osobna pozycja
typedef enum {
    SettingsItem_Start, // przycisk start zeby ruszyc pomiary
    SettingsItem_Address, // adres i2c sensora 0x10 albo 0x11
    SettingsItem_Gain, // gain sensora - 1/8, 1/4, 1, 2
    SettingsItem_Channel, // kanal ALS albo WHITE
    SettingsItem_RefreshRate, // jak czesto odswiezac - dodalem to zeby mozna bylo zwolnic/przyspieszyc
    SettingsItem_DarkMode, // dark mode bo fajnie wyglada
    SettingsItem_Count // to musi byc na koncu! liczy ile jest opcji
} SettingsItem;

// struktura z caloscia apki - tu trzymam wszystkie zmienne
typedef struct {
    // podstawowe rzeczy z systemu flippera
    Gui* gui; // pointer do gui
    ViewPort* view_port; // viewport czyli miejsce gdzie sie rysuje
    FuriMutex* mutex; // mutex zeby nie bylo syfu jak 2 watki chca to samo zmienic

    // stan apki
    AppState current_state; // na ktorym ekranie jestem (menu, settings etc)
    float lux_value; // aktualna wartosc luxow
    bool running; // czy apka dziala czy ma sie wylaczyc
    bool is_sensor_initialized; // czy sensor sie odezwal

    // ustawienia ktore user moze zmienic
    uint8_t settings_cursor; // na ktorej opcji jest kursor w settings
    uint8_t i2c_address; // adres czujnika domyslnie 0x10
    uint8_t als_gain; // gain: 0=1/8, 1=1/4, 2=1, 3=2
    uint8_t channel; // 0=ALS, 1=WHITE
    uint16_t refresh_rate_ms; // dodalem to - jak czesto odswiezac wynik, 100-2000ms
    bool started; // czy user kliknal start

    // dodatkowe bajery
    bool dark_mode; // dark mode bo czemu nie
} VEML7700App;

// Function to draw the main screen
static void draw_main_screen(Canvas* canvas, VEML7700App* app) {
    canvas_clear(canvas);

    // Odwrócona logika dark mode
    if(app->dark_mode) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "VEML7700 LUX Meter");

    // Secure access to sensor data
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool sensor_ok = app->is_sensor_initialized;
    float lux_value = app->lux_value;
    uint8_t channel = app->channel;
    furi_mutex_release(app->mutex);

    if(sensor_ok) {
        // Draw lux value (big)
        canvas_set_font(canvas, FontBigNumbers);
        FuriString* lux_str = furi_string_alloc();
        furi_string_printf(lux_str, "%.1f", (double)lux_value);
        canvas_draw_str_aligned(
            canvas, 64, 25, AlignCenter, AlignTop, furi_string_get_cstr(lux_str));
        furi_string_free(lux_str);

        // Draw 'lx' unit below the value
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "lx");

        // Draw channel label (ALS/WHITE)
        const char* channel_label = (channel == 0) ? "ALS" : "WHITE";
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignTop, channel_label);
    } else {
        // Draw "sensor not connected" message
        canvas_set_font(canvas, FontPrimary);
        const char* msg = "Connect sensor";
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, msg);
    }
}

// rysowanie ekranu settings
static void draw_settings_screen(Canvas* canvas, VEML7700App* app) {
    canvas_clear(canvas); // wyczysc ekran

    // tytul na gorze
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "Settings");

    //string do formatowania wartosci
    FuriString* value_str = furi_string_alloc();

    canvas_set_font(canvas, FontSecondary);

    // teksty do wyswietlania opcji
    const char* gain_values[] = {"1/8", "1/4", "1", "2"};
    const char* channel_values[] = {"ALS", "WHITE"};

    // pokazuje tylko 2 itemy na raz wiec licze od ktorego startowac
    uint8_t start_item = (app->settings_cursor / 2) * 2;

    // scrollbar z prawej strony - zeby user wiedzial gdzie jest
    uint8_t scroll_height = 40;
    uint8_t scroll_y = 15;

    uint8_t slider_height = (2 * scroll_height) / SettingsItem_Count;
    uint8_t max_slider_position = scroll_height - slider_height;

    // zabezpieczenie przed dzieleniem przez 0 (nigdy nie wiadomo)
    uint8_t denom = (SettingsItem_Count > 2) ? (SettingsItem_Count - 2) : 1;

    uint8_t slider_position = (start_item * max_slider_position) / denom;
    if(slider_position > max_slider_position) slider_position = max_slider_position;

    // rysuj scrollbar
    canvas_draw_frame(canvas, 120, scroll_y, 3, scroll_height);
    canvas_draw_box(canvas, 121, scroll_y + slider_position, 1, slider_height);

    // petla po itemach - max 2 na ekranie
    for(uint8_t i = 0; i < 2 && (start_item + i) < SettingsItem_Count; i++) {
        uint8_t current_item = start_item + i;
        uint8_t y_pos = 25 + (i * 15); // kazdy item ma 15px wysokosci

        // jak to jest zaznaczone to odwroc kolory
        if(app->settings_cursor == current_item) {
            canvas_draw_box(canvas, 0, y_pos - 4, 118, 15);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        // strzalka dla zaznaczonego
        if(app->settings_cursor == current_item) {
            canvas_draw_str(canvas, 1, y_pos + 5, ">");
        }

        // rysuj konkretna opcje
        switch(current_item) {
        case SettingsItem_Start:
            canvas_draw_str(canvas, 5, y_pos + 5, "Start");
            break;

        case SettingsItem_Address:
            canvas_draw_str(canvas, 5, y_pos + 5, "I2C Address:");
            furi_string_printf(value_str, "0x%02X", app->i2c_address);
            canvas_draw_str_aligned(
                canvas, 113, y_pos - 1, AlignRight, AlignTop, furi_string_get_cstr(value_str));
            break;

        case SettingsItem_Gain:
            canvas_draw_str(canvas, 5, y_pos + 5, "Gain:");
            canvas_draw_str_aligned(
                canvas, 113, y_pos - 1, AlignRight, AlignTop, gain_values[app->als_gain]);
            break;

        case SettingsItem_Channel:
            canvas_draw_str(canvas, 5, y_pos + 5, "Channel:");
            canvas_draw_str_aligned(
                canvas, 113, y_pos - 1, AlignRight, AlignTop, channel_values[app->channel]);
            break;

        case SettingsItem_RefreshRate:
            // moja nowa opcja - refresh rate
            canvas_draw_str(canvas, 5, y_pos + 5, "Refresh:");
            furi_string_printf(value_str, "%ums", app->refresh_rate_ms);
            canvas_draw_str_aligned(
                canvas, 113, y_pos - 1, AlignRight, AlignTop, furi_string_get_cstr(value_str));
            break;

        case SettingsItem_DarkMode:
            canvas_draw_str(canvas, 5, y_pos + 5, "Dark Mode:");
            canvas_draw_str_aligned(
                canvas, 113, y_pos - 1, AlignRight, AlignTop, app->dark_mode ? "(*)" : "( )");
            break;
        }

        canvas_set_color(canvas, ColorBlack);
    }

    furi_string_free(value_str);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Back");
}

// Function to draw the about screen
static void draw_about_screen(Canvas* canvas, VEML7700App* app) {
    UNUSED(app); // Mark parameter as unused
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "About");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignTop, "VEML7700 Application");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignTop, "Author: Dr Mosfet");

    // Back button
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Back");
}

// Dodaj funkcję rysowania ekranu potwierdzenia
static void draw_start_confirm_screen(Canvas* canvas, VEML7700App* app) {
    UNUSED(app);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, "Start Measurement?");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignBottom, "[Ok] Start [Back] Cancel");
}

// Main drawing function that switches screens
static void veml7700_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    VEML7700App* app = static_cast<VEML7700App*>(context);

    switch(app->current_state) {
    case AppState_Main:
        draw_main_screen(canvas, app);
        break;
    case AppState_Settings:
        draw_settings_screen(canvas, app);
        break;
    case AppState_About:
        draw_about_screen(canvas, app);
        break;
    case AppState_StartConfirm:
        draw_start_confirm_screen(canvas, app);
        break;
    }
}

// Function to configure the VEML7700 sensor
static bool init_veml7700(VEML7700App* app) {
    uint8_t als_gain_val;
    switch(app->als_gain) {
    case 0:
        als_gain_val = ALS_GAIN_1_8_VAL;
        break;
    case 1:
        als_gain_val = ALS_GAIN_1_4_VAL;
        break;
    case 2:
        als_gain_val = ALS_GAIN_1_VAL;
        break;
    case 3:
        als_gain_val = ALS_GAIN_2_VAL;
        break;
    default:
        als_gain_val = ALS_GAIN_1_VAL; // Default value
        break;
    }

    uint16_t als_conf_data = (als_gain_val << 11) | (ALS_IT_100MS_VAL << 6) |
                             (ALS_POWER_SAVE_MODE_1_VAL << 1);

    uint8_t write_buffer[3];
    write_buffer[0] = ALS_CONF_REG; // Configuration register
    write_buffer[1] = als_conf_data & 0xFF; // Low byte
    write_buffer[2] = (als_conf_data >> 8) & 0xFF; // High byte

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool success = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1,
        false,
        write_buffer,
        3,
        FuriHalI2cBeginStart,
        FuriHalI2cEndStop,
        VEML7700_I2C_TIMEOUT);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return success;
}

// Function to read data from the sensor
static bool read_veml7700(VEML7700App* app) {
    uint8_t raw_data[2];
    // choose register depending on selected channel
    uint8_t reg_addr = (app->channel == 1) ? WHITE_MEAS_RESULT_REG : ALS_MEAS_RESULT_REG;

    // Acquire I2C bus access
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    // Attempt to send register address
    bool success_tx = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        app->i2c_address << 1, // Use address from settings
        false,
        &reg_addr,
        1,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        VEML7700_I2C_TIMEOUT);

    // Attempt to read data
    bool success_rx = false;
    if(success_tx) {
        success_rx = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            app->i2c_address << 1, // Use address from settings
            false,
            raw_data,
            2,
            FuriHalI2cBeginRestart,
            FuriHalI2cEndStop,
            VEML7700_I2C_TIMEOUT);
    }

    // Release I2C bus
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(success_tx && success_rx) {
        uint16_t als_value = (static_cast<uint16_t>(raw_data[1]) << 8) | raw_data[0];
        furi_mutex_acquire(app->mutex, FuriWaitForever);

        // Apply compensation factor based on the set gain
        float gain_compensation;
        switch(app->als_gain) {
        case 0: // 1/8
            gain_compensation = 0.005572f;
            break;
        case 1: // 1/4
            gain_compensation = 0.011144f;
            break;
        case 2: // 1
            gain_compensation = 0.044576f;
            break;
        case 3: // 2
            gain_compensation = 0.089152f;
            break;
        default:
            gain_compensation = 0.044576f; // Default for GAIN 1
            break;
        }

        app->lux_value = als_value * gain_compensation;
        app->is_sensor_initialized = true;
        furi_mutex_release(app->mutex);
        return true;
    }

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->is_sensor_initialized = false;
    furi_mutex_release(app->mutex);
    return false;
}

// obsluga przyciskow
static void veml7700_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    VEML7700App* app = static_cast<VEML7700App*>(context);

    // krotkie i powtarzalne nacisniecia
    if(input_event->type == InputTypeShort || input_event->type == InputTypeRepeat) {
        switch(app->current_state) {
        case AppState_Main:
            if(input_event->key == InputKeyOk) {
                app->current_state = AppState_Settings;
            } else if(input_event->key == InputKeyBack) {
                // krotkie back wraca do settings jesli user byl tam wczesniej
                app->current_state = AppState_Settings;
            } else if(input_event->key == InputKeyRight) {
                app->current_state = AppState_About;
            }
            break;
        case AppState_Settings:
            if(input_event->key == InputKeyUp) {
                if(app->settings_cursor > 0) {
                    app->settings_cursor--;
                    // Przeskok do ostatniej pozycji po dojściu do góry
                    if(app->settings_cursor == 0xFF) {
                        app->settings_cursor = SettingsItem_Count - 1;
                    }
                }
            } else if(input_event->key == InputKeyDown) {
                app->settings_cursor++;
                // Przeskok do pierwszej pozycji po dojściu do dołu
                if(app->settings_cursor >= SettingsItem_Count) {
                    app->settings_cursor = 0;
                }
            } else if(input_event->key == InputKeyLeft || input_event->key == InputKeyRight) {
                // lewo/prawo zmienia wartosci

                if(app->settings_cursor == SettingsItem_Address) {
                    // przelacz adres miedzy 0x10 a 0x11
                    app->i2c_address = (app->i2c_address == 0x10) ? 0x11 : 0x10;

                } else if(app->settings_cursor == SettingsItem_Gain) {
                    // gain 0-3 z zawijaniem
                    if(input_event->key == InputKeyLeft) {
                        if(app->als_gain > 0) {
                            app->als_gain--;
                        } else {
                            app->als_gain = 3; // wroc do maks
                        }
                    } else {
                        if(app->als_gain < 3) {
                            app->als_gain++;
                        } else {
                            app->als_gain = 0; // wroc do 0
                        }
                    }

                } else if(app->settings_cursor == SettingsItem_Channel) {
                    // toggle channel
                    app->channel = (app->channel == 0) ? 1 : 0;

                } else if(app->settings_cursor == SettingsItem_RefreshRate) {
                    // refresh rate - dodalem to zeby mozna bylo dostosowac predkosc
                    if(input_event->key == InputKeyLeft) {
                        if(app->refresh_rate_ms > 100) {
                            app->refresh_rate_ms -= 100; // -100ms
                        } else {
                            app->refresh_rate_ms = 2000; // jak juz na minimalnym to wroc na max
                        }
                    } else {
                        if(app->refresh_rate_ms < 2000) {
                            app->refresh_rate_ms += 100; // +100ms
                        } else {
                            app->refresh_rate_ms = 100; // wrap around
                        }
                    }
                }
            } else if(input_event->key == InputKeyOk) {
                if(app->settings_cursor == SettingsItem_Start) {
                    app->current_state = AppState_StartConfirm;
                } else if(app->settings_cursor == SettingsItem_DarkMode) {
                    app->dark_mode = !app->dark_mode;
                }
            } else if(input_event->key == InputKeyBack) {
                // krotkie back wraca do main
                app->current_state = AppState_Main;
            }
            break;
        case AppState_About:
            if(input_event->key == InputKeyOk || input_event->key == InputKeyBack) {
                app->current_state = AppState_Main;
            }
            break;
        case AppState_StartConfirm:
            if(input_event->key == InputKeyOk) {
                app->started = true;
                app->is_sensor_initialized = false;
                app->current_state = AppState_Main;
            } else if(input_event->key == InputKeyBack) {
                app->current_state = AppState_Settings;
            }
            break;
        }
    }

    // dlugie nacisniecie - wyjscie z apki
    if(input_event->type == InputTypeLong) {
        if(input_event->key == InputKeyBack) {
            // dlugie back zawsze wychodzi z apki niezaleznie od ekranu
            app->running = false;
        }
    }
}

// inicjalizacja apki
static VEML7700App* veml7700_app_alloc() {
    // alokuj pamiec na strukture
    VEML7700App* app = static_cast<VEML7700App*>(malloc(sizeof(VEML7700App)));
    furi_assert(app); // jak sie nie uda to niech crashuje

    // mutex do ochrony przed race conditions
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // otworz GUI
    app->gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));

    // viewport to miejsce gdzie rysuje
    app->view_port = view_port_alloc();

    // ustaw callbacki
    view_port_draw_callback_set(app->view_port, veml7700_draw_callback, app);
    view_port_input_callback_set(app->view_port, veml7700_input_callback, app);

    // dodaj do gui fullscreen
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->running = true;
    app->current_state = AppState_Settings; // zaczynam w settings zeby user widzial start
    app->lux_value = 0.0f;
    app->is_sensor_initialized = false;

    // defaultowe ustawienia
    app->dark_mode = false;
    app->i2c_address = VEML7700_I2C_ADDR; // 0x10
    app->als_gain = 2; // gain=1
    app->channel = 0; // ALS
    app->refresh_rate_ms = 500; // 500ms wydaje sie ok na start, da sie pozniej zmienic
    app->started = false;
    app->settings_cursor = 0;

    return app;
}

// Free resources
static void veml7700_app_free(VEML7700App* app) {
    furi_assert(app);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(app->mutex);
    free(app);
}

// main loop apki
extern "C" int32_t veml7700_app(void* p) {
    UNUSED(p);

    VEML7700App* app = veml7700_app_alloc();

    while(app->running) {
        // czytaj z sensora tylko jak user kliknal start
        if(app->started) {
            if(!app->is_sensor_initialized) {
                app->is_sensor_initialized = init_veml7700(app);
            }
            if(app->is_sensor_initialized) {
                read_veml7700(app);
            }
        }

        view_port_update(app->view_port); // odswiez ekran

        // tutaj uzywam refresh_rate_ms zamiast hardcodowanego 500
        // user moze sobie ustawic jak szybko ma odswiezac
        furi_delay_ms(app->refresh_rate_ms);
    }

    veml7700_app_free(app);
    return 0;
}
