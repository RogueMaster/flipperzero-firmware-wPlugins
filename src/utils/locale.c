#include "locale.h"

#include "../api_caller.h"

#include <flipper_format/flipper_format.h>
#include <storage/storage.h>

#define LOCALE_SETTINGS_FILETYPE "ApiCaller Settings"
#define LOCALE_SETTINGS_VERSION  1
#define LOCALE_SETTINGS_KEY_LANG "Language"
#define LOCALE_SETTINGS_LANG_IT  "IT"
#define LOCALE_SETTINGS_LANG_EN  "EN"

// Keep the exact LocKey order from locale.h
static const char* const locale_en_strings[LocKeyCount] = {
    "Connection",
    "Add call",
    "Call list",

    "Scan networks",
    "Saved networks",
    "Connected to",
    "Disconnect",
    "Disconnected",
    "Wi-Fi password",
    "Connected successfully!\nPress BACK.",
    "Connection failed.\n%s\n\nPress BACK.",

    "No networks found",
    "Searching...",
    "Retrying automatically...",
    "Updating...",
    "Wi-Fi networks found",

    "No saved networks",

    "URL",
    "Type",
    "Method",
    "Query",
    "Headers",
    "Body",
    "Save",
    "Delete",
    "URL",
    "Query parameters",
    "Headers (JSON)",
    "Body (JSON)",

    "No saved calls",
    "Saved calls",
    "Run",
    "Edit",
    "Sending request...\nPress BACK to return to the menu.",
    "Send failed.\n%s",
    "Error\n%s",
    "%s %s\nStatus: %d\n\nResponse:\n%s",
    "(empty response)",
    "[response truncated]",
    "Sending request... %lus\nPress BACK to return to the menu.",

    "ESP32 not connected",
    "Send failed (UART)",
    "Board error",
    "Timeout (no reply)",
    "Timeout (incomplete response)",
};

static const char* const locale_it_strings[LocKeyCount] = {
    "Connessione",
    "Aggiungi chiamata",
    "Lista chiamate",

    "Ricerca reti",
    "Reti salvate",
    "Connesso a",
    "Disconnetti",
    "Disconnesso",
    "Password WiFi",
    "Connesso con successo!\nPremi BACK.",
    "Connessione fallita.\n%s\n\nPremi BACK.",

    "Nessuna rete trovata",
    "Ricerca reti...",
    "Riprovo automaticamente...",
    "Aggiornamento...",
    "Reti WiFi trovate",

    "Nessuna rete salvata",

    "URL",
    "Tipo",
    "Metodo",
    "Query",
    "Headers",
    "Body",
    "Salva",
    "Elimina",
    "URL",
    "Query parameters",
    "Headers (JSON)",
    "Body (JSON)",

    "Nessuna chiamata salvata",
    "Chiamate salvate",
    "Esegui",
    "Modifica",
    "Invio richiesta...\nPremi BACK per tornare al menu.",
    "Invio fallito.\n%s",
    "Errore\n%s",
    "%s %s\nStatus: %d\n\nRisposta:\n%s",
    "(risposta vuota)",
    "[risposta troncata]",
    "Invio richiesta... %lus\nPremi BACK per tornare al menu.",

    "ESP32 non collegato",
    "Invio fallito (UART)",
    "Errore della board",
    "Timeout (nessuna risposta)",
    "Timeout (risposta incompleta)",
};

static const char* const* locale_tables[LocLangCount] = {
    locale_en_strings,
    locale_it_strings,
};

/** Build the settings file path and make sure the app data dir exists. */
static void locale_get_path(Storage* storage, FuriString* path) {
    furi_string_set_str(path, APP_DATA_PATH("settings.txt"));
    storage_common_resolve_path_and_ensure_app_directory(storage, path);
}

static LocLang locale_lang_from_cstr(const char* value) {
    return strcmp(value, LOCALE_SETTINGS_LANG_EN) == 0 ? LocLangEn : LocLangIt;
}

void locale_init(AppContext* app) {
    furi_assert(app);

    app->locale_lang = LocLangEn;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc();
    FuriString* language = furi_string_alloc();
    uint32_t version = 0;
    bool loaded = false;

    locale_get_path(storage, path);

    do {
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) {
            break;
        }
        if(!flipper_format_read_header(file, language, &version)) {
            break;
        }
        if(!flipper_format_read_string(file, LOCALE_SETTINGS_KEY_LANG, language)) {
            break;
        }
        app->locale_lang = locale_lang_from_cstr(furi_string_get_cstr(language));
        loaded = true;
    } while(false);

    furi_string_free(language);
    furi_string_free(path);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!loaded) {
        // First launch: create the settings file with the default language
        locale_set(app, app->locale_lang);
    }
}

const char* locale_get(const AppContext* app, LocKey key) {
    furi_assert(app);
    furi_assert(key < LocKeyCount);

    const char* text = locale_tables[app->locale_lang][key];
    if(text == NULL) {
        // Missing translation: fall back to the reference language
        text = locale_en_strings[key];
    }
    return text;
}

void locale_set(AppContext* app, LocLang lang) {
    furi_assert(app);
    furi_assert(lang < LocLangCount);

    app->locale_lang = lang;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* path = furi_string_alloc();

    locale_get_path(storage, path);

    do {
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(path))) {
            break;
        }
        if(!flipper_format_write_header_cstr(
               file, LOCALE_SETTINGS_FILETYPE, LOCALE_SETTINGS_VERSION)) {
            break;
        }
        if(!flipper_format_write_string_cstr(
               file,
               LOCALE_SETTINGS_KEY_LANG,
               lang == LocLangEn ? LOCALE_SETTINGS_LANG_EN : LOCALE_SETTINGS_LANG_IT)) {
            break;
        }
    } while(false);

    flipper_format_free(file);
    furi_string_free(path);
    furi_record_close(RECORD_STORAGE);
}
