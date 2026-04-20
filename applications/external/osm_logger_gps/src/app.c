// src/app.c
#include "app.h"

#include <furi.h>
#include <gui/gui.h>
#include <storage/storage.h>
#include <furi_hal.h>

// UART (SDK récent, API handle-based)
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include "nmea.h"
#include "quick_log.h"
#include "track.h"
#include "status.h"
#include "about.h"
#include "settings.h"
#include "last_points.h"
#include "point_detail.h"
#include "presets.h"
#include "storage_helpers.h"
#include <osm_logger_icons.h>

// --- Forward
static void app_menu_callback(void* ctx, uint32_t index);
static void category_menu_callback(void* ctx, uint32_t index);
static void preset_menu_callback(void* ctx, uint32_t index);
static void rebuild_preset_menu(App* app);
static void serial_rx_callback(
    FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context);

typedef enum {
    MenuItemQuickLog = 0,
    MenuItemTrack = 1,
    MenuItemLastPoints = 2,
    MenuItemStatus = 3,
    MenuItemSettings = 4,
    MenuItemAbout = 5,
} MenuItem;

// Back depuis le menu principal -> on sort de l'app
static bool app_navigation_callback(void* ctx) {
    (void)ctx;
    return false; // false => view_dispatcher_run s'arrête
}

// Reconfigure l'UART GPS avec le baud courant des settings. Safe à appeler
// en cours de route, réinit proprement le handle série.
void app_reconfigure_uart(App* app) {
    if(!app->serial) return;
    furi_hal_serial_async_rx_stop(app->serial);
    furi_hal_serial_deinit(app->serial);
    furi_hal_serial_init(app->serial, app->settings.baud_rate);
    furi_hal_serial_async_rx_start(app->serial, serial_rx_callback, app, false);
    // Reset des compteurs de diag pour repartir propre à l'œil
    app->nmea_bytes_rx = 0;
    app->nmea_lines_rx = 0;
    app->nmea_pos = 0;
}

// Back depuis le sous-menu des presets -> retour à la liste des catégories
static uint32_t preset_menu_previous(void* ctx) {
    (void)ctx;
    return AppViewCategories;
}

// Back depuis le sous-menu des catégories -> retour au menu principal
static uint32_t category_menu_previous(void* ctx) {
    (void)ctx;
    return AppViewMenu;
}

// Tick périodique (500 ms) -> rafraîchit les vues + consomme les notifications
// posées par l'ISR (pas de call GUI depuis IRQ).
static void app_tick_callback(void* ctx) {
    App* app = (App*)ctx;
    if(app->pending_fix_notify) {
        app->pending_fix_notify = false;
        uint32_t now = furi_get_tick();
        uint32_t freq = furi_kernel_get_tick_frequency();
        uint32_t cooldown_ticks = 10 * (freq ? freq : 1);
        if(app->last_notify_tick == 0 || (now - app->last_notify_tick) >= cooldown_ticks) {
            app->last_notify_tick = now;
            notification_message(app->notification, &sequence_success);
            FURI_LOG_I("OSM", "GPS fix acquired");
        }
    }
    // Save différé : à faire ici (tick) et pas dans un input handler, pour que
    // la frame précédente ait le temps d'afficher "Saving..." avant le blocage I/O.
    quick_log_tick_deferred(app);

    quick_log_refresh(app);
    track_refresh(app);
    status_refresh(app);
}

// --- RX async callback (IRQ): NE PAS toucher au GUI depuis une IRQ
static void serial_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    (void)handle;
    App* app = (App*)context;

    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(app->serial)) {
            uint8_t c = furi_hal_serial_async_rx(app->serial);
            app->nmea_bytes_rx++;

            if(c == '\n' || c == '\r') {
                if(app->nmea_pos > 0) {
                    app->nmea_line[app->nmea_pos] = '\0';
                    app->nmea_lines_rx++;

                    bool has_fix = false;
                    // Garde les dernières valeurs connues si la trame en cours
                    // ne les fournit pas (cas courant : GGA sans fix → RMC avec
                    // fix → GGA sans fix → ...). Sans cette init, on recevait
                    // lat=lon=0 à chaque trame sans fix.
                    float lat = app->lat;
                    float lon = app->lon;
                    float hdop = app->hdop;
                    float altitude = app->altitude;
                    float heading = app->heading_deg;
                    float speed_kts = app->speed_knots;
                    uint8_t sats = app->sats;

                    nmea_parse_line(
                        app->nmea_line, &has_fix, &lat, &lon, &hdop, &sats, &altitude,
                        &heading, &speed_kts);

                    bool was_fix = app->has_fix;
                    app->has_fix = has_fix;
                    app->lat = lat;
                    app->lon = lon;
                    app->hdop = hdop;
                    app->altitude = altitude;
                    app->heading_deg = heading;
                    app->speed_knots = speed_kts;
                    app->sats = sats;
                    // Anti-fantôme : un "fix" sans coords réelles ne compte pas
                    bool real_fix = has_fix && (lat != 0.0f || lon != 0.0f);
                    if(real_fix) {
                        app->last_fix_tick = furi_get_tick();
                        if(!was_fix) app->pending_fix_notify = true;
                    }

                    app->nmea_pos = 0;
                }
            } else {
                if(app->nmea_pos < (GPS_RX_BUF_SIZE - 1)) {
                    app->nmea_line[app->nmea_pos++] = (char)c;
                } else {
                    app->nmea_pos = 0; // overflow safety
                }
            }
        }
    }
    (void)event;
}

// --- Menu principal
static void app_menu_callback(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index == MenuItemQuickLog) {
        view_dispatcher_switch_to_view(app->dispatcher, AppViewCategories);
    } else if(index == MenuItemTrack) {
        app_start_track(app);
    } else if(index == MenuItemLastPoints) {
        app_start_last_points(app);
    } else if(index == MenuItemStatus) {
        app_start_status(app);
    } else if(index == MenuItemSettings) {
        app_start_settings(app);
    } else if(index == MenuItemAbout) {
        app_start_about(app);
    }
}

// --- Sous-menu catégories : choix -> remplit preset_menu et bascule dessus
static void category_menu_callback(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index >= PresetCatCount) return;
    app->current_category = (uint8_t)index;
    rebuild_preset_menu(app);
    view_dispatcher_switch_to_view(app->dispatcher, AppViewPresets);
}

// Rebuild du submenu preset en filtrant sur la catégorie courante
static void rebuild_preset_menu(App* app) {
    submenu_reset(app->preset_menu);
    submenu_set_header(app->preset_menu, PRESET_CATEGORY_LABELS[app->current_category]);
    uint8_t n = presets_count_in_category(app->current_category);
    for(uint8_t i = 0; i < n; i++) {
        uint8_t global_idx = presets_get_index_in_category(app->current_category, i);
        if(global_idx == 0xFF) continue;
        const Preset* p = presets_get(global_idx);
        if(!p) continue;
        submenu_add_item(app->preset_menu, p->label, global_idx, preset_menu_callback, app);
    }
}

// --- Sous-menu presets : choix -> ouvre Quick Log avec ce preset
static void preset_menu_callback(void* ctx, uint32_t index) {
    App* app = ctx;
    if(index < presets_count()) {
        app->current_preset = (uint8_t)index;
        app->current_variant = 0;
    }
    app_start_quick_log(app);
}

// --- Note TextInput : validation -> retour Quick Log
static void note_input_callback(void* ctx) {
    App* app = (App*)ctx;
    view_dispatcher_switch_to_view(app->dispatcher, AppViewQuickLog);
}

// Back depuis le TextInput -> retour Quick Log (et non exit app)
static uint32_t note_input_previous(void* ctx) {
    (void)ctx;
    return AppViewQuickLog;
}

// --- Entry point
int32_t app(void* p) {
    (void)p;

    App* app = malloc(sizeof(App));
    furi_assert(app);
    memset(app, 0, sizeof(App));

    // Records
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);

    // Dispatcher
    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, app_navigation_callback);
    view_dispatcher_set_tick_event_callback(app->dispatcher, app_tick_callback, 500);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Charge les settings persistés (baud rate, intervalle trace) -> defaults si absent
    settings_load(&app->settings);

    // Init des presets (depuis SD si dispo, sinon defaults)
    presets_init();

    // Total cumulatif de points (compte les lignes de points.jsonl existant)
    app->total_count = storage_count_saved_points();

    // Menu principal (Menu module Flipper avec icônes)
    app->menu = menu_alloc();
    menu_add_item(
        app->menu, "Quick Log", &I_icon_quicklog_10x10,
        MenuItemQuickLog, app_menu_callback, app);
    menu_add_item(
        app->menu, "Track mode", &I_icon_track_10x10,
        MenuItemTrack, app_menu_callback, app);
    menu_add_item(
        app->menu, "Last points", &I_icon_lastpoints_10x10,
        MenuItemLastPoints, app_menu_callback, app);
    menu_add_item(
        app->menu, "GPS status", &I_icon_status_10x10,
        MenuItemStatus, app_menu_callback, app);
    menu_add_item(
        app->menu, "Settings", &I_icon_settings_10x10,
        MenuItemSettings, app_menu_callback, app);
    menu_add_item(
        app->menu, "About", &I_icon_about_10x10,
        MenuItemAbout, app_menu_callback, app);
    View* menu_view = menu_get_view(app->menu);
    view_dispatcher_add_view(app->dispatcher, AppViewMenu, menu_view);

    // Sous-menu des catégories (premier niveau de navigation presets)
    app->category_menu = submenu_alloc();
    submenu_set_header(app->category_menu, "Category");
    for(uint8_t i = 0; i < PresetCatCount; i++) {
        if(presets_count_in_category(i) == 0) continue; // cache les catégories vides
        submenu_add_item(
            app->category_menu, PRESET_CATEGORY_LABELS[i], i, category_menu_callback, app);
    }
    View* category_view = submenu_get_view(app->category_menu);
    view_set_previous_callback(category_view, category_menu_previous);
    view_dispatcher_add_view(app->dispatcher, AppViewCategories, category_view);

    // Sous-menu des presets (second niveau, rempli dynamiquement)
    app->preset_menu = submenu_alloc();
    View* preset_view = submenu_get_view(app->preset_menu);
    view_set_previous_callback(preset_view, preset_menu_previous);
    view_dispatcher_add_view(app->dispatcher, AppViewPresets, preset_view);

    // Éditeur de note (TextInput partagé, alloué une seule fois)
    app->note_input = text_input_alloc();
    text_input_set_header_text(app->note_input, "Short note (OK to save)");
    text_input_set_result_callback(
        app->note_input,
        note_input_callback,
        app,
        app->quick_note,
        sizeof(app->quick_note),
        false /* on garde le texte précédent */);
    View* note_view = text_input_get_view(app->note_input);
    view_set_previous_callback(note_view, note_input_previous);
    view_dispatcher_add_view(app->dispatcher, AppViewNote, note_view);

    view_dispatcher_switch_to_view(app->dispatcher, AppViewMenu);

    // --- Désactiver l'Expansion service (sinon il intercepte l'UART GPS) ---
    app->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(app->expansion);

    // --- UART init (baudrate depuis les settings) ---
    app->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_hal_serial_init(app->serial, app->settings.baud_rate);
    furi_hal_serial_async_rx_start(app->serial, serial_rx_callback, app, false);

    // UI loop
    view_dispatcher_run(app->dispatcher);

    // --- Teardown ---
    if(app->serial) {
        furi_hal_serial_async_rx_stop(app->serial);
        furi_hal_serial_deinit(app->serial);
        furi_hal_serial_control_release(app->serial);
        app->serial = NULL;
    }

    if(app->expansion) {
        expansion_enable(app->expansion);
        furi_record_close(RECORD_EXPANSION);
        app->expansion = NULL;
    }

    app_stop_track(app);
    app_stop_status(app);
    app_stop_about(app);
    app_stop_settings(app);
    app_stop_last_points(app);
    app_stop_point_detail(app);

    if(app->note_input) {
        view_dispatcher_remove_view(app->dispatcher, AppViewNote);
        text_input_free(app->note_input);
        app->note_input = NULL;
    }

    if(app->quick_view) {
        view_dispatcher_remove_view(app->dispatcher, AppViewQuickLog);
        view_free(app->quick_view);
        app->quick_view = NULL;
    }
    view_dispatcher_remove_view(app->dispatcher, AppViewPresets);
    submenu_free(app->preset_menu);

    view_dispatcher_remove_view(app->dispatcher, AppViewCategories);
    submenu_free(app->category_menu);

    view_dispatcher_remove_view(app->dispatcher, AppViewMenu);
    menu_free(app->menu);

    view_dispatcher_free(app->dispatcher);

    presets_deinit();

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
    return 0;
}
