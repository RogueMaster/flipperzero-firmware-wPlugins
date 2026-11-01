/* Cold code: size-optimized on the embedded target. Every KB of binary
 * is a KB of heap the ROM page cache loses (the FAP loads into RAM), and
 * nothing in this file runs per-instruction. */
#if defined(__arm__) && defined(__GNUC__)
#pragma GCC optimize("Os")
#endif

#include "apu.h"

/* Slow path of Apu::tick (see apu.h): runs the due frame-sequencer steps */
void Apu::seq_run() {
    while(seq_counter >= 2048) {
        seq_counter -= 2048;
        seq_step = (u8)((seq_step + 1) & 7);

        if((seq_step & 1) == 0) clock_lengths(); /* 256 Hz */
        if(seq_step == 2 || seq_step == 6) clock_sweep(); /* 128 Hz */
        if(seq_step == 7) clock_envelopes(); /* 64 Hz */
    }
}

void Apu::clock_lengths() {
    for(uint n = 0; n < 4; n++) {
        Ch& c = ch[n];
        if((c.nr4 & 0x40) && c.length > 0) {
            c.length--;
            if(c.length == 0) c.enabled = false;
        }
    }
}

void Apu::clock_envelopes() {
    static const u8 env_channels[3] = {0, 1, 3}; /* wave has no envelope */
    for(uint i = 0; i < 3; i++) {
        Ch& c = ch[env_channels[i]];
        u8 period = c.nr2 & 0x07;
        if(!period || !c.enabled) continue;
        if(c.env_timer > 0) c.env_timer--;
        if(c.env_timer == 0) {
            c.env_timer = period;
            if(c.nr2 & 0x08) {
                if(c.env_volume < 15) c.env_volume++;
            } else {
                if(c.env_volume > 0) c.env_volume--;
            }
        }
    }
}

auto Apu::sweep_calc() const -> uint {
    uint delta = sweep_shadow >> (ch[0].nr0 & 0x07);
    return (ch[0].nr0 & 0x08) ? sweep_shadow - delta : sweep_shadow + delta;
}

void Apu::clock_sweep() {
    Ch& c = ch[0];
    if(!c.enabled || !sweep_enabled) return;

    if(sweep_timer > 0) sweep_timer--;
    if(sweep_timer != 0) return;

    u8 period = (c.nr0 >> 4) & 0x07;
    sweep_timer = period ? period : 8;
    if(!period) return;

    uint nf = sweep_calc();
    if(nf > 2047) {
        c.enabled = false;
    } else if(c.nr0 & 0x07) {
        sweep_shadow = nf;
        set_ch_freq(0, nf);
        if(sweep_calc() > 2047) c.enabled = false;
    }
}

void Apu::trigger(uint n) {
    Ch& c = ch[n];

    c.enabled = dac_on(n);
    if(c.length == 0) c.length = (n == 2) ? 256 : 64;
    c.env_volume = c.nr2 >> 4;
    u8 period = c.nr2 & 0x07;
    c.env_timer = period ? period : 8;
    c.order = ++trigger_counter;

    if(n == 0) {
        sweep_shadow = ch_freq(0);
        u8 sw_period = (c.nr0 >> 4) & 0x07;
        u8 sw_shift = c.nr0 & 0x07;
        sweep_timer = sw_period ? sw_period : 8;
        sweep_enabled = (sw_period != 0) || (sw_shift != 0);
        if(sw_shift && sweep_calc() > 2047) c.enabled = false;
    }
}

void Apu::power_off() {
    for(uint n = 0; n < 4; n++) {
        ch[n] = Ch(); /* wave RAM survives power-off, registers do not */
    }
    nr50 = 0;
    nr51 = 0;
    sweep_shadow = 0;
    sweep_timer = 0;
    sweep_enabled = false;
    seq_counter = 0;
    seq_step = 0;
}

void Apu::write(u16 addr, u8 value) {
    /* wave RAM is accessible regardless of power */
    if(addr >= 0xFF30 && addr <= 0xFF3F) {
        wave_ram[addr - 0xFF30] = value;
        return;
    }

    if(addr == 0xFF26) { /* NR52: only the power bit is writable */
        bool new_power = (value & 0x80) != 0;
        if(power && !new_power) power_off();
        if(!power && new_power) {
            seq_counter = 0;
            seq_step = 0;
        }
        power = new_power;
        return;
    }

    if(!power) return; /* all other registers are dead while powered off */

    if(addr >= 0xFF10 && addr <= 0xFF23) {
        uint idx = addr - 0xFF10;
        uint n = idx / 5; /* channel */
        Ch& c = ch[n];
        switch(idx % 5) {
        case 0: /* NRx0: CH1 sweep / CH3 DAC enable */
            c.nr0 = value;
            if(n == 2 && !dac_on(2)) c.enabled = false;
            break;
        case 1: /* NRx1: duty/length load */
            c.nr1 = value;
            c.length = (n == 2) ? 256u - value : 64u - (value & 0x3F);
            break;
        case 2: /* NRx2: envelope (CH3: output level) */
            c.nr2 = value;
            if(n != 2 && !dac_on(n)) c.enabled = false;
            break;
        case 3: /* NRx3: frequency low (CH4: polynomial counter) */
            c.nr3 = value;
            break;
        case 4: /* NRx4: frequency high / length enable / trigger */
            c.nr4 = value;
            if(value & 0x80) trigger(n);
            break;
        }
        return;
    }

    if(addr == 0xFF24) {
        nr50 = value;
        return;
    }
    if(addr == 0xFF25) {
        nr51 = value;
        return;
    }
    /* 0xFF27 - 0xFF2F: unmapped */
}

auto Apu::read(u16 addr) const -> u8 {
    if(addr >= 0xFF30 && addr <= 0xFF3F) return wave_ram[addr - 0xFF30];

    /* unused bits read back as 1 (hardware OR masks) */
    static const u8 masks[0x17] = {
        0x80, 0x3F, 0x00, 0xFF, 0xBF, /* NR10-NR14 */
        0xFF, 0x3F, 0x00, 0xFF, 0xBF, /* ----, NR21-NR24 */
        0x7F, 0xFF, 0x9F, 0xFF, 0xBF, /* NR30-NR34 */
        0xFF, 0xFF, 0x00, 0x00, 0xBF, /* ----, NR41-NR44 */
        0x00, 0x00, 0x70, /* NR50, NR51, NR52 */
    };

    if(addr >= 0xFF10 && addr <= 0xFF23) {
        uint idx = addr - 0xFF10;
        const Ch& c = ch[idx / 5];
        u8 raw;
        switch(idx % 5) {
        case 0: raw = c.nr0; break;
        case 1: raw = c.nr1; break;
        case 2: raw = c.nr2; break;
        case 3: raw = c.nr3; break;
        default: raw = c.nr4; break;
        }
        return raw | masks[idx];
    }

    switch(addr) {
    case 0xFF24:
        return nr50;
    case 0xFF25:
        return nr51;
    case 0xFF26: {
        u8 v = (u8)(power ? 0x80 : 0x00) | 0x70;
        for(uint n = 0; n < 4; n++)
            if(ch[n].enabled) v |= (u8)(1 << n);
        return v;
    }
    default:
        return 0xFF; /* 0xFF27 - 0xFF2F */
    }
}

void Apu::get_voice(uint n, ApuVoice* out) const {
    const Ch& c = ch[n];

    out->order = c.order;

    bool routed = ((nr51 >> n) & 1) || ((nr51 >> (n + 4)) & 1);

    u8 vol;
    if(n == 2) {
        /* wave output level: mute / 100% / 50% / 25% */
        switch((c.nr2 >> 5) & 3) {
        case 0: vol = 0; break;
        case 1: vol = 15; break;
        case 2: vol = 7; break;
        default: vol = 3; break;
        }
    } else {
        vol = c.env_volume;
    }
    out->volume = vol;
    out->active = power && c.enabled && dac_on(n) && routed && vol > 0;

    if(n == 3) {
        /* noise: LFSR clock rate (the frontend maps it to a percussive
         * buzz; a piezo cannot reproduce real noise) */
        u8 shift = c.nr3 >> 4;
        u8 r = c.nr3 & 0x07;
        u32 divisor = r ? ((u32)r << 4) : 8u;
        out->freq_hz = (524288u / divisor) >> (shift + 1);
    } else {
        uint x = ch_freq(n);
        out->freq_hz = ((n == 2) ? 65536u : 131072u) / (2048u - x);
    }
}
