#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    GtaCollectionRandy,
    GtaCollectionMoses,
    GtaCollectionWyman,
    GtaCollectionStandout,
    GtaCollectionGarages,
    GtaCollectionHouses,
    GtaCollectionCount,
} GtaCollectionId;

typedef struct {
    uint16_t id;
    const char* name;
    const char* location;
    const char* notes;
} GtaCollectionEntry;

typedef struct {
    const char* title;
    const char* checked_label;
    const GtaCollectionEntry* entries;
    size_t count;
} GtaCollection;

enum {
    GtaProgressBytes = 1024,
    GtaExploreFirstId = 400,
};

extern const GtaCollection gta_collection_catalog[GtaCollectionCount];

/* Standout categories are independent catalog files; index zero is the virtual All view. */
typedef struct {
    const char* title;
    const char* asset;
} GtaStandoutCategory;
#define GTA_STANDOUT_CATEGORY_COUNT 9u
extern const GtaStandoutCategory gta_standout_categories[GTA_STANDOUT_CATEGORY_COUNT];
