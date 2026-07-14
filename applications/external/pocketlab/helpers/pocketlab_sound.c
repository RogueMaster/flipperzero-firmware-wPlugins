#include "pocketlab_sound.h"

#include <notification/notification_messages.h>

static const NotificationSequence sequence_click = {
    &message_note_c5,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_correct = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_wrong = {
    &message_note_e4,
    &message_delay_100,
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

// Triumphant fanfare: two rising phrases climbing to a held high note, with the
// LED blinking and a vibro accent on the finale. "ta-da-da-da daa, ta-dada-da-daa".
static const NotificationSequence sequence_reward = {
    &message_vibro_on,
    &message_green_255,
    // Phrase 1: e5 e5 e5 c5 g5
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    &message_green_0,
    &message_blue_255,
    &message_note_g5,
    &message_delay_250,
    &message_sound_off,
    // Phrase 2: g5 c6 c6 e6 g6 (held)
    &message_blue_0,
    &message_green_255,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_note_c6,
    &message_delay_100,
    &message_sound_off,
    &message_note_c6,
    &message_delay_100,
    &message_sound_off,
    &message_green_0,
    &message_blue_255,
    &message_note_e6,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_on,
    &message_note_g6,
    &message_delay_500,
    &message_sound_off,
    &message_vibro_off,
    &message_blue_0,
    NULL,
};

// Short, bright two-note rise: a step is done. Lighter than the level-up fanfare.
static const NotificationSequence sequence_complete = {
    &message_green_255,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_100,
    &message_green_0,
    &message_sound_off,
    NULL,
};

// Descending "erased" sweep with a red blink: progress was reset.
static const NotificationSequence sequence_reset = {
    &message_red_255,
    &message_note_c6,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_100,
    &message_red_0,
    &message_sound_off,
    NULL,
};

// Crisp metallic tick, like flipping through coins.
static const NotificationSequence sequence_coin = {
    &message_note_c7,
    &message_delay_10,
    &message_note_g6,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

// Soft, quiet click, like the Flipper printing a character on a terminal.
static const NotificationMessage message_type_tick = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 3200.0f, .volume = 0.3f},
};
static const NotificationSequence sequence_type = {
    &message_type_tick,
    &message_delay_1,
    &message_sound_off,
    NULL,
};

// Faint, sharp blip, like a Geiger counter tick over the matrix rain.
static const NotificationMessage message_geiger_tick = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 5200.0f, .volume = 0.22f},
};
static const NotificationSequence sequence_geiger = {
    &message_geiger_tick,
    &message_delay_1,
    &message_sound_off,
    NULL,
};

// Soft, gentle "gulp" for the Pac-Man swallowing a coffee.
static const NotificationMessage message_sip_high = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 900.0f, .volume = 0.35f},
};
static const NotificationMessage message_sip_low = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 600.0f, .volume = 0.3f},
};
static const NotificationSequence sequence_sip = {
    &message_sip_high,
    &message_delay_10,
    &message_sip_low,
    &message_delay_10,
    &message_sound_off,
    NULL,
};

// Whack + tumble: the final mug knocks the Pac-Man off, descending with a vibro.
static const NotificationSequence sequence_thud = {
    &message_vibro_on,
    &message_note_a4,
    &message_delay_25,
    &message_note_e4,
    &message_delay_25,
    &message_note_c4,
    &message_delay_25,
    &message_note_a3,
    &message_delay_100,
    &message_vibro_off,
    &message_sound_off,
    NULL,
};

// Low, short "splat" when it hits the bottom.
static const NotificationSequence sequence_splat = {
    &message_note_c3,
    &message_delay_25,
    &message_note_a2,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

// Faint, short buzz; played repeatedly to sound like flies circling.
static const NotificationMessage message_fly = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 620.0f, .volume = 0.14f},
};
static const NotificationSequence sequence_fly = {
    &message_fly,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

// Rising arpeggio, Mario power-up style: the mug grows.
static const NotificationSequence sequence_grow = {
    &message_note_c5,   &message_delay_25,
    &message_note_e5,   &message_delay_25,
    &message_note_g5,   &message_delay_25,
    &message_note_c6,   &message_delay_25,
    &message_note_e5,   &message_delay_25,
    &message_note_g5,   &message_delay_25,
    &message_note_c6,   &message_delay_25,
    &message_note_e6,   &message_delay_25,
    &message_note_g5,   &message_delay_25,
    &message_note_c6,   &message_delay_25,
    &message_note_e6,   &message_delay_25,
    &message_note_g6,   &message_delay_25,
    &message_sound_off, NULL,
};

// Short digital blip for a text glitch.
static const NotificationMessage message_glitch = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 2000.0f, .volume = 0.25f},
};
static const NotificationSequence sequence_glitch = {
    &message_glitch,
    &message_delay_1,
    &message_sound_off,
    NULL,
};

// Global LED/vibro switches, set from Settings. Sound is passed per call.
static bool s_led = true;
static bool s_vibro = true;

void pocketlab_sound_configure_fx(bool led, bool vibro) {
    s_led = led;
    s_vibro = vibro;
}

static const NotificationSequence* pocketlab_sequence_for(PocketLabSoundId sound) {
    switch(sound) {
    case PocketLabSoundClick:
        return &sequence_click;
    case PocketLabSoundCorrect:
        return &sequence_correct;
    case PocketLabSoundWrong:
        return &sequence_wrong;
    case PocketLabSoundReward:
        return &sequence_reward;
    case PocketLabSoundCoin:
        return &sequence_coin;
    case PocketLabSoundType:
        return &sequence_type;
    case PocketLabSoundGeiger:
        return &sequence_geiger;
    case PocketLabSoundSip:
        return &sequence_sip;
    case PocketLabSoundComplete:
        return &sequence_complete;
    case PocketLabSoundReset:
        return &sequence_reset;
    case PocketLabSoundThud:
        return &sequence_thud;
    case PocketLabSoundSplat:
        return &sequence_splat;
    case PocketLabSoundFly:
        return &sequence_fly;
    case PocketLabSoundGlitch:
        return &sequence_glitch;
    case PocketLabSoundGrow:
        return &sequence_grow;
    }
    return NULL;
}

// Plays `seq`, dropping the sound / LED / vibro messages whose channel is off,
// so the three toggles work independently while timing (delays) is preserved.
static void pocketlab_play_filtered(
    NotificationApp* notifications,
    const NotificationSequence* seq,
    bool sound,
    bool led,
    bool vibro) {
    if(seq == NULL) return;
    if(sound && led && vibro) {
        notification_message(notifications, seq);
        return;
    }
    const NotificationMessage* buf[128];
    size_t k = 0;
    for(const NotificationMessage* const* p = *seq; *p != NULL; p++) {
        const NotificationMessageType t = (*p)->type;
        if(!sound && (t == NotificationMessageTypeSoundOn || t == NotificationMessageTypeSoundOff))
            continue;
        if(!led && (t == NotificationMessageTypeLedRed || t == NotificationMessageTypeLedGreen ||
                    t == NotificationMessageTypeLedBlue))
            continue;
        if(!vibro && t == NotificationMessageTypeVibro) continue;
        if(k < (sizeof(buf) / sizeof(buf[0])) - 1) buf[k++] = *p;
    }
    buf[k] = NULL;
    notification_message(notifications, (const NotificationSequence*)&buf);
}

void pocketlab_sound_play(NotificationApp* notifications, bool enabled, PocketLabSoundId sound) {
    if(notifications == NULL) return;
    // Nothing to emit if sound and both effect channels are off.
    if(!enabled && !s_led && !s_vibro) return;
    pocketlab_play_filtered(notifications, pocketlab_sequence_for(sound), enabled, s_led, s_vibro);
}

void pocketlab_sound_play_jingle(NotificationApp* notifications, bool enabled, uint32_t index) {
    if(!enabled || notifications == NULL) {
        return;
    }

    // Four notes from a C major pentatonic pool, chosen deterministically so
    // each badge gets its own little tune.
    static const NotificationMessage* const pool[6] = {
        &message_note_c5,
        &message_note_d5,
        &message_note_e5,
        &message_note_g5,
        &message_note_a5,
        &message_note_c6,
    };
    static const NotificationMessage* sequence[12];

    const uint32_t hash = index * 2654435761u + 2166136261u;
    size_t k = 0;
    for(uint8_t i = 0; i < 4; i++) {
        sequence[k++] = pool[(hash >> (i * 5)) % 6];
        sequence[k++] = &message_delay_50;
    }
    sequence[k++] = &message_sound_off;
    sequence[k++] = NULL;

    notification_message(notifications, (const NotificationSequence*)&sequence);
}
