#include "protocols.h"

// --- Encode helpers (forward declarations) ---

static uint8_t checksum_sum8_u32(uint32_t data);
static uint8_t reverse_inverse_nibble(uint8_t b);

// --- Decode helpers ---

static bool timing_match(uint16_t actual, uint16_t expected) {
    int32_t diff = (int32_t)actual - (int32_t)expected;
    int32_t tol = (int32_t)expected * 3 / 10; // +/- 30%
    return diff >= -tol && diff <= tol;
}

static bool pulse_match(const OokPulse* p, const OokPulse* expected) {
    return timing_match(p->high_us, expected->high_us) &&
           timing_match(p->low_us, expected->low_us);
}

// Decode bits from pulses using timing-based OOK. Returns false if any bit doesn't match.
static bool decode_bits(
    const OokPulse* pulses,
    size_t num_bits,
    uint64_t* out,
    OokPulse one,
    OokPulse zero) {
    uint64_t data = 0;
    for(size_t i = 0; i < num_bits; i++) {
        data <<= 1;
        if(timing_match(pulses[i].high_us, one.high_us) &&
           timing_match(pulses[i].low_us, one.low_us)) {
            data |= 1;
        } else if(
            timing_match(pulses[i].high_us, zero.high_us) &&
            timing_match(pulses[i].low_us, zero.low_us)) {
            // bit 0, already 0
        } else {
            return false;
        }
    }
    *out = data;
    return true;
}

// --- Decoders ---

static bool decode_caixianlin(const OokPulse* pulses, size_t count, DecodedShocker* r) {
    if(count < 44) return false;
    static const OokPulse preamble = {1400, 750};
    static const OokPulse bit1 = {750, 250};
    static const OokPulse bit0 = {250, 750};
    if(!pulse_match(&pulses[0], &preamble)) return false;

    uint64_t data;
    if(!decode_bits(pulses + 1, 43, &data, bit1, bit0)) return false;

    // Remove 3 trailing zero bits
    data >>= 3;
    uint8_t checksum = data & 0xFF;
    uint32_t payload = (data >> 8) & 0xFFFFFFFF;
    if(checksum_sum8_u32(payload) != checksum) return false;

    r->model = ShockerModelCaiXianlin;
    r->shocker_id = (payload >> 16) & 0xFFFF;
    r->channel = (payload >> 12) & 0xF;
    r->intensity = payload & 0xFF;
    switch((payload >> 8) & 0xF) {
    case 0x01:
        r->command = ShockerCmdShock;
        break;
    case 0x02:
        r->command = ShockerCmdVibrate;
        break;
    case 0x03:
        r->command = ShockerCmdSound;
        break;
    case 0x04:
        r->command = ShockerCmdLight;
        break;
    default:
        return false;
    }
    return true;
}

static bool decode_petrainer(const OokPulse* pulses, size_t count, DecodedShocker* r) {
    if(count < 42) return false;
    static const OokPulse preamble = {750, 750};
    static const OokPulse bit1 = {200, 1500};
    static const OokPulse bit0 = {200, 750};
    if(!pulse_match(&pulses[0], &preamble)) return false;

    uint64_t data;
    if(!decode_bits(pulses + 1, 40, &data, bit1, bit0)) return false;

    uint8_t type_val = (data >> 32) & 0xFF;
    uint8_t type_sum = data & 0xFF;

    uint8_t n_shift;
    switch(type_val) {
    case 0x81:
        n_shift = 0;
        break;
    case 0x82:
        n_shift = 1;
        break;
    case 0x84:
        n_shift = 2;
        break;
    default:
        return false;
    }
    if(type_sum != ((~(0x01 | (0x80 >> n_shift))) & 0xFF)) return false;

    r->model = ShockerModelPetrainer;
    r->shocker_id = (data >> 16) & 0xFFFF;
    r->channel = 0;
    r->intensity = (data >> 8) & 0xFF;
    switch(n_shift) {
    case 0:
        r->command = ShockerCmdShock;
        break;
    case 1:
        r->command = ShockerCmdVibrate;
        break;
    case 2:
        r->command = ShockerCmdSound;
        break;
    default:
        return false;
    }
    return true;
}

static bool decode_petrainer998dr(const OokPulse* pulses, size_t count, DecodedShocker* r) {
    if(count < 42) return false;
    static const OokPulse preamble = {1500, 750};
    static const OokPulse bit1 = {750, 250};
    static const OokPulse bit0 = {250, 750};
    if(!pulse_match(&pulses[0], &preamble)) return false;

    uint64_t data;
    if(!decode_bits(pulses + 1, 40, &data, bit1, bit0)) return false;

    uint8_t channel = (data >> 36) & 0xF;
    uint8_t type_val = (data >> 32) & 0xF;
    uint8_t type_invert = (data >> 4) & 0xF;
    uint8_t channel_invert = data & 0xF;
    if(type_invert != reverse_inverse_nibble(type_val)) return false;
    if(channel_invert != reverse_inverse_nibble(channel)) return false;

    r->model = ShockerModelPetrainer998DR;
    r->shocker_id = (data >> 16) & 0xFFFF;
    r->channel = (channel == 0x08) ? 0 : 1;
    r->intensity = (data >> 8) & 0xFF;
    switch(type_val) {
    case 0x01:
        r->command = ShockerCmdShock;
        break;
    case 0x02:
        r->command = ShockerCmdVibrate;
        break;
    case 0x04:
        r->command = ShockerCmdSound;
        break;
    case 0x08:
        r->command = ShockerCmdLight;
        break;
    default:
        return false;
    }
    return true;
}

static bool decode_t330(const OokPulse* pulses, size_t count, DecodedShocker* r) {
    if(count < 43) return false;
    static const OokPulse preamble = {960, 790};
    static const OokPulse bit1 = {220, 980};
    static const OokPulse bit0 = {220, 580};
    if(!pulse_match(&pulses[0], &preamble)) return false;

    uint64_t data;
    if(!decode_bits(pulses + 1, 41, &data, bit1, bit0)) return false;

    data >>= 1; // trailing zero
    uint8_t ch1 = (data >> 36) & 0xF;
    uint8_t ch2 = data & 0xF;
    if(ch1 != ch2) return false;

    uint8_t type_val = (((data >> 32) & 0xF) << 4) | ((data >> 4) & 0xF);

    r->model = ShockerModelT330;
    r->shocker_id = (data >> 16) & 0xFFFF;
    r->channel = (ch1 == 0x00) ? 0 : 1;
    r->intensity = (data >> 8) & 0xFF;
    switch(type_val) {
    case 0x61:
        r->command = ShockerCmdShock;
        break;
    case 0x72:
        r->command = ShockerCmdVibrate;
        break;
    case 0x84:
        r->command = ShockerCmdSound;
        break;
    default:
        return false;
    }
    return true;
}

static bool decode_d80(const OokPulse* pulses, size_t count, DecodedShocker* r) {
    if(count < 42) return false;
    static const OokPulse preamble = {1900, 4000};
    static const OokPulse bit1 = {900, 300};
    static const OokPulse bit0 = {300, 900};
    if(!pulse_match(&pulses[0], &preamble)) return false;

    uint64_t data;
    if(!decode_bits(pulses + 1, 40, &data, bit1, bit0)) return false;

    if(((data >> 32) & 0xFF) != 0x04) return false;

    uint32_t payload = (data >> 8) & 0xFFFFFFFF;
    uint8_t checksum = data & 0xFF;
    if(checksum_sum8_u32(payload) != checksum) return false;

    r->model = ShockerModelD80;
    r->shocker_id = (payload >> 8) & 0xFFFF;
    r->channel = (payload >> 4) & 0x3;
    uint8_t raw_intensity = payload & 0xF;
    r->intensity = (raw_intensity * 100) / 15; // scale 0-15 back to 0-100
    switch((payload >> 6) & 0x3) {
    case 0x01:
        r->command = ShockerCmdShock;
        break;
    case 0x02:
        r->command = ShockerCmdVibrate;
        break;
    case 0x03:
        r->command = ShockerCmdSound;
        break;
    default:
        return false;
    }
    return true;
}

bool openshock_decode(const OokPulse* pulses, size_t count, DecodedShocker* result) {
    for(size_t i = 0; i < count; i++) {
        size_t remaining = count - i;
        if(remaining >= 44 && decode_caixianlin(pulses + i, remaining, result)) return true;
        if(remaining >= 42 && decode_petrainer(pulses + i, remaining, result)) return true;
        if(remaining >= 42 && decode_petrainer998dr(pulses + i, remaining, result)) return true;
        if(remaining >= 43 && decode_t330(pulses + i, remaining, result)) return true;
        if(remaining >= 42 && decode_d80(pulses + i, remaining, result)) return true;
    }
    return false;
}

// --- Encode helpers ---

static uint8_t checksum_sum8_u32(uint32_t data) {
    uint8_t sum = 0;
    for(int i = 0; i < 4; i++) {
        sum += (uint8_t)(data >> (i * 8));
    }
    return sum;
}

static uint8_t reverse_inverse_nibble(uint8_t b) {
    return (uint8_t)((0x084C2A6E195D3B7FULL >> (b * 4)) & 0xF);
}

static void
    encode_bits(OokPulse* out, uint64_t data, size_t num_bits, OokPulse one, OokPulse zero) {
    data <<= (64 - num_bits);
    uint64_t mask = (uint64_t)1 << 63;
    for(size_t i = 0; i < num_bits; i++) {
        out[i] = (data & mask) ? one : zero;
        data <<= 1;
    }
}

// --- CaiXianlin encoder ---
// Protocol: preamble + 43 data bits (32-bit payload + 8-bit sum8 checksum + 3 trailing zeros)
// Supports: Shock, Vibrate, Sound, Light

static size_t encode_caixianlin(
    uint16_t id,
    ShockerCommand cmd,
    uint8_t intensity,
    uint8_t channel,
    OokPulse* buf) {
    static const OokPulse preamble = {1400, 750};
    static const OokPulse bit1 = {750, 250};
    static const OokPulse bit0 = {250, 750};

    if(intensity > 99) intensity = 99;

    uint8_t type_val;
    switch(cmd) {
    case ShockerCmdShock:
        type_val = 0x01;
        break;
    case ShockerCmdVibrate:
        type_val = 0x02;
        break;
    case ShockerCmdSound:
        type_val = 0x03;
        intensity = 0;
        break;
    case ShockerCmdLight:
        type_val = 0x04;
        break;
    default:
        return 0;
    }

    // [shockerId:16][channelId:4][type:4][intensity:8]
    uint32_t payload = ((uint32_t)id << 16) | ((uint32_t)(channel & 0xF) << 12) |
                       ((uint32_t)(type_val & 0xF) << 8) | (uint32_t)intensity;

    uint8_t checksum = checksum_sum8_u32(payload);
    uint64_t data = ((uint64_t)payload << 8) | (uint64_t)checksum;
    data <<= 3; // 3 trailing zero bits

    buf[0] = preamble;
    encode_bits(buf + 1, data, 43, bit1, bit0);
    return 44;
}

// --- Petrainer encoder ---
// Protocol: preamble + 40 data bits + postamble
// Supports: Shock, Vibrate, Sound

static size_t encode_petrainer(uint16_t id, ShockerCommand cmd, uint8_t intensity, OokPulse* buf) {
    static const OokPulse preamble = {750, 750};
    static const OokPulse bit1 = {200, 1500};
    static const OokPulse bit0 = {200, 750};
    static const OokPulse postamble = {200, 7000};

    if(intensity > 100) intensity = 100;

    uint8_t n_shift;
    switch(cmd) {
    case ShockerCmdShock:
        n_shift = 0;
        break;
    case ShockerCmdVibrate:
        n_shift = 1;
        break;
    case ShockerCmdSound:
        n_shift = 2;
        break;
    default:
        return 0;
    }

    uint8_t type_val = (0x80 | (0x01 << n_shift)) & 0xFF;
    uint8_t type_sum = (~(0x01 | (0x80 >> n_shift))) & 0xFF;

    // [methodBit:8][shockerId:16][intensity:8][methodChecksum:8]
    uint64_t data = ((uint64_t)type_val << 32) | ((uint64_t)id << 16) |
                    ((uint64_t)intensity << 8) | (uint64_t)type_sum;

    buf[0] = preamble;
    encode_bits(buf + 1, data, 40, bit1, bit0);
    buf[41] = postamble;
    return 42;
}

// --- Petrainer 998DR encoder ---
// Protocol: preamble + 40 data bits + postamble
// Supports: Shock, Vibrate, Sound, Light

static size_t encode_petrainer998dr(
    uint16_t id,
    ShockerCommand cmd,
    uint8_t intensity,
    uint8_t channel,
    OokPulse* buf) {
    static const OokPulse preamble = {1500, 750};
    static const OokPulse bit1 = {750, 250};
    static const OokPulse bit0 = {250, 750};
    static const OokPulse postamble = {250, 3750};

    if(intensity > 100) intensity = 100;

    int type_shift;
    switch(cmd) {
    case ShockerCmdShock:
        type_shift = 0;
        break;
    case ShockerCmdVibrate:
        type_shift = 1;
        break;
    case ShockerCmdSound:
        type_shift = 2;
        break;
    case ShockerCmdLight:
        type_shift = 3;
        break;
    default:
        return 0;
    }

    uint8_t type_val = 1 << type_shift;
    uint8_t type_invert = reverse_inverse_nibble(type_val);
    uint8_t ch = (channel == 0) ? 0x08 : 0x0F; // CH1=0b1000, CH2=0b1111
    uint8_t channel_invert = reverse_inverse_nibble(ch);

    // [channel:4][typeVal:4][shockerId:16][intensity:8][typeInvert:4][channelInvert:4]
    uint64_t data = ((uint64_t)(ch & 0xF) << 36) | ((uint64_t)(type_val & 0xF) << 32) |
                    ((uint64_t)(id & 0xFFFF) << 16) | ((uint64_t)(intensity & 0xFF) << 8) |
                    ((uint64_t)(type_invert & 0xF) << 4) | (uint64_t)(channel_invert & 0xF);

    buf[0] = preamble;
    encode_bits(buf + 1, data, 40, bit1, bit0);
    buf[41] = postamble;
    return 42;
}

// --- Wellturnt T330 encoder ---
// Protocol: preamble + 41 data bits + postamble
// Supports: Shock, Vibrate, Sound

static size_t
    encode_t330(uint16_t id, ShockerCommand cmd, uint8_t intensity, uint8_t channel, OokPulse* buf) {
    static const OokPulse preamble = {960, 790};
    static const OokPulse bit1 = {220, 980};
    static const OokPulse bit0 = {220, 580};
    static const OokPulse postamble = {220, 135};

    if(intensity > 100) intensity = 100;

    uint8_t type_val;
    switch(cmd) {
    case ShockerCmdShock:
        type_val = 0x61;
        break;
    case ShockerCmdVibrate:
        type_val = 0x72;
        break;
    case ShockerCmdSound:
        type_val = 0x84;
        intensity = 0;
        break;
    default:
        return 0;
    }

    uint8_t ch = (channel == 0) ? 0x00 : 0x0E; // CH1=0b0000, CH2=0b1110

    // [channelId:4][typeU:4][transmitterId:16][intensity:8][typeL:4][channelId:4]
    uint64_t data = ((uint64_t)(ch & 0xF) << 36) | ((uint64_t)((type_val >> 4) & 0xF) << 32) |
                    ((uint64_t)id << 16) | ((uint64_t)intensity << 8) |
                    ((uint64_t)(type_val & 0xF) << 4) | (uint64_t)(ch & 0xF);
    data <<= 1; // Trailing zero bit

    buf[0] = preamble;
    encode_bits(buf + 1, data, 41, bit1, bit0);
    buf[42] = postamble;
    return 43;
}

// --- D80 encoder ---
// Protocol: preamble + 40 data bits + postamble
// Supports: Shock, Vibrate, Sound
// Intensity is scaled from 0-99 to 0-15 (4-bit)

static size_t
    encode_d80(uint16_t id, ShockerCommand cmd, uint8_t intensity, uint8_t channel, OokPulse* buf) {
    static const OokPulse preamble = {1900, 4000};
    static const OokPulse bit1 = {900, 300};
    static const OokPulse bit0 = {300, 900};
    static const OokPulse postamble = {200, 2200};

    uint8_t type_val;
    switch(cmd) {
    case ShockerCmdShock:
        type_val = 0x01;
        break;
    case ShockerCmdVibrate:
        type_val = 0x02;
        break;
    case ShockerCmdSound:
        type_val = 0x03;
        intensity = 0;
        break;
    default:
        return 0;
    }

    // Scale intensity from 0-99 to 0-15
    if(intensity > 0) {
        int scaled = (intensity * 15) / 100;
        if(scaled < 1) scaled = 1;
        intensity = (uint8_t)scaled;
    }

    // Channel: 1=CH1, 2=CH2, 3=both
    if(channel < 1) channel = 1;
    if(channel > 3) channel = 3;

    // 0x04[shockerId:16][type:2][channel:2][intensity:4]
    uint32_t payload = 0x04000000 | ((uint32_t)id << 8) | ((uint32_t)(type_val & 0x3) << 6) |
                       ((uint32_t)(channel & 0x3) << 4) | (uint32_t)(intensity & 0xF);

    uint8_t checksum = checksum_sum8_u32(payload);
    uint64_t data = ((uint64_t)payload << 8) | (uint64_t)checksum;

    buf[0] = preamble;
    encode_bits(buf + 1, data, 40, bit1, bit0);
    buf[41] = postamble;
    return 42;
}

// --- Public API ---

const char* openshock_model_name(ShockerModel model) {
    switch(model) {
    case ShockerModelCaiXianlin:
        return "CaiXianlin";
    case ShockerModelPetrainer:
        return "Petrainer";
    case ShockerModelPetrainer998DR:
        return "Petrainer998DR";
    case ShockerModelT330:
        return "T330";
    case ShockerModelD80:
        return "D80";
    default:
        return "Unknown";
    }
}

const char* openshock_command_name(ShockerCommand command) {
    switch(command) {
    case ShockerCmdShock:
        return "Shock";
    case ShockerCmdVibrate:
        return "Vibrate";
    case ShockerCmdSound:
        return "Sound";
    case ShockerCmdLight:
        return "Light";
    default:
        return "Unknown";
    }
}

bool openshock_command_supported(ShockerModel model, ShockerCommand command) {
    if(command == ShockerCmdShock || command == ShockerCmdVibrate || command == ShockerCmdSound) {
        return true;
    }
    if(command == ShockerCmdLight) {
        return (model == ShockerModelCaiXianlin || model == ShockerModelPetrainer998DR);
    }
    return false;
}

size_t openshock_encode(
    ShockerModel model,
    uint16_t shocker_id,
    ShockerCommand command,
    uint8_t intensity,
    uint8_t channel,
    OokPulse* buffer,
    size_t buffer_size) {
    if(buffer == NULL || buffer_size < OPENSHOCK_MAX_PULSES) {
        return 0;
    }

    switch(model) {
    case ShockerModelCaiXianlin:
        return encode_caixianlin(shocker_id, command, intensity, channel, buffer);
    case ShockerModelPetrainer:
        return encode_petrainer(shocker_id, command, intensity, buffer);
    case ShockerModelPetrainer998DR:
        return encode_petrainer998dr(shocker_id, command, intensity, channel, buffer);
    case ShockerModelT330:
        return encode_t330(shocker_id, command, intensity, channel, buffer);
    case ShockerModelD80:
        return encode_d80(shocker_id, command, intensity, channel, buffer);
    default:
        return 0;
    }
}
