#ifndef NEIGHBOR_DB_H_
#define NEIGHBOR_DB_H_

#include <furi.h>
#include <furi_hal.h>

#define NEIGHBOR_DB_MAX_ENTRIES 32

/**
 * @brief Discovery protocols that identified a neighbor.
 *
 * Multiple values may be combined as a bitmask when a device
 * is discovered through more than one protocol.
 */

typedef enum {
    NEIGHBOR_SOURCE_LLDP = (1 << 0),
    NEIGHBOR_SOURCE_CDP = (1 << 1),
    NEIGHBOR_SOURCE_EAPOL = (1 << 2),
} neighbor_source_t;

/**
 * @brief Represents a discovered network neighbor.
 *
 * Stores the information collected from one or more passive
 * discovery protocols (LLDP, CDP or EAPOL).
 */

typedef struct {
    uint8_t mac[6];

    char chassis_id[64];

    char name[64];

    char description[128];

    char port[64];

    char management_address[48];

    uint16_t ttl;

    uint16_t capabilities;

    uint16_t enabled_capabilities;

    uint8_t discovery_sources;

    bool occupied;

} neighbor_t;

void neighbor_db_init(void);

void neighbor_db_clear(void);

/**
 * @brief Searches a neighbor by MAC address.
 *
 * @param mac Neighbor MAC address.
 *
 * @return Pointer to the neighbor if found.
 * @return NULL otherwise.
 */
neighbor_t* neighbor_db_find(const uint8_t mac[6]);

bool neighbor_db_add(const neighbor_t* neighbor);

bool neighbor_db_update(const neighbor_t* neighbor);

neighbor_t* neighbor_db_get(size_t index);

neighbor_t* neighbor_db_get_by_position(size_t position);

size_t neighbor_db_count(void);

void neighbor_db_load(void);

void neighbor_db_save(void);

size_t neighbor_db_count_by_source(uint8_t source);

neighbor_t* neighbor_db_get_by_source(uint8_t source, size_t position);

void neighbor_db_clear_by_source(uint8_t source);

#endif
