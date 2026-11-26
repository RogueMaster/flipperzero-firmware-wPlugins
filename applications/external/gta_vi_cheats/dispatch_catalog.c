#include "dispatch_catalog.h"

#define GTA_COUNT(items) (sizeof(items) / sizeof((items)[0]))

const char* const gta_dispatch_regions[GtaDispatchRegionCount] = {
    [GtaDispatchAll] = "All regions",
    [GtaDispatchViceCity] = "Vice City",
    [GtaDispatchKeys] = "Leonida Keys",
    [GtaDispatchGrassrivers] = "Grassrivers",
    [GtaDispatchPortGellhorn] = "Port Gellhorn",
    [GtaDispatchAmbrosia] = "Ambrosia",
    [GtaDispatchMountKalaga] = "Mount Kalaga",
    [GtaDispatchOther] = "Other areas",
};

enum {
    GtaDispatchSourceBookSwaps = GtaDispatchFirstAsset,
    GtaDispatchSourceCards01,
    GtaDispatchSourceCards02,
    GtaDispatchSourceCards03,
    GtaDispatchSourceCards04,
    GtaDispatchSourceCards05,
    GtaDispatchSourceCards06,
    GtaDispatchSourceCards07,
    GtaDispatchSourceCards08,
    GtaDispatchSourceCards09,
    GtaDispatchSourceCards10,
    GtaDispatchSourceCards11,
    GtaDispatchSourceCards12,
    GtaDispatchSourceCards13,
    GtaDispatchSourceCards14,
    GtaDispatchSourceCards15,
    GtaDispatchSourceCards16,
    GtaDispatchSourceCards17,
    GtaDispatchSourceCards18,
    GtaDispatchSourceCuriosities,
    GtaDispatchSourceFlamingos,
    GtaDispatchSourceKenton,
    GtaDispatchSourceLandmarks,
    GtaDispatchSourceMeditations,
    GtaDispatchSourceMurals,
    GtaDispatchSourceNoFlusher,
    GtaDispatchSourcePublications,
    GtaDispatchSourceSculptures,
    GtaDispatchSourceSniperPosters,
    GtaDispatchSourceStylesJason,
    GtaDispatchSourceStylesLucia,
    GtaDispatchSourceTattoos,
    GtaDispatchSourceUnderwater,
    GtaDispatchSourceVaultPoster,
    GtaDispatchSourceWatches,
    GtaDispatchSourceAholPieces,
    GtaDispatchSourceFlamingosInland,
    GtaDispatchSourceFlamingosNorth,
    GtaDispatchSourceCount,
};

const char* const gta_dispatch_assets[] = {
    "book_swaps.tsv",       "cards_01.tsv",        "cards_02.tsv",     "cards_03.tsv",
    "cards_04.tsv",         "cards_05.tsv",        "cards_06.tsv",     "cards_07.tsv",
    "cards_08.tsv",         "cards_09.tsv",        "cards_10.tsv",     "cards_11.tsv",
    "cards_12.tsv",         "cards_13.tsv",        "cards_14.tsv",     "cards_15.tsv",
    "cards_16.tsv",         "cards_17.tsv",        "cards_18.tsv",     "curiosities.tsv",
    "flamingos.tsv",        "kenton.tsv",          "landmarks.tsv",    "meditations.tsv",
    "murals.tsv",           "no_flusher.tsv",      "publications.tsv", "sculptures.tsv",
    "sniper_posters.tsv",   "styles_jason.tsv",    "styles_lucia.tsv", "tattoos.tsv",
    "underwater.tsv",       "vault_poster.tsv",    "watches.tsv",      "ahol_pieces.tsv",
    "flamingos_inland.tsv", "flamingos_north.tsv",
};

_Static_assert(GtaDispatchRegionCount <= 8, "Dispatch region mask must fit in one byte");
_Static_assert(GtaDispatchSourceCount < UINT8_MAX, "Dispatch source must fit in one byte");
_Static_assert(
    GTA_COUNT(gta_dispatch_assets) == GtaDispatchSourceCount - GtaDispatchFirstAsset,
    "Each collectible source must name its asset");

const size_t gta_dispatch_target_count = 968;
const size_t gta_dispatch_source_count = GtaDispatchSourceCount;
