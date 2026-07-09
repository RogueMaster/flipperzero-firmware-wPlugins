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

void pocketlab_sound_play(NotificationApp* notifications, bool enabled, PocketLabSoundId sound) {
    if(!enabled || notifications == NULL) {
        return;
    }

    switch(sound) {
    case PocketLabSoundClick:
        notification_message(notifications, &sequence_click);
        break;
    case PocketLabSoundCorrect:
        notification_message(notifications, &sequence_correct);
        break;
    case PocketLabSoundWrong:
        notification_message(notifications, &sequence_wrong);
        break;
    case PocketLabSoundReward:
        notification_message(notifications, &sequence_reward);
        break;
    case PocketLabSoundCoin:
        notification_message(notifications, &sequence_coin);
        break;
    case PocketLabSoundType:
        notification_message(notifications, &sequence_type);
        break;
    case PocketLabSoundGeiger:
        notification_message(notifications, &sequence_geiger);
        break;
    case PocketLabSoundSip:
        notification_message(notifications, &sequence_sip);
        break;
    case PocketLabSoundComplete:
        notification_message(notifications, &sequence_complete);
        break;
    case PocketLabSoundReset:
        notification_message(notifications, &sequence_reset);
        break;
    case PocketLabSoundThud:
        notification_message(notifications, &sequence_thud);
        break;
    case PocketLabSoundSplat:
        notification_message(notifications, &sequence_splat);
        break;
    case PocketLabSoundFly:
        notification_message(notifications, &sequence_fly);
        break;
    case PocketLabSoundGlitch:
        notification_message(notifications, &sequence_glitch);
        break;
    case PocketLabSoundGrow:
        notification_message(notifications, &sequence_grow);
        break;
    }
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
