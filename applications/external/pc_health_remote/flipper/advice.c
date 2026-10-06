#include "advice.h"
#include <stdio.h>
#include <string.h>

static void put(char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN], size_t* n, const char* s) {
    if(*n >= ADVICE_MAX_LINES) return;
    snprintf(lines[*n], ADVICE_LINE_LEN, "%s", s);
    (*n)++;
}

static void put_close(
    char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN],
    size_t* n,
    const char* name,
    const char* fallback) {
    if(name && name[0]) {
        char buf[ADVICE_LINE_LEN];
        snprintf(buf, sizeof(buf), "Close %s", name);
        put(lines, n, buf);
    } else {
        put(lines, n, fallback);
    }
}

size_t advice_get(
    AlertRuleId rule,
    const PhrTelemetry* t,
    char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN]) {
    size_t n = 0;
    const char* cpu_name = (t && t->top_cpu_pct >= 10) ? t->top_cpu_name : NULL;
    const char* ram_name = t ? t->top_ram_name : NULL;
    for(size_t i = 0; i < ADVICE_MAX_LINES; i++) lines[i][0] = '\0';

    switch(rule) {
    case AlertCpuTemp:
        put(lines, &n, "Check fans / dust");
        put(lines, &n, "Power plan: Balanced");
        put_close(lines, &n, cpu_name, "Improve case airflow");
        break;
    case AlertGpuTemp:
        put(lines, &n, "Cap FPS / V-Sync on");
        put(lines, &n, "Lower graphics preset");
        put(lines, &n, "Check GPU fans / dust");
        break;
    case AlertCpuLoad:
        put_close(lines, &n, cpu_name, "Check background apps");
        put(lines, &n, "Check for runaway apps");
        put(lines, &n, "Power plan: Balanced");
        break;
    case AlertGpuLoad:
        put(lines, &n, "Cap FPS / V-Sync on");
        put(lines, &n, "Lower graphics preset");
        break;
    case AlertRamLoad:
        put_close(lines, &n, ram_name, "Close heavy apps");
        put(lines, &n, "Close unused tabs");
        put(lines, &n, "Restart the browser");
        break;
    case AlertVramLoad:
        put(lines, &n, "Lower texture quality");
        put(lines, &n, "Close GPU-heavy apps");
        put(lines, &n, "Lower resolution");
        break;
    case AlertDiskLoad:
        put(lines, &n, "Run Disk Cleanup");
        put(lines, &n, "Empty the Recycle Bin");
        put(lines, &n, "Uninstall unused apps");
        break;
    case AlertBatteryLow:
        put(lines, &n, "Plug in charger");
        put(lines, &n, "Enable battery saver");
        put(lines, &n, "Lower brightness");
        break;
    case AlertLinkLost:
        put(lines, &n, "Backend running on PC?");
        put(lines, &n, "Check BT range / cable");
        put(lines, &n, "PC asleep or off?");
        break;
    default:
        break;
    }
    return n;
}
