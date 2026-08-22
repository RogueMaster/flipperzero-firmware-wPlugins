#include "delonghi_ir_protocol.h"
#include "ir_build.h"
#include <stdio.h>

// ==========================================================================
// De'Longhi PAC A95, 64-bit protocol.
// Ported from IRremoteESP8266 src/ir_Delonghi.{h,cpp} (DelonghiProtocol union).
//
// Full-state protocol: every press resends the whole frame, so the toggle
// bits ride along inside it.
//
// Bit layout, least significant bit first:
//   0-7    header, always 0x53
//   8-12   temperature (see temp_field)
//   13-14  fan
//   15     Fahrenheit flag (we always send Celsius)
//   16     power
//   17-19  mode
//   20     boost
//   21     sleep
//   24     on-timer enable      25-29 on-hours    32-37 on-minutes
//   40     off-timer enable     41-45 off-hours   48-53 off-minutes
//   56-63  checksum: sum of bytes 0..6
// ==========================================================================

// Line coding (microseconds)
#define HDR_MARK   8984
#define HDR_SPACE  4200
#define BIT_MARK   572
#define ONE_SPACE  1558
#define ZERO_SPACE 510

#define DELONGHI_BITS 64
#define HEADER_BYTE   0x53

// Mode field values
#define D_COOL 0x0
#define D_DRY  0x1
#define D_FAN  0x2
#define D_AUTO 0x4

// Fan field values
#define D_FAN_AUTO 0x0
#define D_FAN_HIGH 0x1
#define D_FAN_MED  0x2
#define D_FAN_LOW  0x3

// Temperature field stand-ins
#define D_TEMP_AUTO_DRY 0x00
#define D_TEMP_FAN      0x06

static const uint8_t MODE_CODES[DelonghiModeCount] = {
    D_COOL, // Off - unused, power bit carries it
    D_COOL,
    D_AUTO,
    D_DRY,
    D_FAN,
};

static const uint8_t FAN_CODES[DelonghiFanCount] = {
    D_FAN_AUTO,
    D_FAN_LOW,
    D_FAN_MED,
    D_FAN_HIGH,
};

// Hours stamped into the off-timer for each preset; index 0 cancels it.
static const uint8_t OFF_TIMER_HOURS[DelonghiExtraCount] = {0, 1, 2, 4, 8};

static const char* MODE_NAMES[DelonghiModeCount] = {"Off", "Cool", "Auto", "Dry", "Fan"};
static const char* FAN_NAMES[DelonghiFanCount] = {"Auto", "Low", "Med", "High"};
static const char* TOGGLE_NAMES[DelonghiToggleCount] = {"Power", "Boost", "Sleep"};
static const char* EXTRA_NAMES[DelonghiExtraCount] =
    {"Timer off", "Off 1h", "Off 2h", "Off 4h", "Off 8h"};

/// Sum of bytes 0..6, stored in byte 7.
static uint8_t delonghi_checksum(uint64_t raw) {
    uint8_t sum = 0;
    for(uint8_t off = 0; off < 56; off += 8) {
        sum += (uint8_t)((raw >> off) & 0xFF);
    }
    return sum;
}

/// The temperature field doubles as a mode marker in Auto, Dry and Fan.
static uint8_t temp_field(DelonghiMode mode, uint8_t temp) {
    if(mode == DelonghiModeAuto || mode == DelonghiModeDry) return D_TEMP_AUTO_DRY;
    if(mode == DelonghiModeFan) return D_TEMP_FAN;

    if(temp < DELONGHI_TEMP_MIN) temp = DELONGHI_TEMP_MIN;
    if(temp > DELONGHI_TEMP_MAX) temp = DELONGHI_TEMP_MAX;
    return (uint8_t)(temp - DELONGHI_TEMP_MIN + 1);
}

/// The unit rejects some fan speeds depending on mode.
static uint8_t fan_field(DelonghiMode mode, DelonghiFan fan) {
    if(mode == DelonghiModeAuto || mode == DelonghiModeDry) return D_FAN_AUTO;
    if(fan >= DelonghiFanCount) fan = DelonghiFanAuto;
    // Fan-only has no auto speed; the handset falls back to high.
    if(mode == DelonghiModeFan && fan == DelonghiFanAuto) return D_FAN_HIGH;
    return FAN_CODES[fan];
}

static uint64_t build_raw(const DelonghiRequest* req, bool power, uint8_t off_timer_hours) {
    DelonghiMode mode = req->mode;
    if(mode == DelonghiModeOff || mode >= DelonghiModeCount) mode = DelonghiModeCool;

    uint64_t raw = HEADER_BYTE;
    raw |= (uint64_t)(temp_field(mode, req->temp) & 0x1F) << 8;
    raw |= (uint64_t)(fan_field(mode, req->fan) & 0x03) << 13;
    // bit 15 Fahrenheit stays 0 - the app works in Celsius
    raw |= (uint64_t)(power ? 1 : 0) << 16;
    raw |= (uint64_t)(MODE_CODES[mode] & 0x07) << 17;

    if((req->toggle_bits >> DelonghiToggleBoost) & 1) raw |= (uint64_t)1 << 20;
    if((req->toggle_bits >> DelonghiToggleSleep) & 1) raw |= (uint64_t)1 << 21;

    if(off_timer_hours) {
        raw |= (uint64_t)1 << 40; // off-timer enable
        raw |= (uint64_t)(off_timer_hours & 0x1F) << 41;
        // minutes stay 0
    }

    raw |= (uint64_t)delonghi_checksum(raw) << 56;
    return raw;
}

static bool encode_raw(uint64_t raw, uint32_t* timings, size_t* count) {
    IrBuild b = ir_build_init(timings, DELONGHI_IR_MAX_TIMINGS);
    ir_item(&b, HDR_MARK, HDR_SPACE);
    // De'Longhi is sent least significant bit first
    for(uint8_t i = 0; i < DELONGHI_BITS; i++) {
        ir_bit(&b, (raw >> i) & 1, BIT_MARK, ONE_SPACE, ZERO_SPACE);
    }
    return ir_build_finish(&b, BIT_MARK, count);
}

bool delonghi_ir_encode_state(const DelonghiRequest* req, uint32_t* timings, size_t* timings_count) {
    if(!req || !timings || !timings_count) return false;
    if(req->mode == DelonghiModeOff || req->mode >= DelonghiModeCount) return false;
    return encode_raw(build_raw(req, true, 0), timings, timings_count);
}

bool delonghi_ir_encode_toggle(
    const DelonghiRequest* req,
    DelonghiToggle toggle,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || toggle >= DelonghiToggleCount) return false;
    bool power = toggle != DelonghiTogglePowerOff;
    return encode_raw(build_raw(req, power, 0), timings, timings_count);
}

bool delonghi_ir_encode_extra(
    const DelonghiRequest* req,
    DelonghiExtra extra,
    uint32_t* timings,
    size_t* timings_count) {
    if(!req || !timings || !timings_count || extra >= DelonghiExtraCount) return false;
    return encode_raw(build_raw(req, true, OFF_TIMER_HOURS[extra]), timings, timings_count);
}

void delonghi_ir_format_state(const DelonghiRequest* req, char* out, size_t len) {
    if(!req || !out || !len) return;
    if(req->mode == DelonghiModeOff) {
        snprintf(out, len, "power off");
        return;
    }
    uint64_t raw = build_raw(req, true, 0);
    // Only the low 3 bytes carry mode, fan and temperature
    snprintf(out, len, "%06lX", (unsigned long)(raw & 0xFFFFFF));
}

void delonghi_ir_format_toggle(
    const DelonghiRequest* req,
    DelonghiToggle toggle,
    char* out,
    size_t len) {
    if(!req || !out || !len) return;
    if(toggle >= DelonghiToggleCount) {
        snprintf(out, len, "-");
        return;
    }
    // Full-state protocol: a button press resends the whole frame
    uint64_t raw = build_raw(req, toggle != DelonghiTogglePowerOff, 0);
    snprintf(out, len, "%06lX", (unsigned long)(raw & 0xFFFFFF));
}

void delonghi_ir_format_extra(
    const DelonghiRequest* req,
    DelonghiExtra extra,
    char* out,
    size_t len) {
    (void)req;
    if(!out || !len) return;
    if(extra >= DelonghiExtraCount) {
        snprintf(out, len, "-");
        return;
    }
    if(!OFF_TIMER_HOURS[extra]) {
        snprintf(out, len, "cancel");
    } else {
        snprintf(out, len, "off in %uh", (unsigned)OFF_TIMER_HOURS[extra]);
    }
}

bool delonghi_ir_toggle_is_momentary(DelonghiToggle toggle) {
    (void)toggle;
    return false;
}

bool delonghi_ir_mode_locks_fan(DelonghiMode mode) {
    return mode == DelonghiModeAuto || mode == DelonghiModeDry;
}

bool delonghi_ir_mode_has_no_temp(DelonghiMode mode) {
    return mode == DelonghiModeFan || mode == DelonghiModeAuto || mode == DelonghiModeDry;
}

const char* delonghi_ir_get_mode_name(DelonghiMode mode) {
    return mode < DelonghiModeCount ? MODE_NAMES[mode] : "?";
}

const char* delonghi_ir_get_fan_name(DelonghiFan fan) {
    return fan < DelonghiFanCount ? FAN_NAMES[fan] : "?";
}

const char* delonghi_ir_get_toggle_name(DelonghiToggle toggle) {
    return toggle < DelonghiToggleCount ? TOGGLE_NAMES[toggle] : "?";
}

const char* delonghi_ir_get_extra_name(DelonghiExtra extra) {
    return extra < DelonghiExtraCount ? EXTRA_NAMES[extra] : "?";
}

uint8_t delonghi_ir_get_option_count(void) {
    return 0; // one variant only
}

const char* delonghi_ir_get_option_label(void) {
    return "Model";
}

const char* delonghi_ir_get_option_name(uint8_t option) {
    (void)option;
    return "-";
}

const char* delonghi_ir_get_protocol_name(void) {
    return "Delonghi";
}
