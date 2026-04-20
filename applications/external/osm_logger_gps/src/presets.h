#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PRESETS_MAX_VARIANTS 8

typedef enum {
    PresetCatStreet = 0,     // mobilier urbain
    PresetCatRoads = 1,      // voirie, signalétique
    PresetCatParking = 2,    // stationnement
    PresetCatSports = 3,     // sports et loisirs
    PresetCatWaste = 4,      // déchets et recyclage
    PresetCatShops = 5,      // commerces
    PresetCatServices = 6,   // services (banque, poste, santé privée...)
    PresetCatEmergency = 7,  // urgence, secours
    PresetCatTourism = 8,    // tourisme, culture
    PresetCatNature = 9,     // nature
    PresetCatEducation = 10, // éducation
    PresetCatReligion = 11,  // religion, cérémonies
    PresetCatTransport = 12, // transports en commun
    PresetCatAddress = 13,   // adresses (utiliser note pour numéro)
    PresetCatOther = 14,     // divers / fallback
    PresetCatCount = 15,
} PresetCategory;

extern const char* const PRESET_CATEGORY_LABELS[PresetCatCount];
// Couleur hex (format "#RRGGBB") pour OsmAnd Favorites, par catégorie.
extern const char* const PRESET_CATEGORY_COLORS[PresetCatCount];

typedef struct {
    const char* label;
    const char* key;
    const char* variants[PRESETS_MAX_VARIANTS]; // [0] = valeur primaire
    uint8_t variant_count;                      // >= 1
    uint8_t category;                           // PresetCategory
} Preset;

// Charge les presets depuis /ext/apps_data/osm_logger/presets.txt si présent,
// sinon retombe sur la liste par défaut compilée en dur.
void presets_init(void);

// Libère toute mémoire allouée (no-op si defaults).
void presets_deinit(void);

uint8_t presets_count(void);
const Preset* presets_get(uint8_t idx); // NULL si idx hors bornes
bool presets_loaded_from_file(void);

// Compte les presets dans la catégorie donnée.
uint8_t presets_count_in_category(uint8_t category);

// Retourne l'index global (dans la liste complète) du N-ième preset de la
// catégorie donnée. Retourne 0xFF si hors bornes.
uint8_t presets_get_index_in_category(uint8_t category, uint8_t nth);

// Retourne la valeur correspondant à la variante donnée, ou la primaire si
// variant_idx >= variant_count.
const char* preset_value(const Preset* p, uint8_t variant_idx);

// Construit la chaîne de tag effective pour (preset, variant_idx) :
//   - si la variante contient '=' : tag additionnel, résultat = "key=primary;variant"
//     (ex. "amenity=bench;material=wood")
//   - sinon : valeur alternative, résultat = "key=variant" (ex. "amenity=bench")
// Écrit dans `out`, retourne le nombre d'octets écrits (hors null), ou 0 si erreur.
size_t preset_build_tag(const Preset* p, uint8_t variant_idx, char* out, size_t out_size);

// Construit le "primary tag" (key=primary_value) — sert de clé stable pour le
// cache de notes. Ignore toujours la variante active. Ex. "amenity=bench".
size_t preset_build_primary_tag(const Preset* p, char* out, size_t out_size);
