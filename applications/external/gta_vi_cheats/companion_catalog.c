#include "companion_catalog.h"
const GtaCollection gta_collection_catalog[GtaCollectionCount] = {
    [GtaCollectionRandy] = {"Randy's Wish List", "Delivered", NULL, 22},
    [GtaCollectionMoses] = {"Moses Drop-Offs", "Delivered", NULL, 7},
    [GtaCollectionWyman] = {"Wyman's Classic Cars", "Acquired", NULL, 5},
    [GtaCollectionStandout] = {"Standout Vehicles", "Acquired", NULL, 281},
    [GtaCollectionGarages] = {"Garages", "Acquired", NULL, 15},
    [GtaCollectionHouses] = {"Houses / Cribs", "Unlocked", NULL, 5},
};

const char* const gta_vehicle_assets[GtaCollectionCount] = {
    [GtaCollectionRandy] = "randy.tsv",
    [GtaCollectionMoses] = "moses.tsv",
    [GtaCollectionWyman] = "wyman.tsv",
    [GtaCollectionStandout] = "standouts.tsv",
    [GtaCollectionGarages] = "garages.tsv",
    [GtaCollectionHouses] = "houses.tsv",
};
