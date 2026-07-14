#include "minesweeper.h"
#include "helpers/mine_sweeper_speaker.h"

#include <notification/notification_messages.h>
#include <notification/notification_messages_notes.h>

// Short, system-aware tones that respect global audio settings
static const NotificationSequence sequence_minesweeper_ok = {
    &message_note_g7,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_minesweeper_flag = {
    &message_note_g4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_minesweeper_oob = {
    &message_note_f4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_minesweeper_wrap = {
    &message_note_f7,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_minesweeper_win = {
    &message_note_a4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_minesweeper_lose = {
    &message_note_c8,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static inline void
    speaker_play_if_enabled(MineSweeperApp* app, const NotificationSequence* sequence) {
    furi_assert(app);
    furi_assert(sequence);
    furi_assert(app->notification);

    if(!app->feedback_enabled) {
        return;
    }

    notification_message(app->notification, sequence);
}

void mine_sweeper_play_ok_sound(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    speaker_play_if_enabled(app, &sequence_minesweeper_ok);
}

void mine_sweeper_play_flag_sound(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    speaker_play_if_enabled(app, &sequence_minesweeper_flag);
}

void mine_sweeper_play_oob_sound(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    speaker_play_if_enabled(app, &sequence_minesweeper_oob);
}

void mine_sweeper_play_wrap_sound(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    speaker_play_if_enabled(app, &sequence_minesweeper_wrap);
}

void mine_sweeper_play_win_sound(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    speaker_play_if_enabled(app, &sequence_minesweeper_win);
}

void mine_sweeper_play_lose_sound(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    speaker_play_if_enabled(app, &sequence_minesweeper_lose);
}

void mine_sweeper_stop_all_sound(void* context) {
    furi_assert(context);
    MineSweeperApp* app = context;
    furi_assert(app->notification);

    notification_message(app->notification, &sequence_reset_sound);
}
