#include "companion_catalog.h"
const GtaCollection gta_collection_catalog[GtaCollectionCount] = {
    [GtaCollectionRandy] = {"Randy's Wish List", "Delivered", NULL, 22},
    [GtaCollectionMoses] = {"Moses Drop-Offs", "Delivered", NULL, 7},
    [GtaCollectionWyman] = {"Wyman's Classic Cars", "Acquired", NULL, 5},
    [GtaCollectionStandout] = {"Standout Vehicles", "Acquired", NULL, 299},
    [GtaCollectionGarages] = {"Garages", "Acquired", NULL, 16},
    [GtaCollectionHouses] = {"Houses / Cribs", "Unlocked", NULL, 5},
};

const char* const gta_vehicle_assets[GtaCollectionCount] = {
    [GtaCollectionRandy] = "randy.tsv",
    [GtaCollectionMoses] = "moses.tsv",
    [GtaCollectionWyman] = "wyman.tsv",
    [GtaCollectionStandout] = NULL, /* virtual multi-file catalog */
    [GtaCollectionGarages] = "garages.tsv",
    [GtaCollectionHouses] = "houses.tsv",
};

const GtaStandoutCategory gta_standout_categories[GTA_STANDOUT_CATEGORY_COUNT] = {
    {"Sports & Supercars", "standouts_sports.tsv"},
    {"Muscle & Classic Cars", "standouts_muscle.tsv"},
    {"Sedans & Coupes", "standouts_sedans.tsv"},
    {"SUVs & Pickups", "standouts_suvs.tsv"},
    {"Motorcycles", "standouts_motorcycles.tsv"},
    {"Vans & Commercial", "standouts_commercial.tsv"},
    {"Off-Road & Utility", "standouts_offroad.tsv"},
    {"Boats & Watercraft", "standouts_watercraft.tsv"},
    {"Aircraft & Special Vehicles", "standouts_special.tsv"},
};
