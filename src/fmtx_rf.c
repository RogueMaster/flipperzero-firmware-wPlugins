#include "fmtx_rf.h"

#include <string.h>
#include <furi_hal.h>
#include <lib/drivers/cc1101_regs.h>

static const uint8_t fmtx_preset[] = {
    CC1101_IOCFG0,
    0x0D,
    CC1101_FSCTRL1,
    0x06,
    CC1101_PKTCTRL0,
    0x32,
    CC1101_PKTCTRL1,
    0x04,
    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x02,
    CC1101_MDMCFG2,
    0x04,
    CC1101_MDMCFG3,
    0xE4,
    CC1101_MDMCFG4,
    0x6A,
    CC1101_DEVIATN,
    0x04,
    CC1101_MCSM0,
    0x18,
    CC1101_FOCCFG,
    0x16,
    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,
    CC1101_WORCTRL,
    0xFB,
    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,
    0x00,
    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

void rfinit(Rf* rf, uint32_t hz) {
    memset(rf, 0, sizeof(*rf));
    rf->hz = hz;
    rf->regs = fmtx_preset;
    rf->sample_decisions = 3U;
    rf->gain = 2;
}

const uint8_t* rfregs(void) {
    return fmtx_preset;
}

uint16_t rfused(const Rf* rf) {
    return (rf->head - rf->tail) & (RINGSZ - 1U);
}

void rfhold(Rf* rf, uint8_t decisions) {
    if(rf->on || decisions == 0) return;
    rf->sample_decisions = decisions;
    rf->sphase = 0;
}

static int16_t rfpop(Rf* rf) {
    uint16_t t;
    if(!rf->prime) {
        uint16_t threshold = rf->drain ? 1U : 512U;
        if(rfused(rf) < threshold) return 0;
        rf->prime = true;
    }
    t = rf->tail;
    if(t == rf->head) {
        rf->prime = false;
        return 0;
    }
    int32_t x = rf->ring[t] * rf->gain / 2;
    if(x > 32767) x = 32767;
    if(x < -32768) x = -32768;
    __DMB();
    rf->tail = (t + 1U) & (RINGSZ - 1U);
    if(rf->played_samples != UINT32_MAX) rf->played_samples++;
    return x;
}

void rfgain(Rf* rf, uint8_t gain) {
    if(!rf) return;
    rf->gain = gain;
    __DMB();
}

static LevelDuration rfbit(void* ctx) {
    Rf* rf = ctx;
    uint32_t us;
    if(rf->lock_decisions) {
        rf->lock_decisions--;
        rf->s = 0;
    } else if(rf->sphase == 0) {
        if(rf->drain && rf->tail == rf->head) return level_duration_reset();
        rf->s = rfpop(rf);
    }
    rf->sphase++;
    if(rf->sphase == rf->sample_decisions) rf->sphase = 0;
    // One PDM bit selects one of the two FSK deviations.
    rf->err += rf->s;
    rf->bit = rf->err >= 0;
    rf->err += rf->bit ? -32767 : 32768;
    // Five decisions use 21 us, then one uses 20 us.
    rf->slot++;
    if(rf->slot == 6U) {
        rf->slot = 0;
        us = 20U;
    } else {
        us = 21U;
    }
    return level_duration_make(rf->bit, us);
}

static void txled(bool on) {
    if(on) {
        furi_hal_light_set(LightBlue, 0);
        furi_hal_light_set(LightGreen, 96);
        furi_hal_light_set(LightRed, 255);
    } else
        furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);
}

bool rfstart(Rf* rf) {
    if(rf->on) return true;
    if(!furi_hal_subghz_is_frequency_valid(rf->hz) ||
       !furi_hal_region_is_frequency_allowed(rf->hz))
        return false;
    furi_hal_power_insomnia_enter();
    rf->awake = true;
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(rf->regs);
    (void)furi_hal_subghz_set_frequency_and_path(rf->hz);
    furi_hal_gpio_init(furi_hal_subghz_get_data_gpio(), GpioModeInput, GpioPullNo, GpioSpeedLow);
    rf->drain = false;
    // Start with 100 ms of zero samples while the carrier settles.
    rf->lock_decisions = 4800U;
    rf->on = furi_hal_subghz_start_async_tx(rfbit, rf);
    if(!rf->on) rfstop(rf);
    if(rf->on) txled(true);

    return rf->on;
}

bool rfresume(Rf* rf) {
    if(rf->on) return true;
    if(!rf->awake) return false;
    furi_hal_subghz_idle();
    (void)furi_hal_subghz_set_frequency_and_path(rf->hz);
    furi_hal_gpio_init(furi_hal_subghz_get_data_gpio(), GpioModeInput, GpioPullNo, GpioSpeedLow);
    rf->drain = false;
    rf->on = furi_hal_subghz_start_async_tx(rfbit, rf);
    txled(rf->on);

    return rf->on;
}

void rfpause(Rf* rf) {
    if(rf->on) furi_hal_subghz_stop_async_tx();
    rf->on = false;
    furi_hal_subghz_idle();
    txled(false);
}

void rfstop(Rf* rf) {
    if(rf->on) furi_hal_subghz_stop_async_tx();
    rf->on = false;
    furi_hal_subghz_idle();
    furi_hal_gpio_init(furi_hal_subghz_get_data_gpio(), GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_sleep();
    txled(false);
    if(rf->awake) furi_hal_power_insomnia_exit();
    rf->awake = false;
}

bool rfput(Rf* rf, int16_t s) {
    // The async TX callback is the only reader.
    while(rf && rf->on) {
        uint16_t h = rf->head;
        uint16_t n = (h + 1U) & (RINGSZ - 1U);
        if(n != rf->tail) {
            rf->ring[h] = s;
            __DMB();
            rf->head = n;
            return true;
        }
        furi_delay_tick(1U);
    }
    return false;
}

void rfend(Rf* rf) {
    __DMB();
    rf->drain = true;
}

bool rfdone(const Rf* rf) {
    return rf->drain && furi_hal_subghz_is_async_tx_complete();
}

bool rfdrain(Rf* rf, uint32_t timeout_ms) {
    uint32_t at;
    if(!rf || !rf->on) return false;
    if(rfused(rf)) rf->prime = true;
    rfend(rf);
    at = furi_get_tick();
    while(!rfdone(rf) && furi_get_tick() - at < furi_ms_to_ticks(timeout_ms))
        furi_delay_tick(1U);
    return rfdone(rf);
}

uint32_t rfplayed(const Rf* rf) {
    if(!rf) return 0;
    __DMB();
    return rf->played_samples;
}

void rfrst(Rf* rf) {
    if(rf->on) return;
    rf->head = 0;
    rf->tail = 0;
    rf->prime = false;
    rf->drain = false;
    rf->s = 0;
    rf->lock_decisions = 0;
    rf->played_samples = 0;
    rf->sphase = 0;
    rf->err = 0;
    rf->slot = 0;
    rf->bit = false;
}
