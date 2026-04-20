#include "presets.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define PRESETS_FILE "/ext/apps_data/osm_logger/presets.txt"
#define PRESETS_MAX_FILE_SIZE 4096
#define PRESETS_MAX_ENTRIES 128

const char* const PRESET_CATEGORY_LABELS[PresetCatCount] = {
    "Street furniture",
    "Roads & signs",
    "Parking",
    "Sports & leisure",
    "Waste",
    "Shops",
    "Services",
    "Emergency",
    "Tourism",
    "Nature",
    "Education",
    "Religion",
    "Transport",
    "Address",
    "Other",
};

// Palette visuelle distincte par catégorie (standard OsmAnd Favorites).
// Affiche dans OsmAnd une pastille colorée pour chaque point → navigation visuelle
// rapide (on voit tout de suite "rouge = urgence", "vert = nature", etc.).
const char* const PRESET_CATEGORY_COLORS[PresetCatCount] = {
    "#10c0f0", // Street furniture — cyan
    "#ff8800", // Roads & signs — orange
    "#808080", // Parking — gray
    "#00c000", // Sports & leisure — vivid green
    "#8800ff", // Waste — purple
    "#ffcc00", // Shops — yellow
    "#0080ff", // Services — blue
    "#ff0000", // Emergency — red
    "#ff80c0", // Tourism — pink
    "#008000", // Nature — dark green
    "#6040a0", // Education — indigo
    "#a08040", // Religion — brown
    "#4040c0", // Transport — navy
    "#a020f0", // Address — purple (visible sur fond carte clair)
    "#c0c0c0", // Other — silver
};

// --- Defaults compilés en dur ---
static const Preset DEFAULT_PRESETS[] = {
    // =========================  Street furniture  ============================
    {"Bench", "amenity", {"bench", "material=wood", "material=metal", "material=concrete"}, 4, PresetCatStreet},
    {"Waste basket", "amenity", {"waste_basket"}, 1, PresetCatStreet},
    {"Drinking water", "amenity", {"drinking_water", "fountain", "bottle=yes"}, 3, PresetCatStreet},
    {"Toilets", "amenity", {"toilets", "fee=yes", "wheelchair=yes"}, 3, PresetCatStreet},
    {"Street lamp", "highway", {"street_lamp"}, 1, PresetCatStreet},
    {"Post box", "amenity", {"post_box"}, 1, PresetCatStreet},
    {"Info board", "tourism", {"information"}, 1, PresetCatStreet},
    {"Picnic table", "leisure", {"picnic_table"}, 1, PresetCatStreet},
    {"Telephone", "amenity", {"telephone"}, 1, PresetCatStreet},
    {"Shelter", "amenity", {"shelter"}, 1, PresetCatStreet},
    {"Vending machine", "amenity", {"vending_machine"}, 1, PresetCatStreet},
    {"Public bookcase", "amenity", {"public_bookcase"}, 1, PresetCatStreet},
    {"Clock", "amenity", {"clock"}, 1, PresetCatStreet},
    {"Bollard", "barrier", {"bollard"}, 1, PresetCatStreet},
    {"Water tap", "man_made", {"water_tap"}, 1, PresetCatStreet},

    // =========================  Roads & signs  ===============================
    {"Crossing", "highway", {"crossing"}, 1, PresetCatRoads},
    {"Traffic signals", "highway", {"traffic_signals"}, 1, PresetCatRoads},
    {"Stop sign", "highway", {"stop"}, 1, PresetCatRoads},
    {"Give way", "highway", {"give_way"}, 1, PresetCatRoads},
    {"Speed bump", "traffic_calming", {"bump", "hump", "table"}, 3, PresetCatRoads},
    {"Mile marker", "highway", {"milestone"}, 1, PresetCatRoads},

    // =========================  Parking  =====================================
    {"Bike parking", "amenity", {"bicycle_parking"}, 1, PresetCatParking},
    {"Car parking", "amenity", {"parking", "motorcycle_parking"}, 2, PresetCatParking},
    {"Charging station", "amenity", {"charging_station"}, 1, PresetCatParking},
    {"Disabled parking", "amenity", {"parking;wheelchair=designated"}, 1, PresetCatParking},

    // =========================  Sports & leisure  ============================
    {"Playground", "leisure", {"playground"}, 1, PresetCatSports},
    {"Basketball hoop", "amenity", {"basketball_hoop"}, 1, PresetCatSports},
    {"Football pitch", "leisure", {"pitch;sport=soccer"}, 1, PresetCatSports},
    {"Tennis court", "leisure", {"pitch;sport=tennis"}, 1, PresetCatSports},
    {"Ping pong", "leisure", {"pitch;sport=table_tennis"}, 1, PresetCatSports},
    {"Skate park", "leisure", {"pitch;sport=skateboard"}, 1, PresetCatSports},
    {"Fitness station", "leisure", {"fitness_station"}, 1, PresetCatSports},
    {"Swimming pool", "leisure", {"swimming_pool"}, 1, PresetCatSports},
    {"Climbing wall", "sport", {"climbing"}, 1, PresetCatSports},

    // =========================  Waste  =======================================
    {"Recycling", "amenity", {"recycling", "recycling:glass=yes", "recycling:paper=yes", "recycling:clothes=yes", "recycling:plastic=yes"}, 5, PresetCatWaste},
    {"Composter", "amenity", {"compost"}, 1, PresetCatWaste},

    // =========================  Shops  =======================================
    {"Bakery", "shop", {"bakery"}, 1, PresetCatShops},
    {"Supermarket", "shop", {"supermarket", "convenience"}, 2, PresetCatShops},
    {"Clothes", "shop", {"clothes", "shoes"}, 2, PresetCatShops},
    {"Hairdresser", "shop", {"hairdresser"}, 1, PresetCatShops},
    {"Beauty", "shop", {"beauty"}, 1, PresetCatShops},
    {"Bookshop", "shop", {"books"}, 1, PresetCatShops},
    {"Hardware", "shop", {"hardware", "doityourself"}, 2, PresetCatShops},
    {"Florist", "shop", {"florist"}, 1, PresetCatShops},
    {"Cafe", "amenity", {"cafe", "pub", "bar", "fast_food"}, 4, PresetCatShops},
    {"Restaurant", "amenity", {"restaurant"}, 1, PresetCatShops},
    {"Pharmacy", "amenity", {"pharmacy"}, 1, PresetCatShops},

    // =========================  Services  ====================================
    {"Bank", "amenity", {"bank"}, 1, PresetCatServices},
    {"ATM", "amenity", {"atm"}, 1, PresetCatServices},
    {"Post office", "amenity", {"post_office"}, 1, PresetCatServices},
    {"Doctor", "amenity", {"doctors"}, 1, PresetCatServices},
    {"Dentist", "amenity", {"dentist"}, 1, PresetCatServices},
    {"Veterinary", "amenity", {"veterinary"}, 1, PresetCatServices},
    {"Gas station", "amenity", {"fuel"}, 1, PresetCatServices},
    {"Car wash", "amenity", {"car_wash"}, 1, PresetCatServices},
    {"Laundry", "shop", {"laundry", "dry_cleaning"}, 2, PresetCatServices},

    // =========================  Emergency  ===================================
    {"Defibrillator", "emergency", {"defibrillator"}, 1, PresetCatEmergency},
    {"Fire hydrant", "emergency", {"fire_hydrant"}, 1, PresetCatEmergency},
    {"Fire station", "amenity", {"fire_station"}, 1, PresetCatEmergency},
    {"Police", "amenity", {"police"}, 1, PresetCatEmergency},
    {"Hospital", "amenity", {"hospital", "clinic"}, 2, PresetCatEmergency},
    {"Emergency phone", "emergency", {"phone"}, 1, PresetCatEmergency},

    // =========================  Tourism  =====================================
    {"Hotel", "tourism", {"hotel", "hostel", "guest_house"}, 3, PresetCatTourism},
    {"Museum", "tourism", {"museum"}, 1, PresetCatTourism},
    {"Viewpoint", "tourism", {"viewpoint"}, 1, PresetCatTourism},
    {"Artwork", "tourism", {"artwork"}, 1, PresetCatTourism},
    {"Monument", "historic", {"monument", "memorial", "statue"}, 3, PresetCatTourism},
    {"Camp site", "tourism", {"camp_site", "caravan_site"}, 2, PresetCatTourism},
    {"Attraction", "tourism", {"attraction"}, 1, PresetCatTourism},

    // =========================  Nature  ======================================
    {"Tree", "natural", {"tree"}, 1, PresetCatNature},
    {"Bush", "natural", {"shrub"}, 1, PresetCatNature},
    {"Peak", "natural", {"peak"}, 1, PresetCatNature},
    {"Spring", "natural", {"spring"}, 1, PresetCatNature},
    {"Park", "leisure", {"park", "garden"}, 2, PresetCatNature},

    // =========================  Education  ===================================
    {"School", "amenity", {"school"}, 1, PresetCatEducation},
    {"Kindergarten", "amenity", {"kindergarten"}, 1, PresetCatEducation},
    {"University", "amenity", {"university", "college"}, 2, PresetCatEducation},
    {"Library", "amenity", {"library"}, 1, PresetCatEducation},

    // =========================  Religion  ====================================
    {"Place of worship", "amenity", {"place_of_worship"}, 1, PresetCatReligion},
    {"Chapel", "building", {"chapel"}, 1, PresetCatReligion},
    {"Cemetery", "landuse", {"cemetery"}, 1, PresetCatReligion},
    {"Wayside cross", "historic", {"wayside_cross", "wayside_shrine"}, 2, PresetCatReligion},

    // =========================  Transport  ===================================
    {"Bus stop", "highway", {"bus_stop"}, 1, PresetCatTransport},
    {"Tram stop", "railway", {"tram_stop"}, 1, PresetCatTransport},
    {"Subway entrance", "railway", {"subway_entrance"}, 1, PresetCatTransport},
    {"Train station", "railway", {"station", "halt"}, 2, PresetCatTransport},
    {"Taxi stand", "amenity", {"taxi"}, 1, PresetCatTransport},
    {"Bike rental", "amenity", {"bicycle_rental"}, 1, PresetCatTransport},
    {"Ferry", "amenity", {"ferry_terminal"}, 1, PresetCatTransport},

    // =========================  Address  =====================================
    // Convention "note-as-value" (v0.15+) : valeur "TBD" = placeholder, la note
    // saisie via Up remplace automatiquement le TBD au save.
    // Ex. preset "House number" + note "42" → tag final addr:housenumber=42.
    {"House number", "addr:housenumber", {"TBD"}, 1, PresetCatAddress},
    {"Building entry", "entrance", {"main", "yes", "service", "emergency"}, 4, PresetCatAddress},
    {"Building", "building", {"yes", "residential", "commercial", "industrial"}, 4, PresetCatAddress},
};
static const uint8_t DEFAULT_PRESETS_COUNT =
    (uint8_t)(sizeof(DEFAULT_PRESETS) / sizeof(DEFAULT_PRESETS[0]));

static char* g_buffer = NULL;
static Preset* g_entries = NULL;
static uint8_t g_count = 0;
static const Preset* g_active = NULL;
static bool g_from_file = false;

// Split une chaîne de valeurs séparées par ',' en N pointeurs (modifie le buffer
// en remplaçant ',' par '\0'). Retourne le nombre de valeurs trouvées.
static uint8_t split_variants(char* value_str, const char* out[]) {
    uint8_t n = 0;
    char* p = value_str;
    if(!p || !*p) return 0;
    out[n++] = p;
    while(*p && n < PRESETS_MAX_VARIANTS) {
        if(*p == ',') {
            *p = '\0';
            p++;
            if(*p) out[n++] = p;
        } else {
            p++;
        }
    }
    return n;
}

// Parse le buffer (modifié en place, ';' et '\n' deviennent '\0').
static uint8_t parse_buffer(char* buf, size_t size, Preset* out, uint8_t max_entries) {
    uint8_t n = 0;
    size_t i = 0;

    while(i < size && n < max_entries) {
        while(i < size && (buf[i] == '\n' || buf[i] == '\r' || buf[i] == ' ' || buf[i] == '\t')) {
            i++;
        }
        if(i >= size) break;

        if(buf[i] == '#') {
            while(i < size && buf[i] != '\n') i++;
            continue;
        }

        char* label = &buf[i];
        while(i < size && buf[i] != ';' && buf[i] != '\n') i++;
        if(i >= size || buf[i] != ';') {
            while(i < size && buf[i] != '\n') i++;
            continue;
        }
        buf[i++] = '\0';

        char* key = &buf[i];
        while(i < size && buf[i] != ';' && buf[i] != '\n') i++;
        if(i >= size || buf[i] != ';') {
            while(i < size && buf[i] != '\n') i++;
            continue;
        }
        buf[i++] = '\0';

        char* values = &buf[i];
        while(i < size && buf[i] != '\n' && buf[i] != '\r' && buf[i] != ';') i++;

        // Catégorie optionnelle : quatrième champ après ';' (ex: "Bench;amenity;bench;0")
        uint8_t category = PresetCatOther;
        if(i < size && buf[i] == ';') {
            buf[i++] = '\0';
            char* cat_str = &buf[i];
            while(i < size && buf[i] != '\n' && buf[i] != '\r') i++;
            if(i < size) buf[i++] = '\0';
            // Parse catégorie numérique (0..7)
            if(*cat_str >= '0' && *cat_str <= '7') {
                category = (uint8_t)(*cat_str - '0');
            }
        } else if(i < size) {
            buf[i++] = '\0';
        }

        if(*label && *key && *values) {
            Preset p = {0};
            p.label = label;
            p.key = key;
            p.variant_count = split_variants(values, p.variants);
            p.category = category;
            if(p.variant_count > 0) {
                out[n++] = p;
            }
        }
    }

    return n;
}

static bool try_load_from_file(void) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    bool ok = false;

    if(f && storage_file_open(f, PRESETS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > 0 && sz <= PRESETS_MAX_FILE_SIZE) {
            char* buf = malloc((size_t)sz + 1);
            if(buf) {
                uint16_t read = storage_file_read(f, buf, (uint16_t)sz);
                buf[read] = '\0';

                Preset* entries = malloc(sizeof(Preset) * PRESETS_MAX_ENTRIES);
                if(entries) {
                    uint8_t n = parse_buffer(buf, read, entries, PRESETS_MAX_ENTRIES);
                    if(n > 0) {
                        g_buffer = buf;
                        g_entries = entries;
                        g_count = n;
                        g_active = entries;
                        g_from_file = true;
                        ok = true;
                        FURI_LOG_I("OSM", "Loaded %u presets from SD", (unsigned)n);
                    } else {
                        free(entries);
                    }
                }
                if(!ok) free(buf);
            }
        } else if(sz > PRESETS_MAX_FILE_SIZE) {
            FURI_LOG_W("OSM", "presets.txt too large (%llu bytes), using defaults", sz);
        }
        storage_file_close(f);
    }

    if(f) storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void presets_init(void) {
    if(try_load_from_file()) return;
    g_active = DEFAULT_PRESETS;
    g_count = DEFAULT_PRESETS_COUNT;
    g_from_file = false;
}

void presets_deinit(void) {
    if(g_from_file) {
        free(g_entries);
        free(g_buffer);
        g_entries = NULL;
        g_buffer = NULL;
    }
    g_active = NULL;
    g_count = 0;
    g_from_file = false;
}

uint8_t presets_count(void) {
    return g_count;
}

const Preset* presets_get(uint8_t idx) {
    if(idx >= g_count || !g_active) return NULL;
    return &g_active[idx];
}

bool presets_loaded_from_file(void) {
    return g_from_file;
}

const char* preset_value(const Preset* p, uint8_t variant_idx) {
    if(!p || p->variant_count == 0) return NULL;
    if(variant_idx >= p->variant_count) variant_idx = 0;
    return p->variants[variant_idx];
}

size_t preset_build_tag(const Preset* p, uint8_t variant_idx, char* out, size_t out_size) {
    if(!p || !out || out_size == 0) return 0;
    const char* v = preset_value(p, variant_idx);
    if(!v) {
        out[0] = '\0';
        return 0;
    }
    // Variante "k=v" -> tag additionnel au primaire
    if(strchr(v, '=')) {
        const char* primary = p->variants[0] ? p->variants[0] : "";
        int n = snprintf(out, out_size, "%s=%s;%s", p->key, primary, v);
        return n > 0 ? (size_t)n : 0;
    }
    // Variante valeur : key=value simple
    int n = snprintf(out, out_size, "%s=%s", p->key, v);
    return n > 0 ? (size_t)n : 0;
}

size_t preset_build_primary_tag(const Preset* p, char* out, size_t out_size) {
    if(!p || !out || out_size == 0) return 0;
    const char* primary = p->variants[0] ? p->variants[0] : "";
    int n = snprintf(out, out_size, "%s=%s", p->key, primary);
    return n > 0 ? (size_t)n : 0;
}

uint8_t presets_count_in_category(uint8_t category) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < g_count; i++) {
        if(g_active[i].category == category) n++;
    }
    return n;
}

uint8_t presets_get_index_in_category(uint8_t category, uint8_t nth) {
    uint8_t n = 0;
    for(uint8_t i = 0; i < g_count; i++) {
        if(g_active[i].category == category) {
            if(n == nth) return i;
            n++;
        }
    }
    return 0xFF;
}
