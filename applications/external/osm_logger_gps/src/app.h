#pragma once
#include <furi.h>
#include <gui/modules/menu.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <expansion/expansion.h>

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

#define GPS_RX_BUF_SIZE 256

typedef struct App {
    // UI
    Gui* gui;
    ViewDispatcher* dispatcher;
    Menu* menu;              // menu principal (avec icônes)
    Submenu* category_menu;  // liste des catégories (premier niveau)
    Submenu* preset_menu;    // liste des presets OSM filtrés par catégorie
    uint8_t current_category; // catégorie courante
    Submenu* last_points_menu; // vue "Last points" (browse + undo)
    TextInput* note_input;   // éditeur de note courte
    NotificationApp* notification;

    // Expansion service (désactivé le temps qu'on utilise l'UART GPS)
    Expansion* expansion;

    // UART handle
    struct FuriHalSerialHandle* serial;

    // Données GPS
    bool has_fix;
    float lat;
    float lon;
    float hdop;
    float altitude;       // mètres au-dessus du geoïde (GGA)
    float heading_deg;    // cap en degrés (course over ground, RMC)
    float speed_knots;    // vitesse instantanée en nœuds (RMC champ 7)
    uint8_t sats;
    uint32_t last_fix_tick; // furi_get_tick() du dernier fix reçu (0 = jamais)
    uint32_t last_notify_tick; // dernier "fix acquired" notifié (cooldown anti-spam)
    bool pending_fix_notify; // posé en ISR à la transition no-fix -> fix, consommé dans le tick

    // Vue Quick Log
    View* quick_view;

    // Buffer de ligne pour NMEA (rempli en RX ISR)
    char nmea_line[GPS_RX_BUF_SIZE];
    size_t nmea_pos;

    // Diagnostics UART (incrémentés en ISR)
    uint32_t nmea_bytes_rx;  // total octets reçus
    uint32_t nmea_lines_rx;  // total lignes NMEA parsées

    // Note rapide optionnelle
    char quick_note[64];

    // Preset courant (index dans presets_get()) + variante active au sein de ce preset
    uint8_t current_preset;
    uint8_t current_variant;

    // Compteur de points sauvegardés depuis le démarrage de l'app
    uint16_t session_count;

    // Total cumulatif de points dans points.jsonl (rechargé au démarrage)
    uint32_t total_count;

    // --- Mode trace ---
    View* track_view;
    FuriTimer* track_timer;
    uint32_t track_points;
    uint32_t track_start_tick;
    bool track_new_segment;
    // Filtre distance : dernière position écrite (pour haversine)
    float last_trk_lat;
    float last_trk_lon;
    bool last_trk_valid;
    // Stats live
    float track_total_dist_m;   // cumul de distance entre trkpts
    float track_max_speed_kmh;  // vitesse max observée dans la session

    // --- Statut GPS ---
    View* status_view;

    // --- About ---
    View* about_view;

    // --- Settings (persistés sur SD) ---
    Settings settings;
    VariableItemList* settings_list;
    View* settings_view;

    // --- Point detail ---
    View* point_detail_view;

    // --- Preview / guard flags (temporaires, overlay Quick Log) ---
    bool preview_pending;     // setting preview_before_save déclenché
    bool duplicate_warning;   // détecté un point proche avec le même tag
    float duplicate_dist_m;   // distance au point le plus proche
    char last_error[64];      // message d'erreur à afficher en overlay (ex. "Disk full")

    // --- Dernier point sauvegardé (pour confirmation / contexte) ---
    uint32_t last_saved_tick;   // 0 = jamais sauvé cette session
    char last_saved_preset[24]; // label du preset (ex. "Bench")
    char last_saved_time[8];    // "HH:MM" du RTC au moment du save

    // --- Save différé au prochain tick (permet d'afficher "Saving..." avant le blocage I/O) ---
    bool save_deferred;
    bool save_deferred_force;

    // --- Averaging mode (collecte de N secondes de samples GPS) ---
    bool averaging;                // true pendant la collecte
    uint32_t averaging_start_tick; // tick au démarrage
    uint32_t averaging_end_tick;   // tick à atteindre pour finir
    double avg_lat_sum;            // somme des samples (double pour la précision)
    double avg_lon_sum;
    uint32_t avg_sample_count;     // nombre de samples accumulés
    float avg_min_hdop;            // meilleur HDOP observé pendant la collecte
    uint32_t avg_last_sampled_fix_tick; // dernier fix tick déjà pris en sample
} App;

void app_reconfigure_uart(App* app);

typedef enum {
    AppViewMenu = 0,
    AppViewPresets = 1,       // liste filtrée par catégorie
    AppViewQuickLog = 2,
    AppViewTrack = 3,
    AppViewStatus = 4,
    AppViewNote = 5,
    AppViewAbout = 6,
    AppViewSettings = 7,
    AppViewLastPoints = 8,
    AppViewPointDetail = 9,
    AppViewCategories = 10,   // nouveau : liste des catégories
} AppView;

