#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t byte;
typedef uint16_t word;

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*((const uint8_t*)(addr)))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*((const uint16_t*)(addr)))
#endif

extern byte trackCount;
extern byte tickRate;
extern const word* trackList;
extern const byte* trackBase;

extern uint8_t pcm;

extern uint16_t cia;
extern uint16_t cia_count;

typedef struct {
    uint8_t vol;
    uint16_t freq;
    uint16_t phase;
} osc_t;

typedef osc_t Oscillator;

extern osc_t osc[4];

void ATM_playroutine(void);

void atm_system_init(void);
void atm_system_deinit(void);
void atm_set_enabled(uint8_t en);
void atm_get_channel_levels(uint8_t out_levels[4]);

class ATMsynth {
public:
    ATMsynth() {}

    static void play(const byte* song);
    static void playPause();
    static void stop();
    static void muteChannel(byte ch);
    static void unMuteChannel(byte ch);

    static void systemInit();
    static void systemDeinit();
    static void setEnabled(bool en);
    static void setMasterVolume(float v);
    static void setUniformToneMode(bool en);
};

extern ATMsynth ATM;

#ifdef __cplusplus
}
#endif
