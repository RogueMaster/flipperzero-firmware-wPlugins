#pragma once

/* User-facing text. The Flipper fonts are ASCII only, so German avoids umlauts
 * by wording rather than "ae/oe/ue" spellings. */

typedef enum {
    YtLangEnglish,
    YtLangGerman,
    YtLangCount,
} YtLang;

typedef struct {
    const char* language_name;
    char thousands_separator;

    /* Cards */
    const char* subscribers;
    const char* views;
    const char* videos;
    const char* hidden;

    /* Status */
    const char* loading;
    const char* offline;
    const char* error;
    const char* not_current;
    const char* button_retry;
    const char* button_connect;
    const char* start_helper;
    const char* helper_unreachable;
    const char* request_rejected;
    const char* no_response;
    const char* response_too_large;
    const char* channel_not_found;
    const char* no_subscriber_count;

    /* Setup */
    const char* config_created;
    const char* key_missing;
    const char* channel_missing;
    const char* bridge_failed;
    const char* wifi_config_invalid;
    const char* edit_config;

    /* Info sheet */
    const char* sheet_last;
    const char* sheet_next;
    const char* sheet_connection;
    const char* just_now;
    const char* minutes_ago; /* %lu */
    const char* hours_ago; /* %lu */
    const char* never;
    const char* now;
    const char* paused;
    const char* connection_offline;
    const char* sheet_refresh;
    const char* transport_bridge;
    const char* transport_wifi;
    const char* wifi_no_board;
    const char* wifi_failed;
    const char* wifi_busy;

    /* Settings */
    const char* settings;
    const char* sound;
    const char* vibration;
    const char* led;
    const char* backlight;
    const char* interval;
    const char* language;
    const char* font;
    const char* test;
    const char* minutes; /* %lu */
    const char* hours; /* %lu */
} YtStrings;

static const YtStrings yt_strings[YtLangCount] = {
    [YtLangEnglish] =
        {
            .language_name = "English",
            .thousands_separator = ',',
            .subscribers = "Subscribers",
            .views = "Views",
            .videos = "Videos",
            .hidden = "Hidden",
            .loading = "Loading",
            .offline = "Offline",
            .error = "Error",
            .not_current = "Not up to date",
            .button_retry = "OK  Retry",
            .button_connect = "OK  Connect",
            .start_helper = "Start the desktop helper",
            .helper_unreachable = "Desktop helper not reachable",
            .request_rejected = "Request rejected",
            .no_response = "No response",
            .response_too_large = "Response too large",
            .channel_not_found = "Channel not found",
            .no_subscriber_count = "No subscriber count",
            .config_created = "Config created",
            .key_missing = "API key missing",
            .channel_missing = "Channel missing",
            .bridge_failed = "Bridge error",
            .wifi_config_invalid = "Wi-Fi: no \" or \\",
            .edit_config = "Edit config.txt",
            .sheet_last = "Updated",
            .sheet_next = "Next",
            .sheet_connection = "Connection",
            .just_now = "just now",
            .minutes_ago = "%lu min ago",
            .hours_ago = "%lu h ago",
            .never = "never",
            .now = "now",
            .paused = "paused",
            .connection_offline = "offline",
            .sheet_refresh = "OK  Refresh",
            .transport_bridge = "Bridge",
            .transport_wifi = "Wi-Fi",
            .wifi_no_board = "Wi-Fi board not found",
            .wifi_failed = "Wi-Fi connection failed",
            .wifi_busy = "GPIO UART is busy",
            .settings = "Settings",
            .sound = "Sound",
            .vibration = "Vibration",
            .led = "LED",
            .backlight = "Backlight",
            .interval = "Interval",
            .language = "Language",
            .font = "Font",
            .test = "Test alert",
            .minutes = "%lu min",
            .hours = "%lu h",
        },
    [YtLangGerman] =
        {
            .language_name = "Deutsch",
            .thousands_separator = '.',
            .subscribers = "Abonnenten",
            .views = "Aufrufe",
            .videos = "Videos",
            .hidden = "Verborgen",
            .loading = "Lade",
            .offline = "Offline",
            .error = "Fehler",
            .not_current = "Nicht aktuell",
            .button_retry = "OK  Neu laden",
            .button_connect = "OK  Verbinden",
            .start_helper = "Desktop-Helper starten",
            .helper_unreachable = "Desktop-Helper nicht erreichbar",
            .request_rejected = "Anfrage abgelehnt",
            .no_response = "Keine Antwort",
            .response_too_large = "Antwort zu lang",
            .channel_not_found = "Kanal nicht gefunden",
            .no_subscriber_count = "Keine Abozahl erhalten",
            .config_created = "Config angelegt",
            .key_missing = "API-Key fehlt",
            .channel_missing = "Kanal fehlt",
            .bridge_failed = "Bridge-Fehler",
            .wifi_config_invalid = "WLAN: kein \" oder \\",
            .edit_config = "config.txt bearbeiten",
            .sheet_last = "Zuletzt",
            .sheet_next = "Update",
            .sheet_connection = "Verbindung",
            .just_now = "gerade eben",
            .minutes_ago = "vor %lu Min",
            .hours_ago = "vor %lu Std",
            .never = "noch nie",
            .now = "jetzt",
            .paused = "pausiert",
            .connection_offline = "offline",
            .sheet_refresh = "OK  Laden",
            .transport_bridge = "Bridge",
            .transport_wifi = "WLAN",
            .wifi_no_board = "WLAN-Board nicht gefunden",
            .wifi_failed = "WLAN-Verbindung fehlgeschlagen",
            .wifi_busy = "GPIO-UART ist belegt",
            .settings = "Einstellungen",
            .sound = "Ton",
            .vibration = "Vibration",
            .led = "LED",
            .backlight = "Display an",
            .interval = "Intervall",
            .language = "Sprache",
            .font = "Schrift",
            .test = "Testen",
            .minutes = "%lu Min",
            .hours = "%lu Std",
        },
};
