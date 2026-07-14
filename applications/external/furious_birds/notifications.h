#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <notification/notification_messages_notes.h>

const NotificationSequence notification_logo_mute = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    NULL,
};

const NotificationSequence* notification_logo[] = {
    &notification_logo_mute,
    &notification_logo_mute,
};

const NotificationSequence notification_red_start_sound = {
    &message_note_c5,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

const NotificationSequence notification_red_start_mute = {
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

const NotificationSequence* notification_red_start[] = {
    &notification_red_start_mute,
    &notification_red_start_sound,
};

const NotificationSequence notification_pig_caught_mute = {
    &message_red_0,
    &message_green_0,
    &message_blue_255,
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_blue_0,
    NULL,
};

const NotificationSequence notification_pig_caught_sound = {
    &message_note_e5,
    &message_red_0,
    &message_green_0,
    &message_blue_255,
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_blue_0,
    NULL,
};

const NotificationSequence* notification_pig_caught[] = {
    &notification_pig_caught_mute,
    &notification_pig_caught_sound,
};

const NotificationSequence notification_next_level_sound = {
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

const NotificationSequence notification_next_level_mute = {

    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_delay_100,

    &message_vibro_off,
    &message_red_0,
    &message_blue_255,
    &message_delay_100,

    &message_vibro_on,
    &message_red_255,
    &message_blue_0,
    &message_delay_100,

    &message_vibro_off,
    &message_red_0,
    &message_blue_255,
    &message_delay_100,

    &message_vibro_on,
    &message_red_255,
    &message_blue_0,
    &message_delay_100,

    &message_vibro_off,
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};

const NotificationSequence* notification_next_level[] = {
    &notification_next_level_mute,
    &notification_next_level_sound};

const NotificationSequence notification_lose_sound = {
    &message_note_g5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

const NotificationSequence notification_lose_mute = {

    &message_vibro_on,  &message_red_255,  &message_green_0,   &message_blue_0, &message_delay_100,

    &message_red_0,     &message_blue_255, &message_delay_100,

    &message_red_255,   &message_blue_0,   &message_delay_100,

    &message_red_0,     &message_blue_255, &message_delay_100,

    &message_red_255,   &message_blue_0,   &message_delay_100,

    &message_vibro_off, &message_red_0,    &message_green_0,   &message_blue_0, NULL,
};

const NotificationSequence* notification_lose_level[] = {
    &notification_lose_mute,
    &notification_lose_sound};
