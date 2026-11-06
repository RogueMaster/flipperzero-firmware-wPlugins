#include <notification/notification_messages.h>

#include "feedback.h"
#include "feedback_play.h"

/* The notification service keeps the sequence pointer until it has played
 * it, so every combination is a static sequence. The service turns the
 * LED off when its sequence ends. */

#define WAKE        &message_display_backlight_on
#define LED_HOLD    &message_delay_100, &message_delay_50
#define VIBRO_SHORT &message_vibro_on, &message_delay_50, &message_vibro_off
#define VIBRO_LONG  &message_vibro_on, &message_delay_250, &message_delay_100, &message_vibro_off
/* the firmware's success and error notes (sequence_success, sequence_error) */
#define BEEPS_SUCCESS                                                                           \
    &message_note_c5, &message_delay_50, &message_note_e5, &message_delay_50, &message_note_g5, \
        &message_delay_50, &message_note_c6, &message_delay_50, &message_sound_off
#define BEEPS_ERROR                                                              \
    &message_note_c5, &message_delay_100, &message_sound_off, &message_delay_10, \
        &message_note_c5, &message_delay_100, &message_sound_off

static const NotificationSequence blue_short =
    {WAKE, &message_blue_255, VIBRO_SHORT, LED_HOLD, NULL};
static const NotificationSequence blue = {WAKE, &message_blue_255, LED_HOLD, NULL};
static const NotificationSequence green_short =
    {WAKE, &message_green_255, VIBRO_SHORT, LED_HOLD, NULL};
static const NotificationSequence green = {WAKE, &message_green_255, LED_HOLD, NULL};
static const NotificationSequence green_short_beeps =
    {WAKE, &message_green_255, VIBRO_SHORT, BEEPS_SUCCESS, NULL};
static const NotificationSequence green_beeps = {WAKE, &message_green_255, BEEPS_SUCCESS, NULL};
static const NotificationSequence beeps_success = {WAKE, BEEPS_SUCCESS, NULL};
static const NotificationSequence red_long = {WAKE, &message_red_255, VIBRO_LONG, LED_HOLD, NULL};
static const NotificationSequence red = {WAKE, &message_red_255, LED_HOLD, NULL};
static const NotificationSequence red_long_beeps =
    {WAKE, &message_red_255, VIBRO_LONG, BEEPS_ERROR, NULL};
static const NotificationSequence red_beeps = {WAKE, &message_red_255, BEEPS_ERROR, NULL};
static const NotificationSequence beeps_error = {WAKE, BEEPS_ERROR, NULL};

#define KEY(led, vibro, sound) ((led) * 9 + (vibro) * 3 + (sound))

static const NotificationSequence* const sequences[KEY(4, 0, 0)] = {
    [KEY(FeedbackLedBlue, FeedbackVibroShort, FeedbackSoundNone)] = &blue_short,
    [KEY(FeedbackLedBlue, FeedbackVibroNone, FeedbackSoundNone)] = &blue,
    [KEY(FeedbackLedGreen, FeedbackVibroShort, FeedbackSoundNone)] = &green_short,
    [KEY(FeedbackLedGreen, FeedbackVibroNone, FeedbackSoundNone)] = &green,
    [KEY(FeedbackLedGreen, FeedbackVibroShort, FeedbackSoundSuccess)] = &green_short_beeps,
    [KEY(FeedbackLedGreen, FeedbackVibroNone, FeedbackSoundSuccess)] = &green_beeps,
    [KEY(FeedbackLedNone, FeedbackVibroNone, FeedbackSoundSuccess)] = &beeps_success,
    [KEY(FeedbackLedRed, FeedbackVibroLong, FeedbackSoundNone)] = &red_long,
    [KEY(FeedbackLedRed, FeedbackVibroNone, FeedbackSoundNone)] = &red,
    [KEY(FeedbackLedRed, FeedbackVibroLong, FeedbackSoundError)] = &red_long_beeps,
    [KEY(FeedbackLedRed, FeedbackVibroNone, FeedbackSoundError)] = &red_beeps,
    [KEY(FeedbackLedNone, FeedbackVibroNone, FeedbackSoundError)] = &beeps_error,
};

void feedback_play(NotificationApp* notifications, AppCue cue, const AppSettings* settings) {
    FeedbackPlan p = feedback_plan(cue, settings);
    if(feedback_plan_empty(p)) return;
    const NotificationSequence* sequence = sequences[KEY(p.led, p.vibro, p.sound)];
    if(sequence) notification_message(notifications, sequence);
}
