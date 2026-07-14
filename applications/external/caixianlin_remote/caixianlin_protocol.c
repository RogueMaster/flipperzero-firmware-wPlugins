#include "caixianlin_protocol.h"

// Build the signal transmission buffer
void caixianlin_protocol_encode_message(CaixianlinRemoteApp* app) {
    TxState* tx = &app->tx_state;
    size_t idx = 0;

    uint8_t checksum =
        caixianlin_protocol_checksum(app->station_id, app->channel, app->mode, app->strength);

    uint8_t bits[PACKET_BITS];
    size_t bit_idx = 0;

    for(int i = 15; i >= 0; i--)
        bits[bit_idx++] = (app->station_id >> i) & 1;
    for(int i = 3; i >= 0; i--)
        bits[bit_idx++] = (app->channel >> i) & 1;
    for(int i = 3; i >= 0; i--)
        bits[bit_idx++] = (app->mode >> i) & 1;
    for(int i = 7; i >= 0; i--)
        bits[bit_idx++] = (app->strength >> i) & 1;
    for(int i = 7; i >= 0; i--)
        bits[bit_idx++] = (checksum >> i) & 1;
    bits[bit_idx++] = 0;
    bits[bit_idx++] = 0;

    // Encode message
    tx->buffer[idx++] = level_duration_make(true, SYNC_HIGH_US);
    tx->buffer[idx++] = level_duration_make(false, SYNC_LOW_US);

    for(size_t i = 0; i < PACKET_BITS; i++) {
        if(bits[i]) {
            tx->buffer[idx++] = level_duration_make(true, BIT_1_HIGH_US);
            tx->buffer[idx++] = level_duration_make(false, BIT_1_LOW_US);
        } else {
            tx->buffer[idx++] = level_duration_make(true, BIT_0_HIGH_US);
            tx->buffer[idx++] = level_duration_make(false, BIT_0_LOW_US);
        }
    }

    // Reset the TX state
    tx->buffer_len = idx;
    tx->buffer_index = 0;
}

// Decode message from work buffer
static bool caixianlin_protocol_decode_from_work_buffer(CaixianlinRemoteApp* app) {
    RxCapture* rx = &app->rx_capture;
    bool found = false;

    // Search for sync pulse in work buffer
    size_t i;
    for(i = 0; i < rx->work_buffer_len - 1; i++) {
        int32_t t1 = rx->work_buffer[i];
        int32_t t2 = rx->work_buffer[i + 1];

        // Check for sync: ~1000-1800µs HIGH, ~500-1200µs LOW
        if(t1 <= SYNC_HIGH_MIN_US || t1 >= SYNC_HIGH_MAX_US || t2 >= -SYNC_LOW_MIN_US ||
           t2 <= -SYNC_LOW_MAX_US) {
            // This isn't a sync pulse, keep looking
            continue;
        }

        // Found potential sync, check if we have enough samples for full message
        if(i + SIGNAL_LENGTH > rx->work_buffer_len) {
            // Not enough samples yet, try again after we receive a new batch
            break;
        }

        // Decode bits
        uint8_t bits[PACKET_BITS];
        size_t bit_count = 0;
        size_t pos = i + 2;

        while(bit_count < PACKET_BITS && pos < rx->work_buffer_len - 1) {
            int32_t high = rx->work_buffer[pos];
            int32_t low = rx->work_buffer[pos + 1];

            // Bit 1 if HIGH > |LOW|, Bit 0 if HIGH < |LOW|
            bits[bit_count++] = (high > -low) ? 1 : 0;
            pos += 2;
        }

        if(bit_count < 40) {
            // Incomplete message; this shouldn't really happen
            FURI_LOG_E(TAG, "Decoder yelded incomplete message! This shouldn't happen!");
            break;
        }

        // Parse packet
        uint16_t station_id = 0;
        for(int j = 0; j < 16; j++)
            station_id = (station_id << 1) | bits[j];

        uint8_t channel = 0;
        for(int j = 16; j < 20; j++)
            channel = (channel << 1) | bits[j];

        uint8_t mode = 0;
        for(int j = 20; j < 24; j++)
            mode = (mode << 1) | bits[j];

        uint8_t strength = 0;
        for(int j = 24; j < 32; j++)
            strength = (strength << 1) | bits[j];

        uint8_t checksum = 0;
        for(int j = 32; j < 40; j++)
            checksum = (checksum << 1) | bits[j];

        // Verify checksum
        uint8_t real_checksum = caixianlin_protocol_checksum(station_id, channel, mode, strength);

        if(checksum != real_checksum) {
            // Invalid message at this sync position, try next position
            continue;
        }
        // Valid message decoded!
        FURI_LOG_I(TAG, "Decoded: ID=%d CH=%d", station_id, channel);

        // Only report findings if the message is different from the last one we decoded
        found = found || !rx->capture_valid || station_id != rx->captured_station_id ||
                channel != rx->captured_channel;

        // Store captured values
        rx->captured_station_id = station_id;
        rx->captured_channel = channel;
        rx->capture_valid = true;

        // Skip over the decoded message
        i = pos - 1;
    }

    // Update processed samples counter
    rx->processed += i;

    // Remove processed samples from work buffer
    if(i > 0 && i <= rx->work_buffer_len) {
        rx->work_buffer_len -= i;
        if(rx->work_buffer_len > 0) {
            memmove(
                &rx->work_buffer[0], &rx->work_buffer[i], rx->work_buffer_len * sizeof(int32_t));
        }
    }

    return found;
}

// Process RX buffer and attempt to decode one message
bool caixianlin_protocol_process_rx(CaixianlinRemoteApp* app) {
    RxCapture* rx = &app->rx_capture;

    // Transfer samples from stream buffer to work buffer
    size_t space_available = WORK_BUFFER_SIZE - rx->work_buffer_len;
    if(space_available > 0) {
        size_t bytes = furi_stream_buffer_receive(
            rx->stream_buffer,
            &rx->work_buffer[rx->work_buffer_len],
            space_available * sizeof(int32_t),
            100);
        rx->work_buffer_len += bytes / sizeof(int32_t);
    }

    // Try to decode
    return caixianlin_protocol_decode_from_work_buffer(app);
}

// Calculate checksum
uint8_t caixianlin_protocol_checksum(
    uint16_t station_id,
    uint8_t channel,
    uint8_t mode,
    uint8_t strength) {
    uint8_t station_high = (station_id >> 8) & 0xFF;
    uint8_t station_low = station_id & 0xFF;
    return (station_high + station_low + channel + mode + strength) & 0xFF;
}

uint16_t digit_multipliers[] = {10000, 1000, 100, 10, 1};

// Get digit from Station ID
uint8_t caixianlin_protocol_get_station_digit(uint16_t station_id, int pos) {
    if(pos < 0 || pos >= 5) return 0;
    return (station_id / digit_multipliers[pos]) % 10;
}

// Set digit in Station ID
uint16_t caixianlin_protocol_set_station_digit(uint16_t station_id, int pos, uint8_t digit) {
    uint8_t old_digit = caixianlin_protocol_get_station_digit(station_id, pos);
    int32_t new_id = (int32_t)station_id - (old_digit * digit_multipliers[pos]) +
                     (digit * digit_multipliers[pos]);
    if(new_id > 65535) new_id = 65535;
    if(new_id < 0) new_id = 0;
    return (uint16_t)new_id;
}
