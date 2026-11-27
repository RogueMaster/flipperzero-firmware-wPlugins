#pragma once
/*
 * fsd_power.h — "did the car cut our power, or did we fall over?"
 *
 * The rear tap gives us CAN, ground and 12V. Whether that 12V is ALWAYS-ON or
 * SWITCHED decides the final install: always-on means the module draws current
 * while the car sleeps, and a module that wakes whenever the car wakes is a
 * failsafe concern of its own.
 *
 * WHY NOT JUST USE A MULTIMETER
 * -----------------------------
 * Because reaching the connector means opening a door, and opening a door wakes
 * the car. The meter would have to be clipped on, read through the glass, and
 * even then it says only "0 V". This says more:
 *
 *   POWERON   the supply went away and came back
 *   BROWNOUT  the supply sagged -- thin wire, bad crimp, e-fuse near its limit
 *   SOFTWARE  we crashed; the supply is not implicated at all
 *
 * Those three want three different responses, and a voltage reading cannot tell
 * them apart.
 *
 * THE TRAP THIS AVOIDS
 * --------------------
 * "Still running after an hour" does NOT mean always-on. It also happens when
 * the car never slept -- sentry left on, a scheduled charge, someone opening
 * the app. So the record carries how long the BUS had been quiet, and a session
 * where the bus never went quiet is reported as inconclusive rather than as a
 * verdict. Answering the wrong question confidently is the failure mode here.
 *
 * PURE. The shim (esp32/.firmware/power_log.cpp) owns NVS, esp_reset_reason()
 * and the clock; this file owns what the numbers mean.
 */

#include <stdbool.h>
#include <stdint.h>

/* Included from power_log.cpp. Without this the C++ side mangles the names and
 * the linker cannot find the C definitions. */
#ifdef __cplusplus
extern "C" {
#endif

/** No CAN frame for this long counts as "the bus went quiet". */
#ifndef FSD_PWR_QUIET_MS
#define FSD_PWR_QUIET_MS 10000u
#endif

/**
 * Still running this long after the bus went quiet -> always-on.
 *
 * A judgement, not a measurement. The car takes 15-30 min to sleep, and a
 * switched feed dies at or shortly after that point. This sits past that, so a
 * board still alive here outlived the longest plausible sleep; a switched feed
 * would already have dropped it and booted it as POWERON. It is evidence, not
 * proof, which is why the reported verdict is what the operator reads rather
 * than what any code branches on. (A board that deep-sleeps never reaches this
 * live; its always-on evidence comes from fsd_pwr_deepsleep_verdict instead.)
 */
#ifndef FSD_PWR_ALWAYS_ON_MS
#define FSD_PWR_ALWAYS_ON_MS 2700000u /* 45 min */
#endif

/**
 * Bus quiet this long before a checkpoint is committed to flash.
 *
 * Longer than FSD_PWR_QUIET_MS so the mark means "settled quiet", and far
 * shorter than the 15-30 min a car takes to sleep, so a switched feed is
 * recorded well before it could drop us. The commit happens with the bus quiet
 * by definition, so there are never frames to lose to the flash stall.
 */
#ifndef FSD_PWR_CHECKPOINT_MS
#define FSD_PWR_CHECKPOINT_MS 30000u
#endif

/**
 * Most checkpoints committed per power session.
 *
 * One per quiet episode; the cap stops a bus that flaps quiet/busy from wearing
 * the flash. The SWITCHED evidence is in flash after the first commit, so the
 * cap costs the verdict nothing.
 */
#ifndef FSD_PWR_MAX_CHECKPOINTS
#define FSD_PWR_MAX_CHECKPOINTS 6u
#endif

/** Why we are running again. Mapped from esp_reset_reason() by the shim. */
typedef enum {
    FSD_PWR_RESET_UNKNOWN = 0,
    FSD_PWR_RESET_POWERON, /* supply went away and came back */
    FSD_PWR_RESET_BROWNOUT, /* supply sagged below the rail */
    FSD_PWR_RESET_SOFTWARE, /* panic, watchdog, esp_restart() */
    FSD_PWR_RESET_EXTERNAL, /* reset pin */
    FSD_PWR_RESET_DEEPSLEEP, /* woke from our own deep sleep (board kept power) */
    FSD_PWR_RESET_USB, /* the USB host reset us (ESP32-S3 USB serial/JTAG) */
} FsdPwrReset;

typedef enum {
    FSD_PWR_UNKNOWN = 0, /* nothing recorded yet, or nothing can be said */
    FSD_PWR_NO_SLEEP, /* the bus never went quiet -- the car did not sleep */
    FSD_PWR_SWITCHED, /* bus went quiet, then the supply died */
    FSD_PWR_ALWAYS_ON, /* bus quiet a long time and we are still running */
    FSD_PWR_BROWNOUT, /* supply sagged -- a wiring verdict, not a switching one */
    FSD_PWR_CRASH, /* we fell over; says nothing about the supply */
    FSD_PWR_USB, /* USB power masks the test -- no verdict possible */
} FsdPwrVerdict;

/**
 * What the shim writes to NVS periodically and reads back on the next boot.
 *
 * `quiet_ms` is a DURATION, not a timestamp: how long the bus had been quiet
 * at the moment of the write. 0 means the bus was still carrying traffic.
 */
typedef struct {
    bool valid;
    uint32_t uptime_ms;
    uint32_t quiet_ms;
} FsdPwrRecord;

/**
 * What the previous session tells us, read at boot.
 *
 * Returns FSD_PWR_UNKNOWN when there is no record or the reset reason does not
 * license a conclusion. That is the honest answer far more often than not, and
 * it is the reason this returns a verdict rather than a bool.
 */
FsdPwrVerdict fsd_pwr_verdict(FsdPwrReset reset, const FsdPwrRecord* prev);

/**
 * What THIS session tells us, while it is still running.
 *
 * The boot path can never report ALWAYS_ON, because a module that kept its
 * power never rebooted to report anything. So the live path covers that case:
 * still alive, bus quiet for long enough, therefore the feed did not die.
 */
FsdPwrVerdict fsd_pwr_live_verdict(uint32_t quiet_ms);

/**
 * How long the bus has been quiet, or 0 while it is carrying traffic.
 *
 * Kept here rather than inlined at the call site so the "never seen a frame"
 * case is decided once: a module that has never received anything has not
 * observed the bus going quiet -- it has observed nothing. Likewise a
 * last_rx_ms later than now_ms (a frame that landed after the caller read its
 * clock) is traffic.
 */
uint32_t fsd_pwr_quiet_ms(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any);

/**
 * Whether the bus can be judged at all, and how long it has been quiet.
 *
 * A hardware acceptance filter (a single- or multi-ID capture) hides most of
 * the bus: with it on a rare id, a busy bus looks silent. So while a filter is
 * installed on any bus nothing is judged -- no checkpoint, no correction, no
 * live verdict. When it comes off, the quiet time counts from that moment, not
 * from the last frame seen through the filter, until a frame arrives again;
 * and the bus is not judged until that count reaches FSD_PWR_QUIET_MS either,
 * since a few silent seconds after the clear prove neither quiet nor traffic.
 *
 * Returns false when the bus cannot be judged now; otherwise true with
 * *quiet_ms as fsd_pwr_quiet_ms() gives it.
 */
typedef struct {
    bool filtered; /* a filter was on at the last call */
    bool restarted; /* it came off, no frame since: quiet counts from since_ms */
    uint32_t since_ms; /* when it came off */
} FsdPwrFilterGate;

bool fsd_pwr_judge_quiet(
    FsdPwrFilterGate* g,
    uint32_t now_ms,
    uint32_t last_rx_ms,
    bool seen_any,
    bool filtered,
    uint32_t* quiet_ms);

/**
 * Flash-checkpoint scheduler. Pure so the shim's write cadence is host-tested.
 *
 * Fed the current quiet duration each tick, it returns true exactly when the
 * shim should commit one record to flash: the bus has been quiet for
 * FSD_PWR_CHECKPOINT_MS, this quiet episode has not been committed yet, and the
 * per-session cap is not reached. While the bus is active (quiet_ms == 0) it
 * only re-arms and returns false, so a commit can never land on the CAN path.
 */
typedef struct {
    bool episode_committed; /* this quiet episode already produced a commit */
    uint8_t writes; /* commits this power session (for the cap) */
    bool quiet_on_flash; /* the record in flash says "quiet" (a commit,
                                * not corrected since) */
} FsdPwrCheckpoint;

bool fsd_pwr_checkpoint_due(FsdPwrCheckpoint* cp, uint32_t quiet_ms);

/**
 * The one write the bus coming back forces.
 *
 * After a checkpoint the flash says "the bus went quiet". If the car then wakes
 * and drives and the power is cut mid-drive, the next boot reads "quiet, then
 * power lost" and answers SWITCHED. So on the first tick the bus carries
 * traffic again (quiet_ms == 0) after a commit, this returns true once and the
 * shim writes a record with quiet 0 (the next boot then says NO_SLEEP, a
 * non-verdict). Adapted from ukinora's fork (778439d).
 *
 * This is the one flash write that lands with frames arriving: at the wake
 * edge, the first frame after at least FSD_PWR_CHECKPOINT_MS of silence, when
 * the car is only just waking. It needs a checkpoint before it, so a session
 * writes at most 2 x FSD_PWR_MAX_CHECKPOINTS records.
 */
bool fsd_pwr_correction_due(FsdPwrCheckpoint* cp, uint32_t quiet_ms);

/**
 * The verdict after waking from the board's own deep sleep.
 *
 * Only a board that kept its power wakes this way: a switched feed would have
 * cut power during the sleep and booted us as POWERON. So a deep-sleep wake
 * that was entered on a quiet bus is ALWAYS_ON; anything else says nothing.
 */
FsdPwrVerdict fsd_pwr_deepsleep_verdict(bool slept_on_quiet_bus);

/**
 * Guard an ALWAYS_ON claim against USB power.
 *
 * A board fed over USB stays alive regardless of the car's feed, so "still
 * running" proves nothing. When USB is present an ALWAYS_ON verdict becomes
 * FSD_PWR_USB (inconclusive); every other verdict is left alone, since it comes
 * from a reboot that happened whether or not USB was attached.
 */
FsdPwrVerdict fsd_pwr_apply_usb_guard(FsdPwrVerdict v, bool usb_present);

/**
 * Restarts since power-on, after a boot with this reset reason.
 *
 * A power-on starts the count at 0. A crash, watchdog, brownout, software or
 * pin reset adds one. A deep-sleep wake leaves it alone: on a board that sleeps
 * on a quiet bus that is a boot every time the car wakes, and counting it would
 * turn a healthy board's counter into what reads like a crash counter.
 *
 * @param prev_valid  whether `prev` survived (RTC RAM intact); if not the count
 *                    starts from 0
 */
uint32_t fsd_pwr_restart_count(FsdPwrReset reset, bool prev_valid, uint32_t prev);

const char* fsd_pwr_reset_str(FsdPwrReset r);
const char* fsd_pwr_verdict_str(FsdPwrVerdict v);

#ifdef __cplusplus
}
#endif
