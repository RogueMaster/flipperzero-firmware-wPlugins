#include "player.h"
#include "morse.h"

#include <furi.h>
#include <furi_hal.h>
#include <notification/notification_messages.h>
#include <string.h>

struct MorsePlayer {
    NotificationApp* notification;
    const MorseSettings* settings;
    FuriThread* thread;
    bool thread_running;
    MorsePlayerCallback callback;
    void* callback_context;

    const char* text;
    volatile MorsePlayerState state;
    volatile bool stop_flag;
    volatile bool pause_flag;
    volatile size_t pos;
    volatile int8_t sym;
    bool speaker_ok;
};

static void player_notify(MorsePlayer* p) {
    if(p->callback) p->callback(p->callback_context);
}

static void player_output_on(MorsePlayer* p) {
    if(p->settings->led) furi_hal_light_set(LightRed | LightGreen | LightBlue, 0xFF);
    if(p->settings->sound && p->speaker_ok) {
        furi_hal_speaker_start((float)p->settings->tone_hz, morse_settings_volume_f(p->settings));
    }
    if(p->settings->vibro) notification_message(p->notification, &sequence_set_vibro_on);
}

static void player_output_off(MorsePlayer* p) {
    furi_hal_light_set(LightRed | LightGreen | LightBlue, 0x00);
    if(p->speaker_ok) furi_hal_speaker_stop();
    notification_message(p->notification, &sequence_reset_vibro);
}

// Espera `units` unidades Morse (1200/wpm ms cada una) en pasos cortos,
// atendiendo stop, pausa y cambios de velocidad. false = reproduccion detenida.
static bool player_wait_units(MorsePlayer* p, uint32_t units, bool output_on) {
    uint32_t elapsed = 0;
    while(true) {
        uint32_t total = units * (1200 / p->settings->wpm);
        if(elapsed >= total) return true;
        if(p->stop_flag) return false;
        if(p->pause_flag) {
            if(output_on) player_output_off(p);
            p->state = MorsePlayerStatePaused;
            player_notify(p);
            while(p->pause_flag && !p->stop_flag) {
                furi_delay_ms(10);
            }
            if(p->stop_flag) return false;
            p->state = MorsePlayerStatePlaying;
            player_notify(p);
            if(output_on) player_output_on(p);
        }
        furi_delay_ms(5);
        elapsed += 5;
    }
}

static int32_t morse_player_worker(void* context) {
    MorsePlayer* p = context;
    p->speaker_ok = furi_hal_speaker_acquire(100);

    const char* text = p->text;
    size_t len = text ? strlen(text) : 0;
    bool running = true;

    for(size_t i = 0; i < len && running; i++) {
        p->pos = i;
        p->sym = -1;
        player_notify(p);

        char c = text[i];
        if(c == ' ') {
            // Tras una letra ya esperamos 3 unidades: 4 mas = 7 (pausa de palabra).
            running = player_wait_units(p, 4, false);
            continue;
        }
        const char* code = morse_lookup(c);
        if(!code) continue;

        for(size_t j = 0; code[j] && running; j++) {
            p->sym = (int8_t)j;
            player_notify(p);
            player_output_on(p);
            running = player_wait_units(p, code[j] == '-' ? 3 : 1, true);
            player_output_off(p);
            if(running && code[j + 1]) running = player_wait_units(p, 1, false);
        }
        if(running) running = player_wait_units(p, 3, false);
    }

    player_output_off(p);
    if(p->speaker_ok) {
        furi_hal_speaker_release();
        p->speaker_ok = false;
    }
    p->sym = -1;
    p->state = p->stop_flag ? MorsePlayerStateIdle : MorsePlayerStateFinished;
    player_notify(p);
    return 0;
}

MorsePlayer* morse_player_alloc(NotificationApp* notification, const MorseSettings* settings) {
    MorsePlayer* p = malloc(sizeof(MorsePlayer));
    memset(p, 0, sizeof(MorsePlayer));
    p->notification = notification;
    p->settings = settings;
    p->sym = -1;
    p->state = MorsePlayerStateIdle;
    p->thread = furi_thread_alloc_ex("MorsePlayer", 1024, morse_player_worker, p);
    return p;
}

void morse_player_free(MorsePlayer* p) {
    morse_player_stop(p);
    furi_thread_free(p->thread);
    free(p);
}

void morse_player_set_callback(MorsePlayer* p, MorsePlayerCallback callback, void* context) {
    p->callback = callback;
    p->callback_context = context;
}

void morse_player_start(MorsePlayer* p, const char* text) {
    morse_player_stop(p);
    p->text = text;
    p->stop_flag = false;
    p->pause_flag = false;
    p->pos = 0;
    p->sym = -1;
    p->state = MorsePlayerStatePlaying;
    p->thread_running = true;
    furi_thread_start(p->thread);
}

void morse_player_stop(MorsePlayer* p) {
    if(!p->thread_running) return;
    p->stop_flag = true;
    furi_thread_join(p->thread);
    p->thread_running = false;
    p->state = MorsePlayerStateIdle;
}

void morse_player_toggle_pause(MorsePlayer* p) {
    if(p->state == MorsePlayerStatePlaying || p->state == MorsePlayerStatePaused) {
        p->pause_flag = !p->pause_flag;
    }
}

MorsePlayerState morse_player_get_state(const MorsePlayer* p) {
    return p->state;
}

size_t morse_player_get_position(const MorsePlayer* p) {
    return p->pos;
}

int8_t morse_player_get_symbol(const MorsePlayer* p) {
    return p->sym;
}
