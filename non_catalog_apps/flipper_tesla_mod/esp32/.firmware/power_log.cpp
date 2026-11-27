#include "power_log.h"
#include "prefs.h"  // prefs_namespace_exists

#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <esp_rom_sys.h>  // esp_rom_get_reset_reason
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
#include <HWCDC.h>
#endif

// Own namespace. prefs.cpp owns the operator settings and is written once when
// something changes; this is a separate, bounded writer and must not share the
// settings blob.
static const char *NS = "pwrlog";

static Preferences g_nvs;
static bool g_ok = false;

static FsdPwrVerdict g_boot_verdict = FSD_PWR_UNKNOWN;
static FsdPwrVerdict g_live_verdict = FSD_PWR_UNKNOWN;
static FsdPwrCheckpoint g_cp = {false, 0, false};
static FsdPwrFilterGate g_gate = {false, false, 0};
static int g_reset_code = PWR_RST_UNKNOWN;
static int g_rtc_reason = 0;  // the chip's own reset reason, for an unknown one

// RTC no-init RAM: kept across a software reset, a crash, a brownout CPU reset
// and (its real job here) deep sleep, but wiped by a true power cut and never
// initialised on the first power-on. That is exactly the line the 12V test
// draws, so it holds the restart counter and the deep-sleep marker -- neither
// needs flash, and keeping them here costs no writes.
typedef struct {
    uint32_t magic;
    uint32_t restarts;    // restarts since power-on (fsd_pwr_restart_count)
    uint8_t  slept_quiet; // we entered deep sleep with the bus quiet
} PwrRtc;
static RTC_NOINIT_ATTR PwrRtc g_rtc;
#define PWR_RTC_MAGIC 0x50574C31u  // 'PWL1'

static bool usb_present(void) {
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    // S3 boards (T-2CAN, Waveshare) expose Serial over the native USB; a
    // connected host feeds the board and would mask a switched feed. On the
    // UART-bridge boards USB power is not observable, so we report "not on USB"
    // and lean on the README's caveat instead.
    return HWCDC::isPlugged();
#else
    return false;
#endif
}

static int fine_reset_code(esp_reset_reason_t r) {
    switch (r) {
    case ESP_RST_POWERON:   return PWR_RST_POWERON;
    case ESP_RST_BROWNOUT:  return PWR_RST_BROWNOUT;
    case ESP_RST_PANIC:     return PWR_RST_PANIC;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return PWR_RST_WATCHDOG;
    case ESP_RST_SW:        return PWR_RST_SOFTWARE;
    case ESP_RST_DEEPSLEEP: return PWR_RST_DEEPSLEEP;
    case ESP_RST_EXT:       return PWR_RST_EXTERNAL;
    default:                return PWR_RST_UNKNOWN;
    }
}

// Coarser category for the verdict: a crash is a crash whether it was a panic,
// a watchdog or esp_restart().
static FsdPwrReset verdict_category(esp_reset_reason_t r) {
    switch (r) {
    case ESP_RST_POWERON:   return FSD_PWR_RESET_POWERON;
    case ESP_RST_BROWNOUT:  return FSD_PWR_RESET_BROWNOUT;
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_SW:        return FSD_PWR_RESET_SOFTWARE;
    case ESP_RST_DEEPSLEEP: return FSD_PWR_RESET_DEEPSLEEP;
    case ESP_RST_EXT:       return FSD_PWR_RESET_EXTERNAL;
    default:                return FSD_PWR_RESET_UNKNOWN;
    }
}

static void print_minutes(const char *label, uint32_t ms) {
    Serial.printf("%s %um %us", label, (unsigned)(ms / 60000u),
                  (unsigned)((ms % 60000u) / 1000u));
}

void power_log_init(void) {
    esp_reset_reason_t r = esp_reset_reason();
    g_rtc_reason = (int)esp_rom_get_reset_reason(0);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    g_reset_code = power_log_refine_reset(fine_reset_code(r), g_rtc_reason, true);
#else
    g_reset_code = power_log_refine_reset(fine_reset_code(r), g_rtc_reason, false);
#endif
    FsdPwrReset cat = (g_reset_code == PWR_RST_USB) ? FSD_PWR_RESET_USB : verdict_category(r);

    // RTC RAM is valid only if it survived (not a power cut, not first boot).
    bool rtc_ok = (g_rtc.magic == PWR_RTC_MAGIC);
    g_rtc.restarts = fsd_pwr_restart_count(cat, rtc_ok, g_rtc.restarts);
    if (!rtc_ok) {
        g_rtc.magic = PWR_RTC_MAGIC;
        g_rtc.slept_quiet = 0;
    }

    // Previous session's flash record. The namespace only exists once a
    // checkpoint has been committed.
    FsdPwrRecord prev;
    prev.valid = false;
    prev.uptime_ms = 0;
    prev.quiet_ms = 0;
    if (prefs_namespace_exists(NS) && g_nvs.begin(NS, /*readOnly=*/true)) {
        if (g_nvs.isKey("v")) {
            prev.valid = true;
            prev.uptime_ms = g_nvs.getULong("up", 0);
            prev.quiet_ms = g_nvs.getULong("q", 0);
        }
        g_nvs.end();
    }

    // Boot verdict. A deep-sleep wake is judged from RTC RAM (the board kept its
    // power); every other reason from the flash record. USB masks an always-on
    // claim either way.
    if (cat == FSD_PWR_RESET_DEEPSLEEP)
        g_boot_verdict = fsd_pwr_deepsleep_verdict(rtc_ok && g_rtc.slept_quiet);
    else
        g_boot_verdict = fsd_pwr_verdict(cat, &prev);
    g_boot_verdict = fsd_pwr_apply_usb_guard(g_boot_verdict, usb_present());

    if (g_reset_code == PWR_RST_UNKNOWN)
        Serial.printf("[PWR] reset: unknown (RTC reason 0x%02X)\n", (unsigned)g_rtc_reason);
    else
        Serial.printf("[PWR] reset: %s\n", power_log_reset_str());
    if (prev.valid) {
        // The uptime is when the record was written, not when the power went:
        // nothing is written while the bus is busy.
        Serial.print("[PWR] last record: ");
        print_minutes("up", prev.uptime_ms);
        if (prev.quiet_ms == 0) Serial.print(" - CAN was flowing");
        else { Serial.print(" - "); print_minutes("CAN quiet", prev.quiet_ms); }
        Serial.println();
    } else {
        Serial.println("[PWR] last session: no record (first boot or cleared)");
    }
    Serial.printf("[PWR] 12V feed: %s\n", power_log_verdict_str());
    Serial.printf("[PWR] restarts since power-on: %u\n", (unsigned)g_rtc.restarts);

    // Clear the record so this power session reasons only from its own
    // checkpoints. Only write when there is something to clear, so a clean boot
    // (no checkpoint last time) costs no flash. Done in setup(), before the loop
    // drains CAN, so it is off the frame path.
    if (prev.valid && prev.quiet_ms != 0 && g_nvs.begin(NS, /*readOnly=*/false)) {
        g_nvs.putULong("q", 0);
        g_nvs.end();
    }
    g_rtc.slept_quiet = 0;  // this session has not slept yet
    g_ok = true;
}

// Commit one record: a quiet checkpoint, whose bus is quiet so the flash
// stall has no frames to drop, or (quiet_ms 0) the correction at the wake
// edge (fsd_pwr_correction_due). noinline: ci_check_stack.py names it on its
// chains.
static __attribute__((noinline)) void power_log_commit(uint32_t now_ms, uint32_t quiet_ms) {
    if (!g_nvs.begin(NS, /*readOnly=*/false)) return;
    g_nvs.putUChar("v", 1);
    g_nvs.putULong("up", now_ms);
    g_nvs.putULong("q", quiet_ms);
    g_nvs.end();
    if (quiet_ms)
        Serial.printf("[PWR] checkpoint: bus quiet %us, record committed\n",
                      (unsigned)(quiet_ms / 1000u));
    else
        Serial.println("[PWR] bus active again: record corrected (no longer quiet)");
}

void power_log_tick(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any, bool filtered) {
    if (!g_ok) return;

    // A capture's hardware filter hides most of the bus: no verdict and no
    // write while it is on, nor until the bus can be judged again after it.
    uint32_t quiet;
    if (!fsd_pwr_judge_quiet(&g_gate, now_ms, last_rx_ms, seen_any, filtered, &quiet)) return;

    // Live verdict: a board that never sleeps and is still running long after
    // the bus went quiet is on an always-on feed (unless USB is masking it).
    FsdPwrVerdict live = fsd_pwr_apply_usb_guard(fsd_pwr_live_verdict(quiet), usb_present());
    if (live != FSD_PWR_UNKNOWN && live != g_live_verdict) {
        g_live_verdict = live;
        Serial.printf("[PWR] bus quiet %us, still running -> %s\n",
                      (unsigned)(quiet / 1000u), fsd_pwr_verdict_str(live));
    }

    // Commit a checkpoint only when the scheduler says so -- bus settled quiet,
    // no frames to lose. The one exception is the correction when the bus
    // comes back after a checkpoint: without it a power cut mid-drive reads as
    // SWITCHED at the next boot.
    if (fsd_pwr_correction_due(&g_cp, quiet))
        power_log_commit(now_ms, 0);
    if (fsd_pwr_checkpoint_due(&g_cp, quiet))
        power_log_commit(now_ms, quiet);
}

void power_log_note_deep_sleep(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any,
                               bool filtered) {
    if (!g_ok) return;
    uint32_t quiet;
    if (!fsd_pwr_judge_quiet(&g_gate, now_ms, last_rx_ms, seen_any, filtered, &quiet)) quiet = 0;

    // Remember we slept on a quiet bus, so a deep-sleep wake can tell the feed
    // held through the sleep. RTC RAM survives the sleep; a switched feed that
    // cuts power during it wipes RTC RAM and boots us as POWERON instead.
    g_rtc.slept_quiet = (quiet > 0) ? 1 : 0;

    // Safety net: make sure the quiet checkpoint is in flash before we sleep, in
    // case the cap or timing left it uncommitted. The bus is quiet here (that is
    // why we are sleeping), so the write is still off the frame path. Normally
    // power_log_tick already committed it at FSD_PWR_CHECKPOINT_MS.
    if (fsd_pwr_checkpoint_due(&g_cp, quiet))
        power_log_commit(now_ms, quiet);
}

FsdPwrVerdict power_log_verdict(void) {
    // What this session can prove outranks what the last one suggested: a live
    // or deep-sleep ALWAYS_ON is happening now, the boot verdict reads something
    // already over.
    return (g_live_verdict != FSD_PWR_UNKNOWN) ? g_live_verdict : g_boot_verdict;
}

const char *power_log_verdict_str(void) { return fsd_pwr_verdict_str(power_log_verdict()); }
int         power_log_verdict_code(void) { return (int)power_log_verdict(); }
int         power_log_reset_code(void)  { return g_reset_code; }
uint32_t    power_log_restart_count(void) { return g_rtc.restarts; }

const char *power_log_reset_str(void) {
    // Fine-grained for the banner; the verdict uses the coarse category.
    switch (g_reset_code) {
    case PWR_RST_POWERON:   return "power applied";
    case PWR_RST_BROWNOUT:  return "brownout (supply sagged)";
    case PWR_RST_PANIC:     return "panic (crash)";
    case PWR_RST_WATCHDOG:  return "watchdog";
    case PWR_RST_SOFTWARE:  return "software restart";
    case PWR_RST_DEEPSLEEP: return "deep-sleep wake";
    case PWR_RST_EXTERNAL:  return "reset pin";
    case PWR_RST_USB:       return "USB reset (host on the USB port)";
    default:                return "unknown";
    }
}
