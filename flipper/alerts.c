#include "alerts.h"
#include <string.h>

static const AlertRuleMeta metas[AlertRuleCount] = {
    [AlertCpuTemp] = {"CPU temp", "C", 60, 105, 5, false},
    [AlertGpuTemp] = {"GPU temp", "C", 60, 105, 5, false},
    [AlertCpuLoad] = {"CPU load", "%", 50, 100, 5, false},
    [AlertGpuLoad] = {"GPU load", "%", 50, 100, 5, false},
    [AlertRamLoad] = {"RAM load", "%", 50, 100, 5, false},
    [AlertVramLoad] = {"VRAM load", "%", 50, 100, 5, false},
    [AlertDiskLoad] = {"Disk used", "%", 50, 100, 5, false},
    [AlertBatteryLow] = {"Battery low", "%", 5, 50, 5, true},
    [AlertLinkLost] = {"PC link lost", "s", 5, 60, 5, false},
};

const AlertRuleMeta* alerts_meta(AlertRuleId id) {
    return &metas[id];
}

AlertRuleConfig alerts_default_config(AlertRuleId id) {
    AlertRuleConfig c = {true, 90, 0, 5, AlertSignalVibroSound};
    switch(id) {
    case AlertCpuTemp:
        c.threshold = 90;
        break;
    case AlertGpuTemp:
        c.threshold = 85;
        break;
    case AlertCpuLoad:
        c.threshold = 95;
        c.sustain_s = 30;
        c.signal = AlertSignalVibro;
        break;
    case AlertGpuLoad:
        c.enabled = false;
        c.threshold = 98;
        c.sustain_s = 30;
        c.signal = AlertSignalVibro;
        break;
    case AlertRamLoad:
        c.threshold = 90;
        c.sustain_s = 10;
        c.signal = AlertSignalVibro;
        break;
    case AlertVramLoad:
        c.threshold = 95;
        c.sustain_s = 10;
        c.signal = AlertSignalVibro;
        break;
    case AlertDiskLoad:
        c.threshold = 95;
        c.signal = AlertSignalSound;
        break;
    case AlertBatteryLow:
        c.threshold = 20;
        break;
    case AlertLinkLost:
        c.threshold = 10;
        break;
    default:
        break;
    }
    return c;
}

void alerts_reset_state(AlertEngine* e) {
    memset(e->st, 0, sizeof(e->st));
}

void alerts_set_defaults(AlertEngine* e) {
    for(int i = 0; i < AlertRuleCount; i++) e->cfg[i] = alerts_default_config((AlertRuleId)i);
    e->cooldown_s = 300;
    e->snooze_s = 600;
    alerts_reset_state(e);
}

// Extract the monitored value; returns false if the field is not valid in this frame.
static bool rule_value(AlertRuleId id, const PhrTelemetry* t, uint32_t silence_s, uint32_t* v) {
    if(id == AlertLinkLost) {
        *v = silence_s;
        return true;
    }
    if(!t) return false;
    switch(id) {
    case AlertCpuTemp:
        *v = t->cpu_temp;
        return t->flags & PHR_FLAG_CPU_TEMP_VALID;
    case AlertGpuTemp:
        *v = t->gpu_temp;
        return (t->flags & PHR_FLAG_GPU_PRESENT) && (t->flags & PHR_FLAG_GPU_TEMP_VALID);
    case AlertCpuLoad:
        *v = t->cpu_load;
        return true;
    case AlertGpuLoad:
        *v = t->gpu_load;
        return t->flags & PHR_FLAG_GPU_PRESENT;
    case AlertRamLoad:
        *v = t->ram_load;
        return true;
    case AlertVramLoad:
        *v = t->vram_load;
        return t->flags & PHR_FLAG_GPU_PRESENT;
    case AlertDiskLoad:
        *v = t->disk_load;
        return true;
    case AlertBatteryLow:
        *v = t->battery;
        return (t->flags & PHR_FLAG_BATTERY_PRESENT) && (t->flags & PHR_FLAG_ON_BATTERY);
    default:
        return false;
    }
}

size_t alerts_update(
    AlertEngine* e,
    uint32_t now_s,
    const PhrTelemetry* t,
    uint32_t silence_s,
    AlertEvent* out) {
    size_t n = 0;
    for(int i = 0; i < AlertRuleCount; i++) {
        const AlertRuleConfig* c = &e->cfg[i];
        AlertRuleState* s = &e->st[i];
        uint32_t v = 0;
        bool valid = c->enabled && rule_value((AlertRuleId)i, t, silence_s, &v);

        bool cond = false;
        if(valid) {
            int thr = c->threshold;
            int hyst = c->hysteresis;
            int val = (int)v;
            if(metas[i].lower_is_worse) {
                cond = s->active ? (val <= thr + hyst) : (val <= thr);
            } else {
                cond = s->active ? (val >= thr - hyst) : (val >= thr);
            }
        }

        if(!cond) {
            s->active = false;
            s->fired_this_episode = false;
            continue;
        }
        if(!s->active) {
            s->active = true;
            s->fired_this_episode = false;
            s->over_since_s = now_s;
        }
        if((now_s - s->over_since_s) < c->sustain_s) continue;
        if(s->snoozed && (int32_t)(now_s - s->snooze_until_s) < 0) continue;
        if(s->fired_this_episode && (now_s - s->last_fired_s) < e->cooldown_s) continue;

        s->fired_this_episode = true;
        s->last_fired_s = now_s;
        out[n].rule = (AlertRuleId)i;
        out[n].value = (uint8_t)(v > 255 ? 255 : v);
        out[n].threshold = c->threshold;
        out[n].signal = c->signal;
        n++;
    }
    return n;
}

void alerts_acknowledge(AlertEngine* e, AlertRuleId id, uint32_t now_s) {
    e->st[id].snoozed = true;
    e->st[id].snooze_until_s = now_s + e->snooze_s;
}

bool alerts_is_active(const AlertEngine* e, AlertRuleId id) {
    return e->st[id].active;
}
