/*
 * capability.cpp — tap capability checker (ESP32, #125).
 *
 * Counts the capability-relevant CAN ids per bus over a short listen window,
 * then renders per-feature verdicts from the pure logic in fsd_capability.h.
 * Pure RX / read-only — never transmits.
 */

#include "capability.h"
#include "config.h"
#include "../../fsd_logic/fsd_capability.h"  // fsd_capability_eval

// Capability-relevant ids, in a fixed slot order. Kept local so the counting
// table and the seen-set construction can't drift.
enum CapId : uint8_t {
    CAP_ID_EPAS = 0,   // 0x370 EPAS_STATUS
    CAP_ID_DAS_HW4,    // 0x39B DAS_STATUS_HW4
    CAP_ID_DAS_HW3,    // 0x399 DAS_STATUS_HW3 / ISA_SPEED-HW4
    CAP_ID_AP_CTRL,    // 0x3FD AP_CONTROL
    CAP_ID_AP_LEGACY,  // 0x3EE AP_LEGACY
    CAP_ID_STEER,      // 0x129 STEER_ANGLE
    CAP_ID_COUNT,
};

static const uint32_t kCapCanId[CAP_ID_COUNT] = {
    CAN_ID_EPAS_STATUS, CAN_ID_DAS_STATUS_HW4, CAN_ID_DAS_STATUS_HW3,
    CAN_ID_AP_CONTROL,  CAN_ID_AP_LEGACY,      CAN_ID_STEER_ANGLE,
};

enum CapRunState : uint8_t { CAP_STATE_IDLE = 0, CAP_STATE_RUNNING, CAP_STATE_DONE };

static FSDState*      g_state     = nullptr;
static portMUX_TYPE*  g_state_mux = nullptr;

static volatile uint8_t  g_cap_state = CAP_STATE_IDLE;
static uint32_t          g_cap_deadline_ms = 0;
static volatile uint16_t g_count[CAN_BUS_COUNT][CAP_ID_COUNT] = {};

void capability_init(FSDState* state, portMUX_TYPE* state_mux) {
    g_state     = state;
    g_state_mux = state_mux;
    g_cap_state = CAP_STATE_IDLE;
}

void capability_start(uint32_t now_ms) {
    for (uint8_t b = 0; b < CAN_BUS_COUNT; b++)
        for (uint8_t i = 0; i < CAP_ID_COUNT; i++) g_count[b][i] = 0;
    g_cap_deadline_ms = now_ms + CAP_WINDOW_MS;
    g_cap_state = CAP_STATE_RUNNING;
}

void capability_record(CanBusId bus, const CanFrame& frame, uint32_t now_ms) {
    if (g_cap_state != CAP_STATE_RUNNING) return;
    if (now_ms >= g_cap_deadline_ms) return;  // tick() will finalize
    if (bus >= CAN_BUS_COUNT) return;
    for (uint8_t i = 0; i < CAP_ID_COUNT; i++) {
        if (frame.id == kCapCanId[i]) {
            if (g_count[bus][i] < 0xFFFF) g_count[bus][i]++;
            break;
        }
    }
}

void capability_tick(uint32_t now_ms) {
    if (g_cap_state == CAP_STATE_RUNNING && now_ms >= g_cap_deadline_ms)
        g_cap_state = CAP_STATE_DONE;
}

static FSDCapSeen seen_for_bus(uint8_t bus) {
    FSDCapSeen s;
    s.epas       = g_count[bus][CAP_ID_EPAS]      >= CAP_MIN_FRAMES;
    s.das_hw4    = g_count[bus][CAP_ID_DAS_HW4]   >= CAP_MIN_FRAMES;
    s.das_hw3    = g_count[bus][CAP_ID_DAS_HW3]   >= CAP_MIN_FRAMES;
    s.ap_control = g_count[bus][CAP_ID_AP_CTRL]   >= CAP_MIN_FRAMES;
    s.ap_legacy  = g_count[bus][CAP_ID_AP_LEGACY] >= CAP_MIN_FRAMES;
    s.steer      = g_count[bus][CAP_ID_STEER]     >= CAP_MIN_FRAMES;
    return s;
}

static uint32_t bus_total(uint8_t bus) {
    uint32_t t = 0;
    for (uint8_t i = 0; i < CAP_ID_COUNT; i++) t += g_count[bus][i];
    return t;
}

static const char* hint_str(FSDCapBusHint h) {
    switch (h) {
        case CAP_HINT_PARTY:   return "looks Party-like";
        case CAP_HINT_CHASSIS: return "looks Chassis/Vehicle-like";
        default:               return "";
    }
}

String capability_status_json() {
    uint8_t st = g_cap_state;
    uint32_t now = millis();
    uint32_t ms_left = (st == CAP_STATE_RUNNING && g_cap_deadline_ms > now)
                           ? (g_cap_deadline_ms - now) : 0;

    TeslaHWVersion hw = TeslaHW_Unknown;
    if (g_state && g_state_mux) {
        portENTER_CRITICAL(g_state_mux);
        hw = g_state->hw_version;
        portEXIT_CRITICAL(g_state_mux);
    }

    String j;
    j.reserve(640);
    j  = "{";
    j += "\"state\":";   j += (int)st;       j += ',';
    j += "\"ms_left\":"; j += ms_left;       j += ',';
    j += "\"window_ms\":"; j += CAP_WINDOW_MS; j += ',';
    j += "\"hw\":";      j += (int)hw;       j += ',';
    j += "\"buses\":[";

    bool first = true;
    for (uint8_t b = 0; b < CAN_BUS_COUNT; b++) {
        uint32_t total = bus_total(b);
        // Always report can0; report a secondary bus only when it carried frames
        // (single-CAN boards never receive on can1).
        if (b != 0 && total == 0) continue;

        FSDCapSeen seen = seen_for_bus(b);
        FSDCapReport r = fsd_capability_eval(seen, hw);

        if (!first) j += ',';
        first = false;
        j += "{\"bus\":\""; j += can_bus_name((CanBusId)b); j += "\",";
        j += "\"frames\":"; j += total; j += ',';
        j += "\"ids\":{";
        j += "\"epas\":";      j += seen.epas       ? "true" : "false"; j += ',';
        j += "\"das_hw4\":";   j += seen.das_hw4    ? "true" : "false"; j += ',';
        j += "\"das_hw3\":";   j += seen.das_hw3    ? "true" : "false"; j += ',';
        j += "\"ap_ctrl\":";   j += seen.ap_control ? "true" : "false"; j += ',';
        j += "\"ap_legacy\":"; j += seen.ap_legacy  ? "true" : "false"; j += ',';
        j += "\"steer\":";     j += seen.steer      ? "true" : "false"; j += "},";
        j += "\"nag_killer\":";     j += (int)r.nag_killer;     j += ',';
        j += "\"ap_first\":";       j += (int)r.ap_first;       j += ',';
        j += "\"fsd_activation\":"; j += (int)r.fsd_activation; j += ',';
        j += "\"soft_engage\":";    j += (int)r.soft_engage;    j += ',';
        j += "\"hw_unconfirmed\":"; j += r.hw_unconfirmed ? "true" : "false"; j += ',';
        j += "\"hint\":\"";         j += hint_str(r.bus_hint);  j += "\"}";
    }
    j += "]}";
    return j;
}
