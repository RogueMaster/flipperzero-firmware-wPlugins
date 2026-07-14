#pragma once

#include "definitions.h"

/* Register-level APU (no waveform synthesis).
 *
 * The Flipper Zero speaker is a single-tone piezo: it can play exactly one
 * frequency at one volume at a time. Synthesizing the real 4-channel GB mix
 * would be wasted CPU (there is no DAC to play it on), so this APU only
 * models what games actually program into the sound registers:
 *
 *  - channel frequencies (including the CH1 frequency sweep)
 *  - volume envelopes (64 Hz), length counters (256 Hz), sweep (128 Hz)
 *  - trigger / DAC-enable / NR52 power semantics and register read-back
 *    masks (some games poll NR52 channel-status bits)
 *
 * The frontend queries the per-channel state once per frame and decides
 * which voice to send to the piezo. Cost per emulated instruction: one
 * counter add + compare. Extra RAM: ~120 bytes.
 */

struct ApuVoice {
    bool active; /* audible now: triggered, DAC on, length alive, routed */
    u32 freq_hz; /* square/wave: tone frequency. noise: LFSR clock rate */
    u8 volume; /* current volume 0..15 (wave level mapped to 0/3/7/15) */
    u32 order; /* trigger recency; higher = more recently triggered */
};

class Apu {
public:
    void tick(uint cycles);

    /* 0xFF10 - 0xFF3F (sound registers + wave RAM) */
    auto read(u16 addr) const -> u8;
    void write(u16 addr, u8 value);

    /* n: 0 = pulse 1, 1 = pulse 2, 2 = wave, 3 = noise */
    void get_voice(uint n, ApuVoice* out) const;

    /* Master volume 0..7 (louder of the two NR50 output terminals) */
    auto master_volume() const -> u8 {
        u8 l = (nr50 >> 4) & 7;
        u8 r = nr50 & 7;
        return l > r ? l : r;
    }

private:
    struct Ch {
        u8 nr0 = 0, nr1 = 0, nr2 = 0, nr3 = 0, nr4 = 0;
        bool enabled = false;
        uint length = 0;
        u8 env_volume = 0;
        u8 env_timer = 0;
        u32 order = 0;
    };
    Ch ch[4]; /* 0 = pulse1, 1 = pulse2, 2 = wave, 3 = noise */

    /* channel 1 sweep unit */
    uint sweep_shadow = 0;
    u8 sweep_timer = 0;
    bool sweep_enabled = false;

    u8 nr50 = 0, nr51 = 0;
    bool power = false;
    u8 wave_ram[16] = {};

    uint seq_counter = 0;
    u8 seq_step = 0;
    u32 trigger_counter = 0;

    auto dac_on(uint n) const -> bool {
        if(n == 2) return (ch[2].nr0 & 0x80) != 0;
        return (ch[n].nr2 & 0xF8) != 0;
    }
    auto ch_freq(uint n) const -> uint {
        return ((uint)(ch[n].nr4 & 0x07) << 8) | ch[n].nr3;
    }
    void set_ch_freq(uint n, uint f) {
        ch[n].nr3 = (u8)(f & 0xFF);
        ch[n].nr4 = (u8)((ch[n].nr4 & ~0x07) | ((f >> 8) & 0x07));
    }

    void trigger(uint n);
    void clock_lengths();
    void clock_envelopes();
    void clock_sweep();
    auto sweep_calc() const -> uint;
    void power_off();
};
