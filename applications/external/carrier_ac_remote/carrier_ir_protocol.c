#include "carrier_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>

// ==========================================================================
// Carrier / Surrey 64-bit protocol (CARRIER_AC64).
// Ported from IRremoteESP8266 src/ir_Carrier.{h,cpp}, CarrierProtocol union.
//
// Full-state protocol, sent least significant bit first.
//
// Bit layout:
//   0-7    fixed 0x84          8-15   fixed 0x55
//   16-19  checksum            20-21  mode        22-23  fan
//   24-27  temperature - 16    29     vertical swing
//   36     power               37     off-timer enable
//   38     on-timer enable     39     sleep
//   52-55  on-timer hours      60-63  off-timer hours
//
// Bits this app does not drive are inherited from the library's documented
// reset state rather than zeroed, so the frame keeps whatever the real
// handset puts there.
// ==========================================================================

// Line coding (microseconds)
#define HDR_MARK   8940
#define HDR_SPACE  4556
#define BIT_MARK   503
#define ONE_SPACE  1736
#define ZERO_SPACE 615

#define CARRIER_BITS 64

/// IRCarrierAc64::stateReset()
#define CARRIER_RESET_STATE 0x109000002C2A5584ULL

// Mode field (bits 20-21)
#define C_HEAT 0b01
#define C_COOL 0b10
#define C_FAN  0b11

// Fan field (bits 22-23)
#define C_FAN_AUTO 0b00
#define C_FAN_LOW  0b01
#define C_FAN_MED  0b10
#define C_FAN_HIGH 0b11

#define TIMER_MIN 1
#define TIMER_MAX 9

static const uint8_t MODE_CODES[CarrierModeCount] = {
    C_COOL, // Off - unused, the power bit carries it
    C_COOL,
    C_HEAT,
    C_FAN,
};

static const uint8_t FAN_CODES[CarrierFanCount] = {
    C_FAN_AUTO,
    C_FAN_LOW,
    C_FAN_MED,
    C_FAN_HIGH,
};

// Hours for each Extra preset; 0 means "cancel both timers"
static const uint8_t EXTRA_HOURS[CarrierExtraCount] = {0, 1, 2, 4, 8, 1, 2, 4, 8};

static const char* MODE_NAMES[CarrierModeCount] = {"Off", "Cool", "Heat", "Fan"};
static const char* FAN_NAMES[CarrierFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[CarrierToggleCount] = {"Power", "Swing", "Sleep"};
static const char* EXTRA_NAMES[CarrierExtraCount] =
    {"Timers off", "Off 1h", "Off 2h", "Off 4h", "Off 8h", "On 1h", "On 2h", "On 4h", "On 8h"};

static inline uint64_t put(uint64_t raw, uint8_t offset, uint8_t width, uint64_t value) {
    uint64_t mask = ((1ULL << width) - 1) << offset;
    return (raw & ~mask) | ((value << offset) & mask);
}

/// Sum every nibble above the checksum field, keep the low 4 bits.
static uint8_t carrier_checksum(uint64_t raw) {
    uint64_t data = raw >> 20;
    uint8_t result = 0;
    for(; data; data >>= 4) {
        result += data & 0xF;
    }
    return result & 0xF;
}

static uint64_t build_raw(const CarrierRequest* req, bool power, CarrierExtra timer) {
    CarrierMode mode = req->mode;
    if(mode == CarrierModeOff || mode >= CarrierModeCount) mode = CarrierModeCool;

    CarrierFan fan = req->fan >= CarrierFanCount ? CarrierFanAuto : req->fan;

    uint8_t temp = req->temp;
    if(temp < CARRIER_TEMP_MIN) temp = CARRIER_TEMP_MIN;
    if(temp > CARRIER_TEMP_MAX) temp = CARRIER_TEMP_MAX;

    uint64_t raw = CARRIER_RESET_STATE;
    raw = put(raw, 20, 2, MODE_CODES[mode]);
    raw = put(raw, 22, 2, FAN_CODES[fan]);
    raw = put(raw, 24, 4, (uint64_t)(temp - CARRIER_TEMP_MIN));
    raw = put(raw, 29, 1, (req->toggle_bits >> CarrierToggleSwing) & 1);
    raw = put(raw, 36, 1, power ? 1 : 0);
    raw = put(raw, 39, 1, (req->toggle_bits >> CarrierToggleSleep) & 1);

    // Timers are off unless an Extra preset asked for one
    raw = put(raw, 37, 1, 0);
    raw = put(raw, 38, 1, 0);
    if(timer < CarrierExtraCount && EXTRA_HOURS[timer]) {
        uint8_t hours = EXTRA_HOURS[timer];
        if(hours < TIMER_MIN) hours = TIMER_MIN;
        if(hours > TIMER_MAX) hours = TIMER_MAX;
        if(timer >= CarrierExtraOn1h) {
            raw = put(raw, 38, 1, 1);
            raw = put(raw, 52, 4, hours);
        } else {
            raw = put(raw, 37, 1, 1);
            raw = put(raw, 60, 4, hours);
        }
    }

    return put(raw, 16, 4, carrier_checksum(raw));
}

static bool encode_raw(uint64_t raw, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, CARRIER_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    for(uint8_t i = 0; i < CARRIER_BITS; i++) {
        ir_bit(&b, (raw >> i) & 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    }
    return ir_build_finish(&b, BIT_MARK, count);
}

bool carrier_ir_encode_state(const CarrierRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == CarrierModeOff || req->mode >= CarrierModeCount) return false;
    return encode_raw(build_raw(req, true, CarrierExtraCount), timings, timings_count);
}

bool carrier_ir_encode_toggle(
    const CarrierRequest* req,
    CarrierToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= CarrierToggleCount) return false;
    bool power = toggle != CarrierTogglePowerOff;
    return encode_raw(build_raw(req, power, CarrierExtraCount), timings, timings_count);
}

bool carrier_ir_encode_extra(
    const CarrierRequest* req,
    CarrierExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= CarrierExtraCount) return false;
    return encode_raw(build_raw(req, true, extra), timings, timings_count);
}

/// Bits 16-39 carry everything the user can see; the rest is fixed or timers.
static void format_raw(uint64_t raw, char* out, size_t len) {
    snprintf(out, len, "%06lX", (unsigned long)((raw >> 16) & 0xFFFFFF));
}

void carrier_ir_format_state(const CarrierRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == CarrierModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    format_raw(build_raw(req, true, CarrierExtraCount), out, len);
}

void carrier_ir_format_toggle(
    const CarrierRequest* req,
    CarrierToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= CarrierToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    format_raw(build_raw(req, toggle != CarrierTogglePowerOff, CarrierExtraCount), out, len);
}

void carrier_ir_format_extra(const CarrierRequest* req, CarrierExtra extra, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(extra >= CarrierExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    if(!EXTRA_HOURS[extra]) {
        snprintf(out, len, "cancel");
    } else {
        snprintf(
            out,
            len,
            "%s %uh",
            extra >= CarrierExtraOn1h ? "on" : "off",
            (unsigned)EXTRA_HOURS[extra]);
    }
}

bool carrier_ir_toggle_is_momentary(CarrierToggle toggle) {
    (void)toggle;
    return false;
}

bool carrier_ir_mode_locks_fan(CarrierMode mode) {
    // Carrier accepts any fan speed in any of its three modes
    (void)mode;
    return false;
}

bool carrier_ir_mode_has_no_temp(CarrierMode mode) {
    return mode == CarrierModeFan;
}

const char* carrier_ir_get_mode_name(CarrierMode mode) {
    return mode < CarrierModeCount ? MODE_NAMES[mode] : "?";
}

const char* carrier_ir_get_fan_name(CarrierFan fan) {
    return fan < CarrierFanCount ? FAN_NAMES[fan] : "?";
}

const char* carrier_ir_get_toggle_name(CarrierToggle toggle) {
    return toggle < CarrierToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* carrier_ir_get_extra_name(CarrierExtra extra) {
    return extra < CarrierExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t carrier_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* carrier_ir_get_option_label(void) {
    return "Model";
}

const char* carrier_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* carrier_ir_get_protocol_name(void) {
    return "Carrier";
}
