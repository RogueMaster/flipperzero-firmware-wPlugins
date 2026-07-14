#include "minesweeper.h"
#include "helpers/mine_sweeper_haptic.h"

static const NotificationSequence sequence_minesweeper_haptic_short = {
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_minesweeper_haptic_double_short = {
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    &message_delay_25,
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_minesweeper_haptic_lose = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_250,
    &message_delay_100,
    &message_delay_50,
    NULL,
};

static const NotificationSequence sequence_minesweeper_haptic_win = {
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_100,
    NULL,
};

static inline MineSweeperApp* mine_sweeper_haptic_get_app(void* context) {
    furi_assert(context);

    MineSweeperApp* app = context;
    furi_assert(app->notification);

    return app;
}

static inline void
    mine_sweeper_haptic_play_if_enabled(MineSweeperApp* app, const NotificationSequence* sequence) {
    furi_assert(app);
    furi_assert(sequence);
    furi_assert(app->notification);

    if(!app->feedback_enabled) {
        return;
    }

    notification_message(app->notification, sequence);
}

void mine_sweeper_play_haptic_short(void* context) {
    MineSweeperApp* app = mine_sweeper_haptic_get_app(context);
    mine_sweeper_haptic_play_if_enabled(app, &sequence_minesweeper_haptic_short);
}

void mine_sweeper_play_haptic_double_short(void* context) {
    MineSweeperApp* app = mine_sweeper_haptic_get_app(context);
    mine_sweeper_haptic_play_if_enabled(app, &sequence_minesweeper_haptic_double_short);
}

void mine_sweeper_play_haptic_lose(void* context) {
    MineSweeperApp* app = mine_sweeper_haptic_get_app(context);
    mine_sweeper_haptic_play_if_enabled(app, &sequence_minesweeper_haptic_lose);
}

void mine_sweeper_play_haptic_win(void* context) {
    MineSweeperApp* app = mine_sweeper_haptic_get_app(context);
    mine_sweeper_haptic_play_if_enabled(app, &sequence_minesweeper_haptic_win);
}
