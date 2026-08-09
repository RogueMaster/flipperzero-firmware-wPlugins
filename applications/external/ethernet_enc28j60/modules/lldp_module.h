#pragma once

#include "../app_user.h"
#include "../libraries/protocol_tools/lldp.h"
#include "../libraries/protocol_tools/neighbor_db.h"
#include "../libraries/scanner/scanner_session.h"
#include "passive_discovery_handler.h"

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initializes the LLDP discovery module.
 *
 * Clears the neighbor database before starting a new passive scan.
 */
void lldp_module_init(void);

/**
 * @brief Clears all discovered LLDP neighbors.
 */
void lldp_module_reset(void);

/**
 * @brief Processes a received Ethernet frame.
 *
 * If the frame contains a valid LLDPDU, it is parsed and the neighbor
 * database is updated.
 *
 * @param frame Pointer to the received Ethernet frame.
 * @param length Frame length in bytes.
 *
 * @return true if the frame contained valid LLDP information.
 * @return false otherwise.
 */
bool lldp_module_process_frame(uint8_t* frame, uint16_t length);

/**
 * @brief Returns the number of discovered neighbors.
 *
 * @return Number of occupied entries.
 */
size_t lldp_module_count(void);

/**
 * @brief Returns a discovered neighbor.
 *
 * @param index Neighbor index.
 *
 * @return Pointer to the neighbor entry or NULL if index is invalid.
 */
neighbor_t* lldp_module_get(size_t index);

bool lldp_packet_predicate(const uint8_t* frame, uint16_t len, void* ctx);

bool lldp_module_run(scanner_session_t* session, uint32_t timeout_ms);

extern const PassiveProtocolHandler lldp_protocol_handler;
