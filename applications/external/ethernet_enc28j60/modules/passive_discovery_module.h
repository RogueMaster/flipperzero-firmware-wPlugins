#pragma once

#include "../app_user.h"
#include "../libraries/protocol_tools/neighbor_db.h"

/**
 * @brief Start the passive discovery background scanning process.
 *
 * Allocates and spawns a background thread to orchestrate packet captures.
 *
 * @param app Pointer to the main App application context.
 */
void passive_discovery_module_start(App* app);

/**
 * @brief Stop the passive discovery scanning process and clean up the thread.
 *
 * @param app Pointer to the main App application context.
 */
void passive_discovery_module_stop(App* app);

/**
 * @brief Get the count of supported discovery protocols.
 */
size_t passive_discovery_module_get_protocol_count(void);

/**
 * @brief Get the user-friendly display name of a passive discovery protocol.
 */
const char* passive_discovery_module_get_protocol_name(passive_protocol_t protocol);

/**
 * @brief Get the page count for a neighbor's details view.
 */
uint8_t passive_discovery_module_get_details_page_count(
    passive_protocol_t protocol,
    neighbor_t* neighbor);

/**
 * @brief Construct the details text lines for a neighbor based on the active protocol.
 */
void passive_discovery_module_build_details_page(
    passive_protocol_t protocol,
    neighbor_t* neighbor,
    uint8_t page,
    char* line1,
    size_t line1_size,
    char* line2,
    size_t line2_size,
    char* line3,
    size_t line3_size,
    char* line4,
    size_t line4_size);

/**
 * @brief Get the count of active discovered neighbors in the database.
 */
size_t passive_discovery_module_get_neighbor_count(void);

/**
 * @brief Access a discovered neighbor in the database by index.
 */
neighbor_t* passive_discovery_module_get_neighbor(size_t index);
