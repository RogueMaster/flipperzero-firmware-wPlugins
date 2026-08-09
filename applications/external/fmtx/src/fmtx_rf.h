#ifndef yo3gnd_rf_08a1
#define yo3gnd_rf_08a1

#include <stdbool.h>
#include <stdint.h>

#define RINGSZ 4096U

typedef struct {
    uint32_t hz;
    const uint8_t* regs;
    volatile uint16_t head;
    volatile uint16_t tail;
    int16_t ring[RINGSZ];
    volatile bool on;
    volatile bool prime;
    volatile bool drain;
    uint16_t lock_decisions;
    volatile uint32_t played_samples;
    int16_t s;
    int32_t err;
    uint8_t sphase;
    uint8_t sample_decisions;
    uint8_t slot;
    volatile uint8_t gain;
    bool bit;
    bool awake;
} Rf;

void rfinit(Rf* rf, uint32_t hz);
const uint8_t* rfregs(void);
bool rfstart(Rf* rf);
bool rfresume(Rf* rf);
void rfpause(Rf* rf);
void rfstop(Rf* rf);
bool rfput(Rf* rf, int16_t s);
uint16_t rfused(const Rf* rf);
void rfhold(Rf* rf, uint8_t decisions);
void rfgain(Rf* rf, uint8_t gain);
void rfend(Rf* rf);
bool rfdone(const Rf* rf);
bool rfdrain(Rf* rf, uint32_t timeout_ms);
uint32_t rfplayed(const Rf* rf);
void rfrst(Rf* rf);

#endif
