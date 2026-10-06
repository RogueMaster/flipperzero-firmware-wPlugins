#pragma once
// Alert engine: pure C, no firmware dependencies, unit-testable on the host.
#include "protocol.h"

typedef enum {
    AlertCpuTemp,
    AlertGpuTemp,
    AlertCpuLoad,
    AlertGpuLoad,
    AlertRamLoad,
    AlertVramLoad,
    AlertDiskLoad,
    AlertBatteryLow,
    AlertLinkLost,
    AlertRuleCount
} AlertRuleId;

typedef enum {
    AlertSignalVibro,
    AlertSignalSound,
    AlertSignalVibroSound,
    AlertSignalLed,
    AlertSignalOff,
    AlertSignalCount
} AlertSignal;

typedef struct {
    bool enabled;
    uint8_t threshold; // units depend on the rule (C, %, s)
    uint16_t sustain_s; // condition must hold this long before firing
    uint8_t hysteresis; // clears when value drops this far below threshold
    uint8_t signal; // AlertSignal
} AlertRuleConfig;

typedef struct {
    const char* name; // short name for menus, e.g. "CPU temp"
    const char* unit; // "C", "%", "s"
    uint8_t min, max, step; // allowed threshold range for the settings UI
    bool lower_is_worse; // battery: fires when value <= threshold
} AlertRuleMeta;

typedef struct {
    bool active; // condition currently held (with hysteresis)
    bool fired_this_episode;
    uint32_t over_since_s;
    uint32_t last_fired_s;
    uint32_t snooze_until_s;
    bool snoozed; // snooze_until_s valid
} AlertRuleState;

typedef struct {
    AlertRuleId rule;
    uint8_t value;
    uint8_t threshold;
    uint8_t signal;
} AlertEvent;

typedef struct {
    AlertRuleConfig cfg[AlertRuleCount];
    AlertRuleState st[AlertRuleCount];
    uint32_t cooldown_s; // repeat cooldown, default 300
    uint32_t snooze_s; // snooze on acknowledge, default 600
} AlertEngine;

const AlertRuleMeta* alerts_meta(AlertRuleId id);
AlertRuleConfig alerts_default_config(AlertRuleId id);
void alerts_set_defaults(AlertEngine* e);
void alerts_reset_state(AlertEngine* e);

/**
 * Evaluate all rules. `t` may be NULL when no telemetry is available (only link_lost
 * is evaluated then). `silence_s` = seconds since the last valid telemetry.
 * Writes at most AlertRuleCount events into `out`; returns count.
 */
size_t alerts_update(
    AlertEngine* e,
    uint32_t now_s,
    const PhrTelemetry* t,
    uint32_t silence_s,
    AlertEvent* out);

void alerts_acknowledge(AlertEngine* e, AlertRuleId id, uint32_t now_s);
bool alerts_is_active(const AlertEngine* e, AlertRuleId id);
