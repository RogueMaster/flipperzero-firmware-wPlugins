#ifndef yo3gnd_dsp_24f1
#define yo3gnd_dsp_24f1

#include <stdbool.h>
#include <stdint.h>

extern const uint32_t dsp_hz;

typedef struct {
    volatile bool on;
    bool was;
    float x1;
    float x2;
    float h1;
    float h2;
    float l1;
    float l2;
    float ha;
    float la;
    float comp;
} Dsp;

void dspinit(Dsp* dsp);
void dsprst(Dsp* dsp);
bool dspon(const Dsp* dsp);
bool dsptoggle(Dsp* dsp);
float comphard(Dsp* d, float x);
int16_t dspsample(Dsp* dsp, int16_t s);

#endif
