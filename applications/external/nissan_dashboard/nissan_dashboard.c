/*
 * Nissan TPMS Dashboard - Flipper Zero
 *
 * Single app with main menu:
 *   Monitor       receives at 433.92 MHz and displays pressure/temperature
 *   Transmit      sends TPMS frames using the selected pressure for each wheel
 *   Pair sensors  discovers nearby sensors and assigns each one to a wheel
 *   Settings      edits sensor IDs and saves, loads, or deletes vehicle profiles
 *   About         version and author
 *
 * Formato frame (protocollo "Renault" di TPMS-Editor), FSK, Manchester, 433.92 MHz:
 *
 *   preambolo: "01" x15 + "10", simboli da 50 us
 *   dati (9 byte, Manchester: 1 = "10", 0 = "01", MSB first):
 *     [0] bit7..2 = flag , bit1..0 = pressione bit 9..8
 *     [1] pressione bit 7..0          kPa = valore * 0.75
 *     [2] temperatura + 30            (C)
 *     [3..5] ID sensore (3 byte)
 *     [6] byte stato 1   [7] byte stato 2
 *     [8] CRC8 (poly 0x07, init 0x00) sui byte 0..7
 *
 * Auto (una configurazione per ogni auto):
 *   /ext/apps_data/nissan_dashboard/cars/<NOME>.car
 *       ID, flag, byte di stato, temperatura e pressione di ogni ruota
 *   /ext/apps_data/nissan_dashboard/config.txt
 *       ricorda l'ultima auto usata (caricata all'avvio). Il vecchio config.txt
 *       con i soli ID (versione 1) si legge ancora.
 *
 * In TX, all'ingresso della sezione legge i profili .tpms di TPMS-Editor
 * (/ext/apps_data/tpms_editor, anche nelle sottocartelle) e prende da li' flag e
 * byte di stato delle ruote il cui ID corrisponde.
 *
 * Controls:
 *   Menu       Up/Down selects, OK opens, Back exits the app
 *              Left/Right switches between saved vehicles
 *   Monitor    Left/Right selects a wheel, hold OK to reset, Back returns to menu
 *   Transmit   Left/Right selects a wheel (FL, FR, RL, RR, RESET)
 *              Up/Down changes pressure by 1 kPa (hold for +/-5)
 *              OK sends the selected wheel; hold OK to send all wheels
 *              RESET + OK applies 230 kPa front / 210 kPa rear and sends all
 *   Pair       Up/Down selects a sensor, Left/Right assigns a wheel
 *              (FL, FR, RL, RR, --); OK saves when all four are assigned
 *              Hold OK to clear the list; Back returns to menu
 *   Settings   Up/Down selects an item, OK opens/runs it, Back returns to menu
 *              Save / Save as / Load vehicle (Left deletes a selected vehicle)
 *              ID editor: Left/Right selects a digit, Up/Down changes it
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <lib/toolbox/level_duration.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <stdio.h>
#include <string.h>

#define TAG "NissanDash"

#define FREQ_HZ      433920000UL
#define TE_US        50U
#define REPEATS      4U
#define GAP_US       10000U
#define FRAME_LEN    9U
#define MAX_PULSES   900U
#define PROFILE_ROOT EXT_PATH("apps_data/tpms_editor")

#define CONFIG_DIR      EXT_PATH("apps_data/nissan_dashboard")
#define CONFIG_PATH     CONFIG_DIR "/config.txt"
#define CONFIG_FILETYPE "Nissan Dashboard Config"
#define CONFIG_VERSION        2U
#define CONFIG_VERSION_LEGACY 1U

#define CARS_DIR        CONFIG_DIR "/cars"
#define CAR_FILETYPE    "Nissan Dashboard Car"
#define CAR_VERSION     1U
#define CAR_NAME_LEN    10U
#define CAR_WHEEL_BYTES 6U /* ID(3) + flag + byte stato 1 + byte stato 2 */
#define MAX_CARS        12U

#define MAX_PAIR     12U
#define PAIR_VISIBLE 4U
#define NO_WHEEL     0xFFU

#define N_WHEELS  4U
#define SEL_RESET 4U /* TX: quinta voce, pulsante RESET */

#define KPA_FRONT_DEFAULT 230U
#define KPA_REAR_DEFAULT  210U
#define KPA_MAX           500U

#define RX_BUF_ITEMS  2048U
#define DRAIN_MAX     1024U
#define SYNC_NORMAL   0x556UL /* simboli ...0101 0101 0110 (fine preambolo) */
#define SYNC_INVERTED 0xAA9UL /* stessa cosa a polarita' invertita */

#define MENU_ITEMS  5U
#define SET_ROWS    8U /* 4 ID + Salva + Salva con nome + Carica + Predefiniti */
#define SET_VISIBLE 5U
#define SET_SAVE    4U
#define SET_SAVEAS  5U
#define SET_LOAD    6U
#define SET_DEFAULT 7U

/* ------------------------------------------------------------------------ */
/* Sensori, in ordine: ANT SX, ANT DX, POST SX, POST DX.                      */
/* ------------------------------------------------------------------------ */
static const char* const k_names[N_WHEELS] = {"FL", "FR", "RL", "RR"};
static const char* const k_cfg_keys[N_WHEELS] = {"ANT_SX", "ANT_DX", "POST_SX", "POST_DX"};

/* ID segnaposto (usati finche' non ne imposti o accoppi di reali): cambiali da
 * Impostazioni oppure usa TPMS PAIR per ricevere e assegnare i tuoi sensori. */
static const uint8_t k_default_ids[N_WHEELS][3] = {
    {0x00, 0x00, 0x01},
    {0x00, 0x00, 0x02},
    {0x00, 0x00, 0x03},
    {0x00, 0x00, 0x04},
};

static uint8_t g_id[N_WHEELS][3];       /* ID in uso (RX e TX) */
static uint8_t g_saved[N_WHEELS][CAR_WHEEL_BYTES]; /* ultimo stato salvato/caricato (per il "*") */
static char g_car_name[CAR_NAME_LEN + 1];          /* auto attiva ("" = nessuna) */

/* ---- dati TX di ogni ruota ---------------------------------------------- */
typedef struct {
    uint8_t flags6; /* raw[0] >> 2 */
    uint8_t b6;     /* raw[6] */
    uint8_t b7;     /* raw[7] */
    uint16_t kpa;   /* pressione impostata, in kPa */
    int8_t temp;    /* gradi C */
    bool from_profile; /* flag/stato letti dal profilo .tpms salvato */
    bool locked;       /* dati arrivati da un'auto salvata o da PAIR: i .tpms non li toccano */
} TxSensor;

/* byte di stato provvisori (copiati dalle anteriori) finche' non si trova il profilo */
static const TxSensor k_tx_default[N_WHEELS] = {
    {0x36, 0x09, 0xBD, KPA_FRONT_DEFAULT, 25, false, false},
    {0x36, 0x09, 0xBD, KPA_FRONT_DEFAULT, 25, false, false},
    {0x36, 0x09, 0xBD, KPA_REAR_DEFAULT, 25, false, false},
    {0x36, 0x09, 0xBD, KPA_REAR_DEFAULT, 25, false, false},
};
static TxSensor g_tx[N_WHEELS];

/* ---- dati RX di ogni ruota ---------------------------------------------- */
typedef struct {
    bool has_data;
    uint16_t kpa;
    int16_t temp; /* gradi C */
    uint32_t last_tick;
} RxSensor;
static RxSensor g_rx[N_WHEELS];

/* Preset CC1101 (2-FSK async) da capture .sub di sensori reali: coppie registro/valore,
 * terminatore 00 00, poi PATABLE da 8 byte. */
static const uint8_t k_preset[] = {
    0x02, 0x0D, 0x0B, 0x06, 0x08, 0x32, 0x07, 0x04, 0x14, 0x00, 0x13, 0x02, 0x12, 0x04,
    0x11, 0x93, 0x10, 0x59, 0x15, 0x41, 0x18, 0x18, 0x19, 0x16, 0x1D, 0x91, 0x1C, 0x00,
    0x1B, 0x07, 0x20, 0xFB, 0x22, 0x10, 0x21, 0x56, 0x00, 0x00,
    0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* ------------------------------------------------------------------------ */
/* LED RGB: un lampo (100 ms) a ogni frame valido, con colore che cambia ogni */
/* volta: rosso, verde, blu, giallo, ciano, magenta, bianco.                  */
/* ------------------------------------------------------------------------ */
static const NotificationSequence seq_flash_red = {
    &message_red_255, &message_delay_100, &message_red_0, NULL};
static const NotificationSequence seq_flash_green = {
    &message_green_255, &message_delay_100, &message_green_0, NULL};
static const NotificationSequence seq_flash_blue = {
    &message_blue_255, &message_delay_100, &message_blue_0, NULL};
static const NotificationSequence seq_flash_yellow = {
    &message_red_255,
    &message_green_255,
    &message_delay_100,
    &message_red_0,
    &message_green_0,
    NULL};
static const NotificationSequence seq_flash_cyan = {
    &message_green_255,
    &message_blue_255,
    &message_delay_100,
    &message_green_0,
    &message_blue_0,
    NULL};
static const NotificationSequence seq_flash_magenta = {
    &message_red_255,
    &message_blue_255,
    &message_delay_100,
    &message_red_0,
    &message_blue_0,
    NULL};
static const NotificationSequence seq_flash_white = {
    &message_red_255,
    &message_green_255,
    &message_blue_255,
    &message_delay_100,
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL};

#define N_FLASH_COLORS 7U
static const NotificationSequence* const k_flash_seq[N_FLASH_COLORS] = {
    &seq_flash_red,
    &seq_flash_green,
    &seq_flash_blue,
    &seq_flash_yellow,
    &seq_flash_cyan,
    &seq_flash_magenta,
    &seq_flash_white,
};

/* ------------------------------------------------------------------------ */
/* BEGIN DECODER                                                              */
/* ------------------------------------------------------------------------ */

static uint8_t crc8_07(const uint8_t* d, size_t n) {
    uint8_t c = 0;
    for(size_t i = 0; i < n; i++) {
        c ^= d[i];
        for(int b = 0; b < 8; b++) {
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
        }
    }
    return c;
}

/* codice grezzo -> kPa (1 unita' = 0.75 kPa) */
static uint16_t raw_to_kpa(uint16_t raw) {
    return (uint16_t)(((uint32_t)raw * 3U + 2U) / 4U);
}

/* kPa -> codice grezzo */
static uint16_t kpa_to_raw(uint16_t kpa) {
    return (uint16_t)(((uint32_t)kpa * 4U + 1U) / 3U);
}

typedef struct {
    uint32_t reg; /* registro a scorrimento degli ultimi simboli */
    bool in_data;
    bool invert;
    uint8_t half;  /* 0 = attendo 1o simbolo della coppia, 1 = attendo il 2o */
    uint8_t first; /* 1o simbolo della coppia */
    uint8_t nbits;
    uint8_t frame[FRAME_LEN];
} Decoder;

static void dec_reset(Decoder* d) {
    d->reg = 0;
    d->in_data = false;
    d->half = 0;
    d->nbits = 0;
}

/* Un simbolo (mezzo bit da 50 us). Ritorna true a frame completo (72 bit). */
static bool dec_symbol(Decoder* d, uint8_t sym) {
    if(!d->in_data) {
        d->reg = (d->reg << 1U) | sym;
        uint32_t tail = d->reg & 0xFFFU;
        if(tail == SYNC_NORMAL || tail == SYNC_INVERTED) {
            d->invert = (tail == SYNC_INVERTED);
            d->in_data = true;
            d->half = 0;
            d->nbits = 0;
            memset(d->frame, 0, FRAME_LEN);
        }
        return false;
    }

    if(d->half == 0U) {
        d->first = sym;
        d->half = 1;
        return false;
    }

    d->half = 0;
    if(d->first == sym) { /* Manchester non valido: torna a cercare il preambolo */
        dec_reset(d);
        return false;
    }

    uint8_t bit = (uint8_t)(d->first ^ (d->invert ? 1U : 0U)); /* "10" = 1 */
    if(bit) d->frame[d->nbits >> 3] |= (uint8_t)(0x80U >> (d->nbits & 7U));
    d->nbits++;

    if(d->nbits >= FRAME_LEN * 8U) {
        d->in_data = false;
        d->reg = 0;
        return true;
    }
    return false;
}

/* Un impulso (livello + durata in us). Ritorna true a frame completo. */
static bool dec_pulse(Decoder* d, bool level, uint32_t dur_us) {
    uint32_t n = (dur_us + TE_US / 2U) / TE_US; /* durata in simboli */
    if(n == 0U) { /* glitch */
        dec_reset(d);
        return false;
    }
    /* Un impulso lungo (es. la pausa dopo il frame) contiene al piu' 2 simboli
     * ancora validi: li passo al decoder, poi azzero. */
    uint32_t k = (n > 2U) ? 2U : n;
    bool done = false;
    for(uint32_t i = 0; i < k && !done; i++) {
        done = dec_symbol(d, level ? 1U : 0U);
    }
    if(n > 2U && !done) dec_reset(d);
    return done;
}

/* ------------------------------------------------------------------------ */
/* END DECODER                                                                */
/* ------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------ */
/* Stato dell'app                                                             */
/* ------------------------------------------------------------------------ */
typedef enum {
    ScreenMenu,
    ScreenRx,
    ScreenTx,
    ScreenSettings,
    ScreenIdEdit,
    ScreenPair,
    ScreenCars,
    ScreenName,
    ScreenAbout,
} Screen;

/* Un sensore sentito da TPMS PAIR */
typedef struct {
    uint8_t id[3];
    uint8_t flags6; /* raw[0] >> 2 */
    uint8_t b6;
    uint8_t b7;
    uint8_t wheel; /* 0..3 ruota assegnata, NO_WHEEL = non assegnato */
    uint16_t kpa;
    int16_t temp;
} PairSensor;

typedef struct {
    Screen screen;
    bool running;
    ViewPort* vp;
    FuriMessageQueue* q;
    NotificationApp* notif;

    uint8_t menu_sel;

    /* RX */
    bool rx_active;
    FuriStreamBuffer* rxbuf;
    Decoder dec;
    uint8_t rx_sel; /* 0..3 ruota */
    uint32_t frames_ok;
    uint32_t frames_other;
    uint32_t frames_crc;
    uint8_t color_idx; /* prossimo colore del LED */

    /* TX */
    uint8_t tx_sel; /* 0..3 ruota, 4 = RESET */
    bool sending;
    char tx_msg[20];
    FuriThread* scan_thread; /* scansione profili .tpms in background (NULL = ferma) */

    /* Impostazioni */
    uint8_t set_sel;
    uint8_t set_scroll;
    char set_msg[16];

    /* Editor ID */
    uint8_t edit_wheel;
    uint8_t edit_pos; /* cifra 0..5 */
    uint8_t edit_id[3];
    char edit_msg[20];

    /* PAIR */
    PairSensor pair[MAX_PAIR];
    uint8_t pair_n;
    uint8_t pair_sel;
    uint8_t pair_scroll;
    char pair_msg[24];

    /* Elenco auto */
    char cars[MAX_CARS][CAR_NAME_LEN + 1];
    uint8_t cars_n;
    uint8_t cars_sel;
    uint8_t cars_scroll;
    bool cars_del; /* in attesa di conferma eliminazione */
    char cars_msg[16];

    /* Editor nome auto */
    char name_buf[CAR_NAME_LEN + 1];
    uint8_t name_pos;
    uint8_t name_from; /* 0 = Impostazioni, 1 = PAIR */
} App;

/* ------------------------------------------------------------------------ */
/* ID sensori e configurazione                                                */
/* ------------------------------------------------------------------------ */

static void profiles_rescan(void); /* definita nella sezione TX */

/* Assegna un nuovo ID a una ruota: i dati TX tornano provvisori (poi si
 * rileggono i profili), i dati RX ricevuti con il vecchio ID vengono azzerati. */
static void wheel_set_id(uint8_t i, const uint8_t* id) {
    memcpy(g_id[i], id, 3);
    uint16_t kpa = g_tx[i].kpa;
    g_tx[i] = k_tx_default[i];
    g_tx[i].kpa = kpa;
    memset(&g_rx[i], 0, sizeof(RxSensor));
}

static bool id_in_use(const uint8_t* id, uint8_t except) {
    for(uint8_t i = 0; i < N_WHEELS; i++) {
        if(i != except && memcmp(g_id[i], id, 3) == 0) return true;
    }
    return false;
}

/* Imposta tutti i dati di una ruota (da un'auto salvata o da PAIR). I dati sono
 * "bloccati": la scansione dei profili .tpms non li sovrascrive. */
static void wheel_apply(
    uint8_t i,
    const uint8_t* id,
    uint8_t flags6,
    uint8_t b6,
    uint8_t b7,
    uint16_t kpa,
    int8_t temp) {
    memcpy(g_id[i], id, 3);
    memset(&g_rx[i], 0, sizeof(RxSensor));
    TxSensor* s = &g_tx[i];
    s->flags6 = (uint8_t)(flags6 & 0x3FU);
    s->b6 = b6;
    s->b7 = b7;
    s->kpa = (kpa > KPA_MAX) ? (uint16_t)KPA_MAX : kpa;
    s->temp = temp;
    s->from_profile = true;
    s->locked = true;
}

/* Parametri di ogni ruota che finiscono nel file dell'auto. */
static void cfg_snapshot(uint8_t out[N_WHEELS][CAR_WHEEL_BYTES]) {
    for(uint8_t i = 0; i < N_WHEELS; i++) {
        memcpy(out[i], g_id[i], 3);
        out[i][3] = g_tx[i].flags6;
        out[i][4] = g_tx[i].b6;
        out[i][5] = g_tx[i].b7;
    }
}

static bool config_dirty(void) {
    uint8_t cur[N_WHEELS][CAR_WHEEL_BYTES];
    cfg_snapshot(cur);
    return memcmp(cur, g_saved, sizeof(cur)) != 0;
}

static void car_path(char* out, size_t n, const char* name) {
    snprintf(out, n, "%s/%s.car", CARS_DIR, name);
}

/* Salva la configurazione attuale come auto "name". */
static bool car_save(const char* name) {
    /* per le ruote non ancora "fisse" prendo flag/stato dai profili .tpms */
    profiles_rescan();

    char path[96];
    car_path(path, sizeof(path), name);

    uint8_t d[N_WHEELS][CAR_WHEEL_BYTES];
    cfg_snapshot(d);
    int32_t temp[N_WHEELS];
    uint32_t kpa[N_WHEELS];
    for(uint8_t i = 0; i < N_WHEELS; i++) {
        temp[i] = g_tx[i].temp;
        kpa[i] = g_tx[i].kpa;
    }

    Storage* st = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(st, EXT_PATH("apps_data"));
    storage_simply_mkdir(st, CONFIG_DIR);
    storage_simply_mkdir(st, CARS_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(st);
    bool ok = flipper_format_file_open_always(ff, path);
    if(ok) {
        ok = flipper_format_write_header_cstr(ff, CAR_FILETYPE, CAR_VERSION);
        for(uint8_t i = 0; ok && i < N_WHEELS; i++) {
            ok = flipper_format_write_hex(ff, k_cfg_keys[i], d[i], CAR_WHEEL_BYTES);
        }
        ok = ok && flipper_format_write_int32(ff, "TEMP", temp, N_WHEELS);
        ok = ok && flipper_format_write_uint32(ff, "KPA", kpa, N_WHEELS);
        flipper_format_file_close(ff);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(ok) memcpy(g_saved, d, sizeof(g_saved));
    return ok;
}

/* Carica l'auto "name" e la rende quella in uso. */
static bool car_load(const char* name) {
    char path[96];
    car_path(path, sizeof(path), name);

    uint8_t d[N_WHEELS][CAR_WHEEL_BYTES];
    int32_t temp[N_WHEELS] = {0};
    uint32_t kpa[N_WHEELS] = {0};
    bool have_temp = false;
    bool have_kpa = false;
    bool ok = false;

    Storage* st = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(st);
    FuriString* ft = furi_string_alloc();
    if(flipper_format_file_open_existing(ff, path)) {
        uint32_t ver = 0;
        ok = flipper_format_read_header(ff, ft, &ver) && ver == CAR_VERSION &&
             furi_string_cmp_str(ft, CAR_FILETYPE) == 0;
        for(uint8_t i = 0; ok && i < N_WHEELS; i++) {
            ok = flipper_format_rewind(ff) &&
                 flipper_format_read_hex(ff, k_cfg_keys[i], d[i], CAR_WHEEL_BYTES);
        }
        if(ok) {
            have_temp = flipper_format_rewind(ff) &&
                        flipper_format_read_int32(ff, "TEMP", temp, N_WHEELS);
            have_kpa = flipper_format_rewind(ff) &&
                       flipper_format_read_uint32(ff, "KPA", kpa, N_WHEELS);
        }
        flipper_format_file_close(ff);
    }
    furi_string_free(ft);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    /* due ruote con lo stesso ID non hanno senso: file scartato */
    for(uint8_t i = 0; ok && i < N_WHEELS; i++) {
        for(uint8_t j = (uint8_t)(i + 1U); j < N_WHEELS; j++) {
            if(memcmp(d[i], d[j], 3) == 0) ok = false;
        }
    }
    if(!ok) return false;

    for(uint8_t i = 0; i < N_WHEELS; i++) {
        uint16_t k = k_tx_default[i].kpa;
        if(have_kpa) k = (uint16_t)((kpa[i] > KPA_MAX) ? KPA_MAX : kpa[i]);
        int8_t t = have_temp ? (int8_t)temp[i] : k_tx_default[i].temp;
        wheel_apply(i, d[i], d[i][3], d[i][4], d[i][5], k, t);
    }
    snprintf(g_car_name, sizeof(g_car_name), "%s", name);
    cfg_snapshot(g_saved);
    return true;
}

/* config.txt: ricorda l'auto usata per ultima. */
static void active_save(void) {
    if(!g_car_name[0]) return;
    Storage* st = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(st, EXT_PATH("apps_data"));
    storage_simply_mkdir(st, CONFIG_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(st);
    if(flipper_format_file_open_always(ff, CONFIG_PATH)) {
        if(flipper_format_write_header_cstr(ff, CONFIG_FILETYPE, CONFIG_VERSION)) {
            flipper_format_write_string_cstr(ff, "Car", g_car_name);
        }
        flipper_format_file_close(ff);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

/* All'avvio: ricarica l'ultima auto (o i soli ID del vecchio config.txt v1). */
static void active_load(void) {
    char name[CAR_NAME_LEN + 1] = {0};
    uint8_t tmp[N_WHEELS][3];
    uint8_t mode = 0; /* 1 = auto salvata, 2 = vecchio config v1 */

    Storage* st = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(st);
    FuriString* ft = furi_string_alloc();
    FuriString* fn = furi_string_alloc();
    if(flipper_format_file_open_existing(ff, CONFIG_PATH)) {
        uint32_t ver = 0;
        bool ok =
            flipper_format_read_header(ff, ft, &ver) && furi_string_cmp_str(ft, CONFIG_FILETYPE) == 0;
        if(ok && ver == CONFIG_VERSION) {
            if(flipper_format_read_string(ff, "Car", fn)) {
                snprintf(name, sizeof(name), "%s", furi_string_get_cstr(fn));
                mode = 1;
            }
        } else if(ok && ver == CONFIG_VERSION_LEGACY) {
            for(uint8_t i = 0; ok && i < N_WHEELS; i++) {
                ok = flipper_format_rewind(ff) && flipper_format_read_hex(ff, k_cfg_keys[i], tmp[i], 3);
            }
            if(ok) mode = 2;
        }
        flipper_format_file_close(ff);
    }
    furi_string_free(fn);
    furi_string_free(ft);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);

    if(mode == 1U) {
        if(name[0]) car_load(name);
    } else if(mode == 2U) {
        bool ok = true;
        for(uint8_t i = 0; i < N_WHEELS; i++) {
            for(uint8_t j = (uint8_t)(i + 1U); j < N_WHEELS; j++) {
                if(memcmp(tmp[i], tmp[j], 3) == 0) ok = false;
            }
        }
        if(ok) {
            for(uint8_t i = 0; i < N_WHEELS; i++) {
                if(memcmp(g_id[i], tmp[i], 3) != 0) wheel_set_id(i, tmp[i]);
            }
            cfg_snapshot(g_saved);
        }
    }
}

/* Elenco delle auto presenti su SD, in ordine alfabetico. */
static void cars_scan(App* app) {
    app->cars_n = 0;

    Storage* st = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(st);
    if(storage_dir_open(dir, CARS_DIR)) {
        FileInfo info;
        char name[48];
        while(app->cars_n < MAX_CARS && storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t l = strlen(name);
            if(file_info_is_dir(&info) || l <= 4U || (l - 4U) > CAR_NAME_LEN) continue;
            if(strcmp(name + l - 4U, ".car") != 0) continue;
            memcpy(app->cars[app->cars_n], name, l - 4U);
            app->cars[app->cars_n][l - 4U] = 0;
            app->cars_n++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    for(uint8_t i = 1; i < app->cars_n; i++) {
        char tmp[CAR_NAME_LEN + 1];
        memcpy(tmp, app->cars[i], sizeof(tmp));
        int j = (int)i - 1;
        while(j >= 0 && strcmp(app->cars[j], tmp) > 0) {
            memcpy(app->cars[j + 1], app->cars[j], sizeof(tmp));
            j--;
        }
        memcpy(app->cars[j + 1], tmp, sizeof(tmp));
    }
}

static void car_delete(const char* name) {
    char path[96];
    car_path(path, sizeof(path), name);
    Storage* st = furi_record_open(RECORD_STORAGE);
    storage_simply_remove(st, path);
    furi_record_close(RECORD_STORAGE);
}

/* Carica un'auto e la ricorda per il prossimo avvio. */
static bool car_activate(const char* name) {
    bool ok = car_load(name);
    if(ok) active_save();
    return ok;
}

/* Auto precedente (-1) / successiva (+1) tra quelle salvate. */
static void car_cycle(App* app, int dir) {
    cars_scan(app);
    if(app->cars_n == 0U) return;
    int n = (int)app->cars_n;
    int cur = -1;
    for(int i = 0; i < n; i++) {
        if(strcmp(app->cars[i], g_car_name) == 0) cur = i;
    }
    int next = (cur < 0) ? ((dir > 0) ? 0 : (n - 1)) : ((cur + dir + n) % n);
    car_activate(app->cars[next]);
}

static void ids_reset_default(void) {
    for(uint8_t i = 0; i < N_WHEELS; i++) {
        if(memcmp(g_id[i], k_default_ids[i], 3) != 0) wheel_set_id(i, k_default_ids[i]);
    }
}

/* ------------------------------------------------------------------------ */
/* PAIR                                                                       */
/* ------------------------------------------------------------------------ */

static void list_fix_scroll(uint8_t sel, uint8_t* scroll, uint8_t visible) {
    if(sel < *scroll) *scroll = sel;
    if(sel >= *scroll + visible) *scroll = (uint8_t)(sel - visible + 1U);
}

/* Frame ricevuto in PAIR: ogni ID nuovo finisce in lista, poi si aggiorna. */
static bool pair_handle_frame(App* app, const uint8_t* f) {
    if(crc8_07(f, 8U) != f[8]) {
        app->frames_crc++;
        return false;
    }

    PairSensor* p = NULL;
    for(uint8_t i = 0; i < app->pair_n; i++) {
        if(memcmp(app->pair[i].id, f + 3, 3) == 0) {
            p = &app->pair[i];
            break;
        }
    }
    if(p == NULL) {
        if(app->pair_n >= MAX_PAIR) return true; /* lista piena: svuotala con OK lungo */
        p = &app->pair[app->pair_n++];
        memset(p, 0, sizeof(PairSensor));
        memcpy(p->id, f + 3, 3);
        p->wheel = NO_WHEEL;
    }

    uint16_t raw = (uint16_t)((((uint16_t)f[0] & 3U) << 8) | f[1]);
    p->flags6 = (uint8_t)(f[0] >> 2);
    p->b6 = f[6];
    p->b7 = f[7];
    p->kpa = raw_to_kpa(raw);
    p->temp = (int16_t)((int)f[2] - 30);
    return true;
}

static uint8_t pair_assigned(const App* app) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < app->pair_n; i++) {
        if(app->pair[i].wheel != NO_WHEEL) n++;
    }
    return n;
}

/* Cambia la ruota assegnata al sensore selezionato: -- > ANT SX > ANT DX >
 * POST SX > POST DX > --. Una ruota ha un solo sensore: se era gia' di un
 * altro sensore, quello torna non assegnato. */
static void pair_cycle_wheel(App* app, int dir) {
    PairSensor* p = &app->pair[app->pair_sel];
    int cur = (p->wheel == NO_WHEEL) ? -1 : (int)p->wheel;
    cur += dir;
    if(cur > (int)N_WHEELS - 1) cur = -1;
    if(cur < -1) cur = (int)N_WHEELS - 1;
    if(cur >= 0) {
        for(uint8_t i = 0; i < app->pair_n; i++) {
            if(app->pair[i].wheel == (uint8_t)cur) app->pair[i].wheel = NO_WHEEL;
        }
    }
    p->wheel = (cur < 0) ? NO_WHEEL : (uint8_t)cur;
}

/* Porta le 4 assegnazioni nella configurazione in uso (RX/TX). */
static void pair_commit(const App* app) {
    for(uint8_t w = 0; w < N_WHEELS; w++) {
        for(uint8_t i = 0; i < app->pair_n; i++) {
            const PairSensor* p = &app->pair[i];
            if(p->wheel == w) {
                wheel_apply(w, p->id, p->flags6, p->b6, p->b7, p->kpa, (int8_t)p->temp);
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* RX                                                                         */
/* ------------------------------------------------------------------------ */

static void rx_callback(bool level, uint32_t duration, void* context) {
    FuriStreamBuffer* buf = context;
    LevelDuration ld = level_duration_make(level, duration);
    furi_stream_buffer_send(buf, &ld, sizeof(LevelDuration), 0);
}

static void rx_enter(App* app) {
    app->rxbuf =
        furi_stream_buffer_alloc(RX_BUF_ITEMS * sizeof(LevelDuration), sizeof(LevelDuration));
    dec_reset(&app->dec);

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(k_preset);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_set_frequency_and_path(FREQ_HZ);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();
    furi_hal_subghz_start_async_rx(rx_callback, app->rxbuf);
    app->rx_active = true;
}

static void rx_leave(App* app) {
    if(!app->rx_active) return;
    furi_hal_subghz_stop_async_rx();
    furi_hal_subghz_sleep();
    furi_stream_buffer_free(app->rxbuf);
    app->rxbuf = NULL;
    app->rx_active = false;
}

/* Ritorna true se il frame ha CRC valido (di una tua ruota o di un altro ID). */
static bool rx_handle_frame(App* app, const uint8_t* f) {
    if(app->screen == ScreenPair) return pair_handle_frame(app, f);
    if(crc8_07(f, 8U) != f[8]) {
        app->frames_crc++;
        return false;
    }

    uint16_t raw = (uint16_t)((((uint16_t)f[0] & 3U) << 8) | f[1]);
    uint16_t kpa = raw_to_kpa(raw);
    int16_t temp = (int16_t)((int)f[2] - 30);
    uint32_t now = furi_get_tick();

    for(uint8_t i = 0; i < N_WHEELS; i++) {
        if(memcmp(f + 3, g_id[i], 3) == 0) {
            RxSensor* s = &g_rx[i];
            s->has_data = true;
            s->kpa = kpa;
            s->temp = temp;
            s->last_tick = now;
            app->frames_ok++;
            FURI_LOG_I(TAG, "%s %u kPa %d C", k_names[i], (unsigned)kpa, (int)temp);
            return true;
        }
    }
    app->frames_other++;
    return true;
}

/* Lampo del LED, colore diverso a ogni frame ricevuto. */
static void flash_led(App* app) {
    notification_message(app->notif, k_flash_seq[app->color_idx]);
    app->color_idx = (uint8_t)((app->color_idx + 1U) % N_FLASH_COLORS);
}

static void rx_clear_all(App* app) {
    memset(g_rx, 0, sizeof(g_rx));
    app->frames_ok = 0;
    app->frames_other = 0;
    app->frames_crc = 0;
}

/* Svuota gli impulsi ricevuti e decodifica. Ritorna true se e' arrivato un frame. */
static bool rx_poll(App* app) {
    bool got = false;
    LevelDuration ld;
    uint32_t guard = 0;
    while(guard < DRAIN_MAX &&
          furi_stream_buffer_receive(app->rxbuf, &ld, sizeof(LevelDuration), 0) ==
              sizeof(LevelDuration)) {
        guard++;
        if(dec_pulse(&app->dec, level_duration_get_level(ld), level_duration_get_duration(ld))) {
            if(rx_handle_frame(app, app->dec.frame)) flash_led(app);
            got = true;
        }
    }
    return got;
}

/* ------------------------------------------------------------------------ */
/* TX                                                                         */
/* ------------------------------------------------------------------------ */

static void build_frame(uint8_t idx, uint8_t* d) {
    const TxSensor* s = &g_tx[idx];
    uint16_t p = kpa_to_raw(s->kpa);
    d[0] = (uint8_t)((s->flags6 << 2) | ((p >> 8) & 3));
    d[1] = (uint8_t)(p & 0xFF);
    d[2] = (uint8_t)(s->temp + 30);
    memcpy(&d[3], g_id[idx], 3);
    d[6] = s->b6;
    d[7] = s->b7;
    d[8] = crc8_07(d, 8);
}

/* ---- lettura profili .tpms: flag, byte di stato e temperatura ---------- */

static uint8_t g_profiles_loaded = 0;
static volatile bool g_scan_abort = false;

static void try_profile(Storage* st, const char* path) {
    FlipperFormat* ff = flipper_format_file_alloc(st);
    if(flipper_format_file_open_existing(ff, path)) {
        uint32_t size = 0;
        uint8_t raw[FRAME_LEN] = {0};
        bool ok = flipper_format_rewind(ff) &&
                  flipper_format_read_uint32(ff, "RawPayloadSize", &size, 1) && size == FRAME_LEN;
        ok = ok && flipper_format_rewind(ff) &&
             flipper_format_read_hex(ff, "RawPayload", raw, FRAME_LEN);
        if(ok && crc8_07(raw, 8) == raw[8]) {
            for(uint8_t i = 0; i < N_WHEELS; i++) {
                TxSensor* s = &g_tx[i];
                if(!s->from_profile && memcmp(raw + 3, g_id[i], 3) == 0) {
                    s->flags6 = raw[0] >> 2;
                    s->temp = (int8_t)((int)raw[2] - 30);
                    s->b6 = raw[6];
                    s->b7 = raw[7];
                    s->from_profile = true;
                    g_profiles_loaded++;
                }
            }
        }
        flipper_format_file_close(ff);
    }
    flipper_format_free(ff);
}

static void scan_dir(Storage* st, const char* dir_path, uint8_t depth) {
    File* dir = storage_file_alloc(st);
    if(storage_dir_open(dir, dir_path)) {
        FileInfo info;
        char name[48];
        /* esce appena tutte e 4 le ruote hanno i loro dati, o se l'app sta chiudendo */
        while(!g_scan_abort && g_profiles_loaded < N_WHEELS &&
              storage_dir_read(dir, &info, name, sizeof(name))) {
            char full[120];
            snprintf(full, sizeof(full), "%s/%s", dir_path, name);
            if(file_info_is_dir(&info)) {
                if(depth < 3) scan_dir(st, full, depth + 1);
            } else {
                size_t l = strlen(name);
                if(l > 5 && strcmp(name + l - 5, ".tpms") == 0) try_profile(st, full);
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

/* Cache: se l'ultima scansione ha trovato tutte e 4 le ruote e gli ID non sono
 * cambiati, non c'e' nulla da cercare e la scansione si salta. */
static uint8_t g_scan_ids[N_WHEELS][3];
static bool g_scan_full = false;

static volatile bool g_scan_running = false;

static void profiles_scan_body(void) {
    g_profiles_loaded = 0;
    for(uint8_t i = 0; i < N_WHEELS; i++) {
        g_tx[i].from_profile = g_tx[i].locked;
        if(g_tx[i].locked) g_profiles_loaded++;
    }
    if(g_profiles_loaded < N_WHEELS) {
        Storage* st = furi_record_open(RECORD_STORAGE);
        scan_dir(st, PROFILE_ROOT, 0);
        furi_record_close(RECORD_STORAGE);
    }
    if(!g_scan_abort) {
        g_scan_full = (g_profiles_loaded >= N_WHEELS);
        memcpy(g_scan_ids, g_id, sizeof(g_id));
    }
}

static int32_t profiles_scan_thread(void* ctx) {
    UNUSED(ctx);
    profiles_scan_body();
    g_scan_running = false;
    return 0;
}

/* true se i dati TX sono gia' completi e valgono ancora per gli ID attuali */
static bool profiles_up_to_date(void) {
    uint8_t n = 0;
    bool all_locked = true;
    for(uint8_t i = 0; i < N_WHEELS; i++) {
        if(g_tx[i].from_profile) n++;
        if(!g_tx[i].locked) all_locked = false;
    }
    if(all_locked) {
        g_profiles_loaded = N_WHEELS;
        return true;
    }
    if(g_scan_full && n == N_WHEELS && memcmp(g_scan_ids, g_id, sizeof(g_id)) == 0) {
        g_profiles_loaded = N_WHEELS;
        return true;
    }
    return false;
}

/* Versione bloccante, per il salvataggio auto: aspetta l'eventuale scansione in
 * background e rilegge solo se serve. */
static void profiles_rescan(void) {
    while(g_scan_running) furi_delay_ms(10);
    if(profiles_up_to_date()) return;
    profiles_scan_body();
}

/* Entrando in TX: la schermata si apre subito, i profili si leggono in background. */
static void profiles_rescan_async(App* app) {
    if(app->scan_thread) return; /* gia' in corso */
    if(profiles_up_to_date()) {
        snprintf(app->tx_msg, sizeof(app->tx_msg), "Prof. %u/4", (unsigned)g_profiles_loaded);
        return;
    }
    g_scan_abort = false;
    g_scan_running = true;
    app->tx_msg[0] = 0;
    app->scan_thread = furi_thread_alloc_ex("TpmsScan", 4096, profiles_scan_thread, NULL);
    furi_thread_start(app->scan_thread);
}

/* Chiamata dal ciclo principale: chiude il thread quando ha finito. */
static bool profiles_scan_poll(App* app) {
    if(!app->scan_thread) return false;
    if(furi_thread_get_state(app->scan_thread) != FuriThreadStateStopped) return false;
    furi_thread_join(app->scan_thread);
    furi_thread_free(app->scan_thread);
    app->scan_thread = NULL;
    snprintf(app->tx_msg, sizeof(app->tx_msg), "Prof. %u/4", (unsigned)g_profiles_loaded);
    return true;
}

/* Chiamata in uscita: interrompe e attende la scansione. */
static void profiles_scan_stop(App* app) {
    if(!app->scan_thread) return;
    g_scan_abort = true;
    furi_thread_join(app->scan_thread);
    furi_thread_free(app->scan_thread);
    app->scan_thread = NULL;
}

/* ---- trasmissione ------------------------------------------------------ */

static LevelDuration g_pulses[MAX_PULSES];
static size_t g_n_pulses = 0;
static size_t g_tx_idx = 0;

static void push_sym(bool level, uint32_t dur) {
    if(g_n_pulses > 0 && level_duration_get_level(g_pulses[g_n_pulses - 1]) == level) {
        uint32_t d = level_duration_get_duration(g_pulses[g_n_pulses - 1]) + dur;
        g_pulses[g_n_pulses - 1] = level_duration_make(level, d);
    } else if(g_n_pulses < MAX_PULSES) {
        g_pulses[g_n_pulses++] = level_duration_make(level, dur);
    }
}

static void build_pulses(uint8_t idx) {
    uint8_t d[FRAME_LEN];
    build_frame(idx, d);
    g_n_pulses = 0;

    for(uint32_t r = 0; r < REPEATS; r++) {
        /* preambolo: "01" x15 + "10" */
        for(int i = 0; i < 15; i++) {
            push_sym(false, TE_US);
            push_sym(true, TE_US);
        }
        push_sym(true, TE_US);
        push_sym(false, TE_US);
        /* dati Manchester: 1 = "10", 0 = "01" */
        for(size_t i = 0; i < FRAME_LEN; i++) {
            for(int b = 7; b >= 0; b--) {
                if((d[i] >> b) & 1) {
                    push_sym(true, TE_US);
                    push_sym(false, TE_US);
                } else {
                    push_sym(false, TE_US);
                    push_sym(true, TE_US);
                }
            }
        }
        push_sym(false, GAP_US);
    }
}

static LevelDuration tx_callback(void* context) {
    UNUSED(context);
    if(g_tx_idx >= g_n_pulses) return level_duration_reset();
    return g_pulses[g_tx_idx++];
}

static bool transmit_sensor(uint8_t idx) {
    if(!furi_hal_subghz_is_tx_allowed(FREQ_HZ)) {
        FURI_LOG_E(TAG, "TX not allowed on this frequency/region");
        return false;
    }
    build_pulses(idx);
    g_tx_idx = 0;

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(k_preset);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_cc1101_g0, false);
    furi_hal_subghz_set_frequency_and_path(FREQ_HZ);

    bool ok = furi_hal_subghz_start_async_tx(tx_callback, NULL);
    if(ok) {
        while(!furi_hal_subghz_is_async_tx_complete()) furi_delay_ms(5);
        furi_hal_subghz_stop_async_tx();
    }
    furi_hal_subghz_sleep();
    return ok;
}

static void tx_send_one(App* app, uint8_t idx) {
    app->sending = true;
    view_port_update(app->vp);
    bool ok = transmit_sensor(idx);
    app->sending = false;
    snprintf(app->tx_msg, sizeof(app->tx_msg), ok ? "Sent" : "TX error");
}

static void tx_send_all(App* app) {
    bool ok = true;
    for(uint8_t i = 0; i < N_WHEELS; i++) {
        snprintf(app->tx_msg, sizeof(app->tx_msg), "TX %u/%u", (unsigned)(i + 1U), (unsigned)N_WHEELS);
        app->sending = true;
        view_port_update(app->vp);
        ok = transmit_sensor(i) && ok;
        app->sending = false;
        furi_delay_ms(400);
    }
    snprintf(app->tx_msg, sizeof(app->tx_msg), ok ? "Sent 4/4" : "TX error");
}

static void tx_do_reset(App* app) {
    g_tx[0].kpa = KPA_FRONT_DEFAULT;
    g_tx[1].kpa = KPA_FRONT_DEFAULT;
    g_tx[2].kpa = KPA_REAR_DEFAULT;
    g_tx[3].kpa = KPA_REAR_DEFAULT;
    tx_send_all(app);
}

static void tx_adjust(TxSensor* s, int delta) {
    int v = (int)s->kpa + delta;
    if(v < 0) v = 0;
    if(v > (int)KPA_MAX) v = (int)KPA_MAX;
    s->kpa = (uint16_t)v;
}

/* ------------------------------------------------------------------------ */
/* GUI                                                                        */
/* ------------------------------------------------------------------------ */

/* Testo a lato della ruota, invertito se selezionata. */
static void draw_value(Canvas* c, int x, int y, bool right_aligned, const char* text, bool sel) {
    int w = (int)canvas_string_width(c, text);
    int x0 = right_aligned ? x - w : x;
    if(sel) {
        canvas_draw_rbox(c, x0 - 2, y - 9, w + 4, 11, 2);
        canvas_set_color(c, ColorWhite);
    }
    canvas_draw_str(c, x0, y, text);
    canvas_set_color(c, ColorBlack);
}

static void draw_wheel(Canvas* c, int x, int y, bool sel) {
    if(sel) {
        canvas_draw_rbox(c, x, y, 6, 13, 2);
    } else {
        canvas_draw_rframe(c, x, y, 6, 13, 2);
    }
}

/* Auto vista dall'alto con le 4 ruote (valori passati come testo). */
static void draw_car(Canvas* canvas, uint8_t sel, const char* const vals[N_WHEELS]) {
    canvas_draw_rframe(canvas, 26, 4, 16, 44, 5); /* carrozzeria */
    canvas_draw_line(canvas, 29, 14, 38, 14);      /* parabrezza */
    canvas_draw_line(canvas, 29, 38, 38, 38);      /* lunotto */
    canvas_draw_line(canvas, 29, 7, 30, 7);        /* fari */
    canvas_draw_line(canvas, 37, 7, 38, 7);

    canvas_set_font(canvas, FontSecondary);
    draw_wheel(canvas, 19, 7, sel == 0U);  /* ANT SX */
    draw_wheel(canvas, 43, 7, sel == 1U);  /* ANT DX */
    draw_wheel(canvas, 19, 31, sel == 2U); /* POST SX */
    draw_wheel(canvas, 43, 31, sel == 3U); /* POST DX */

    draw_value(canvas, 16, 17, true, vals[0], sel == 0U);
    draw_value(canvas, 52, 17, false, vals[1], sel == 1U);
    draw_value(canvas, 16, 41, true, vals[2], sel == 2U);
    draw_value(canvas, 52, 41, false, vals[3], sel == 3U);
}

static void format_age(char* out, size_t n, uint32_t age_ms) {
    uint32_t s = age_ms / 1000U;
    if(s < 60U) {
        snprintf(out, n, "%us ago", (unsigned)s);
    } else if(s < 3600U) {
        snprintf(out, n, "%um ago", (unsigned)(s / 60U));
    } else {
        snprintf(out, n, "%uh ago", (unsigned)(s / 3600U));
    }
}

/* ---- Main menu --------------------------------------------------------- */

static const char* const k_menu_items[MENU_ITEMS] = {
    "Monitor", "Transmit", "Pair sensors", "Settings", "About"};

static void draw_menu(Canvas* canvas, const App* app) {
    const char* title = g_car_name[0] ? g_car_name : "NISSAN TPMS";
    canvas_set_font(canvas, FontPrimary);
    if(canvas_string_width(canvas, title) > 108) canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 9, title);

    /* Minimal text-only menu: five compact rows and a simple selection marker. */
    canvas_draw_line(canvas, 2, 12, 125, 12);
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < MENU_ITEMS; i++) {
        int y = 14 + (int)i * 10;
        if(app->menu_sel == i) {
            canvas_draw_box(canvas, 2, y, 123, 9);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 6, y + 7, "> ");
            canvas_draw_str(canvas, 18, y + 7, k_menu_items[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 18, y + 7, k_menu_items[i]);
        }
    }
}

/* ---- About --------------------------------------------------------------- */

static void draw_about(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "Version 1.2beta");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "created by Marcopiiit");
}

/* ---- RX ------------------------------------------------------------------ */

static void draw_rx(Canvas* canvas, const App* app) {
    char buf[32];
    char vals_buf[N_WHEELS][8];
    const char* vals[N_WHEELS];
    uint32_t now = furi_get_tick();

    for(uint8_t i = 0; i < N_WHEELS; i++) {
        if(g_rx[i].has_data) {
            snprintf(vals_buf[i], sizeof(vals_buf[i]), "%u", (unsigned)g_rx[i].kpa);
        } else {
            snprintf(vals_buf[i], sizeof(vals_buf[i]), "--");
        }
        vals[i] = vals_buf[i];
    }

    /* pannello sinistro: auto con le 4 pressioni */
    draw_car(canvas, app->rx_sel, vals);

    /* contatori in basso a sinistra */
    canvas_set_font(canvas, FontSecondary);
    snprintf(
        buf, sizeof(buf), "OK:%u altri:%u", (unsigned)app->frames_ok, (unsigned)app->frames_other);
    canvas_draw_str(canvas, 2, 55, buf);
    snprintf(buf, sizeof(buf), "CRC err:%u", (unsigned)app->frames_crc);
    canvas_draw_str(canvas, 2, 63, buf);

    /* separatore */
    canvas_draw_line(canvas, 72, 2, 72, 61);

    /* pannello destro: dettaglio ruota selezionata */
    const RxSensor* s = &g_rx[app->rx_sel];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 76, 11, k_names[app->rx_sel]);

    if(s->has_data) {
        canvas_set_font(canvas, FontBigNumbers);
        snprintf(buf, sizeof(buf), "%u", (unsigned)s->kpa);
        canvas_draw_str(canvas, 76, 33, buf);

        canvas_set_font(canvas, FontSecondary);
        unsigned kpa = (unsigned)s->kpa; /* kPa = bar*100 */
        unsigned psi = (kpa * 145U + 500U) / 1000U;
        snprintf(buf, sizeof(buf), "%u.%02u bar", kpa / 100U, kpa % 100U);
        canvas_draw_str(canvas, 76, 43, buf);
        snprintf(buf, sizeof(buf), "%upsi %dC", psi, (int)s->temp);
        canvas_draw_str(canvas, 76, 52, buf);
        format_age(buf, sizeof(buf), now - s->last_tick);
        canvas_draw_str(canvas, 76, 61, buf);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 76, 33, "--");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 76, 43, "Waiting for");
        canvas_draw_str(canvas, 76, 52, "sensor");
    }
}

/* ---- TX ------------------------------------------------------------------ */

static void draw_tx(Canvas* canvas, const App* app) {
    char buf[32];
    char vals_buf[N_WHEELS][8];
    const char* vals[N_WHEELS];

    for(uint8_t i = 0; i < N_WHEELS; i++) {
        snprintf(vals_buf[i], sizeof(vals_buf[i]), "%u", (unsigned)g_tx[i].kpa);
        vals[i] = vals_buf[i];
    }

    /* pannello sinistro: auto con le 4 pressioni */
    draw_car(canvas, app->tx_sel, vals);

    /* pulsante RESET */
    canvas_set_font(canvas, FontSecondary);
    if(app->tx_sel == SEL_RESET) {
        canvas_draw_rbox(canvas, 2, 52, 46, 11, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 2, 52, 46, 11, 3);
    }
    canvas_draw_str_aligned(canvas, 25, 57, AlignCenter, AlignCenter, "RESET");
    canvas_set_color(canvas, ColorBlack);

    /* separatore */
    canvas_draw_line(canvas, 72, 2, 72, 61);

    /* pannello destro: dettaglio ruota selezionata */
    if(app->tx_sel < N_WHEELS) {
        const TxSensor* s = &g_tx[app->tx_sel];

        /* pannello destro: x da 76 a 127 (solo 52 px), tutto deve starci */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 76, 12, k_names[app->tx_sel]);

        /* unita' in alto a destra, cosi' il numero grande ha tutta la larghezza */
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 127, 6, AlignRight, AlignCenter, "kPa");

        canvas_set_font(canvas, FontBigNumbers);
        snprintf(buf, sizeof(buf), "%u", (unsigned)s->kpa);
        canvas_draw_str(canvas, 76, 34, buf);

        unsigned psi10 = (unsigned)(((uint32_t)s->kpa * 145U + 50U) / 100U);
        canvas_set_font(canvas, FontSecondary);

        /* bar e psi su due righe: insieme non entrano nei 52 px */
        snprintf(buf, sizeof(buf), "%u.%02u bar", (unsigned)(s->kpa / 100U), (unsigned)(s->kpa % 100U));
        canvas_draw_str(canvas, 76, 44, buf);

        snprintf(buf, sizeof(buf), "%u psi", (psi10 + 5U) / 10U);
        canvas_draw_str(canvas, 76, 53, buf);
        if(!s->from_profile) canvas_draw_str_aligned(canvas, 127, 49, AlignRight, AlignCenter, "dflt");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 76, 12, "RESET");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 76, 26, "Front");
        snprintf(buf, sizeof(buf), "%u kPa", (unsigned)KPA_FRONT_DEFAULT);
        canvas_draw_str(canvas, 76, 35, buf);
        canvas_draw_str(canvas, 76, 45, "Rear");
        snprintf(buf, sizeof(buf), "%u kPa", (unsigned)KPA_REAR_DEFAULT);
        canvas_draw_str(canvas, 76, 54, buf);
    }

    /* stato in basso a destra */
    canvas_set_font(canvas, FontSecondary);
    if(app->sending) {
        canvas_draw_str(canvas, 76, 63, app->tx_msg[0] ? app->tx_msg : "TX...");
    } else if(app->scan_thread) {
        canvas_draw_str(canvas, 76, 63, "Scan...");
    } else if(app->tx_msg[0]) {
        canvas_draw_str(canvas, 76, 63, app->tx_msg);
    } else {
        canvas_draw_str(canvas, 76, 63, "OK: send");
    }
}

/* ---- Settings ----------------------------------------------------------- */

static void draw_settings(Canvas* canvas, const App* app) {
    char buf[24];

    canvas_set_font(canvas, FontPrimary);
    snprintf(
        buf,
        sizeof(buf),
        "%s%s",
        g_car_name[0] ? g_car_name : "Settings",
        config_dirty() ? "*" : "");
    canvas_draw_str(canvas, 2, 10, buf);
    canvas_set_font(canvas, FontSecondary);
    if(app->set_msg[0]) {
        canvas_draw_str_aligned(canvas, 126, 5, AlignRight, AlignCenter, app->set_msg);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    for(uint8_t r = app->set_scroll; r < SET_ROWS && r < app->set_scroll + SET_VISIBLE; r++) {
        int y0 = 14 + (int)(r - app->set_scroll) * 10;
        bool sel = (app->set_sel == r);
        if(sel) {
            canvas_draw_rbox(canvas, 2, y0, 121, 10, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        if(r < N_WHEELS) {
            canvas_draw_str_aligned(canvas, 5, y0 + 5, AlignLeft, AlignCenter, k_names[r]);
            snprintf(buf, sizeof(buf), "%02X %02X %02X", g_id[r][0], g_id[r][1], g_id[r][2]);
            canvas_draw_str_aligned(canvas, 119, y0 + 5, AlignRight, AlignCenter, buf);
        } else {
            const char* label = "Reset defaults";
            if(r == SET_SAVE) {
                label = "Save vehicle";
            } else if(r == SET_SAVEAS) {
                label = "Save as...";
            } else if(r == SET_LOAD) {
                label = "Load vehicle";
            }
            canvas_draw_str_aligned(canvas, 5, y0 + 5, AlignLeft, AlignCenter, label);
        }
        canvas_set_color(canvas, ColorBlack);
    }

    /* barra di scorrimento */
    int thumb_h = (int)(50U * SET_VISIBLE / SET_ROWS);
    int thumb_y = 14 + (int)app->set_scroll * (50 - thumb_h) / (int)(SET_ROWS - SET_VISIBLE);
    canvas_draw_box(canvas, 125, thumb_y, 2, (size_t)thumb_h);
}

/* ---- ID editor ---------------------------------------------------------- */

static const char k_hex[] = "0123456789ABCDEF";

static uint8_t edit_get_nibble(const App* app, uint8_t pos) {
    uint8_t byte = app->edit_id[pos >> 1];
    return (pos & 1U) ? (uint8_t)(byte & 0x0FU) : (uint8_t)(byte >> 4);
}

static void edit_add_nibble(App* app, int delta) {
    uint8_t idx = (uint8_t)(app->edit_pos >> 1);
    uint8_t byte = app->edit_id[idx];
    bool hi = ((app->edit_pos & 1U) == 0U);
    int v = hi ? (int)(byte >> 4) : (int)(byte & 0x0FU);
    v = (v + delta + 16) & 0x0F;
    if(hi) {
        byte = (uint8_t)((byte & 0x0FU) | (uint8_t)(v << 4));
    } else {
        byte = (uint8_t)((byte & 0xF0U) | (uint8_t)v);
    }
    app->edit_id[idx] = byte;
}

static void draw_id_edit(Canvas* canvas, const App* app) {
    char buf[24];

    canvas_set_font(canvas, FontPrimary);
    snprintf(buf, sizeof(buf), "ID %s", k_names[app->edit_wheel]);
    canvas_draw_str(canvas, 2, 10, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    for(uint8_t pos = 0; pos < 6U; pos++) {
        int x = 12 + (int)pos * 14 + (int)(pos / 2U) * 10;
        char ch[2] = {k_hex[edit_get_nibble(app, pos) & 0x0FU], 0};
        bool sel = (pos == app->edit_pos);
        if(sel) {
            canvas_draw_rbox(canvas, x - 3, 22, 13, 16, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, x, 35, ch);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontSecondary);
    if(app->edit_msg[0]) canvas_draw_str(canvas, 2, 47, app->edit_msg);
    canvas_draw_str(canvas, 2, 55, "Up/Down: value");
    canvas_draw_str(canvas, 2, 63, "Left/Right: digit");
    canvas_draw_str(canvas, 80, 55, "OK: save");
    canvas_draw_str(canvas, 80, 63, "Back: exit");
}

/* ---- PAIR --------------------------------------------------------------- */

static const char* const k_short[N_WHEELS] = {"FL", "FR", "RL", "RR"};

static void draw_pair(Canvas* canvas, const App* app) {
    char buf[24];

    canvas_set_font(canvas, FontPrimary);
    snprintf(buf, sizeof(buf), "PAIR %u/4", (unsigned)pair_assigned(app));
    canvas_draw_str(canvas, 2, 10, buf);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u found", (unsigned)app->pair_n);
    canvas_draw_str_aligned(canvas, 126, 5, AlignRight, AlignCenter, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(app->pair_n == 0U) {
        canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignCenter, "Listening: 433.92 MHz");
        canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignCenter, "Wake up sensors");
    }

    for(uint8_t r = app->pair_scroll; r < app->pair_n && r < app->pair_scroll + PAIR_VISIBLE; r++) {
        const PairSensor* p = &app->pair[r];
        int y0 = 14 + (int)(r - app->pair_scroll) * 10;
        if(app->pair_sel == r) {
            canvas_draw_rbox(canvas, 2, y0, 121, 10, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        snprintf(buf, sizeof(buf), "%02X%02X%02X", p->id[0], p->id[1], p->id[2]);
        canvas_draw_str_aligned(canvas, 5, y0 + 5, AlignLeft, AlignCenter, buf);
        snprintf(buf, sizeof(buf), "%ukPa", (unsigned)p->kpa);
        canvas_draw_str_aligned(canvas, 96, y0 + 5, AlignRight, AlignCenter, buf);
        canvas_draw_str_aligned(
            canvas,
            119,
            y0 + 5,
            AlignRight,
            AlignCenter,
            (p->wheel < N_WHEELS) ? k_short[p->wheel] : "--");
        canvas_set_color(canvas, ColorBlack);
    }

    if(app->pair_n > PAIR_VISIBLE) {
        int thumb_h = (int)(40U * PAIR_VISIBLE / app->pair_n);
        int thumb_y = 14 + (int)app->pair_scroll * (40 - thumb_h) / (int)(app->pair_n - PAIR_VISIBLE);
        canvas_draw_box(canvas, 125, thumb_y, 2, (size_t)thumb_h);
    }

    canvas_draw_str(canvas, 2, 63, app->pair_msg[0] ? app->pair_msg : "L/R: wheel  OK: save");
}

/* ---- Vehicle list ------------------------------------------------------- */

static void draw_cars(Canvas* canvas, const App* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Load vehicle");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 126, 5, AlignRight, AlignCenter, app->cars_msg[0] ? app->cars_msg : "Left: delete");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(app->cars_n == 0U) {
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "No saved vehicles");
    }

    for(uint8_t r = app->cars_scroll; r < app->cars_n && r < app->cars_scroll + SET_VISIBLE; r++) {
        int y0 = 14 + (int)(r - app->cars_scroll) * 10;
        if(app->cars_sel == r) {
            canvas_draw_rbox(canvas, 2, y0, 121, 10, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 5, y0 + 5, AlignLeft, AlignCenter, app->cars[r]);
        if(strcmp(app->cars[r], g_car_name) == 0) {
            canvas_draw_str_aligned(canvas, 119, y0 + 5, AlignRight, AlignCenter, "*");
        }
        canvas_set_color(canvas, ColorBlack);
    }

    if(app->cars_n > SET_VISIBLE) {
        int thumb_h = (int)(50U * SET_VISIBLE / app->cars_n);
        int thumb_y = 14 + (int)app->cars_scroll * (50 - thumb_h) / (int)(app->cars_n - SET_VISIBLE);
        canvas_draw_box(canvas, 125, thumb_y, 2, (size_t)thumb_h);
    }
}

/* ---- Vehicle name editor ------------------------------------------------ */

static void draw_name(Canvas* canvas, const App* app) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Vehicle name");
    canvas_draw_line(canvas, 0, 12, 127, 12);

    for(uint8_t pos = 0; pos < CAR_NAME_LEN; pos++) {
        int x = 6 + (int)pos * 11;
        char ch[2] = {app->name_buf[pos], 0};
        if(pos == app->name_pos) {
            canvas_draw_rbox(canvas, x - 1, 20, 11, 16, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, x + 1, 33, ch);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, x, 39, x + 8, 39);
    }

    canvas_set_font(canvas, FontSecondary);
    if(app->edit_msg[0]) canvas_draw_str(canvas, 2, 47, app->edit_msg);
    canvas_draw_str(canvas, 2, 55, "Up/Down: letter");
    canvas_draw_str(canvas, 2, 63, "Left/Right: position");
    canvas_draw_str(canvas, 88, 55, "OK: save");
    canvas_draw_str(canvas, 88, 63, "Back: exit");
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(app->screen) {
    case ScreenMenu:
        draw_menu(canvas, app);
        break;
    case ScreenRx:
        draw_rx(canvas, app);
        break;
    case ScreenTx:
        draw_tx(canvas, app);
        break;
    case ScreenSettings:
        draw_settings(canvas, app);
        break;
    case ScreenIdEdit:
        draw_id_edit(canvas, app);
        break;
    case ScreenPair:
        draw_pair(canvas, app);
        break;
    case ScreenCars:
        draw_cars(canvas, app);
        break;
    case ScreenName:
        draw_name(canvas, app);
        break;
    case ScreenAbout:
        draw_about(canvas);
        break;
    default:
        break;
    }
}

static void input_cb(InputEvent* ev, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, ev, 0);
}

/* ------------------------------------------------------------------------ */
/* Gestione tasti (ritornano true se serve ridisegnare)                       */
/* ------------------------------------------------------------------------ */

static void goto_menu(App* app) {
    if(app->screen == ScreenRx || app->screen == ScreenPair) rx_leave(app);
    app->screen = ScreenMenu;
}

static void pair_enter(App* app) {
    app->pair_n = 0;
    app->pair_sel = 0;
    app->pair_scroll = 0;
    app->pair_msg[0] = 0;
    app->frames_crc = 0;
    app->screen = ScreenPair;
    rx_enter(app);
}

/* ---- editor nome auto: lettere, cifre, '-' e '_' ------------------------- */

static const char k_namechars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";

static void name_init(App* app, uint8_t from) {
    memset(app->name_buf, ' ', CAR_NAME_LEN);
    app->name_buf[CAR_NAME_LEN] = 0;
    const char* src = g_car_name[0] ? g_car_name : "CAR";
    size_t l = strlen(src);
    if(l > CAR_NAME_LEN) l = CAR_NAME_LEN;
    memcpy(app->name_buf, src, l);
    app->name_pos = 0;
    app->name_from = from;
    app->edit_msg[0] = 0;
    app->screen = ScreenName;
}

static void name_step(App* app, int delta) {
    const char* p = strchr(k_namechars, app->name_buf[app->name_pos]);
    int n = (int)strlen(k_namechars);
    int idx = p ? (int)(p - k_namechars) : 0;
    idx = (idx + delta + n) % n;
    app->name_buf[app->name_pos] = k_namechars[idx];
}

static void name_confirm(App* app) {
    char name[CAR_NAME_LEN + 1];
    int last = -1;
    for(uint8_t i = 0; i < CAR_NAME_LEN; i++) {
        if(app->name_buf[i] != ' ') last = (int)i;
    }
    if(last < 0) {
        snprintf(app->edit_msg, sizeof(app->edit_msg), "Name is empty!");
        return;
    }
    for(int i = 0; i <= last; i++) {
        name[i] = (app->name_buf[i] == ' ') ? '_' : app->name_buf[i];
    }
    name[last + 1] = 0;

    if(app->name_from == 1U) pair_commit(app); /* le 4 assegnazioni diventano la config in uso */

    bool ok = car_save(name);
    if(ok) {
        snprintf(g_car_name, sizeof(g_car_name), "%s", name);
        active_save();
    }
    snprintf(app->set_msg, sizeof(app->set_msg), "%s", ok ? "Saved" : "SD card error");
    app->set_sel = 0;
    app->set_scroll = 0;
    app->screen = ScreenSettings;
}

static void name_cancel(App* app) {
    if(app->name_from == 1U) {
        app->screen = ScreenPair;
        rx_enter(app); /* riprende l'ascolto, la lista resta */
    } else {
        app->set_msg[0] = 0;
        app->screen = ScreenSettings;
    }
}

static bool on_input_menu(App* app, const InputEvent* ev) {
    if(ev->type != InputTypeShort) return false;
    switch(ev->key) {
    case InputKeyUp:
        app->menu_sel = (uint8_t)((app->menu_sel + MENU_ITEMS - 1U) % MENU_ITEMS);
        return true;
    case InputKeyDown:
        app->menu_sel = (uint8_t)((app->menu_sel + 1U) % MENU_ITEMS);
        return true;
    case InputKeyLeft:
        car_cycle(app, -1);
        return true;
    case InputKeyRight:
        car_cycle(app, 1);
        return true;
    case InputKeyOk:
        if(app->menu_sel == 0U) {
            app->screen = ScreenRx;
            rx_enter(app);
        } else if(app->menu_sel == 1U) {
            app->screen = ScreenTx; /* si apre subito */
            profiles_rescan_async(app);
        } else if(app->menu_sel == 2U) {
            pair_enter(app);
        } else if(app->menu_sel == 3U) {
            app->set_sel = 0;
            app->set_scroll = 0;
            app->set_msg[0] = 0;
            app->screen = ScreenSettings;
        } else {
            app->screen = ScreenAbout;
        }
        return true;
    case InputKeyBack:
        app->running = false;
        return true;
    default:
        return false;
    }
}

static bool on_input_about(App* app, const InputEvent* ev) {
    if(ev->type != InputTypeShort) return false;
    if(ev->key == InputKeyBack || ev->key == InputKeyOk || ev->key == InputKeyLeft) {
        app->screen = ScreenMenu;
        return true;
    }
    return false;
}

static bool on_input_rx(App* app, const InputEvent* ev) {
    if(ev->type == InputTypeShort) {
        switch(ev->key) {
        case InputKeyLeft:
            app->rx_sel = (uint8_t)((app->rx_sel + N_WHEELS - 1U) % N_WHEELS);
            return true;
        case InputKeyRight:
            app->rx_sel = (uint8_t)((app->rx_sel + 1U) % N_WHEELS);
            return true;
        case InputKeyBack:
            goto_menu(app);
            return true;
        default:
            return false;
        }
    } else if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        rx_clear_all(app);
        return true;
    }
    return false;
}

static bool on_input_tx(App* app, const InputEvent* ev) {
    int step = (ev->type == InputTypeRepeat) ? 5 : 1;
    if(ev->type == InputTypeShort || ev->type == InputTypeRepeat) {
        switch(ev->key) {
        case InputKeyUp:
            if(app->tx_sel < N_WHEELS) tx_adjust(&g_tx[app->tx_sel], step);
            app->tx_msg[0] = 0;
            break;
        case InputKeyDown:
            if(app->tx_sel < N_WHEELS) tx_adjust(&g_tx[app->tx_sel], -step);
            app->tx_msg[0] = 0;
            break;
        case InputKeyLeft:
            if(ev->type == InputTypeShort) app->tx_sel = (uint8_t)((app->tx_sel + 4U) % 5U);
            app->tx_msg[0] = 0;
            break;
        case InputKeyRight:
            if(ev->type == InputTypeShort) app->tx_sel = (uint8_t)((app->tx_sel + 1U) % 5U);
            app->tx_msg[0] = 0;
            break;
        case InputKeyOk:
            if(app->scan_thread) break; /* dati ancora in lettura */
            if(ev->type == InputTypeShort) {
                if(app->tx_sel == SEL_RESET) {
                    tx_do_reset(app);
                } else {
                    tx_send_one(app, app->tx_sel);
                }
            }
            break;
        case InputKeyBack:
            if(ev->type == InputTypeShort) goto_menu(app);
            break;
        default:
            break;
        }
    } else if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        if(app->scan_thread) return true;
        if(app->tx_sel == SEL_RESET) {
            tx_do_reset(app);
        } else {
            /* stessa pressione della ruota scelta su tutte le ruote, poi invio */
            uint16_t kpa = g_tx[app->tx_sel].kpa;
            for(uint8_t i = 0; i < N_WHEELS; i++) g_tx[i].kpa = kpa;
            tx_send_all(app);
        }
    } else {
        return false;
    }
    return true;
}

static void settings_fix_scroll(App* app) {
    if(app->set_sel < app->set_scroll) app->set_scroll = app->set_sel;
    if(app->set_sel >= app->set_scroll + SET_VISIBLE) {
        app->set_scroll = (uint8_t)(app->set_sel - SET_VISIBLE + 1U);
    }
}

static bool on_input_settings(App* app, const InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;
    switch(ev->key) {
    case InputKeyUp:
        app->set_sel = (uint8_t)((app->set_sel + SET_ROWS - 1U) % SET_ROWS);
        settings_fix_scroll(app);
        app->set_msg[0] = 0;
        return true;
    case InputKeyDown:
        app->set_sel = (uint8_t)((app->set_sel + 1U) % SET_ROWS);
        settings_fix_scroll(app);
        app->set_msg[0] = 0;
        return true;
    case InputKeyOk:
        if(ev->type != InputTypeShort) return false;
        if(app->set_sel < N_WHEELS) {
            app->edit_wheel = app->set_sel;
            app->edit_pos = 0;
            memcpy(app->edit_id, g_id[app->set_sel], 3);
            app->edit_msg[0] = 0;
            app->screen = ScreenIdEdit;
        } else if(app->set_sel == SET_SAVE) {
            if(g_car_name[0]) {
                bool ok = car_save(g_car_name);
                if(ok) active_save();
                snprintf(app->set_msg, sizeof(app->set_msg), "%s", ok ? "Saved" : "SD card error");
            } else {
                name_init(app, 0); /* nessuna auto attiva: serve un nome */
            }
        } else if(app->set_sel == SET_SAVEAS) {
            name_init(app, 0);
        } else if(app->set_sel == SET_LOAD) {
            cars_scan(app);
            app->cars_sel = 0;
            for(uint8_t i = 0; i < app->cars_n; i++) {
                if(strcmp(app->cars[i], g_car_name) == 0) app->cars_sel = i;
            }
            app->cars_scroll = 0;
            list_fix_scroll(app->cars_sel, &app->cars_scroll, SET_VISIBLE);
            app->cars_del = false;
            app->cars_msg[0] = 0;
            app->screen = ScreenCars;
        } else {
            ids_reset_default();
            snprintf(app->set_msg, sizeof(app->set_msg), "Defaults reset");
        }
        return true;
    case InputKeyBack:
        if(ev->type != InputTypeShort) return false;
        goto_menu(app);
        return true;
    default:
        return false;
    }
}

static bool on_input_id_edit(App* app, const InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;
    switch(ev->key) {
    case InputKeyUp:
        edit_add_nibble(app, 1);
        app->edit_msg[0] = 0;
        return true;
    case InputKeyDown:
        edit_add_nibble(app, -1);
        app->edit_msg[0] = 0;
        return true;
    case InputKeyLeft:
        if(ev->type != InputTypeShort) return false;
        app->edit_pos = (uint8_t)((app->edit_pos + 5U) % 6U);
        return true;
    case InputKeyRight:
        if(ev->type != InputTypeShort) return false;
        app->edit_pos = (uint8_t)((app->edit_pos + 1U) % 6U);
        return true;
    case InputKeyOk:
        if(ev->type != InputTypeShort) return false;
        if(id_in_use(app->edit_id, app->edit_wheel)) {
            snprintf(app->edit_msg, sizeof(app->edit_msg), "ID already in use!");
            return true;
        }
        if(memcmp(g_id[app->edit_wheel], app->edit_id, 3) != 0) {
            wheel_set_id(app->edit_wheel, app->edit_id);
            snprintf(app->set_msg, sizeof(app->set_msg), "ID updated");
        } else {
            app->set_msg[0] = 0;
        }
        app->screen = ScreenSettings;
        return true;
    case InputKeyBack:
        if(ev->type != InputTypeShort) return false;
        app->set_msg[0] = 0;
        app->screen = ScreenSettings;
        return true;
    default:
        return false;
    }
}

static bool on_input_pair(App* app, const InputEvent* ev) {
    if(ev->type == InputTypeLong && ev->key == InputKeyOk) {
        app->pair_n = 0;
        app->pair_sel = 0;
        app->pair_scroll = 0;
        snprintf(app->pair_msg, sizeof(app->pair_msg), "List cleared");
        return true;
    }
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;

    switch(ev->key) {
    case InputKeyUp:
        if(app->pair_n > 0U) {
            app->pair_sel = (uint8_t)((app->pair_sel + app->pair_n - 1U) % app->pair_n);
            list_fix_scroll(app->pair_sel, &app->pair_scroll, PAIR_VISIBLE);
        }
        app->pair_msg[0] = 0;
        return true;
    case InputKeyDown:
        if(app->pair_n > 0U) {
            app->pair_sel = (uint8_t)((app->pair_sel + 1U) % app->pair_n);
            list_fix_scroll(app->pair_sel, &app->pair_scroll, PAIR_VISIBLE);
        }
        app->pair_msg[0] = 0;
        return true;
    case InputKeyLeft:
        if(ev->type != InputTypeShort || app->pair_n == 0U) return false;
        pair_cycle_wheel(app, -1);
        app->pair_msg[0] = 0;
        return true;
    case InputKeyRight:
        if(ev->type != InputTypeShort || app->pair_n == 0U) return false;
        pair_cycle_wheel(app, 1);
        app->pair_msg[0] = 0;
        return true;
    case InputKeyOk: {
        if(ev->type != InputTypeShort) return false;
        uint8_t n = pair_assigned(app);
        if(n < N_WHEELS) {
            snprintf(app->pair_msg, sizeof(app->pair_msg), "Assign 4 wheels (%u/4)", (unsigned)n);
        } else {
            rx_leave(app);
            name_init(app, 1);
        }
        return true;
    }
    case InputKeyBack:
        if(ev->type != InputTypeShort) return false;
        goto_menu(app);
        return true;
    default:
        return false;
    }
}

static bool on_input_cars(App* app, const InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;

    switch(ev->key) {
    case InputKeyUp:
        if(app->cars_n > 0U) {
            app->cars_sel = (uint8_t)((app->cars_sel + app->cars_n - 1U) % app->cars_n);
            list_fix_scroll(app->cars_sel, &app->cars_scroll, SET_VISIBLE);
        }
        app->cars_del = false;
        app->cars_msg[0] = 0;
        return true;
    case InputKeyDown:
        if(app->cars_n > 0U) {
            app->cars_sel = (uint8_t)((app->cars_sel + 1U) % app->cars_n);
            list_fix_scroll(app->cars_sel, &app->cars_scroll, SET_VISIBLE);
        }
        app->cars_del = false;
        app->cars_msg[0] = 0;
        return true;
    case InputKeyLeft:
        if(ev->type != InputTypeShort || app->cars_n == 0U) return false;
        app->cars_del = true; /* serve una conferma con OK */
        snprintf(app->cars_msg, sizeof(app->cars_msg), "OK: delete?");
        return true;
    case InputKeyOk:
        if(ev->type != InputTypeShort || app->cars_n == 0U) return false;
        if(app->cars_del) {
            char name[CAR_NAME_LEN + 1];
            snprintf(name, sizeof(name), "%s", app->cars[app->cars_sel]);
            car_delete(name);
            if(strcmp(name, g_car_name) == 0) g_car_name[0] = 0;
            cars_scan(app);
            if(app->cars_sel >= app->cars_n) app->cars_sel = app->cars_n ? (uint8_t)(app->cars_n - 1U) : 0U;
            app->cars_scroll = 0;
            list_fix_scroll(app->cars_sel, &app->cars_scroll, SET_VISIBLE);
            app->cars_del = false;
            snprintf(app->cars_msg, sizeof(app->cars_msg), "Deleted");
        } else if(car_activate(app->cars[app->cars_sel])) {
            snprintf(app->set_msg, sizeof(app->set_msg), "Loaded");
            app->set_sel = 0;
            app->set_scroll = 0;
            app->screen = ScreenSettings;
        } else {
            snprintf(app->cars_msg, sizeof(app->cars_msg), "Invalid file");
        }
        return true;
    case InputKeyBack:
        if(ev->type != InputTypeShort) return false;
        if(app->cars_del) {
            app->cars_del = false;
            app->cars_msg[0] = 0;
        } else {
            app->set_msg[0] = 0;
            app->screen = ScreenSettings;
        }
        return true;
    default:
        return false;
    }
}

static bool on_input_name(App* app, const InputEvent* ev) {
    if(ev->type != InputTypeShort && ev->type != InputTypeRepeat) return false;

    switch(ev->key) {
    case InputKeyUp:
        name_step(app, 1);
        app->edit_msg[0] = 0;
        return true;
    case InputKeyDown:
        name_step(app, -1);
        app->edit_msg[0] = 0;
        return true;
    case InputKeyLeft:
        if(ev->type != InputTypeShort) return false;
        app->name_pos = (uint8_t)((app->name_pos + CAR_NAME_LEN - 1U) % CAR_NAME_LEN);
        return true;
    case InputKeyRight:
        if(ev->type != InputTypeShort) return false;
        app->name_pos = (uint8_t)((app->name_pos + 1U) % CAR_NAME_LEN);
        return true;
    case InputKeyOk:
        if(ev->type != InputTypeShort) return false;
        name_confirm(app);
        return true;
    case InputKeyBack:
        if(ev->type != InputTypeShort) return false;
        name_cancel(app);
        return true;
    default:
        return false;
    }
}

static bool on_input(App* app, const InputEvent* ev) {
    switch(app->screen) {
    case ScreenMenu:
        return on_input_menu(app, ev);
    case ScreenRx:
        return on_input_rx(app, ev);
    case ScreenTx:
        return on_input_tx(app, ev);
    case ScreenSettings:
        return on_input_settings(app, ev);
    case ScreenIdEdit:
        return on_input_id_edit(app, ev);
    case ScreenPair:
        return on_input_pair(app, ev);
    case ScreenCars:
        return on_input_cars(app, ev);
    case ScreenName:
        return on_input_name(app, ev);
    case ScreenAbout:
        return on_input_about(app, ev);
    default:
        return false;
    }
}

/* ------------------------------------------------------------------------ */

int32_t nissan_dashboard_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->screen = ScreenMenu;
    app->running = true;

    /* stato iniziale: ID predefiniti, dati TX provvisori, poi ultima auto salvata */
    memcpy(g_id, k_default_ids, sizeof(g_id));
    memcpy(g_tx, k_tx_default, sizeof(g_tx));
    memset(g_rx, 0, sizeof(g_rx));
    g_car_name[0] = 0;
    cfg_snapshot(g_saved);
    active_load(); /* ultima auto usata, se c'e' */

    app->q = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->notif = furi_record_open(RECORD_NOTIFICATION);

    app->vp = view_port_alloc();
    view_port_draw_callback_set(app->vp, draw_cb, app);
    view_port_input_callback_set(app->vp, input_cb, app->q);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->vp, GuiLayerFullscreen);

    uint32_t last_draw = furi_get_tick();
    InputEvent ev;

    while(app->running) {
        bool changed = false;

        /* 1) ricezione: svuota gli impulsi e decodifica */
        if(app->rx_active) changed = rx_poll(app);
        if(profiles_scan_poll(app)) changed = true;

        /* 2) tasti (in RX attesa breve, per non far traboccare il buffer) */
        uint32_t timeout = app->rx_active ? 5U : 100U;
        if(furi_message_queue_get(app->q, &ev, timeout) == FuriStatusOk) {
            if(on_input(app, &ev)) changed = true;
        }

        /* 3) ridisegna su evento, e comunque ogni secondo (eta' dei dati) */
        uint32_t now = furi_get_tick();
        if(changed || (now - last_draw) >= 1000U) {
            view_port_update(app->vp);
            last_draw = now;
        }
    }

    profiles_scan_stop(app);
    rx_leave(app);
    notification_message(app->notif, &sequence_reset_rgb);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(gui, app->vp);
    furi_record_close(RECORD_GUI);
    view_port_free(app->vp);
    furi_message_queue_free(app->q);
    free(app);
    return 0;
}