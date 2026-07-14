#ifndef CAIXIANLIN_PROTOCOL_H
#define CAIXIANLIN_PROTOCOL_H

#include "caixianlin_types.h"

// Build complete signal buffer for transmission
void caixianlin_protocol_encode_message(CaixianlinRemoteApp* app);

// Process RX buffer and attempt to decode one message
// Returns true if a valid message was decoded
bool caixianlin_protocol_process_rx(CaixianlinRemoteApp* app);

// Calculate packet checksum
uint8_t caixianlin_protocol_checksum(
    uint16_t station_id,
    uint8_t channel,
    uint8_t mode,
    uint8_t strength);

// Station ID digit utilities
uint8_t caixianlin_protocol_get_station_digit(uint16_t station_id, int pos);
uint16_t caixianlin_protocol_set_station_digit(uint16_t station_id, int pos, uint8_t digit);

#endif // CAIXIANLIN_PROTOCOL_H
