/**
 * POCSAG TX — interface Flipper Zero.
 * Écrans : menu -> RIC -> message -> prévisualisation -> simulation | émission -> résultat.
 * Les chaînes affichées sont en ASCII : les polices du Flipper n'ont pas d'accents.
 */
#include "pocsag.h"
#include "pocsag_tx.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/popup.h>
#include <storage/storage.h>
#include <ctype.h>
#include <stdlib.h>

#define SETTINGS_PATH APP_DATA_PATH("settings.bin")
#define HISTORY_PATH  APP_DATA_PATH("history.txt")
#define SIM_PATH      APP_DATA_PATH("sim.txt")
#define HISTORY_TAIL  1500u /* octets affichés (~20 dernières entrées) */

static const uint16_t bauds[] = {512, 1200, 2400};
static const char* const baud_labels[] = {"512", "1200", "2400"};
static const char* const func_labels[] = {"Auto", "0", "1", "2", "3"};
static const char* const off_on[] = {"Off", "On"};
static const char* const polarity_labels[] = {"Normale", "Inversee"};

typedef enum {
    ViewMenu,
    ViewInput,
    ViewWidget,
    ViewSettings,
    ViewPopup
} ViewId;

typedef enum {
    ScrMenu,
    ScrRic,
    ScrMsg,
    ScrPreview,
    ScrResult,
    ScrSettings,
    ScrTx,
} Screen;

typedef enum {
    EvNumeric,
    EvAlpha,
    EvHistory,
    EvSettings,
    EvInputDone,
    EvSimulate,
    EvTransmit,
} Event;

typedef struct {
    uint8_t freq; /* index dans pocsag_freqs */
    uint8_t baud; /* index dans bauds */
    uint8_t func; /* 0 = auto (0 numérique / 3 alpha), sinon func-1 */
    uint8_t invert;
    uint8_t tx_enabled; /* Off par défaut : simulation uniquement */
} Settings;

typedef struct {
    ViewDispatcher* vd;
    Submenu* menu;
    TextInput* input;
    Widget* widget;
    VariableItemList* settings_list;
    Popup* popup;
    Storage* storage;
    FuriString* text;

    Settings s;
    Screen screen;
    PocsagType type;
    char ric_str[8];
    char msg[POCSAG_MSG_MAX + 1];
    uint32_t ric;
    uint32_t cw[POCSAG_MAX_WORDS];
    size_t n;
    uint32_t tx_start_tick;
    uint32_t cooldown_until; /* tick */
} App;

/* ---------- utilitaires ---------- */

static uint8_t effective_func(App* app) {
    if(app->s.func) return app->s.func - 1;
    return app->type == PocsagAlpha ? 3 : 0;
}

/* Le clavier du Flipper n'a pas d'espace : '_' en tient lieu. Numérique en majuscules. */
static char normalize(PocsagType type, char c) {
    if(c == '_') return ' ';
    return type == PocsagNumeric ? (char)toupper((unsigned char)c) : c;
}

static void append_line(App* app, const char* path, const char* line) {
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_write(f, line, strlen(line));
        storage_file_write(f, "\n", 1);
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void log_history(App* app, const char* mode) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    FuriString* line = furi_string_alloc_printf(
        "%02u/%02u %02u:%02u %s %s %s %lu %c:%s",
        dt.day,
        dt.month,
        dt.hour,
        dt.minute,
        mode,
        pocsag_freqs[app->s.freq].label,
        baud_labels[app->s.baud],
        app->ric,
        app->type == PocsagAlpha ? 'A' : 'N',
        app->msg);
    append_line(app, HISTORY_PATH, furi_string_get_cstr(line));
    furi_string_free(line);
}

static void settings_load(App* app) {
    File* f = storage_file_alloc(app->storage);
    Settings s;
    if(storage_file_open(f, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_read(f, &s, sizeof(s)) == sizeof(s) && s.freq < pocsag_freqs_count &&
       s.baud < COUNT_OF(bauds) && s.func < COUNT_OF(func_labels)) {
        app->s = s;
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void settings_save(App* app) {
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(f, &app->s, sizeof(app->s));
    storage_file_close(f);
    storage_file_free(f);
}

static void send_event(void* ctx, uint32_t ev) {
    view_dispatcher_send_custom_event(((App*)ctx)->vd, ev);
}

static void input_done(void* ctx) {
    send_event(ctx, EvInputDone);
}

/* ---------- écrans ---------- */

static void show_menu(App* app) {
    app->screen = ScrMenu;
    view_dispatcher_switch_to_view(app->vd, ViewMenu);
}

static void show_text(App* app, Screen scr, const char* text) {
    app->screen = scr;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
    view_dispatcher_switch_to_view(app->vd, ViewWidget);
}

static bool validate_ric(const char* text, FuriString* error, void* ctx) {
    UNUSED(ctx);
    char* end;
    unsigned long v = strtoul(text, &end, 10);
    if(!*text || *end || v > POCSAG_RIC_MAX) {
        furi_string_set(error, "RIC: chiffres\n0 a 2097151");
        return false;
    }
    return true;
}

static bool validate_msg(const char* text, FuriString* error, void* ctx) {
    App* app = ctx;
    for(const char* c = text; *c; c++) {
        if(!pocsag_char_valid(app->type, normalize(app->type, *c))) {
            furi_string_set(
                error,
                app->type == PocsagNumeric ? "Autorise:\n0-9 * U - ( ) _" : "ASCII\nuniquement");
            return false;
        }
    }
    return true;
}

static void show_input(App* app, Screen scr) {
    app->screen = scr;
    text_input_reset(app->input);
    if(scr == ScrRic) {
        text_input_set_header_text(app->input, "RIC (0-2097151)");
        text_input_set_validator(app->input, validate_ric, app);
        text_input_set_result_callback(
            app->input, input_done, app, app->ric_str, sizeof(app->ric_str), false);
    } else {
        text_input_set_header_text(
            app->input,
            app->type == PocsagNumeric ? "Message numerique" : "Message alpha (_=espace)");
        text_input_set_validator(app->input, validate_msg, app);
        text_input_set_result_callback(
            app->input, input_done, app, app->msg, sizeof(app->msg), true);
    }
    text_input_set_minimum_length(app->input, 1);
    view_dispatcher_switch_to_view(app->vd, ViewInput);
}

static void preview_button(GuiButtonType button, InputType type, void* ctx) {
    if(type != InputTypeShort) return;
    send_event(ctx, button == GuiButtonTypeRight ? EvTransmit : EvSimulate);
}

static void append_codewords(App* app, FuriString* s) {
    for(size_t i = 0; i < app->n; i++)
        furi_string_cat_printf(s, "%08lX%c", app->cw[i], i % 2 ? '\n' : ' ');
}

static void show_preview(App* app) {
    for(char* c = app->msg; *c; c++)
        *c = normalize(app->type, *c);
    app->ric = strtoul(app->ric_str, NULL, 10);
    app->n = pocsag_encode(
        app->ric, effective_func(app), app->type, app->msg, app->cw, POCSAG_MAX_WORDS);
    if(!app->n) {
        show_text(app, ScrResult, "\e#Erreur\nEncodage impossible");
        return;
    }
    uint32_t bits = POCSAG_PREAMBLE_BITS + app->n * 32;
    uint32_t ms = bits * 1000 / bauds[app->s.baud];
    furi_string_printf(
        app->text,
        "RIC %lu %s F%u\n%s MHz %s bd\n%u mots, %lu.%02lu s\n> %s\n\n",
        app->ric,
        app->type == PocsagAlpha ? "Alpha" : "Num",
        effective_func(app),
        pocsag_freqs[app->s.freq].label,
        baud_labels[app->s.baud],
        app->n,
        ms / 1000,
        ms % 1000 / 10,
        app->msg);
    append_codewords(app, app->text);

    app->screen = ScrPreview;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 51, furi_string_get_cstr(app->text));
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Simuler", preview_button, app);
    if(app->s.tx_enabled)
        widget_add_button_element(app->widget, GuiButtonTypeRight, "Emettre", preview_button, app);
    view_dispatcher_switch_to_view(app->vd, ViewWidget);
}

static void do_simulate(App* app) {
    FuriString* s = furi_string_alloc();
    furi_string_printf(
        s,
        "# RIC %lu F%u %c %s bd : %s\n",
        app->ric,
        effective_func(app),
        app->type == PocsagAlpha ? 'A' : 'N',
        baud_labels[app->s.baud],
        app->msg);
    append_codewords(app, s);
    append_line(app, SIM_PATH, furi_string_get_cstr(s));
    furi_string_free(s);
    log_history(app, "SIM");
    show_text(
        app,
        ScrResult,
        "\e#Simulation OK\nAucune emission radio.\nTrames ajoutees a\napps_data/pocsag/sim.txt");
}

static void do_transmit(App* app) {
    uint32_t now = furi_get_tick();
    if((int32_t)(app->cooldown_until - now) > 0) {
        furi_string_printf(
            app->text,
            "\e#Duty cycle 10%%\nAttendre encore %lu s",
            (app->cooldown_until - now) / 1000 + 1);
        show_text(app, ScrResult, furi_string_get_cstr(app->text));
        return;
    }
    const PocsagFreq* f = &pocsag_freqs[app->s.freq];
    if(!pocsag_tx_start(f->hz, bauds[app->s.baud], app->s.invert, app->cw, app->n)) {
        furi_string_printf(app->text, "\e#Emission refusee\n%s", pocsag_tx_last_error());
        show_text(app, ScrResult, furi_string_get_cstr(app->text));
        return;
    }
    app->tx_start_tick = now;
    app->screen = ScrTx;
    popup_reset(app->popup);
    popup_set_header(app->popup, "Emission...", 64, 20, AlignCenter, AlignCenter);
    popup_set_text(app->popup, "Retour = annuler", 64, 40, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->vd, ViewPopup);
}

/* Fin (ou annulation) d'émission : libère la radio et arme la garde de duty cycle. */
static void finish_transmit(App* app, bool cancelled) {
    pocsag_tx_stop();
    uint32_t elapsed = furi_get_tick() - app->tx_start_tick;
    /* ponytail: garde globale (toutes bandes 10 %), par fréquence si plusieurs bandes utilisées en alternance */
    if(pocsag_freqs[app->s.freq].duty_10pct) app->cooldown_until = furi_get_tick() + elapsed * 9;
    log_history(app, cancelled ? "ANNUL" : "TX");
    show_text(app, ScrResult, cancelled ? "\e#Emission annulee" : "\e#Emission terminee");
}

static void show_history(App* app) {
    File* f = storage_file_alloc(app->storage);
    furi_string_reset(app->text);
    if(storage_file_open(f, HISTORY_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(f);
        if(size > HISTORY_TAIL) storage_file_seek(f, size - HISTORY_TAIL, true);
        char buf[65];
        size_t r;
        while((r = storage_file_read(f, buf, sizeof(buf) - 1)) > 0) {
            buf[r] = 0;
            furi_string_cat_str(app->text, buf);
        }
        if(size > HISTORY_TAIL) { /* retire la première ligne tronquée */
            size_t nl = furi_string_search_char(app->text, '\n', 0);
            if(nl != FURI_STRING_FAILURE) furi_string_right(app->text, nl + 1);
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    if(furi_string_empty(app->text)) furi_string_set(app->text, "Historique vide");
    show_text(app, ScrResult, furi_string_get_cstr(app->text));
}

/* ---------- paramètres ---------- */

static void setting_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t i = variable_item_get_current_value_index(item);
    uint8_t row = variable_item_list_get_selected_item_index(app->settings_list);
    const char* label = NULL;
    switch(row) {
    case 0:
        app->s.freq = i;
        label = pocsag_freqs[i].label;
        break;
    case 1:
        app->s.baud = i;
        label = baud_labels[i];
        break;
    case 2:
        app->s.func = i;
        label = func_labels[i];
        break;
    case 3:
        app->s.invert = i;
        label = polarity_labels[i];
        break;
    default:
        app->s.tx_enabled = i;
        label = off_on[i];
        break;
    }
    variable_item_set_current_value_text(item, label);
}

static void
    add_setting(App* app, const char* name, uint8_t count, uint8_t value, const char* text) {
    VariableItem* it =
        variable_item_list_add(app->settings_list, name, count, setting_changed, app);
    variable_item_set_current_value_index(it, value);
    variable_item_set_current_value_text(it, text);
}

static void show_settings(App* app) {
    variable_item_list_reset(app->settings_list);
    add_setting(
        app, "Frequence", pocsag_freqs_count, app->s.freq, pocsag_freqs[app->s.freq].label);
    add_setting(app, "Debit", COUNT_OF(bauds), app->s.baud, baud_labels[app->s.baud]);
    add_setting(app, "Fonction", COUNT_OF(func_labels), app->s.func, func_labels[app->s.func]);
    add_setting(app, "Polarite", 2, app->s.invert, polarity_labels[app->s.invert]);
    add_setting(app, "Emission radio", 2, app->s.tx_enabled, off_on[app->s.tx_enabled]);
    app->screen = ScrSettings;
    view_dispatcher_switch_to_view(app->vd, ViewSettings);
}

/* ---------- boucle d'événements ---------- */

static bool on_custom(void* ctx, uint32_t ev) {
    App* app = ctx;
    switch(ev) {
    case EvNumeric:
    case EvAlpha:
        app->type = ev == EvAlpha ? PocsagAlpha : PocsagNumeric;
        show_input(app, ScrRic);
        break;
    case EvHistory:
        show_history(app);
        break;
    case EvSettings:
        show_settings(app);
        break;
    case EvInputDone:
        if(app->screen == ScrRic)
            show_input(app, ScrMsg);
        else
            show_preview(app);
        break;
    case EvSimulate:
        do_simulate(app);
        break;
    case EvTransmit:
        do_transmit(app);
        break;
    }
    return true;
}

static bool on_back(void* ctx) {
    App* app = ctx;
    switch(app->screen) {
    case ScrMenu:
        return false; /* quitte l'application */
    case ScrTx:
        finish_transmit(app, true);
        return true;
    case ScrMsg:
        show_input(app, ScrRic);
        return true;
    case ScrSettings:
        settings_save(app);
        /* fall through */
    default:
        show_menu(app);
        return true;
    }
}

static void on_tick(void* ctx) {
    App* app = ctx;
    if(app->screen == ScrTx && pocsag_tx_is_done()) finish_transmit(app, false);
}

static uint32_t back_to_dispatcher(void* ctx) {
    UNUSED(ctx);
    return VIEW_IGNORE; /* delegue le Back au navigation_event_callback (on_back) */
}

int32_t pocsag_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->s.baud = 1; /* 1200 bd */
    app->storage = furi_record_open(RECORD_STORAGE);
    app->text = furi_string_alloc();
    settings_load(app);

    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_custom_event_callback(app->vd, on_custom);
    view_dispatcher_set_navigation_event_callback(app->vd, on_back);
    view_dispatcher_set_tick_event_callback(app->vd, on_tick, furi_ms_to_ticks(100));

    app->menu = submenu_alloc();
    submenu_set_header(app->menu, "POCSAG TX");
    submenu_add_item(app->menu, "Message numerique", EvNumeric, send_event, app);
    submenu_add_item(app->menu, "Message alphanumerique", EvAlpha, send_event, app);
    submenu_add_item(app->menu, "Historique", EvHistory, send_event, app);
    submenu_add_item(app->menu, "Parametres", EvSettings, send_event, app);
    app->input = text_input_alloc();
    app->widget = widget_alloc();
    app->settings_list = variable_item_list_alloc();
    app->popup = popup_alloc();

    View* views[] = {
        submenu_get_view(app->menu),
        text_input_get_view(app->input),
        widget_get_view(app->widget),
        variable_item_list_get_view(app->settings_list),
        popup_get_view(app->popup),
    };
    for(size_t i = 0; i < COUNT_OF(views); i++) {
        view_set_previous_callback(views[i], back_to_dispatcher);
        view_dispatcher_add_view(app->vd, i, views[i]);
    }

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->vd, gui, ViewDispatcherTypeFullscreen);
    show_menu(app);
    view_dispatcher_run(app->vd);

    pocsag_tx_stop(); /* sécurité : jamais de radio active en sortie */
    for(size_t i = 0; i < COUNT_OF(views); i++)
        view_dispatcher_remove_view(app->vd, i);
    view_dispatcher_free(app->vd);
    submenu_free(app->menu);
    text_input_free(app->input);
    widget_free(app->widget);
    variable_item_list_free(app->settings_list);
    popup_free(app->popup);
    furi_record_close(RECORD_GUI);
    furi_string_free(app->text);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
