#pragma once
/*
 * power_log.h — ties fsd_logic/fsd_power to the ESP32: NVS, esp_reset_reason(),
 * RTC no-init memory, the millis() clock and Serial.
 *
 * It answers two questions a user of this device cannot otherwise answer:
 *
 *   - WHY DID THE BOARD RESTART? Power-on, brownout, panic, watchdog, a plain
 *     software restart, or a deep-sleep wake each call for a different response.
 *     The last two releases rebooted whenever a browser opened the dashboard; a
 *     line on the dashboard saying "last reset was a crash" would have settled
 *     it in one screenshot.
 *
 *   - IS THE 12V AT THE TAP SWITCHED OR ALWAYS-ON? The tap carries CAN, ground
 *     and 12V. A switched feed dies when the car sleeps; an always-on feed draws
 *     current overnight. A multimeter can't answer it (reaching the connector
 *     wakes the car) but the board can: if it loses power after the bus has gone
 *     quiet, the feed is switched; if it is still running (or kept its RTC state
 *     across a deep sleep) long after the bus went quiet, it is always-on.
 *
 * PERSISTENCE. The 12V test has to survive the board losing power, so the
 * evidence lives in flash (NVS). To keep flash writes off the CAN path, nothing
 * is written while the bus is active: a single record is committed only after
 * the bus has been quiet for FSD_PWR_CHECKPOINT_MS, when there are no frames to
 * lose (fsd_pwr_checkpoint_due), and the commits per session are capped. The
 * one exception is a correcting record at the first frame after a checkpoint,
 * the car waking (fsd_pwr_correction_due): without it a power cut later in
 * that drive reads as a switched feed. Each boot reads the record, judges it,
 * then clears it, so a session reasons only from its own checkpoints. State
 * that only has to survive a reset, not a power cut -- the restart counter and
 * "we slept on a quiet bus" -- lives in RTC no-init memory, which costs no
 * flash.
 *
 * The judgement itself is in fsd_logic/fsd_power.{h,c} and host-tested. This
 * file owns only the hardware: NVS, esp_reset_reason(), RTC RAM, the clock and
 * Serial.
 *
 * Listen-Only: nothing here transmits.
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../fsd_logic/fsd_power.h"

/* Fine-grained reset reasons for the dashboard. Coarser FsdPwrReset drives the
 * verdict (panic/watchdog/software all mean "we fell over"); these keep them
 * apart for the "why did it restart" line. The dashboard JS mirrors this order. */
enum {
    PWR_RST_UNKNOWN = 0,
    PWR_RST_POWERON,
    PWR_RST_BROWNOUT,
    PWR_RST_PANIC,
    PWR_RST_WATCHDOG,
    PWR_RST_SOFTWARE,
    PWR_RST_DEEPSLEEP,
    PWR_RST_EXTERNAL,
    PWR_RST_USB,
};

/* ESP32-S3 RTC reset reasons (soc/reset_reasons.h) for a reset from the USB
 * host: the serial port's DTR/RTS reset (boot log "rst:0x15
 * (USB_UART_CHIP_RESET)") and the JTAG one. esp_reset_reason() in the pinned
 * IDF 4.4 has no code for them and says ESP_RST_UNKNOWN; IDF 5 says
 * ESP_RST_USB. */
#define PWR_S3_RTC_USB_UART 0x15
#define PWR_S3_RTC_USB_JTAG 0x16

/* The PWR_RST_* code for a reset esp_reset_reason() could not name, from the
 * chip's own RTC reset reason. @p fine is what esp_reset_reason() mapped to;
 * only an unknown one on the S3 is looked at again. */
static inline int power_log_refine_reset(int fine, int rtc_reason, bool s3) {
    if (fine != PWR_RST_UNKNOWN || !s3) return fine;
    if (rtc_reason == PWR_S3_RTC_USB_UART || rtc_reason == PWR_S3_RTC_USB_JTAG) return PWR_RST_USB;
    return fine;
}

/**
 * Read the previous session's record, print the verdict, start a new session.
 * Call once from setup(), after Serial is up.
 */
void power_log_init(void);

/**
 * Feed the clock and the bus. Call from loop().
 *
 * A flash commit happens here (and on the way into deep sleep): once the bus
 * has been quiet past the checkpoint window, and once more at the first frame
 * after such a commit, to correct it -- never during sustained traffic.
 *
 * @param last_rx_ms  millis() of the last CAN frame (0 until the first one)
 * @param seen_any    whether any frame has ever arrived (0 is a legal clock
 *                    value, so last_rx_ms alone cannot say)
 * @param filtered    a hardware acceptance filter is on some bus (a capture):
 *                    nothing is judged or written (fsd_pwr_judge_quiet)
 */
void power_log_tick(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any, bool filtered);

/**
 * Note that the board is about to deep-sleep on a quiet bus (LilyGO). Records
 * that in RTC RAM so a later deep-sleep wake can conclude the feed held through
 * the sleep, and flushes the quiet checkpoint if it has not already been
 * committed. Call right before esp_deep_sleep_start(); the bus is quiet there.
 */
void power_log_note_deep_sleep(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any,
                               bool filtered);

/** The verdict as it stands (live/deep-sleep outranks the boot reading). */
FsdPwrVerdict power_log_verdict(void);

/* Reporting helpers for the boot banner and the dashboard JSON. */
const char *power_log_verdict_str(void);
int         power_log_verdict_code(void);   /* FsdPwrVerdict */
const char *power_log_reset_str(void);
int         power_log_reset_code(void);     /* PWR_RST_* */
uint32_t    power_log_restart_count(void);  /* restarts since power-on; a deep-sleep wake is not one */
