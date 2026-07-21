#include "tone_player.h"

#include <furi.h>
#include <furi_hal_speaker.h>

#include "intervals.h"

#define GAP_MS 90 /* silence between the two notes */

typedef enum {
    PlayerMsgInterval,
    PlayerMsgNote,
    PlayerMsgQuit,
} PlayerMsgType;

typedef struct {
    PlayerMsgType type;
    uint8_t first;
    uint8_t second;
} PlayerMsg;

struct TonePlayer {
    FuriThread* thread;
    FuriMessageQueue* queue;
    NotificationApp* notifications;
    const EarSettings* settings;
    volatile bool aborted;
};

/* Sleep in short slices so an abort lands promptly instead of after a whole
 * note. */
static void interruptible_delay(TonePlayer* player, uint32_t ms) {
    const uint32_t slice = 20;
    while(ms > 0 && !player->aborted) {
        uint32_t chunk = ms < slice ? ms : slice;
        furi_delay_ms(chunk);
        ms -= chunk;
    }
}

static void play_one(TonePlayer* player, bool have_speaker, uint8_t midi_note) {
    float freq = note_frequency(midi_note);
    if(freq <= 0.0f) return;
    if(have_speaker) furi_hal_speaker_start(freq, 1.0f);
    interruptible_delay(player, ear_note_duration_ms(player->settings->note_ms));
    if(have_speaker) furi_hal_speaker_stop();
}

static int32_t player_worker(void* context) {
    TonePlayer* player = context;
    PlayerMsg msg;
    for(;;) {
        furi_message_queue_get(player->queue, &msg, FuriWaitForever);
        if(msg.type == PlayerMsgQuit) break;
        player->aborted = false;

        /* Acquire once per message and release before idling: holding the
         * speaker while idle would starve system sounds. */
        bool have_speaker = furi_hal_speaker_acquire(500);

        play_one(player, have_speaker, msg.first);
        if(msg.type == PlayerMsgInterval && !player->aborted) {
            interruptible_delay(player, GAP_MS);
            if(!player->aborted) play_one(player, have_speaker, msg.second);
        }

        if(have_speaker) furi_hal_speaker_release();
    }
    return 0;
}

TonePlayer* tone_player_alloc(NotificationApp* notifications, const EarSettings* settings) {
    TonePlayer* player = malloc(sizeof(TonePlayer));
    player->notifications = notifications;
    player->settings = settings;
    player->aborted = false;
    player->queue = furi_message_queue_alloc(2, sizeof(PlayerMsg));
    player->thread = furi_thread_alloc_ex("EarTonePlayer", 1024, player_worker, player);
    furi_thread_start(player->thread);
    return player;
}

void tone_player_free(TonePlayer* player) {
    player->aborted = true;
    PlayerMsg msg = {.type = PlayerMsgQuit};
    furi_message_queue_put(player->queue, &msg, FuriWaitForever);
    furi_thread_join(player->thread);
    furi_thread_free(player->thread);
    furi_message_queue_free(player->queue);
    free(player);
}

static void player_submit(TonePlayer* player, const PlayerMsg* msg) {
    /* Interrupt whatever is playing and drop stale requests so a replay
     * starts immediately. */
    player->aborted = true;
    furi_message_queue_reset(player->queue);
    furi_message_queue_put(player->queue, msg, 0);
}

void tone_player_play_interval(TonePlayer* player, uint8_t first_midi, uint8_t second_midi) {
    PlayerMsg msg = {.type = PlayerMsgInterval, .first = first_midi, .second = second_midi};
    player_submit(player, &msg);
}

void tone_player_play_note(TonePlayer* player, uint8_t midi_note) {
    PlayerMsg msg = {.type = PlayerMsgNote, .first = midi_note, .second = midi_note};
    player_submit(player, &msg);
}

void tone_player_stop(TonePlayer* player) {
    player->aborted = true;
    furi_message_queue_reset(player->queue);
}
