/*
 * IR Identifier - Flipper Zero (Momentum Dev)
 *
 * Receives a signal from an IR remote and identifies the device type and brand.
 * It collects ALL the matches found (up to MAX_CAND) and shows them one at a time:
 * if the first one is not the right one, move on to the next.
 *
 * Sources, in order:
 *  1) Built-in table (protocol + address -> brand/type) for well-known cases.
 *  2) Flipper universal databases in the /ext/infrared/assets folder (tv.ir, ac.ir, ...):
 *     the file name gives the device TYPE (these databases do not contain brands).
 *  3) User .ir files in /ext/infrared and subfolders (e.g. Flipper-IRDB): the BRAND and
 *     the type are derived from the path/file name (e.g. TVs/Samsung/xxx.ir).
 *  4) For long RAW signals (typically air conditioners): estimate from the header timings.
 *
 * Ordering of the matches (see cand_key): recognized brands always come before unknown
 * ones; among them, the best signal match, the most famous brand (BRAND_KEYS) and a known
 * device type rank higher. Unknown folder names are shown last.
 *
 * Controls
 *   Main menu    : Up/Down = select, OK = enter, Back = exit
 *                  Scan = start the app, Log = saved logs, Settings = options,
 *                  About = version/credits
 *   Log list     : Up/Down = select, OK = open, Left = delete (asks), Back = menu
 *   Log viewer   : Up/Down = scroll, Left/Right (or OK) = page, Back = list
 *   Settings     : Up/Down = select row, Left/Right/OK = change value
 *                  User DB: ON/OFF
 *                  Log    : Yes = always save the log of a successful search
 *                           No  = never save
 *                           Ask = ask before leaving the results screen
 *                  Clear all logs: OK = delete every log (asks for confirmation)
 *   About        : Back/OK = menu
 *   Waiting      : point the remote and press a button, Back = menu
 *   While scanning: Back = stop and show what was found so far
 *   Result screen : Left/Right (or Up/Down) = previous/next match
 *                   Back = resume the interrupted scan (if it was interrupted),
 *                          otherwise start a new scan
 *                   OK = new scan
 *                   (with Log = Yes/Ask the log is saved / requested when leaving this screen)
 *   Ask log      : OK = save the log, Back = skip
 *   Confirm      : OK = confirm the deletion, Back = cancel
 *
 * Logs are text files in /ext/apps_data/ir_identifier/ (log_YYYYMMDD_HHMMSS.txt) and list
 * the full path of every matching .ir file.
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>
#include <infrared_worker.h>
#include <furi_hal_rtc.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAW_MAX        1024
#define NAME_LEN       128
#define SCAN_MAX_DEPTH 4
#define MAX_CAND       24
#define ASSETS_DIR     EXT_PATH("infrared/assets")
#define USER_DIR       EXT_PATH("infrared")
#define PATH_LEN       128

#define LOG_DIR        EXT_PATH("apps_data/ir_identifier")
#define LOG_NAME_LEN   32
#define LOG_MAX_FILES  40 /* newest files kept in the list */
#define LOG_LIST_ROWS  4
#define LOG_VIEW_ROWS  6
#define LOG_COLS       22 /* characters per line in the viewer (hard wrap) */
#define LOG_VIEW_MAX   6144 /* bytes of a log file shown in the viewer */
#define LOG_MAX_LINES  400

#define MENU_COUNT     4 /* Scan, Log, Settings, About */

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    AppStateMenu,
    AppStateSettings,
    AppStateAbout,
    AppStateWaiting,
    AppStateAnalyzing,
    AppStateResult,
    AppStateAskLog,
    AppStateLogList,
    AppStateLogView,
    AppStateConfirmDelLog,
    AppStateConfirmClearAll,
} AppState;

typedef enum {
    LogModeYes, /* always save the log of a successful search */
    LogModeNo, /* never save */
    LogModeAsk, /* ask before leaving the results screen */
} LogMode;

typedef enum {
    EvtKey,
    EvtSignal,
} EvtType;

typedef enum {
    OriginAsset,
    OriginUser,
    OriginKnown,
} Origin;

typedef struct {
    EvtType type;
    InputEvent input;
} AppEvent;

typedef struct {
    bool decoded;
    char protocol[24];
    uint32_t address;
    uint32_t command;
    uint32_t* raw;
    size_t raw_len;
} RxSignal;

typedef struct {
    char type[32];
    char brand[32];
    char source[40];
    char path[PATH_LEN]; /* full path of the matching .ir file (empty for built-in/heuristic) */
    uint32_t hash; /* path hash, used to avoid duplicates */
    uint8_t score; /* 0 weak/estimate, 1 protocol+address (or similar RAW), 2 also command */
    uint8_t fame; /* brand popularity: 0 unknown, 1 known, 2 common, 3 very famous */
    Origin origin;
} Candidate;

typedef struct {
    Gui* gui;
    ViewPort* vp;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    Storage* storage;
    InfraredWorker* worker;

    AppState state;
    RxSignal rx;
    uint32_t* db_raw;

    Candidate known; /* built-in table or RAW heuristic */
    bool has_known;
    Candidate cand[MAX_CAND]; /* matches found in .ir files */
    size_t ncand;
    Candidate* order[MAX_CAND + 1]; /* sorted list shown to the user */
    size_t nord;
    size_t sel;
    uint8_t menu_sel; /* 0 = Scan, 1 = Log, 2 = Settings, 3 = About */
    uint8_t set_sel; /* Settings row: 0 = User DB, 1 = Log, 2 = Clear all logs */
    char set_msg[24]; /* short message shown in Settings (e.g. "Logs deleted") */

    volatile bool pending;
    bool rx_running;
    bool abort;
    bool stop;
    bool search_user_db;
    LogMode log_mode;
    char notice[24]; /* short message shown on the waiting screen (e.g. "Log saved") */

    /* log list / viewer */
    char log_names[LOG_MAX_FILES][LOG_NAME_LEN];
    size_t log_count;
    size_t log_sel;
    size_t log_top;
    char* log_text; /* contents of the open log (allocated only while viewing) */
    size_t log_len;
    uint16_t* log_lines; /* start offset of every wrapped line */
    size_t log_nlines;
    size_t log_scroll;

    char status[40];
    uint32_t files_scanned;

    /* Resume state: for every directory level, the index of the entry that was being
     * processed when the scan was interrupted. On resume the walk fast-forwards along
     * this path (skipped entries are never opened, nothing is descended into). */
    uint32_t resume_pos[SCAN_MAX_DEPTH + 1];
    bool resume_active; /* still fast-forwarding to the interrupted point */
    uint8_t resume_phase; /* 0 = assets DB, 1 = user files */
    bool can_resume; /* scan was interrupted: Back resumes it */
} App;

/* ------------------------------------------------------------------ */
/* Tables                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char* key;
    const char* label;
} KeyLabel;

static const KeyLabel TYPE_KEYS[] = {
    {"tv", "TV"},
    {"tvs", "TV"},
    {"television", "TV"},
    {"televisions", "TV"},
    {"ac", "Air conditioner"},
    {"acs", "Air conditioner"},
    {"aircon", "Air conditioner"},
    {"conditioner", "Air conditioner"},
    {"audio", "Audio"},
    {"soundbar", "Audio"},
    {"speaker", "Audio"},
    {"speakers", "Audio"},
    {"receiver", "Audio"},
    {"amplifier", "Audio"},
    {"amp", "Audio"},
    {"hifi", "Audio"},
    {"stereo", "Audio"},
    {"projector", "Projector"},
    {"projectors", "Projector"},
    {"beamer", "Projector"},
    {"fan", "Fan"},
    {"fans", "Fan"},
    {"led", "LED lighting"},
    {"leds", "LED lighting"},
    {"light", "LED lighting"},
    {"lights", "LED lighting"},
    {"lamp", "LED lighting"},
    {"dvd", "DVD/BD player"},
    {"bluray", "DVD/BD player"},
    {"blu", "DVD/BD player"},
    {"cd", "CD player"},
    {"stb", "Set-top box"},
    {"settop", "Set-top box"},
    {"sat", "Set-top box"},
    {"satellite", "Set-top box"},
    {"cable", "Set-top box"},
    {"decoder", "Set-top box"},
    {"heater", "Heater"},
    {"humidifier", "Humidifier"},
    {"purifier", "Air purifier"},
    {"vacuum", "Robot vacuum"},
};

typedef struct {
    const char* key; /* lowercase token found in a path / file name */
    const char* label; /* name shown to the user */
    uint8_t fame; /* 3 = very famous, 2 = common, 1 = known / niche */
} BrandKey;

/* Brands recognized in paths and file names. The fame value drives the ordering of the
 * results: famous brands are shown before niche ones, and anything NOT in this table
 * (unknown folder names, "Not specified") is shown last. Add or re-rank brands here. */
static const BrandKey BRAND_KEYS[] = {
    {"samsung", "Samsung", 3}, {"lg", "LG", 3},
    {"sony", "Sony", 3}, {"philips", "Philips", 3},
    {"panasonic", "Panasonic", 3}, {"toshiba", "Toshiba", 3},
    {"sharp", "Sharp", 3}, {"hisense", "Hisense", 3},
    {"tcl", "TCL", 3}, {"xiaomi", "Xiaomi", 3},
    {"apple", "Apple", 3}, {"amazon", "Amazon", 3},
    {"firetv", "Amazon", 3}, {"google", "Google", 3},
    {"chromecast", "Google", 3}, {"daikin", "Daikin", 3},
    {"mitsubishi", "Mitsubishi", 3}, {"yamaha", "Yamaha", 3},
    {"pioneer", "Pioneer", 3}, {"bose", "Bose", 3},
    {"denon", "Denon", 3}, {"epson", "Epson", 3},
    {"hitachi", "Hitachi", 3}, {"haier", "Haier", 3},
    {"midea", "Midea", 3}, {"sky", "Sky", 3},
    {"gree", "Gree", 2}, {"carrier", "Carrier", 2},
    {"fujitsu", "Fujitsu", 2}, {"whirlpool", "Whirlpool", 2},
    {"electrolux", "Electrolux", 2}, {"beko", "Beko", 2},
    {"bosch", "Bosch", 2}, {"siemens", "Siemens", 2},
    {"onkyo", "Onkyo", 2}, {"marantz", "Marantz", 2},
    {"jvc", "JVC", 2}, {"benq", "BenQ", 2},
    {"optoma", "Optoma", 2}, {"dyson", "Dyson", 2},
    {"vizio", "Vizio", 2}, {"jbl", "JBL", 2},
    {"sonos", "Sonos", 2}, {"grundig", "Grundig", 2},
    {"thomson", "Thomson", 2}, {"rca", "RCA", 2},
    {"hyundai", "Hyundai", 2}, {"vestel", "Vestel", 2},
    {"sanyo", "Sanyo", 2}, {"nvidia", "NVIDIA", 2},
    {"tivo", "TiVo", 2}, {"insignia", "Insignia", 2},
    {"roku", "Roku", 2}, {"acer", "Acer", 2},
    {"asus", "ASUS", 2}, {"hp", "HP", 2},
    {"viewsonic", "ViewSonic", 2}, {"huawei", "Huawei", 2},
    {"oppo", "Oppo", 2}, {"oneplus", "OnePlus", 2},
    {"nokia", "Nokia", 2}, {"harman", "Harman", 2},
    {"kenwood", "Kenwood", 2}, {"technics", "Technics", 2},
    {"klipsch", "Klipsch", 2}, {"sennheiser", "Sennheiser", 2},
    {"logitech", "Logitech", 2}, {"ikea", "IKEA", 2},
    {"osram", "Osram", 2}, {"ledvance", "Ledvance", 2},
    {"roborock", "Roborock", 2}, {"irobot", "iRobot", 2},
    {"roomba", "iRobot", 2}, {"ecovacs", "Ecovacs", 2},
    {"eufy", "Eufy", 2}, {"xbox", "Xbox", 2},
    {"lennox", "Lennox", 2}, {"trane", "Trane", 2},
    {"rheem", "Rheem", 2}, {"olimpia", "Olimpia Splendid", 2},
    {"argo", "Argo", 2}, {"ariston", "Ariston", 2},
    {"baxi", "Baxi", 2}, {"delonghi", "De'Longhi", 2},
    {"humax", "Humax", 2}, {"technisat", "TechniSat", 2},
    {"kathrein", "Kathrein", 2}, {"dreambox", "Dreambox", 2},
    {"tivusat", "Tivusat", 2}, {"arris", "ARRIS", 2},
    {"motorola", "Motorola", 2}, {"cisco", "Cisco", 2},
    {"comcast", "Comcast", 2}, {"xfinity", "Comcast", 2},
    {"directv", "DirecTV", 2}, {"echostar", "EchoStar", 2},
    {"vodafone", "Vodafone", 2}, {"blaupunkt", "Blaupunkt", 2},
    {"medion", "Medion", 2}, {"loewe", "Loewe", 2},
    {"metz", "Metz", 2}, {"akai", "Akai", 1},
    {"telefunken", "Telefunken", 1}, {"westinghouse", "Westinghouse", 1},
    {"element", "Element", 1}, {"magnavox", "Magnavox", 1},
    {"sceptre", "Sceptre", 1}, {"funai", "Funai", 1},
    {"emerson", "Emerson", 1}, {"polaroid", "Polaroid", 1},
    {"finlux", "Finlux", 1}, {"sencor", "Sencor", 1},
    {"nordmende", "Nordmende", 1}, {"schneider", "Schneider", 1},
    {"daewoo", "Daewoo", 1}, {"orion", "Orion", 1},
    {"skyworth", "Skyworth", 1}, {"changhong", "Changhong", 1},
    {"konka", "Konka", 1}, {"coocaa", "Coocaa", 1},
    {"kogan", "Kogan", 1}, {"realme", "Realme", 1},
    {"hannspree", "Hannspree", 1}, {"proscan", "Proscan", 1},
    {"coby", "Coby", 1}, {"seiki", "Seiki", 1},
    {"bush", "Bush", 1}, {"luxor", "Luxor", 1},
    {"cello", "Cello", 1}, {"aoc", "AOC", 1},
    {"dell", "Dell", 1}, {"casio", "Casio", 1},
    {"vivitek", "Vivitek", 1}, {"infocus", "InFocus", 1},
    {"xgimi", "XGIMI", 1}, {"sim2", "SIM2", 1},
    {"eiki", "Eiki", 1}, {"ricoh", "Ricoh", 1},
    {"sinclair", "Sinclair", 1}, {"airwell", "Airwell", 1},
    {"chigo", "Chigo", 1}, {"kelon", "Kelon", 1},
    {"tadiran", "Tadiran", 1}, {"tosot", "Tosot", 1},
    {"mcquay", "Mcquay", 1}, {"york", "York", 1},
    {"rowenta", "Rowenta", 1}, {"honeywell", "Honeywell", 1},
    {"vornado", "Vornado", 1}, {"lasko", "Lasko", 1},
    {"govee", "Govee", 1}, {"dreame", "Dreame", 1},
    {"shark", "Shark", 1}, {"neato", "Neato", 1},
    {"nad", "NAD", 1}, {"arcam", "Arcam", 1},
    {"rotel", "Rotel", 1}, {"sherwood", "Sherwood", 1},
    {"sansui", "Sansui", 1}, {"teac", "TEAC", 1},
    {"polk", "Polk", 1}, {"wharfedale", "Wharfedale", 1},
    {"edifier", "Edifier", 1}, {"anker", "Anker", 1},
    {"soundcore", "Soundcore", 1}, {"mecool", "Mecool", 1},
    {"tanix", "Tanix", 1}, {"minix", "Minix", 1},
    {"zidoo", "Zidoo", 1}, {"formuler", "Formuler", 1},
    {"swisscom", "Swisscom", 1}, {"orange", "Orange", 1},
};

/* Folder names that say nothing about the brand (never used as a brand). */
static const char* const GENERIC_DIRS[] = {
    "misc", "miscellaneous", "other", "others", "various", "unknown", "universal", "remote",
    "remotes", "custom", "test", "tests", "backup", "new", "old", "my", "flipper", "irdb",
    "ir", "saved", "captured", "signals", "devices", "user", "download", "downloads", "temp",
    "tmp", "stuff", "misc_ir",
};

/* Well-known protocol + address pairs. address = -1 -> any. First match wins. */
typedef struct {
    const char* protocol;
    int32_t address;
    const char* brand;
    const char* type; /* NULL = cannot be determined */
    uint8_t conf;
} KnownDev;

static const KnownDev KNOWN[] = {
    {"Samsung32", 0x07, "Samsung", "TV", 2},
    {"Samsung32", -1, "Samsung", NULL, 1},
    {"NEC", 0x04, "LG", "TV", 1},
    {"NEC", 0x40, "Toshiba", "TV", 1},
    {"SIRC", 0x01, "Sony", "TV", 2},
    {"SIRC15", 0x01, "Sony", "TV", 2},
    {"SIRC20", 0x01, "Sony", "TV", 2},
    {"SIRC", -1, "Sony", NULL, 2},
    {"SIRC15", -1, "Sony", NULL, 2},
    {"SIRC20", -1, "Sony", NULL, 2},
    {"RC5", 0x00, "Philips (RC5)", "TV", 1},
    {"RC5", 0x05, "Philips (RC5)", "VCR", 1},
    {"RC5", 0x10, "Philips (RC5)", "Audio (preamp)", 1},
    {"RC5", 0x14, "Philips (RC5)", "CD player", 1},
    {"RC5", -1, "Philips (RC5)", NULL, 0},
    {"RC6", -1, "Philips (RC6)", NULL, 0},
    {"Kaseikyo", -1, "Panasonic/Kaseikyo", NULL, 1},
    {"RCA", -1, "RCA/Thomson", NULL, 1},
    {"Pioneer", -1, "Pioneer", "Audio/AV", 2},
};

/* Heuristic on long RAW headers (air conditioners). Values in microseconds, approximate. */
typedef struct {
    const char* brand;
    uint16_t m1, s1, m2, s2; /* m2/s2 = 0 -> first pair only */
} AcFp;

static const AcFp AC_FP[] = {
    {"Haier", 3000, 3000, 3000, 4300},
    {"Daikin", 3650, 1623, 0, 0},
    {"Mitsubishi", 3400, 1750, 0, 0},
    {"Hitachi", 3300, 1700, 0, 0},
    {"Panasonic", 3456, 1728, 0, 0},
    {"Fujitsu", 3324, 1574, 0, 0},
    {"Gree", 9000, 4500, 0, 0},
    {"Midea/Carrier", 4480, 4480, 0, 0},
    {"Toshiba", 4400, 4300, 0, 0},
};

/* ------------------------------------------------------------------ */
/* Utilities                                                           */
/* ------------------------------------------------------------------ */

static bool str_ieq(const char* a, const char* b) {
    while(*a && *b) {
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++;
        b++;
    }
    return *a == *b;
}

static bool is_ir_file(const char* name) {
    size_t l = strlen(name);
    return l > 3 && str_ieq(name + l - 3, ".ir");
}

static uint32_t path_hash(const char* s) {
    uint32_t h = 2166136261u;
    while(*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h;
}

static const char* lookup(const KeyLabel* tbl, size_t n, const char* tok) {
    for(size_t i = 0; i < n; i++) {
        if(strcmp(tbl[i].key, tok) == 0) return tbl[i].label;
    }
    return NULL;
}

static const BrandKey* lookup_brand(const char* tok) {
    for(size_t i = 0; i < COUNT_OF(BRAND_KEYS); i++) {
        if(strcmp(BRAND_KEYS[i].key, tok) == 0) return &BRAND_KEYS[i];
    }
    return NULL;
}

static bool is_generic_dir(const char* low) {
    for(size_t i = 0; i < COUNT_OF(GENERIC_DIRS); i++) {
        if(strcmp(GENERIC_DIRS[i], low) == 0) return true;
    }
    return false;
}

/* Popularity of a brand label: exact label match ("Amazon", "De'Longhi"), otherwise the
 * first word ("Philips (RC5)", "Haier?", "Midea/Carrier"). 0 = unknown brand. */
static uint8_t brand_fame(const char* brand) {
    uint8_t best = 0;
    for(size_t i = 0; i < COUNT_OF(BRAND_KEYS); i++) {
        if(str_ieq(BRAND_KEYS[i].label, brand) && BRAND_KEYS[i].fame > best) {
            best = BRAND_KEYS[i].fame;
        }
    }
    if(best) return best;

    char tok[24];
    size_t n = 0;
    for(const char* p = brand; *p && isalnum((unsigned char)*p) && n < sizeof(tok) - 1; p++) {
        tok[n++] = (char)tolower((unsigned char)*p);
    }
    tok[n] = 0;
    if(!n) return 0;
    const BrandKey* b = lookup_brand(tok);
    return b ? b->fame : 0;
}

/* Reliability score used to sort the results (higher = shown first):
 *  - a recognized brand ALWAYS ranks above an unknown one (+100)
 *  - then how well the signal matched (score), how famous the brand is, and whether the
 *    device type is known ("complete" result)
 *  - finally built-in table > user files > assets */
static int cand_key(const Candidate* c) {
    int k = c->score * 15 + c->fame * 10;
    if(c->fame > 0) k += 100;
    if(c->type[0] && strcmp(c->type, "Unknown") != 0) k += 6;
    if(c->origin == OriginKnown) k += 2;
    if(c->origin == OriginUser) k += 1;
    return k;
}

/* Derives type and brand from a path/file name (tokens split on non-alphanumerics). */
static void classify_path(
    const char* path,
    bool want_brand,
    char* type,
    size_t tn,
    char* brand,
    size_t bn) {
    type[0] = 0;
    brand[0] = 0;
    char tok[24];
    size_t tl = 0;

    for(const char* p = path;; p++) {
        char c = *p;
        if(isalnum((unsigned char)c)) {
            if(tl < sizeof(tok) - 1) tok[tl++] = (char)tolower((unsigned char)c);
            continue;
        }
        if(tl) {
            tok[tl] = 0;
            tl = 0;
            if(!type[0]) {
                const char* t = lookup(TYPE_KEYS, COUNT_OF(TYPE_KEYS), tok);
                if(t) strlcpy(type, t, tn);
            }
            if(want_brand && !brand[0]) {
                const BrandKey* b = lookup_brand(tok);
                if(b) strlcpy(brand, b->label, bn);
            }
        }
        if(!c) break;
    }

    /* Brand fallback: the folder containing the file (unless it is "infrared" or a type). */
    if(want_brand && !brand[0]) {
        const char* base = strrchr(path, '/');
        if(base && base > path) {
            const char* start = base;
            while(start > path && *(start - 1) != '/') start--;
            size_t len = (size_t)(base - start);
            char low[32];
            if(len > 0 && len < sizeof(low)) {
                for(size_t i = 0; i < len; i++) low[i] = (char)tolower((unsigned char)start[i]);
                low[len] = 0;
                if(strcmp(low, "infrared") != 0 && !is_generic_dir(low) &&
                   !lookup(TYPE_KEYS, COUNT_OF(TYPE_KEYS), low)) {
                    size_t n = len < bn - 1 ? len : bn - 1;
                    memcpy(brand, start, n);
                    brand[n] = 0;
                }
            }
        }
    }
}

static bool tol_eq(uint32_t a, uint32_t b) {
    uint32_t d = a > b ? a - b : b - a;
    uint32_t m = a > b ? a : b;
    return d <= 150 || (d * 4) <= m; /* 150 us or 25% */
}

static bool raw_similar(const uint32_t* a, size_t na, const uint32_t* b, size_t nb) {
    if(na < 4 || nb < 4) return false;
    size_t lo = na < nb ? na : nb;
    size_t hi = na < nb ? nb : na;
    size_t diff = hi - lo;
    if(lo < 80) {
        if(diff > 2) return false;
    } else if(diff > hi / 5) {
        return false;
    }
    size_t n = lo < 80 ? 4 : 8;
    for(size_t i = 0; i < n; i++) {
        if(!tol_eq(a[i], b[i])) return false;
    }
    return true;
}

static uint32_t rel_err_permille(uint32_t a, uint32_t b) {
    uint32_t d = a > b ? a - b : b - a;
    return (d * 1000u) / b;
}

static void set_status(App* app, const char* name) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strlcpy(app->status, name, sizeof(app->status));
    app->files_scanned++;
    furi_mutex_release(app->mutex);
    view_port_update(app->vp);
}

/* While scanning, checks whether the user pressed Back to interrupt. */
static void poll_abort(App* app) {
    AppEvent e;
    while(furi_message_queue_get(app->queue, &e, 0) == FuriStatusOk) {
        if(e.type == EvtKey && e.input.key == InputKeyBack) app->abort = true;
    }
}

/* ------------------------------------------------------------------ */
/* Identification engine                                               */
/* ------------------------------------------------------------------ */

static void apply_known(App* app) {
    const RxSignal* s = &app->rx;
    if(!s->decoded) return;

    for(size_t i = 0; i < COUNT_OF(KNOWN); i++) {
        const KnownDev* k = &KNOWN[i];
        if(!str_ieq(k->protocol, s->protocol)) continue;
        if(k->address >= 0 && (uint32_t)k->address != s->address) continue;

        Candidate* c = &app->known;
        memset(c, 0, sizeof(Candidate));
        strlcpy(c->brand, k->brand, sizeof(c->brand));
        if(k->type) strlcpy(c->type, k->type, sizeof(c->type));
        strlcpy(c->source, "built-in table", sizeof(c->source));
        c->score = k->conf;
        c->fame = brand_fame(c->brand);
        c->origin = OriginKnown;
        app->has_known = true;
        break;
    }
}

static void apply_raw_heuristics(App* app) {
    const RxSignal* s = &app->rx;
    if(s->decoded || s->raw_len < 80) return;

    uint32_t best = 80; /* 8% threshold */
    const AcFp* best_fp = NULL;
    size_t lim = s->raw_len > 44 ? 40 : s->raw_len - 4;

    for(size_t i = 0; i < lim; i++) {
        for(size_t k = 0; k < COUNT_OF(AC_FP); k++) {
            const AcFp* fp = &AC_FP[k];
            uint32_t e = rel_err_permille(s->raw[i], fp->m1);
            uint32_t e2 = rel_err_permille(s->raw[i + 1], fp->s1);
            if(e2 > e) e = e2;
            if(fp->m2) {
                e2 = rel_err_permille(s->raw[i + 2], fp->m2);
                if(e2 > e) e = e2;
                e2 = rel_err_permille(s->raw[i + 3], fp->s2);
                if(e2 > e) e = e2;
            }
            if(e < best) {
                best = e;
                best_fp = fp;
            }
        }
    }

    if(best_fp || app->ncand == 0) {
        Candidate* c = &app->known;
        memset(c, 0, sizeof(Candidate));
        if(best_fp) {
            snprintf(c->brand, sizeof(c->brand), "%s?", best_fp->brand);
        } else {
            strlcpy(c->brand, "Not identified", sizeof(c->brand));
        }
        strlcpy(c->type, "Air conditioner?", sizeof(c->type));
        strlcpy(c->source, "RAW heuristic", sizeof(c->source));
        c->score = 0;
        c->fame = brand_fame(c->brand);
        c->origin = OriginKnown;
        app->has_known = true;
    }
}

/* Records a match found in a .ir file. */
static void consider(App* app, uint8_t score, const char* path, bool is_asset) {
    uint32_t h = path_hash(path);

    /* same file already listed: just update its score */
    for(size_t i = 0; i < app->ncand; i++) {
        if(app->cand[i].hash == h) {
            if(score > app->cand[i].score) app->cand[i].score = score;
            return;
        }
    }

    /* build the new candidate first, so it can be compared with the others */
    Candidate n;
    memset(&n, 0, sizeof(Candidate));
    n.hash = h;
    n.score = score;
    n.origin = is_asset ? OriginAsset : OriginUser;

    char t[32];
    char b[32];
    classify_path(path, !is_asset, t, sizeof(t), b, sizeof(b));
    strlcpy(n.type, t[0] ? t : "Unknown", sizeof(n.type));
    strlcpy(n.brand, b[0] ? b : "Not specified", sizeof(n.brand));
    n.fame = brand_fame(n.brand);
    const char* base = strrchr(path, '/');
    strlcpy(n.source, base ? base + 1 : path, sizeof(n.source));
    strlcpy(n.path, path, sizeof(n.path));

    size_t slot;
    if(app->ncand < MAX_CAND) {
        slot = app->ncand++;
    } else {
        /* list full: replace the worst entry (lowest score, then least famous brand)
         * only if the new one ranks higher */
        slot = 0;
        for(size_t i = 1; i < app->ncand; i++) {
            if(cand_key(&app->cand[i]) < cand_key(&app->cand[slot])) slot = i;
        }
        if(cand_key(&n) <= cand_key(&app->cand[slot])) return;
    }
    app->cand[slot] = n;

    /* stop scanning only when the list is full of exact matches from well-known brands */
    if(app->ncand == MAX_CAND) {
        bool all_top = true;
        for(size_t i = 0; i < app->ncand; i++) {
            if(app->cand[i].score < 2 || app->cand[i].fame < 2) all_top = false;
        }
        if(all_top) app->stop = true;
    }
}

static void scan_file(App* app, const char* path, bool is_asset) {
    FlipperFormat* ff = flipper_format_buffered_file_alloc(app->storage);
    FuriString* s = furi_string_alloc();
    uint32_t ver = 0;

    if(flipper_format_buffered_file_open_existing(ff, path) &&
       flipper_format_read_header(ff, s, &ver)) {
        while(!app->abort && !app->stop && flipper_format_read_string(ff, "name", s)) {
            poll_abort(app);
            if(!flipper_format_read_string(ff, "type", s)) break;

            if(furi_string_cmp_str(s, "parsed") == 0) {
                if(!app->rx.decoded) continue;
                if(!flipper_format_read_string(ff, "protocol", s)) break;
                if(!str_ieq(furi_string_get_cstr(s), app->rx.protocol)) continue;

                uint32_t addr = 0;
                uint32_t cmd = 0;
                if(!flipper_format_read_hex(ff, "address", (uint8_t*)&addr, 4)) break;
                if(!flipper_format_read_hex(ff, "command", (uint8_t*)&cmd, 4)) break;

                if(addr == app->rx.address) {
                    consider(app, (cmd == app->rx.command) ? 2 : 1, path, is_asset);
                }
            } else if(furi_string_cmp_str(s, "raw") == 0) {
                if(app->rx.decoded) continue;

                uint32_t freq = 0;
                float duty = 0;
                uint32_t cnt = 0;
                if(!flipper_format_read_uint32(ff, "frequency", &freq, 1)) break;
                if(!flipper_format_read_float(ff, "duty_cycle", &duty, 1)) break;
                if(!flipper_format_get_value_count(ff, "data", &cnt)) break;
                if(cnt == 0 || cnt > RAW_MAX) continue;
                if(!flipper_format_read_uint32(ff, "data", app->db_raw, (uint16_t)cnt)) break;

                if(raw_similar(app->rx.raw, app->rx.raw_len, app->db_raw, cnt)) {
                    consider(app, 1, path, is_asset);
                }
            }
        }
    }

    furi_string_free(s);
    flipper_format_free(ff);
}

static void scan_dir(App* app, const char* dir_path, uint8_t depth, bool is_asset) {
    if(app->abort || app->stop) return;

    File* dir = storage_file_alloc(app->storage);
    char* name = malloc(NAME_LEN);

    if(storage_dir_open(dir, dir_path)) {
        FileInfo info;
        uint32_t i = 0;
        while(!app->abort && !app->stop && storage_dir_read(dir, &info, name, NAME_LEN)) {
            uint32_t cur = i++;

            /* resuming: entries before the interrupted one are skipped without
             * allocating anything or descending into them */
            if(app->resume_active && cur < app->resume_pos[depth]) continue;

            FuriString* full = furi_string_alloc_printf("%s/%s", dir_path, name);
            if(file_info_is_dir(&info)) {
                bool skip = (!is_asset && strcmp(name, "assets") == 0);
                if(!skip && depth < SCAN_MAX_DEPTH) {
                    scan_dir(app, furi_string_get_cstr(full), depth + 1, is_asset);
                }
            } else if(is_ir_file(name)) {
                set_status(app, name);
                scan_file(app, furi_string_get_cstr(full), is_asset);
                poll_abort(app);
            }
            furi_string_free(full);

            /* interrupted: remember where we were at this level (the interrupted
             * file will be read again on resume) */
            if(app->abort) app->resume_pos[depth] = cur;

            /* the interrupted point has been reached: from now on, process everything */
            app->resume_active = false;
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    free(name);
}

static void build_order(App* app) {
    app->nord = 0;
    app->sel = 0;
    if(app->has_known) app->order[app->nord++] = &app->known;
    for(size_t i = 0; i < app->ncand; i++) app->order[app->nord++] = &app->cand[i];

    /* stable insertion sort, descending */
    for(size_t i = 1; i < app->nord; i++) {
        Candidate* x = app->order[i];
        size_t j = i;
        while(j > 0 && cand_key(app->order[j - 1]) < cand_key(x)) {
            app->order[j] = app->order[j - 1];
            j--;
        }
        app->order[j] = x;
    }
}

/* resume = true: continues an interrupted scan, keeping the matches already found
 * and skipping the files already read. */
static void identify(App* app, bool resume) {
    if(!resume) {
        app->ncand = 0;
        app->files_scanned = 0;
        app->resume_phase = 0;
        memset(app->resume_pos, 0, sizeof(app->resume_pos));
    }
    app->resume_active = resume;
    app->has_known = false;
    app->nord = 0;
    app->sel = 0;
    app->abort = false;
    app->stop = false;
    app->can_resume = false;

    /* 1) built-in table */
    apply_known(app);

    /* 2) Flipper universal DBs (top level only, no subfolders) */
    if(app->resume_phase == 0) {
        scan_dir(app, ASSETS_DIR, SCAN_MAX_DEPTH, true);
        app->resume_active = false;
        if(!app->abort) {
            app->resume_phase = 1;
            memset(app->resume_pos, 0, sizeof(app->resume_pos));
        }
    }

    /* 3) user .ir files (brand from path) */
    if(app->search_user_db && app->resume_phase == 1 && !app->abort) {
        app->stop = false;
        scan_dir(app, USER_DIR, 0, false);
        app->resume_active = false;
    }

    app->can_resume = app->abort;

    /* 4) RAW heuristic */
    apply_raw_heuristics(app);

    /* if the built-in table does not know the type, take it from the best DB match */
    if(app->has_known && !app->known.type[0]) {
        const Candidate* best = NULL;
        for(size_t i = 0; i < app->ncand; i++) {
            const Candidate* c = &app->cand[i];
            if(strcmp(c->type, "Unknown") == 0) continue;
            if(!best || c->score > best->score) best = c;
        }
        strlcpy(app->known.type, best ? best->type : "Unknown", sizeof(app->known.type));
    }

    build_order(app);
}

/* ------------------------------------------------------------------ */
/* GUI                                                                 */
/* ------------------------------------------------------------------ */

static void draw_fit(Canvas* canvas, uint8_t y, const char* text) {
    canvas_set_font(canvas, strlen(text) > 17 ? FontSecondary : FontPrimary);
    canvas_draw_str(canvas, 2, y, text);
}

static const char* conf_label(uint8_t score) {
    return score >= 2 ? "High" : (score == 1 ? "Medium" : "Low");
}

/* ------------------------------------------------------------------ */
/* Logs                                                                */
/* ------------------------------------------------------------------ */

static const char* log_mode_label(LogMode m) {
    return m == LogModeYes ? "Yes" : (m == LogModeNo ? "No" : "Ask");
}

/* A search is "successful" (worth logging) when at least one .ir file matched, or when
 * the built-in table recognized the signal. */
static bool log_worthy(const App* app) {
    if(app->ncand > 0) return true;
    return app->has_known && strcmp(app->known.source, "built-in table") == 0;
}

/* Saves the current search as a new text file. Returns true on success. */
static bool save_log(App* app) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    char stamp[32];
    snprintf(
        stamp,
        sizeof(stamp),
        "%04u%02u%02u_%02u%02u%02u",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second);

    const RxSignal* s = &app->rx;
    FuriString* txt = furi_string_alloc();
    furi_string_cat_printf(
        txt,
        "IR Identifier log\n%04u-%02u-%02u %02u:%02u:%02u\n",
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second);
    if(s->decoded) {
        furi_string_cat_printf(
            txt,
            "Signal: %s A:%02lX C:%02lX\n",
            s->protocol,
            (unsigned long)s->address,
            (unsigned long)s->command);
    } else {
        furi_string_cat_printf(txt, "Signal: RAW %u samples\n", (unsigned)s->raw_len);
    }
    if(app->can_resume) furi_string_cat_printf(txt, "Scan: interrupted\n");
    furi_string_cat_printf(txt, "Results: %u\n", (unsigned)app->nord);
    for(size_t i = 0; i < app->nord; i++) {
        const Candidate* c = app->order[i];
        furi_string_cat_printf(
            txt,
            "#%u %s - %s (%s)\n",
            (unsigned)(i + 1),
            c->brand,
            c->type[0] ? c->type : "Unknown",
            conf_label(c->score));
        if(c->path[0]) {
            furi_string_cat_printf(txt, "%s\n", c->path);
        } else {
            furi_string_cat_printf(txt, "[%s]\n", c->source);
        }
    }

    storage_simply_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(app->storage, LOG_DIR);

    FuriString* path = furi_string_alloc();
    File* f = storage_file_alloc(app->storage);
    bool ok = false;
    for(uint8_t n = 0; n < 10; n++) {
        if(n == 0) {
            furi_string_printf(path, "%s/log_%s.txt", LOG_DIR, stamp);
        } else {
            furi_string_printf(path, "%s/log_%s_%u.txt", LOG_DIR, stamp, (unsigned)n);
        }
        /* CREATE_NEW fails if the file exists: try the next suffix */
        if(storage_file_open(f, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_NEW)) {
            size_t len = furi_string_size(txt);
            ok = storage_file_write(f, furi_string_get_cstr(txt), len) == len;
            storage_file_close(f);
            break;
        }
    }

    storage_file_free(f);
    furi_string_free(path);
    furi_string_free(txt);
    return ok;
}

/* "log_20261022_123456.txt" -> "2026-10-22 12:34:56" */
static void log_pretty_name(const char* name, char* out, size_t n) {
    if(strlen(name) >= 19 && name[12] == '_') {
        snprintf(
            out,
            n,
            "%.4s-%.2s-%.2s %.2s:%.2s:%.2s",
            name + 4,
            name + 8,
            name + 10,
            name + 13,
            name + 15,
            name + 17);
    } else {
        strlcpy(out, name, n);
    }
}

/* Reads the log folder: keeps the newest LOG_MAX_FILES names, sorted newest first. */
static void log_load_list(App* app) {
    app->log_count = 0;
    app->log_sel = 0;
    app->log_top = 0;

    storage_simply_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(app->storage, LOG_DIR);

    File* dir = storage_file_alloc(app->storage);
    char* name = malloc(NAME_LEN);

    if(storage_dir_open(dir, LOG_DIR)) {
        FileInfo info;
        while(storage_dir_read(dir, &info, name, NAME_LEN)) {
            if(file_info_is_dir(&info)) continue;
            size_t l = strlen(name);
            if(l < 9 || l >= LOG_NAME_LEN) continue;
            if(strncmp(name, "log_", 4) != 0 || !str_ieq(name + l - 4, ".txt")) continue;

            size_t slot;
            if(app->log_count < LOG_MAX_FILES) {
                slot = app->log_count++;
            } else {
                /* full: replace the oldest one, only if this one is newer */
                slot = 0;
                for(size_t i = 1; i < app->log_count; i++) {
                    if(strcmp(app->log_names[i], app->log_names[slot]) < 0) slot = i;
                }
                if(strcmp(name, app->log_names[slot]) <= 0) continue;
            }
            strlcpy(app->log_names[slot], name, LOG_NAME_LEN);
        }
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    free(name);

    /* insertion sort, descending (names start with the timestamp) */
    char tmp[LOG_NAME_LEN];
    for(size_t i = 1; i < app->log_count; i++) {
        strlcpy(tmp, app->log_names[i], sizeof(tmp));
        size_t j = i;
        while(j > 0 && strcmp(app->log_names[j - 1], tmp) < 0) {
            strlcpy(app->log_names[j], app->log_names[j - 1], LOG_NAME_LEN);
            j--;
        }
        strlcpy(app->log_names[j], tmp, LOG_NAME_LEN);
    }
}

/* Splits the loaded text into lines of at most LOG_COLS characters. */
static void log_wrap(App* app) {
    app->log_nlines = 0;
    size_t i = 0;
    while(i < app->log_len && app->log_nlines < LOG_MAX_LINES) {
        app->log_lines[app->log_nlines++] = (uint16_t)i;
        size_t col = 0;
        while(i < app->log_len && app->log_text[i] != '\n' && col < LOG_COLS) {
            i++;
            col++;
        }
        if(i < app->log_len && app->log_text[i] == '\n') i++;
    }
}

static void log_open_view(App* app) {
    app->log_text = malloc(LOG_VIEW_MAX + 1);
    app->log_lines = malloc(LOG_MAX_LINES * sizeof(uint16_t));
    app->log_len = 0;
    app->log_scroll = 0;

    FuriString* path =
        furi_string_alloc_printf("%s/%s", LOG_DIR, app->log_names[app->log_sel]);
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        app->log_len = storage_file_read(f, app->log_text, LOG_VIEW_MAX);
        storage_file_close(f);
    }
    storage_file_free(f);
    furi_string_free(path);

    if(app->log_len == 0) {
        strlcpy(app->log_text, "Cannot read file", LOG_VIEW_MAX);
        app->log_len = strlen(app->log_text);
    }
    app->log_text[app->log_len] = 0;
    log_wrap(app);
}

static void log_close_view(App* app) {
    free(app->log_text);
    app->log_text = NULL;
    free(app->log_lines);
    app->log_lines = NULL;
    app->log_len = 0;
    app->log_nlines = 0;
}

/* Deletes the log selected in the list, then reloads the list keeping the position. */
static bool log_delete_selected(App* app) {
    if(app->log_sel >= app->log_count) return false;
    FuriString* path = furi_string_alloc_printf("%s/%s", LOG_DIR, app->log_names[app->log_sel]);
    bool ok = storage_simply_remove(app->storage, furi_string_get_cstr(path));
    furi_string_free(path);

    size_t keep = app->log_sel;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    log_load_list(app);
    if(app->log_count > 0) {
        if(keep >= app->log_count) keep = app->log_count - 1;
        app->log_sel = keep;
        if(app->log_sel >= LOG_LIST_ROWS) app->log_top = app->log_sel + 1 - LOG_LIST_ROWS;
    }
    furi_mutex_release(app->mutex);
    return ok;
}

/* Deletes every log file in LOG_DIR. Returns the number of files removed. */
static size_t log_delete_all(App* app) {
    enum { BATCH = 12 };
    size_t removed = 0;
    char(*names)[LOG_NAME_LEN] = malloc(BATCH * LOG_NAME_LEN);
    char* name = malloc(NAME_LEN);
    File* dir = storage_file_alloc(app->storage);

    for(;;) {
        size_t n = 0;
        if(storage_dir_open(dir, LOG_DIR)) {
            FileInfo info;
            while(n < BATCH && storage_dir_read(dir, &info, name, NAME_LEN)) {
                if(file_info_is_dir(&info)) continue;
                size_t l = strlen(name);
                if(l < 9 || l >= LOG_NAME_LEN) continue;
                if(strncmp(name, "log_", 4) != 0 || !str_ieq(name + l - 4, ".txt")) continue;
                strlcpy(names[n++], name, LOG_NAME_LEN);
            }
        }
        storage_dir_close(dir);
        if(n == 0) break;

        size_t done = 0;
        for(size_t i = 0; i < n; i++) {
            FuriString* path = furi_string_alloc_printf("%s/%s", LOG_DIR, names[i]);
            if(storage_simply_remove(app->storage, furi_string_get_cstr(path))) done++;
            furi_string_free(path);
        }
        removed += done;
        if(done == 0) break; /* nothing could be removed: avoid looping forever */
    }

    storage_file_free(dir);
    free(name);
    free(names);
    return removed;
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* app = ctx;
    char buf[64];

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);

    if(app->state == AppStateMenu) {
        static const char* const items[MENU_COUNT] = {"Scan", "Log", "Settings", "About"};
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "IR Identifier");
        for(uint8_t i = 0; i < MENU_COUNT; i++) {
            uint8_t y = 14 + i * 12;
            if(i == app->menu_sel) {
                canvas_draw_rbox(canvas, 24, y, 80, 11, 2);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str_aligned(canvas, 64, y + 6, AlignCenter, AlignCenter, items[i]);
            canvas_set_color(canvas, ColorBlack);
        }

    } else if(app->state == AppStateSettings) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Settings");
        for(uint8_t i = 0; i < 3; i++) {
            if(i == 0) {
                snprintf(
                    buf, sizeof(buf), "< User DB: %s >", app->search_user_db ? "ON" : "OFF");
            } else if(i == 1) {
                snprintf(buf, sizeof(buf), "< Log: %s >", log_mode_label(app->log_mode));
            } else {
                snprintf(buf, sizeof(buf), "Clear all logs");
            }
            uint8_t y = 13 + i * 13;
            if(i == app->set_sel) {
                canvas_draw_rbox(canvas, 8, y, 112, 12, 2);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str_aligned(canvas, 64, y + 6, AlignCenter, AlignCenter, buf);
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_set_font(canvas, FontSecondary);
        const char* hint;
        if(app->set_msg[0]) {
            hint = app->set_msg;
        } else if(app->set_sel == 0) {
            hint = "Also scan user .ir files";
        } else if(app->set_sel == 2) {
            hint = "OK: delete every log";
        } else if(app->log_mode == LogModeYes) {
            hint = "Always save positive log";
        } else if(app->log_mode == LogModeNo) {
            hint = "Never save logs";
        } else {
            hint = "Ask when leaving results";
        }
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, hint);

    } else if(app->state == AppStateAbout) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, "V.1.0");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "created by Marcopiiit");

    } else if(app->state == AppStateAskLog) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "Save log?");
        canvas_set_font(canvas, FontSecondary);
        snprintf(buf, sizeof(buf), "Results: %u", (unsigned)app->nord);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, buf);
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "OK: save   Back: skip");

    } else if(app->state == AppStateConfirmDelLog) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "Delete log?");
        canvas_set_font(canvas, FontSecondary);
        if(app->log_sel < app->log_count) {
            log_pretty_name(app->log_names[app->log_sel], buf, sizeof(buf));
            canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, buf);
        }
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "OK: delete  Back: cancel");

    } else if(app->state == AppStateConfirmClearAll) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignTop, "Clear all logs?");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignTop, "Every log will be deleted");
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "OK: delete  Back: cancel");

    } else if(app->state == AppStateLogList) {
        canvas_set_font(canvas, FontPrimary);
        snprintf(buf, sizeof(buf), "Logs (%u)", (unsigned)app->log_count);
        canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, buf);
        canvas_set_font(canvas, FontSecondary);
        if(app->log_count == 0) {
            canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, "No logs yet");
        } else {
            for(uint8_t i = 0; i < LOG_LIST_ROWS; i++) {
                size_t idx = app->log_top + i;
                if(idx >= app->log_count) break;
                uint8_t y = 14 + i * 12;
                if(idx == app->log_sel) {
                    canvas_draw_rbox(canvas, 2, y, 118, 11, 2);
                    canvas_set_color(canvas, ColorWhite);
                }
                log_pretty_name(app->log_names[idx], buf, sizeof(buf));
                canvas_draw_str_aligned(canvas, 8, y + 6, AlignLeft, AlignCenter, buf);
                canvas_set_color(canvas, ColorBlack);
            }
            if(app->log_count > LOG_LIST_ROWS) {
                /* scroll thumb */
                uint8_t track = LOG_LIST_ROWS * 12;
                uint8_t th = (uint8_t)(track * LOG_LIST_ROWS / app->log_count);
                if(th < 4) th = 4;
                size_t maxs = app->log_count - LOG_LIST_ROWS;
                uint8_t ty = 14 + (uint8_t)(((size_t)(track - th) * app->log_top) / maxs);
                canvas_draw_box(canvas, 125, ty, 2, th);
            }
        }

    } else if(app->state == AppStateLogView) {
        canvas_set_font(canvas, FontSecondary);
        for(uint8_t i = 0; i < LOG_VIEW_ROWS; i++) {
            size_t k = app->log_scroll + i;
            if(k >= app->log_nlines) break;
            size_t st = app->log_lines[k];
            size_t n = 0;
            char ln[LOG_COLS + 1];
            while(st + n < app->log_len && app->log_text[st + n] != '\n' && n < LOG_COLS) {
                ln[n] = app->log_text[st + n];
                n++;
            }
            ln[n] = 0;
            canvas_draw_str(canvas, 1, 8 + i * 10, ln);
        }
        if(app->log_nlines > LOG_VIEW_ROWS) {
            size_t maxs = app->log_nlines - LOG_VIEW_ROWS;
            uint8_t th = (uint8_t)(64 * LOG_VIEW_ROWS / app->log_nlines);
            if(th < 6) th = 6;
            uint8_t ty = (uint8_t)(((size_t)(64 - th) * app->log_scroll) / maxs);
            canvas_draw_box(canvas, 126, ty, 2, th);
        }

    } else if(app->state == AppStateWaiting) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "IR Identifier");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, "Point the remote here");
        canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignTop, "and press a button");
        if(app->notice[0]) {
            canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignTop, app->notice);
        }
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Back: menu");

    } else if(app->state == AppStateAnalyzing) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Analyzing...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, app->status);
        snprintf(buf, sizeof(buf), "Files read: %lu", (unsigned long)app->files_scanned);
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignTop, buf);
        snprintf(buf, sizeof(buf), "Matches: %u", (unsigned)app->ncand);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignTop, buf);
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Back: show results");

    } else {
        const RxSignal* s = &app->rx;
        const Candidate* c = app->nord ? app->order[app->sel] : NULL;

        draw_fit(canvas, 11, c ? c->type : "Unknown");
        draw_fit(canvas, 24, c ? c->brand : "Not identified");

        canvas_set_font(canvas, FontSecondary);
        if(s->decoded) {
            snprintf(
                buf,
                sizeof(buf),
                "%s A:%02lX C:%02lX",
                s->protocol,
                (unsigned long)s->address,
                (unsigned long)s->command);
        } else {
            snprintf(buf, sizeof(buf), "RAW: %u samples", (unsigned)s->raw_len);
        }
        canvas_draw_str(canvas, 2, 36, buf);

        if(c) {
            snprintf(buf, sizeof(buf), "Source: %s", c->source);
            canvas_draw_str(canvas, 2, 46, buf);
            if(app->can_resume) {
                snprintf(
                    buf,
                    sizeof(buf),
                    "%u/%u <> Back:resume",
                    (unsigned)(app->sel + 1),
                    (unsigned)app->nord);
            } else {
                snprintf(
                    buf,
                    sizeof(buf),
                    "%u/%u %s <> OK:new",
                    (unsigned)(app->sel + 1),
                    (unsigned)app->nord,
                    conf_label(c->score));
            }
            canvas_draw_str(canvas, 2, 57, buf);
        } else {
            canvas_draw_str(canvas, 2, 46, "No match found");
            canvas_draw_str(
                canvas,
                2,
                57,
                app->can_resume ? "Back:resume OK:new" : "OK: new  Back: return");
        }
    }

    furi_mutex_release(app->mutex);
}

static void input_cb(InputEvent* event, void* ctx) {
    App* app = ctx;
    if(event->type == InputTypeShort) {
        AppEvent e = {.type = EvtKey, .input = *event};
        furi_message_queue_put(app->queue, &e, 0);
    }
}

/* ------------------------------------------------------------------ */
/* IR reception (worker thread)                                        */
/* ------------------------------------------------------------------ */

static void ir_signal_cb(void* ctx, InfraredWorkerSignal* sig) {
    App* app = ctx;
    if(app->pending) return;
    app->pending = true;

    app->rx.decoded = infrared_worker_signal_is_decoded(sig);
    app->rx.raw_len = 0;

    if(app->rx.decoded) {
        const InfraredMessage* m = infrared_worker_get_decoded_signal(sig);
        const char* name = infrared_get_protocol_name(m->protocol);
        strlcpy(app->rx.protocol, name ? name : "?", sizeof(app->rx.protocol));
        app->rx.address = m->address;
        app->rx.command = m->command;
    } else {
        const uint32_t* timings = NULL;
        size_t cnt = 0;
        infrared_worker_get_raw_signal(sig, &timings, &cnt);
        if(cnt > RAW_MAX) cnt = RAW_MAX;
        if(timings && cnt) memcpy(app->rx.raw, timings, cnt * sizeof(uint32_t));
        app->rx.raw_len = cnt;
        app->rx.protocol[0] = 0;
        app->rx.address = 0;
        app->rx.command = 0;
    }

    AppEvent e = {.type = EvtSignal};
    furi_message_queue_put(app->queue, &e, 0);
}

static void stop_rx(App* app) {
    if(app->rx_running) {
        infrared_worker_rx_stop(app->worker);
        app->rx_running = false;
    }
}

static void set_state(App* app, AppState st) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = st;
    furi_mutex_release(app->mutex);
    view_port_update(app->vp);
}

static void restart_rx(App* app) {
    app->pending = false;
    if(!app->rx_running) {
        infrared_worker_rx_start(app->worker);
        app->rx_running = true;
    }
    app->state = AppStateWaiting;
    view_port_update(app->vp);
}

static void set_notice(App* app, const char* msg) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    strlcpy(app->notice, msg, sizeof(app->notice));
    furi_mutex_release(app->mutex);
}

/* Called when the user leaves the results screen (to start a new scan). Depending on the
 * Log setting, the successful search is saved, asked about, or ignored. */
static void leave_result(App* app) {
    set_notice(app, "");
    if(log_worthy(app)) {
        if(app->log_mode == LogModeYes) {
            set_notice(app, save_log(app) ? "Log saved" : "Log error");
        } else if(app->log_mode == LogModeAsk) {
            set_state(app, AppStateAskLog);
            return;
        }
    }
    restart_rx(app);
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int32_t ir_identifier_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->rx.raw = malloc(RAW_MAX * sizeof(uint32_t));
    app->db_raw = malloc(RAW_MAX * sizeof(uint32_t));
    app->queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->search_user_db = true;
    app->log_mode = LogModeAsk;
    app->state = AppStateMenu;

    app->worker = infrared_worker_alloc();
    infrared_worker_rx_enable_blink_on_receiving(app->worker, true);
    infrared_worker_rx_enable_signal_decoding(app->worker, true);
    infrared_worker_rx_set_received_signal_callback(app->worker, ir_signal_cb, app);

    app->vp = view_port_alloc();
    view_port_draw_callback_set(app->vp, draw_cb, app);
    view_port_input_callback_set(app->vp, input_cb, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->vp, GuiLayerFullscreen);

    bool running = true;
    AppEvent e;
    while(running) {
        if(furi_message_queue_get(app->queue, &e, FuriWaitForever) != FuriStatusOk) continue;

        if(e.type == EvtSignal) {
            if(app->state != AppStateWaiting) continue;
            set_notice(app, "");
            if(app->rx_running) {
                infrared_worker_rx_stop(app->worker);
                app->rx_running = false;
            }
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->state = AppStateAnalyzing;
            strlcpy(app->status, "Starting...", sizeof(app->status));
            furi_mutex_release(app->mutex);
            view_port_update(app->vp);

            identify(app, false);

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            app->state = AppStateResult;
            furi_mutex_release(app->mutex);
            view_port_update(app->vp);

        } else if(e.type == EvtKey) {
            switch(app->state) {
            case AppStateMenu:
                if(e.input.key == InputKeyBack) {
                    running = false;
                } else if(e.input.key == InputKeyUp) {
                    app->menu_sel = (app->menu_sel + MENU_COUNT - 1) % MENU_COUNT;
                    view_port_update(app->vp);
                } else if(e.input.key == InputKeyDown) {
                    app->menu_sel = (app->menu_sel + 1) % MENU_COUNT;
                    view_port_update(app->vp);
                } else if(e.input.key == InputKeyOk) {
                    if(app->menu_sel == 0) {
                        restart_rx(app); /* Scan: start receiving */
                    } else if(app->menu_sel == 1) {
                        log_load_list(app);
                        set_state(app, AppStateLogList);
                    } else if(app->menu_sel == 2) {
                        set_state(app, AppStateSettings);
                    } else {
                        set_state(app, AppStateAbout);
                    }
                }
                break;
            case AppStateSettings:
                app->set_msg[0] = 0;
                if(e.input.key == InputKeyBack) {
                    set_state(app, AppStateMenu);
                } else if(e.input.key == InputKeyUp) {
                    app->set_sel = (app->set_sel + 2) % 3;
                    view_port_update(app->vp);
                } else if(e.input.key == InputKeyDown) {
                    app->set_sel = (app->set_sel + 1) % 3;
                    view_port_update(app->vp);
                } else if(app->set_sel == 2) {
                    if(e.input.key == InputKeyOk) set_state(app, AppStateConfirmClearAll);
                    else view_port_update(app->vp);
                } else if(
                    e.input.key == InputKeyLeft || e.input.key == InputKeyRight ||
                    e.input.key == InputKeyOk) {
                    if(app->set_sel == 0) {
                        app->search_user_db = !app->search_user_db;
                    } else if(e.input.key == InputKeyLeft) {
                        app->log_mode = (LogMode)((app->log_mode + 2) % 3);
                    } else {
                        app->log_mode = (LogMode)((app->log_mode + 1) % 3);
                    }
                    view_port_update(app->vp);
                }
                break;
            case AppStateLogList:
                if(e.input.key == InputKeyBack) {
                    set_state(app, AppStateMenu);
                } else if(app->log_count > 0) {
                    if(e.input.key == InputKeyUp || e.input.key == InputKeyDown) {
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        if(e.input.key == InputKeyDown) {
                            app->log_sel = (app->log_sel + 1) % app->log_count;
                        } else {
                            app->log_sel = (app->log_sel + app->log_count - 1) % app->log_count;
                        }
                        if(app->log_sel < app->log_top) app->log_top = app->log_sel;
                        if(app->log_sel >= app->log_top + LOG_LIST_ROWS) {
                            app->log_top = app->log_sel + 1 - LOG_LIST_ROWS;
                        }
                        furi_mutex_release(app->mutex);
                        view_port_update(app->vp);
                    } else if(e.input.key == InputKeyOk) {
                        log_open_view(app); /* draw_cb does not touch the text in this state */
                        set_state(app, AppStateLogView);
                    } else if(e.input.key == InputKeyLeft) {
                        set_state(app, AppStateConfirmDelLog);
                    }
                }
                break;
            case AppStateConfirmDelLog:
                if(e.input.key == InputKeyOk) {
                    log_delete_selected(app);
                    set_state(app, AppStateLogList);
                } else if(e.input.key == InputKeyBack) {
                    set_state(app, AppStateLogList);
                }
                break;
            case AppStateConfirmClearAll:
                if(e.input.key == InputKeyOk) {
                    size_t n = log_delete_all(app);
                    snprintf(app->set_msg, sizeof(app->set_msg), "%u log(s) deleted", (unsigned)n);
                    set_state(app, AppStateSettings);
                } else if(e.input.key == InputKeyBack) {
                    set_state(app, AppStateSettings);
                }
                break;
            case AppStateLogView: {
                size_t maxs =
                    app->log_nlines > LOG_VIEW_ROWS ? app->log_nlines - LOG_VIEW_ROWS : 0;
                if(e.input.key == InputKeyBack) {
                    set_state(app, AppStateLogList); /* leave the view state before freeing */
                    log_close_view(app);
                } else {
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    if(e.input.key == InputKeyUp) {
                        if(app->log_scroll > 0) app->log_scroll--;
                    } else if(e.input.key == InputKeyDown) {
                        if(app->log_scroll < maxs) app->log_scroll++;
                    } else if(e.input.key == InputKeyLeft) {
                        app->log_scroll =
                            app->log_scroll > LOG_VIEW_ROWS ? app->log_scroll - LOG_VIEW_ROWS : 0;
                    } else if(e.input.key == InputKeyRight || e.input.key == InputKeyOk) {
                        app->log_scroll += LOG_VIEW_ROWS;
                        if(app->log_scroll > maxs) app->log_scroll = maxs;
                    }
                    furi_mutex_release(app->mutex);
                    view_port_update(app->vp);
                }
                break;
            }
            case AppStateAskLog:
                if(e.input.key == InputKeyOk) {
                    set_notice(app, save_log(app) ? "Log saved" : "Log error");
                    restart_rx(app);
                } else if(e.input.key == InputKeyBack) {
                    set_notice(app, "");
                    restart_rx(app);
                }
                break;
            case AppStateAbout:
                if(e.input.key == InputKeyBack || e.input.key == InputKeyOk) {
                    set_state(app, AppStateMenu);
                }
                break;
            case AppStateWaiting:
                if(e.input.key == InputKeyBack) {
                    stop_rx(app);
                    app->pending = false;
                    set_notice(app, "");
                    set_state(app, AppStateMenu);
                }
                break;
            case AppStateResult:
                if(e.input.key == InputKeyBack && app->can_resume) {
                    /* interrupted scan: Back restarts it from where it stopped */
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->state = AppStateAnalyzing;
                    strlcpy(app->status, "Resuming...", sizeof(app->status));
                    furi_mutex_release(app->mutex);
                    view_port_update(app->vp);

                    identify(app, true);

                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->state = AppStateResult;
                    furi_mutex_release(app->mutex);
                    view_port_update(app->vp);
                } else if(e.input.key == InputKeyOk || e.input.key == InputKeyBack) {
                    leave_result(app); /* saves / asks for the log, then new scan */
                } else if(app->nord > 1) {
                    bool next = (e.input.key == InputKeyRight || e.input.key == InputKeyDown);
                    bool prev = (e.input.key == InputKeyLeft || e.input.key == InputKeyUp);
                    if(next || prev) {
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        app->sel = next ? (app->sel + 1) % app->nord :
                                          (app->sel + app->nord - 1) % app->nord;
                        furi_mutex_release(app->mutex);
                        view_port_update(app->vp);
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    if(app->rx_running) infrared_worker_rx_stop(app->worker);
    infrared_worker_free(app->worker);
    log_close_view(app);

    gui_remove_view_port(app->gui, app->vp);
    view_port_free(app->vp);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app->rx.raw);
    free(app->db_raw);
    free(app);
    return 0;
}
