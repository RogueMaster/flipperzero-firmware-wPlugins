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
    GtaProgressBytes = 512,
    GtaExploreFirstId = 400,
};

extern const GtaCollection gta_collection_catalog[GtaCollectionCount];
