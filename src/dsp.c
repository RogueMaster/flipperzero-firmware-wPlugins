#include "dsp.h"

#include <string.h>

#define TAU 6.283185307f

const uint32_t dsp_hz = 8000U;

void dspinit(Dsp* dsp) {
    float t;
    float rc;
    memset(dsp, 0, sizeof(*dsp));
    dsp->on = true;

    t = 1.0f / dsp_hz;
    rc = 1.0f / (TAU * 400U);
    dsp->ha = rc / (rc + t);
    rc = 1.0f / (TAU * 2400U);
    dsp->la = t / (rc + t);
}

void dsprst(Dsp* dsp) {
    dsp->x1 = 0.0f;
    dsp->x2 = 0.0f;
    dsp->h1 = 0.0f;
    dsp->h2 = 0.0f;
    dsp->l1 = 0.0f;
    dsp->l2 = 0.0f;
    dsp->comp = 0.0f;
    dsp->was = dsp->on;
}

bool dspon(const Dsp* dsp) {
    return dsp && dsp->on;
}

bool dsptoggle(Dsp* dsp) {
    if(!dsp) return false;
    dsp->on = !dsp->on;
    return dsp->on;
}

float comphard(Dsp* d, float x) {
    float a = x < 0.0f ? -x : x;
    float want;
    float g;

    // Follow peaks slowly, compress above 6000 at 8:1, then restore 2x gain.
    if(a > d->comp)
        d->comp = a;
    else
        d->comp += (a - d->comp) * 0.001f;

    if(d->comp <= 6000.0f) return x * 2.0f;
    want = 6000.0f + (d->comp - 6000.0f) / 8.0f;
    g = want / d->comp;

    return x * g * 2.0f;
}

int16_t dspsample(Dsp* dsp, int16_t s) {
    float in;
    int32_t out;
    if(!dsp) return s;
    if(dsp->on != dsp->was) dsprst(dsp);
    if(!dsp->on) return s;

    in = s;
    // Two high-pass and two low-pass stages sharpen the passband edges.
    dsp->h1 = dsp->ha * (dsp->h1 + in - dsp->x1);
    dsp->x1 = in;
    dsp->h2 = dsp->ha * (dsp->h2 + dsp->h1 - dsp->x2);
    dsp->x2 = dsp->h1;
    dsp->l1 += dsp->la * (dsp->h2 - dsp->l1);
    dsp->l2 += dsp->la * (dsp->l1 - dsp->l2);
    out = comphard(dsp, dsp->l2);
    if(out > 32767) out = 32767;
    if(out < -32768) out = -32768;
    return out;
}
