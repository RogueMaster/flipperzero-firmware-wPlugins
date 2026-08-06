#ifndef yo3gnd_12093fpoe
#define yo3gnd_12093fpoe

#include <stdint.h>

typedef struct {
    uint32_t rp;
    uint32_t cp;
    uint32_t ri;
    uint32_t ci;
    uint16_t tone;
    uint16_t gap;
    uint8_t key;
} Dtmf;

int16_t u8pcm(uint8_t s);
void dtmfinit(Dtmf *dtmf);
void dtmfpick(Dtmf *dtmf);
int16_t dtmfnext(Dtmf *dtmf);

#endif
