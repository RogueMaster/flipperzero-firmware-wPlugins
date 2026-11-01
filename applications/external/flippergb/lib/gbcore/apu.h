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
    /* Called once per emulated instruction; the fast path (powered off, or
     * no sequencer step due) must stay inline and branch-cheap. One
     * frame-sequencer step every 2048 M-cycles = 8192 T-cycles = 512 Hz. */
    void tick(uint cycles) {
        if(!power) return;
        seq_counter += cycles;
        if(seq_counter >= 2048) seq_run();
    }

    /* 0xFF10 - 0xFF3F (sound registers + wave RAM) */
    auto read(u16 addr) const -> u8;
    void write(u16 addr, u8 value);

    /* n: 0 = pulse 1, 1 = pulse 2, 2 = wave, 3 = noise */
    void get_voice(uint n, ApuVoice* out) const;

    /* save-state support: exact raw state (masked IO reads lose
     * write-only bits like the frequency-low registers) */
    struct State {
        u8 regs[20]; /* ch[4] x nr0..nr4 */
        u8 nr50, nr51, power;
        u8 wave[16];
        u8 enabled_mask;
        u8 env_vol[4], env_tim[4];
        u16 length[4];
        u32 order[4];
        u32 sweep_shadow;
        u8 sweep_timer, sweep_en;
        u32 seq_counter;
        u8 seq_step;
        u32 trig_counter;
    };
    void export_state(State* s) const {
        for(uint n = 0; n < 4; n++) {
            s->regs[n * 5 + 0] = ch[n].nr0;
            s->regs[n * 5 + 1] = ch[n].nr1;
            s->regs[n * 5 + 2] = ch[n].nr2;
            s->regs[n * 5 + 3] = ch[n].nr3;
            s->regs[n * 5 + 4] = ch[n].nr4;
            s->env_vol[n] = ch[n].env_volume;
            s->env_tim[n] = ch[n].env_timer;
            s->length[n] = (u16)ch[n].length;
            s->order[n] = ch[n].order;
            if(ch[n].enabled) s->enabled_mask |= (u8)(1 << n);
        }
        s->nr50 = nr50;
        s->nr51 = nr51;
        s->power = power;
        for(uint i = 0; i < 16; i++)
            s->wave[i] = wave_ram[i];
        s->sweep_shadow = sweep_shadow;
        s->sweep_timer = sweep_timer;
        s->sweep_en = sweep_enabled;
        s->seq_counter = seq_counter;
        s->seq_step = seq_step;
        s->trig_counter = trigger_counter;
    }
    void import_state(const State* s) {
        for(uint n = 0; n < 4; n++) {
            ch[n].nr0 = s->regs[n * 5 + 0];
            ch[n].nr1 = s->regs[n * 5 + 1];
            ch[n].nr2 = s->regs[n * 5 + 2];
            ch[n].nr3 = s->regs[n * 5 + 3];
            ch[n].nr4 = s->regs[n * 5 + 4];
            ch[n].env_volume = s->env_vol[n];
            ch[n].env_timer = s->env_tim[n];
            ch[n].length = s->length[n];
            ch[n].order = s->order[n];
            ch[n].enabled = (s->enabled_mask >> n) & 1;
        }
        nr50 = s->nr50;
        nr51 = s->nr51;
        power = s->power != 0;
        for(uint i = 0; i < 16; i++)
            wave_ram[i] = s->wave[i];
        sweep_shadow = s->sweep_shadow;
        sweep_timer = s->sweep_timer;
        sweep_enabled = s->sweep_en != 0;
        seq_counter = s->seq_counter;
        seq_step = s->seq_step;
        trigger_counter = s->trig_counter;
    }

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
    void seq_run();
    void clock_lengths();
    void clock_envelopes();
    void clock_sweep();
    auto sweep_calc() const -> uint;
    void power_off();
};
