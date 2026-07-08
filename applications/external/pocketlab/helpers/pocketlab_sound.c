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

// Reward fanfare: the LED blinks in time with each note of the melody.
static const NotificationSequence sequence_reward = {
    &message_vibro_on, &message_green_255, &message_note_c5,   &message_delay_50,
    &message_green_0,  &message_blue_255,  &message_note_e5,   &message_delay_50,
    &message_blue_0,   &message_green_255, &message_note_g5,   &message_delay_50,
    &message_green_0,  &message_blue_255,  &message_note_c6,   &message_delay_100,
    &message_blue_0,   &message_sound_off, &message_vibro_off, NULL,
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
    }
}
