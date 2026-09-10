/*
 * BEEPBACK - LED colours.
 *
 * The stock notification sequences only cover a few fixed colours, so
 * the five button hues are defined here as custom messages. Each
 * sequence ends with message_do_not_reset, which leaves the LED lit
 * until the main loop decides the flash is over and sets BbLedOff.
 */
#include "beepback.h"

#define BB_LED_MSG(name, r, g, b)                                                        \
    static const NotificationMessage bb_msg_##name##_r = {                               \
        .type = NotificationMessageTypeLedRed,                                           \
        .data.led.value = (r)};                                                          \
    static const NotificationMessage bb_msg_##name##_g = {                               \
        .type = NotificationMessageTypeLedGreen,                                         \
        .data.led.value = (g)};                                                          \
    static const NotificationMessage bb_msg_##name##_b = {                               \
        .type = NotificationMessageTypeLedBlue,                                          \
        .data.led.value = (b)};                                                          \
    static const NotificationSequence bb_seq_##name = {                                  \
        &bb_msg_##name##_r,                                                              \
        &bb_msg_##name##_g,                                                              \
        &bb_msg_##name##_b,                                                              \
        &message_do_not_reset,                                                           \
        NULL};

/* The ladder follows pitch: low note, long wavelength. */
BB_LED_MSG(off, 0, 0, 0)
BB_LED_MSG(red, 255, 0, 0) /* DOWN  */
BB_LED_MSG(yellow, 255, 170, 0) /* LEFT  */
BB_LED_MSG(green, 0, 255, 0) /* OK    */
BB_LED_MSG(blue, 0, 80, 255) /* RIGHT */
BB_LED_MSG(violet, 160, 0, 255) /* UP    */

static const NotificationSequence* const bb_led_seq[BbLedCount] = {
    &bb_seq_off,
    &bb_seq_red,
    &bb_seq_yellow,
    &bb_seq_green,
    &bb_seq_blue,
    &bb_seq_violet,
};

void bb_led_apply(BeepbackApp* app, BbLedColor color) {
    if(color >= BbLedCount) color = BbLedOff;
    notification_message(app->notifications, bb_led_seq[color]);
}
