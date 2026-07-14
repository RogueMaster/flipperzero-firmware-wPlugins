#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <storage/storage.h>

#define TAG                   "FlightMonitor"
#define BT_SERIAL_BUFFER_SIZE 128
#define CONFIG_FILE_PATH      APP_DATA_PATH("settings.cfg")

#define SCREEN_HEIGHT 64
#define LINE_HEIGHT   11

// Scenes
typedef enum {
    SceneMenu,
    SceneRunning,
} Scene;

// Bluetooth states
typedef enum {
    BtStateChecking,
    BtStateInactive,
    BtStateWaiting,
    BtStateRecieving,
    BtStateNoData,
    BtStateLost
} BtState;

// View modes - różne widoki danych
typedef enum {
    ViewModeAll, // Wszystkie parametry (główny widok)
    ViewModeAltitude, // Tylko wysokość - DUŻA
    ViewModeSpeed, // Tylko prędkość - DUŻA
    ViewModeVertical, // Tylko V/S - DUŻA
    ViewModeThrottle, // Tylko przepustnica - DUŻA
    ViewModeOrientation, // Pitch i Roll - DUŻE
    ViewModeCount // Liczba widoków (do nawigacji)
} ViewMode;

// Alert types - typy alarmów
typedef enum {
    AlertNone = 0,
    AlertLowAltitude, // Za nisko (< 100m)
    AlertLowSpeed, // Za wolno (stall warning)
    AlertGearWarning, // Podwozie schowane przy niskiej wysokości
    AlertOverspeed, // Za szybko
    AlertCriticalAltitude, // Krytycznie nisko (< 30m)
} AlertType;

// Configuration structure - progi alarmów
typedef struct {
    uint16_t altitude_alarm; // Próg alarmu wysokości (domyślnie 120m)
    uint16_t gear_warning_alt; // Wysokość dla alarmu podwozia (domyślnie 150m)
    uint16_t gear_retract_alt; // Wysokość dla "chowaj podwozie" (domyślnie 200m)
    uint16_t stall_speed; // Prędkość przeciągnięcia (domyślnie 100 km/h)
    uint16_t overspeed; // Prędkość overspeed (domyślnie 700 km/h)
    bool enable_gear_warnings; // Włącz/wyłącz alarmy podwozia (dla samolotów z podwoziem stałym)
} FlightConfig;

// Structure for aircraft data
// Struktura przechowująca dane samolotu
#pragma pack(push, 1)
typedef struct {
    int16_t altitude; // Wysokość w metrach
    uint16_t speed; // Prędkość w km/h
    int8_t throttle; // Przepustnica 0-100%
    int16_t vertical_speed; // Prędkość wznoszenia m/s (x10)
    int8_t pitch; // Pochylenie -90 do +90 stopni
    int8_t roll; // Przechylenie -180 do +180 stopni
    uint8_t flaps; // Klapy 0-100%
    uint8_t gear; // Podwozie 0=schowane, 1=wypuszczone
    int16_t engine_power; // Moc silnika w HP (0 = silnik padł!)
    int16_t fuel; // Paliwo w kg (0-10000)
} FlightData;
#pragma pack(pop)

// Main app structure
typedef struct {
    Bt* bt;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    NotificationApp* notification;
    void* ble_serial_profile;

    Scene scene;
    VariableItemList* variable_item_list;
    VariableItem* items[5]; // 5 ustawień
    uint8_t menu_cursor; // Cursor for menu items

    BtState bt_state;
    FlightData data;
    FlightConfig config;
    uint32_t last_packet;
    ViewMode view_mode; // Aktualny tryb widoku
    AlertType current_alert; // Aktualny alarm
    uint32_t last_beep; // Ostatnie piknięcie (dla timingu)
    FlightData last_data; // Poprzednie dane (do wykrywania ruchu)
    bool data_changing; // Czy dane się zmieniają (wykrywa crash)
    bool show_splash; // Czy pokazać splash screen na starcie
} FlightMonitorApp;

// Function declarations
void draw_flight_view(Canvas* canvas, FlightMonitorApp* app);
void draw_connect_view(Canvas* canvas);
void draw_status_view(Canvas* canvas, FlightMonitorApp* app);
