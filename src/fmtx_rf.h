#ifndef yo3gnd_rf_08a1
#define yo3gnd_rf_08a1

#include <stdint.h>

typedef struct
{
    uint32_t hz;
    const uint8_t *regs;
} Rf;

void rfinit(Rf *rf, uint32_t hz);
const uint8_t *rfregs(void);

#endif
