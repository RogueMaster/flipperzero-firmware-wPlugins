#include "fmtx_rf.h"

#include <string.h>
#include <furi_hal.h>
#include <lib/drivers/cc1101_regs.h>

static const uint8_t fmtx_preset[] =
{
    CC1101_IOCFG0, 0x0D,
    CC1101_FSCTRL1, 0x06,
    CC1101_PKTCTRL0, 0x32,
    CC1101_PKTCTRL1, 0x04,
    CC1101_MDMCFG0, 0x00,
    CC1101_MDMCFG1, 0x02,
    CC1101_MDMCFG2, 0x04,
    CC1101_MDMCFG3, 0xE4,
    CC1101_MDMCFG4, 0x6A,
    CC1101_DEVIATN, 0x04,
    CC1101_MCSM0, 0x18,
    CC1101_FOCCFG, 0x16,
    CC1101_AGCCTRL0, 0x91,
    CC1101_AGCCTRL1, 0x00,
    CC1101_AGCCTRL2, 0x07,
    CC1101_WORCTRL, 0xFB,
    CC1101_FREND0, 0x10,
    CC1101_FREND1, 0x56,
    0x00, 0x00,
    0xC0, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

void rfinit(Rf *rf, uint32_t hz)
{
    memset(rf, 0, sizeof(*rf));
    rf->hz = hz;
    rf->regs = fmtx_preset;
    rf->sample_decisions = 3U;
}

const uint8_t *rfregs(void)
{
    return fmtx_preset;
}

uint16_t rfused(const Rf *rf)
{
    return (rf->head - rf->tail) & (RINGSZ - 1U);
}

void rfhold(Rf *rf, uint8_t decisions)
{
    if(rf->on || decisions == 0) return;
    rf->sample_decisions = decisions;
    rf->sphase = 0;
}

static int16_t rfpop(Rf *rf)
{
    uint16_t t;
    if(!rf->prime)
    {
        if(rfused(rf) < 128U) return 0;
        rf->prime = true;
    }
    t = rf->tail;
    if(t == rf->head)
    {
        rf->prime = false;
        return 0;
    }
    int16_t s = rf->ring[t];
    __DMB();
    rf->tail = (t + 1U) & (RINGSZ - 1U);
    return s;
}

static LevelDuration rfbit(void *ctx)
{
    Rf *rf = ctx;
    uint32_t us;
    if(rf->sphase == 0)
    {
        if(rf->drain && rf->tail == rf->head) return level_duration_reset();
        rf->s = rfpop(rf);
    }
    rf->sphase++;
    if(rf->sphase == rf->sample_decisions) rf->sphase = 0;
    rf->err += rf->s;
    rf->bit = rf->err >= 0;
    rf->err += rf->bit ? -32767 : 32768;
    rf->slot++;
    if(rf->slot == 6U)
    {
        rf->slot = 0;
        us = 20U;
    }
    else
    {
        us = 21U;
    }
    return level_duration_make(rf->bit, us);
}

static void txled(bool on)
{
    if(on)
    {
        furi_hal_light_set(LightBlue, 0);
        furi_hal_light_set(LightGreen, 96);
        furi_hal_light_set(LightRed, 255);
    }
    else furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);
}

bool rfstart(Rf *rf)
{
    if(rf->on) return true;
    if(!furi_hal_subghz_is_frequency_valid(rf->hz) || !furi_hal_region_is_frequency_allowed(rf->hz)) return false;
    furi_hal_power_insomnia_enter();
    rf->awake = true;
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(rf->regs);
    (void)furi_hal_subghz_set_frequency_and_path(rf->hz);
    furi_hal_gpio_init(furi_hal_subghz_get_data_gpio(), GpioModeInput, GpioPullNo, GpioSpeedLow);
    rf->drain = false;
    rf->on = furi_hal_subghz_start_async_tx(rfbit, rf);
    if(!rf->on) rfstop(rf);
    if(rf->on) txled(true);


    return rf->on;
}

bool rfresume(Rf *rf)
{
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

void rfpause(Rf *rf)
{
    if(rf->on) furi_hal_subghz_stop_async_tx();
    rf->on = false;
    furi_hal_subghz_idle();
    txled(false);
}

void rfstop(Rf *rf)
{
    if(rf->on) furi_hal_subghz_stop_async_tx();
    rf->on = false;
    furi_hal_subghz_idle();
    furi_hal_gpio_init(furi_hal_subghz_get_data_gpio(), GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_sleep();
    txled(false);
    if(rf->awake) furi_hal_power_insomnia_exit();
    rf->awake = false;
}

bool rfput(Rf *rf, int16_t s)
{
    uint16_t h = rf->head;
    uint16_t n = (h + 1U) & (RINGSZ - 1U);
    if(n == rf->tail) return false;
    rf->ring[h] = s;
    __DMB();
    rf->head = n;
    return true;
}

void rfend(Rf *rf)
{
    __DMB();
    rf->drain = true;
}

bool rfdone(const Rf *rf)
{
    return rf->drain && furi_hal_subghz_is_async_tx_complete();
}

void rfrst(Rf *rf)
{
    if(rf->on) return;
    rf->head = 0;
    rf->tail = 0;
    rf->prime = false;
    rf->drain = false;
    rf->s = 0;
    rf->sphase = 0;
    rf->err = 0;
    rf->slot = 0;
    rf->bit = false;
}
