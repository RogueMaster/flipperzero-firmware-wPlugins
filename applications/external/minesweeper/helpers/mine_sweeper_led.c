#include "minesweeper.h"
#include "helpers/mine_sweeper_led.h"

static inline MineSweeperApp* mine_sweeper_led_get_app(void* context) {
    furi_assert(context);

    MineSweeperApp* app = context;
    furi_assert(app->notification);

    return app;
}

static inline void
    mine_sweeper_led_play_sequence(MineSweeperApp* app, const NotificationSequence* sequence) {
    furi_assert(app);
    furi_assert(sequence);
    furi_assert(app->notification);

    notification_message(app->notification, sequence);
}

static inline bool mine_sweeper_led_is_on(int value) {
    return value > 0;
}

void mine_sweeper_led_set_rgb(void* context, int red, int green, int blue) {
    MineSweeperApp* app = mine_sweeper_led_get_app(context);

    // Keep LED updates sequence-based/safe by quantizing RGB channels to off/on states.
    const bool red_on = mine_sweeper_led_is_on(red);
    const bool green_on = mine_sweeper_led_is_on(green);
    const bool blue_on = mine_sweeper_led_is_on(blue);

    mine_sweeper_led_play_sequence(app, &sequence_reset_rgb);

    if(red_on) {
        mine_sweeper_led_play_sequence(app, &sequence_set_red_255);
    }
    if(green_on) {
        mine_sweeper_led_play_sequence(app, &sequence_set_green_255);
    }
    if(blue_on) {
        mine_sweeper_led_play_sequence(app, &sequence_set_blue_255);
    }
}

void mine_sweeper_led_blink_red(void* context) {
    MineSweeperApp* app = mine_sweeper_led_get_app(context);
    mine_sweeper_led_play_sequence(app, &sequence_blink_red_100);
}

void mine_sweeper_led_blink_yellow(void* context) {
    MineSweeperApp* app = mine_sweeper_led_get_app(context);
    mine_sweeper_led_play_sequence(app, &sequence_blink_yellow_100);
}

void mine_sweeper_led_blink_magenta(void* context) {
    MineSweeperApp* app = mine_sweeper_led_get_app(context);
    mine_sweeper_led_play_sequence(app, &sequence_blink_magenta_100);
}

void mine_sweeper_led_blink_cyan(void* context) {
    MineSweeperApp* app = mine_sweeper_led_get_app(context);
    mine_sweeper_led_play_sequence(app, &sequence_blink_cyan_100);
}

void mine_sweeper_led_reset(void* context) {
    MineSweeperApp* app = mine_sweeper_led_get_app(context);
    mine_sweeper_led_play_sequence(app, &sequence_reset_rgb);
}
