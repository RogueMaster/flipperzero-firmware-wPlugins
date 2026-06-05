#include "hvac_samsung.h"

// Library default template. Carries the fixed bytes and sane defaults
// (cool, 24C, fan auto, swing off, power off). Checksums are recomputed
// before every transmission.
static const uint8_t hvac_samsung_reset[HVAC_SAMSUNG_PACKET_SIZE] = {
    0x02,
    0x92,
    0x0F,
    0x00,
    0x00,
    0x00,
    0xF0,
    0x01,
    0x02,
    0xAE,
    0x71,
    0x00,
    0x15,
    0xF0};

static void hvac_samsung_set_field(uint8_t* byte, uint8_t mask, uint8_t shift, uint8_t value) {
    *byte = (*byte & ~mask) | ((value << shift) & mask);
}

static uint8_t hvac_samsung_popcount(uint8_t value) {
    uint8_t count = 0;
    while(value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

static uint8_t hvac_samsung_section_checksum(const uint8_t* section) {
    uint8_t sum = 0;
    sum += hvac_samsung_popcount(section[0]);
    sum += hvac_samsung_popcount(section[1] & 0x0F);
    sum += hvac_samsung_popcount((section[2] >> 4) & 0x0F);
    for(uint8_t i = 3; i < HVAC_SAMSUNG_SECTION_SIZE; i++) {
        sum += hvac_samsung_popcount(section[i]);
    }
    return sum ^ 0xFF;
}

static void hvac_samsung_checksum(HvacSamsungPacket packet) {
    uint8_t sum = hvac_samsung_section_checksum(packet);
    packet[1] = (packet[1] & 0x0F) | ((sum & 0x0F) << 4); // Sum1Lower
    packet[2] = (packet[2] & 0xF0) | ((sum >> 4) & 0x0F); // Sum1Upper

    sum = hvac_samsung_section_checksum(packet + HVAC_SAMSUNG_SECTION_SIZE);
    packet[8] = (packet[8] & 0x0F) | ((sum & 0x0F) << 4); // Sum2Lower
    packet[9] = (packet[9] & 0xF0) | ((sum >> 4) & 0x0F); // Sum2Upper
}

HvacSamsungPacket hvac_samsung_create_packet(void) {
    HvacSamsungPacket packet = malloc(sizeof(uint8_t) * HVAC_SAMSUNG_PACKET_SIZE);
    furi_assert(packet);
    memcpy(packet, hvac_samsung_reset, HVAC_SAMSUNG_PACKET_SIZE);
    return packet;
}

void hvac_samsung_free_packet(HvacSamsungPacket packet) {
    furi_assert(packet);
    free(packet);
}

void hvac_samsung_set_power(HvacSamsungPacket packet, bool on) {
    furi_assert(packet);
    uint8_t value = on ? 0b11 : 0b00;
    hvac_samsung_set_field(&packet[6], 0x30, 4, value); // Power1
    hvac_samsung_set_field(&packet[13], 0x30, 4, value); // Power2
}

void hvac_samsung_set_mode(HvacSamsungPacket packet, HvacSamsungMode mode) {
    furi_assert(packet);
    static const uint8_t raw[] = {
        [HvacSamsungModeCool] = 1,
        [HvacSamsungModeHeat] = 4,
        [HvacSamsungModeDry] = 2,
        [HvacSamsungModeFan] = 3,
        [HvacSamsungModeAuto] = 0,
    };
    hvac_samsung_set_field(&packet[12], 0x70, 4, raw[mode]); // Mode
}

void hvac_samsung_set_fan(HvacSamsungPacket packet, HvacSamsungFan fan) {
    furi_assert(packet);
    static const uint8_t raw[] = {
        [HvacSamsungFanAuto] = 0,
        [HvacSamsungFanLow] = 2,
        [HvacSamsungFanMed] = 4,
        [HvacSamsungFanHigh] = 5,
    };
    hvac_samsung_set_field(&packet[12], 0x0E, 1, raw[fan]); // Fan
}

void hvac_samsung_set_temperature(HvacSamsungPacket packet, HvacSamsungTemperature temperature) {
    furi_assert(packet);
    if(temperature < HVAC_SAMSUNG_TEMPERATURE_MIN) temperature = HVAC_SAMSUNG_TEMPERATURE_MIN;
    if(temperature > HVAC_SAMSUNG_TEMPERATURE_MAX) temperature = HVAC_SAMSUNG_TEMPERATURE_MAX;
    hvac_samsung_set_field(
        &packet[11], 0xF0, 4, temperature - HVAC_SAMSUNG_TEMPERATURE_MIN); // Temp
}

void hvac_samsung_set_swing(HvacSamsungPacket packet, bool on) {
    furi_assert(packet);
    hvac_samsung_set_field(&packet[9], 0x70, 4, on ? 0b010 : 0b111); // Swing
}

void hvac_samsung_send(HvacSamsungPacket packet) {
    furi_assert(packet);
    hvac_samsung_checksum(packet);

    uint32_t* timings = malloc(sizeof(uint32_t) * HVAC_SAMSUNG_TIMINGS_LEN);
    furi_assert(timings);
    furi_assert(HVAC_SAMSUNG_TIMINGS_LEN <= MAX_TIMINGS_AMOUNT);

    size_t idx = 0;
    timings[idx++] = HVAC_SAMSUNG_HDR_MARK;
    timings[idx++] = HVAC_SAMSUNG_HDR_SPACE;

    for(uint8_t offset = 0; offset < HVAC_SAMSUNG_PACKET_SIZE; offset += HVAC_SAMSUNG_SECTION_SIZE) {
        timings[idx++] = HVAC_SAMSUNG_SECTION_MARK;
        timings[idx++] = HVAC_SAMSUNG_SECTION_SPACE;

        for(uint8_t i = 0; i < HVAC_SAMSUNG_SECTION_SIZE; i++) {
            uint8_t byte = packet[offset + i];
            for(uint8_t mask = 1; mask > 0; mask <<= 1) {
                timings[idx++] = HVAC_SAMSUNG_BIT_MARK;
                timings[idx++] =
                    (byte & mask) ? HVAC_SAMSUNG_ONE_SPACE : HVAC_SAMSUNG_ZERO_SPACE;
            }
        }

        timings[idx++] = HVAC_SAMSUNG_BIT_MARK;
        timings[idx++] = HVAC_SAMSUNG_SECTION_GAP;
    }

    infrared_send_raw_ext(
        timings,
        HVAC_SAMSUNG_TIMINGS_LEN,
        true,
        HVAC_SAMSUNG_TRANSMIT_FREQUENCY,
        HVAC_SAMSUNG_TRANSMIT_DUTY_CYCLE);
    free(timings);
}
