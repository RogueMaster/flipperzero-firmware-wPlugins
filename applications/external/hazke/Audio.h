#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <math.h>
#include <stdint.h>

// Piezo sound effects and vibration, with the same call names the shared
// game logic uses on the Cardputer.
//
// Each effect is a short list of steps. A step glides its pitch from f0 to
// f1 over `ms`; a noise step jumps to a random pitch under that glide every
// tick, which is as close to a hiss as a piezo gets. A sound thread plays
// the steps every 10 ms, so the game loop only has to name an effect. The
// speaker belongs to the thread that acquired it, so that same thread
// acquires, drives and releases it.

namespace Audio {

struct Step {
    uint16_t f0, f1, ms;
    uint8_t noise;
};
struct Fx {
    const Step* steps;
    uint8_t n;
    uint8_t prio;
    uint16_t vibroMs;
};

constexpr uint32_t TickMs = 10;
constexpr float Volume = 0.6f;

// FX setting: what the game is allowed to use.
enum Mode : uint8_t {
    All = 0,
    SoundOnly,
    VibroOnly,
    Off,
    ModeCount
};
inline uint8_t mode = All;
inline bool soundOn() {
    return mode == All || mode == SoundOnly;
}
inline bool vibroOn() {
    return mode == All || mode == VibroOnly;
}
inline const char* modeName() {
    static const char* const names[ModeCount] = {"ALL", "SOUND", "VIBRO", "OFF"};
    return names[mode < ModeCount ? mode : 0];
}

inline FuriThread* thread = nullptr;
inline volatile bool running = false;
inline bool owned = false;

// Written by the game thread, read by the sound thread. Pointer stores are
// atomic on the Cortex-M4, and `pendingSeq` tells the sound thread something new
// arrived.
inline const Fx* volatile pending = nullptr;
inline volatile uint32_t pendingSeq = 0;
inline volatile uint16_t pendingVibro = 0;

// Sound-thread state.
inline const Fx* cur = nullptr;
inline uint8_t stepIdx = 0;
inline uint16_t stepT = 0;
inline uint32_t seenSeq = 0;
inline uint16_t vibroLeft = 0;
inline bool toneOn = false;
inline float toneHz = 0.0f;
inline uint32_t rng = 0x1234567u;

inline void silence() {
    if(owned && toneOn) furi_hal_speaker_stop();
    toneOn = false;
}

inline void tick() {
    if(seenSeq != pendingSeq) {
        seenSeq = pendingSeq;
        const Fx* p = pending;
        if(p && (!cur || p->prio >= cur->prio)) {
            cur = p;
            stepIdx = 0;
            stepT = 0;
        }
        uint16_t v = pendingVibro;
        if(v > vibroLeft) vibroLeft = v;
    }
    if(vibroLeft > 0) {
        furi_hal_vibro_on(true);
        vibroLeft = (vibroLeft > TickMs) ? (uint16_t)(vibroLeft - TickMs) : 0;
        if(vibroLeft == 0) furi_hal_vibro_on(false);
    }
    if(!cur) {
        silence();
        return;
    }
    const Step& s = cur->steps[stepIdx];
    float t = s.ms ? (float)stepT / (float)s.ms : 1.0f;
    float f = (float)s.f0 + ((float)s.f1 - (float)s.f0) * t;
    if(s.noise) {
        rng = rng * 1664525u + 1013904223u;
        f = f * (0.35f + 0.65f * (float)(rng >> 16) / 65535.0f);
    }
    if(f < 20.0f || !owned) {
        silence();
    } else if(!toneOn || fabsf(f - toneHz) > 1.0f) {
        // Only reprogram the speaker when the pitch moves.
        furi_hal_speaker_start(f, Volume);
        toneOn = true;
        toneHz = f;
    }
    stepT = (uint16_t)(stepT + TickMs);
    if(stepT >= s.ms) {
        stepT = 0;
        if(++stepIdx >= cur->n) {
            cur = nullptr;
            silence();
        }
    }
}

inline int32_t threadMain(void* ctx) {
    (void)ctx;
    owned = furi_hal_speaker_acquire(500);
    while(running) {
        tick();
        furi_delay_ms(TickMs);
    }
    furi_hal_vibro_on(false);
    if(owned) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        owned = false;
    }
    return 0;
}

inline void begin() {
    running = true;
    thread = furi_thread_alloc_ex("HazkeSound", 1024, threadMain, nullptr);
    furi_thread_start(thread);
}

inline void end() {
    if(!thread) return;
    running = false;
    furi_thread_join(thread);
    furi_thread_free(thread);
    thread = nullptr;
}

inline void play(const Fx& fx) {
    bool snd = soundOn(), vib = vibroOn() && fx.vibroMs > 0;
    if(!snd && !vib) return;
    pending = snd ? &fx : nullptr;
    pendingVibro = vib ? fx.vibroMs : 0;
    pendingSeq = pendingSeq + 1;
}

#define HZ_FX(name, prio, vib, ...)                                             \
    inline const Step name##Steps[] = {__VA_ARGS__};                            \
    inline const Fx name##Fx = {                                                \
        name##Steps, (uint8_t)(sizeof(name##Steps) / sizeof(Step)), prio, vib}; \
    inline void name() {                                                        \
        play(name##Fx);                                                         \
    }

// UI
HZ_FX(uiMove, 1, 0, {1800, 1800, 10, 0})
HZ_FX(uiSelect, 2, 0, {1200, 1200, 20, 0}, {1800, 1800, 30, 0})
HZ_FX(uiBack, 2, 0, {1400, 900, 40, 0})
HZ_FX(deny, 3, 0, {220, 220, 60, 0}, {0, 0, 30, 0}, {220, 220, 80, 0})
HZ_FX(cash, 2, 0, {2000, 2000, 20, 0}, {2600, 2600, 30, 0})
HZ_FX(throttleStep, 2, 0, {700, 900, 30, 0})
// Flight
HZ_FX(hitTarget, 4, 0, {600, 300, 40, 0})
HZ_FX(explosion, 7, 0, {900, 100, 350, 1})
HZ_FX(hostileAlert, 5, 0, {880, 880, 80, 0}, {0, 0, 40, 0}, {1320, 1320, 80, 0})
HZ_FX(hullHit, 6, 70, {180, 120, 120, 1})
HZ_FX(shieldHit, 5, 30, {500, 250, 60, 0})
HZ_FX(shieldDown, 6, 0, {800, 200, 250, 0})
HZ_FX(missileLaunch, 5, 0, {300, 1200, 200, 1})
HZ_FX(ecmBurst, 6, 0, {4000, 500, 250, 1})
HZ_FX(
    rankPromote,
    6,
    0,
    {1047, 1047, 100, 0},
    {1319, 1319, 100, 0},
    {1568, 1568, 100, 0},
    {2093, 2093, 200, 0})
HZ_FX(zoneOut, 3, 0, {800, 400, 150, 0})
HZ_FX(zoneIn, 3, 0, {400, 800, 150, 0})
HZ_FX(alarm, 4, 0, {1500, 1500, 80, 0}, {1000, 1000, 80, 0})
HZ_FX(collisionThump, 5, 90, {150, 80, 100, 1})
HZ_FX(incomingBeep, 4, 0, {2400, 2400, 40, 0})
HZ_FX(hullCritical, 4, 0, {600, 600, 150, 0}, {450, 450, 150, 0})
HZ_FX(lockOn, 3, 0, {1600, 1600, 40, 0}, {0, 0, 20, 0}, {2400, 2400, 60, 0})
HZ_FX(lockFail, 3, 0, {300, 300, 120, 0})
HZ_FX(dockChime, 5, 0, {784, 784, 100, 0}, {1047, 1047, 100, 0}, {1568, 1568, 180, 0})
HZ_FX(warpWhoosh, 6, 0, {200, 3000, 600, 1})
HZ_FX(launchRoar, 5, 0, {120, 400, 400, 1})
HZ_FX(playerDeath, 8, 400, {1200, 60, 900, 1})
HZ_FX(missionAccept, 5, 0, {880, 880, 80, 0}, {1175, 1175, 120, 0})
HZ_FX(
    missionComplete,
    6,
    60,
    {1047, 1047, 90, 0},
    {1319, 1319, 90, 0},
    {1568, 1568, 90, 0},
    {2093, 2093, 90, 0},
    {2637, 2637, 220, 0})

#undef HZ_FX

inline void laserZap(uint8_t tier) {
    static const Step s0[] = {{2600, 900, 70, 0}};
    static const Step s1[] = {{3000, 700, 80, 0}};
    static const Step s2[] = {{3600, 1200, 50, 0}, {3600, 1200, 50, 0}};
    static const Fx f[3] = {{s0, 1, 3, 0}, {s1, 1, 3, 0}, {s2, 2, 3, 0}};
    play(f[tier < 3 ? tier : 2]);
}

} // namespace Audio
