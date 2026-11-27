#include "fsd_power.h"

#include <stddef.h>

uint32_t fsd_pwr_quiet_ms(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any) {
    /* Never received anything. We have not watched the bus fall silent -- we
     * have watched nothing. Reporting a huge quiet time here would make a
     * module that was never wired to the car look exactly like a car that fell
     * asleep, which is the one confusion this whole file exists to prevent. */
    if(!seen_any) return 0u;

    /* Unsigned on purpose: the ms clock wraps after ~49 days and this stays
     * correct across the wrap. A signed comparison would briefly report a bus
     * that had been quiet for weeks. */
    uint32_t gap = now_ms - last_rx_ms;
    if(gap < FSD_PWR_QUIET_MS) return 0u;

    /* A stamp ahead of now. The caller reads its clock before it drains the
     * bus, and each frame stamps the time it lands, so the newest frame can be
     * a few ms later than now_ms. Then the gap wraps to just under 2^32: a bus
     * busy this very moment read as 49.7 days of quiet and committed a
     * checkpoint mid-traffic. A gap past half the clock range is a stamp from
     * the future, and that is traffic. It costs quiet spells over 24.8 days,
     * which nothing here waits for. */
    if(gap > 0x7FFFFFFFu) return 0u;
    return gap;
}

bool fsd_pwr_judge_quiet(
    FsdPwrFilterGate* g,
    uint32_t now_ms,
    uint32_t last_rx_ms,
    bool seen_any,
    bool filtered,
    uint32_t* quiet_ms) {
    if(!g || !quiet_ms) return false;
    if(filtered) {
        g->filtered = true;
        return false;
    }
    if(g->filtered) {
        g->filtered = false;
        g->restarted = true;
        g->since_ms = now_ms;
    }
    if(g->restarted) {
        /* A frame after the clear (signed: the stamp may be a few ms ahead of
         * now): the bus speaks for itself again. Never heard one at all: the
         * normal path says "not quiet" for that. */
        if(!seen_any || (int32_t)(last_rx_ms - g->since_ms) > 0) {
            g->restarted = false;
        } else {
            uint32_t q = fsd_pwr_quiet_ms(now_ms, g->since_ms, true);
            if(q == 0u) return false;
            *quiet_ms = q;
            return true;
        }
    }
    *quiet_ms = fsd_pwr_quiet_ms(now_ms, last_rx_ms, seen_any);
    return true;
}

FsdPwrVerdict fsd_pwr_verdict(FsdPwrReset reset, const FsdPwrRecord* prev) {
    /* A host on the USB port reset us, so USB is connected and feeds the
     * board: whatever the record says, this boot cannot speak for the 12V. */
    if(reset == FSD_PWR_RESET_USB) return FSD_PWR_USB;
    if(!prev || !prev->valid) return FSD_PWR_UNKNOWN;

    switch(reset) {
    case FSD_PWR_RESET_BROWNOUT:
        /* The supply sagged rather than vanished. Thin tap wire, a cold crimp,
         * an e-fuse near its limit. Calling this "switched" would send the
         * operator looking at the wrong thing entirely. */
        return FSD_PWR_BROWNOUT;

    case FSD_PWR_RESET_SOFTWARE:
    case FSD_PWR_RESET_EXTERNAL:
        /* We fell over, or someone pressed reset. The supply is not implicated
         * and must not be blamed. */
        return FSD_PWR_CRASH;

    case FSD_PWR_RESET_POWERON:
        /* 🔴 The trap. Coming back from a power cut looks like proof of a
         * switched feed, but only if the car actually slept. Sentry left on, a
         * scheduled charge, or someone opening the app all keep the bus alive;
         * then a power cut means something else happened entirely and we have
         * no business calling it. */
        if(prev->quiet_ms == 0u) return FSD_PWR_NO_SLEEP;
        return FSD_PWR_SWITCHED;

    case FSD_PWR_RESET_UNKNOWN:
    default:
        return FSD_PWR_UNKNOWN;
    }
}

bool fsd_pwr_checkpoint_due(FsdPwrCheckpoint* cp, uint32_t quiet_ms) {
    if(!cp) return false;

    /* Bus active. Never written here -- frames are arriving and a flash stall
     * would drop them. Just re-arm so the next quiet episode may commit. */
    if(quiet_ms == 0u) {
        cp->episode_committed = false;
        return false;
    }

    if(cp->episode_committed) return false; /* one commit per episode */
    if(quiet_ms < FSD_PWR_CHECKPOINT_MS) return false; /* not settled quiet yet */
    if(cp->writes >= FSD_PWR_MAX_CHECKPOINTS) return false;

    cp->episode_committed = true;
    cp->writes++;
    cp->quiet_on_flash = true;
    return true;
}

bool fsd_pwr_correction_due(FsdPwrCheckpoint* cp, uint32_t quiet_ms) {
    if(!cp || quiet_ms != 0u || !cp->quiet_on_flash) return false;
    cp->quiet_on_flash = false;
    return true;
}

FsdPwrVerdict fsd_pwr_deepsleep_verdict(bool slept_on_quiet_bus) {
    return slept_on_quiet_bus ? FSD_PWR_ALWAYS_ON : FSD_PWR_UNKNOWN;
}

FsdPwrVerdict fsd_pwr_apply_usb_guard(FsdPwrVerdict v, bool usb_present) {
    if(usb_present && v == FSD_PWR_ALWAYS_ON) return FSD_PWR_USB;
    return v;
}

uint32_t fsd_pwr_restart_count(FsdPwrReset reset, bool prev_valid, uint32_t prev) {
    uint32_t base = prev_valid ? prev : 0u;
    switch(reset) {
    case FSD_PWR_RESET_POWERON:
        return 0u;
    case FSD_PWR_RESET_DEEPSLEEP:
        return base;
    default:
        return (base == UINT32_MAX) ? base : base + 1u;
    }
}

FsdPwrVerdict fsd_pwr_live_verdict(uint32_t quiet_ms) {
    /* The boot path can never reach ALWAYS_ON: a module that kept its power
     * never rebooted to report anything. So it is decided here, while running,
     * and only after the bus has been silent long enough that a switched feed
     * would already have dropped us. */
    if(quiet_ms >= FSD_PWR_ALWAYS_ON_MS) return FSD_PWR_ALWAYS_ON;
    return FSD_PWR_UNKNOWN;
}

const char* fsd_pwr_reset_str(FsdPwrReset r) {
    switch(r) {
    case FSD_PWR_RESET_POWERON:
        return "power applied";
    case FSD_PWR_RESET_BROWNOUT:
        return "supply sagged (brownout)";
    case FSD_PWR_RESET_SOFTWARE:
        return "software reset";
    case FSD_PWR_RESET_EXTERNAL:
        return "reset pin";
    case FSD_PWR_RESET_DEEPSLEEP:
        return "deep-sleep wake";
    case FSD_PWR_RESET_USB:
        return "USB reset";
    case FSD_PWR_RESET_UNKNOWN:
    default:
        return "unknown";
    }
}

const char* fsd_pwr_verdict_str(FsdPwrVerdict v) {
    switch(v) {
    case FSD_PWR_NO_SLEEP:
        return "inconclusive - the car never slept";
    case FSD_PWR_SWITCHED:
        return "SWITCHED - the feed dies with the car";
    case FSD_PWR_ALWAYS_ON:
        return "ALWAYS-ON - the feed outlives the car";
    case FSD_PWR_BROWNOUT:
        return "BROWNOUT - check the wiring, not the switching";
    case FSD_PWR_CRASH:
        return "we crashed - says nothing about the supply";
    case FSD_PWR_USB:
        return "inconclusive - the module is on USB power";
    case FSD_PWR_UNKNOWN:
    default:
        return "no verdict yet";
    }
}
