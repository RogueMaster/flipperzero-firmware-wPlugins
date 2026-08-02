#include "fmtx_rf.h"

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
    CC1101_MDMCFG3, 0x83,
    CC1101_MDMCFG4, 0x67,
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
    rf->hz = hz;
    rf->regs = fmtx_preset;
}

const uint8_t *rfregs(void)
{
    return fmtx_preset;
}
