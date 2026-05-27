#include "zeromesh_notify.h"
#include <notification/notification.h>
#include <notification/notification_messages.h>

const char* ringtone_names[] = {
    "Off",
    "Short",
    "Double",
    "Triple",
    "Long",
    "SOS",
    "Chirp",
    "Nokia",
    "Descend",
    "Bounce",
    "Alert",
    "Pulse",
    "Siren",
    "Beep3",
    "Trill",
    "Mario",
    "LevelUp",
    "Metric",
    "Minimal",
};

static const NotificationSequence seq_vibro_short = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence seq_led_flash = {
    &message_blue_255,
    &message_delay_250,
    &message_blue_0,
    &message_delay_100,
    &message_blue_255,
    &message_delay_250,
    &message_blue_0,
    NULL,
};

static const NotificationSequence seq_vibro_led = {
    &message_vibro_on,
    &message_blue_255,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_50,
    &message_blue_0,
    &message_delay_100,
    &message_blue_255,
    &message_delay_250,
    &message_blue_0,
    NULL,
};

static void play_nokia(void) {
    const int melody[] = {659, 587, 370, 415, 554, 494, 370, 330};
    const int durations[] = {125, 125, 250, 250, 125, 125, 250, 250};
    for(int i = 0; i < 8; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        furi_delay_ms(durations[i]);
        furi_hal_speaker_stop();
        furi_delay_ms(20);
    }
}

static void play_descend(void) {
    for(int f = 1200; f >= 600; f -= 100) {
        furi_hal_speaker_start((float)f, 0.6f);
        furi_delay_ms(80);
    }
    furi_hal_speaker_stop();
}

static void play_bounce(void) {
    const int melody[] = {600, 800, 600, 900, 600, 1000};
    for(int i = 0; i < 6; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        furi_delay_ms(100);
        furi_hal_speaker_stop();
        furi_delay_ms(30);
    }
}

static void play_alert(void) {
    for(int i = 0; i < 3; i++) {
        furi_hal_speaker_start(900.0f, 0.7f);
        furi_delay_ms(150);
        furi_hal_speaker_stop();
        furi_delay_ms(100);
        furi_hal_speaker_start(700.0f, 0.7f);
        furi_delay_ms(150);
        furi_hal_speaker_stop();
        if(i < 2) furi_delay_ms(100);
    }
}

static void play_pulse(void) {
    for(int i = 0; i < 4; i++) {
        furi_hal_speaker_start(800.0f, 0.5f + (float)i * 0.1f);
        furi_delay_ms(120);
        furi_hal_speaker_stop();
        furi_delay_ms(80);
    }
}

static void play_siren(void) {
    for(int cycle = 0; cycle < 2; cycle++) {
        for(int f = 600; f <= 900; f += 75) {
            furi_hal_speaker_start((float)f, 0.6f);
            furi_delay_ms(40);
        }
        for(int f = 900; f >= 600; f -= 75) {
            furi_hal_speaker_start((float)f, 0.6f);
            furi_delay_ms(40);
        }
    }
    furi_hal_speaker_stop();
}

static void play_beep3(void) {
    for(int i = 0; i < 3; i++) {
        furi_hal_speaker_start(1000.0f, 0.6f);
        furi_delay_ms(100);
        furi_hal_speaker_stop();
        if(i < 2) furi_delay_ms(100);
    }
}

static void play_trill(void) {
    const int melody[] = {800, 1000, 800, 1000, 800, 1000, 1200};
    const int durations[] = {80, 80, 80, 80, 80, 80, 200};
    for(int i = 0; i < 7; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        furi_delay_ms(durations[i]);
        furi_hal_speaker_stop();
        if(i < 6) furi_delay_ms(30);
    }
}

static void play_mario(void) {
    const int melody[] = {659, 659, 659, 523, 659, 784};
    const int durations[] = {150, 150, 150, 100, 150, 300};
    for(int i = 0; i < 6; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        furi_delay_ms(durations[i]);
        furi_hal_speaker_stop();
        if(i < 5) furi_delay_ms(50);
    }
}

static void play_levelup(void) {
    const int melody[] = {523, 659, 784, 1047, 1319, 1568};
    for(int i = 0; i < 6; i++) {
        furi_hal_speaker_start((float)melody[i], 0.6f);
        furi_delay_ms(100);
        furi_hal_speaker_stop();
        if(i < 5) furi_delay_ms(30);
    }
}

static void play_metric(void) {
    for(int i = 0; i < 4; i++) {
        furi_hal_speaker_start(1047.0f, 0.6f);
        furi_delay_ms(120);
        furi_hal_speaker_stop();
        if(i < 3) furi_delay_ms(120);
    }
}

static void play_minimalist(void) {
    furi_hal_speaker_start(1047.0f, 0.6f);
    furi_delay_ms(150);
    furi_hal_speaker_stop();
    furi_delay_ms(150);
    furi_hal_speaker_start(1047.0f, 0.6f);
    furi_delay_ms(150);
    furi_hal_speaker_stop();
}

void play_ringtone(ZeroMeshApp* app) {
    if(app->notify_ringtone == RingtoneNone) return;

    if(!furi_hal_speaker_acquire(1000)) return;

    switch(app->notify_ringtone) {
    case RingtoneShort:
        furi_hal_speaker_start(800.0f, 0.6f);
        furi_delay_ms(100);
        furi_hal_speaker_stop();
        break;

    case RingtoneDouble:
        furi_hal_speaker_start(800.0f, 0.6f);
        furi_delay_ms(80);
        furi_hal_speaker_stop();
        furi_delay_ms(60);
        furi_hal_speaker_start(1000.0f, 0.6f);
        furi_delay_ms(80);
        furi_hal_speaker_stop();
        break;

    case RingtoneTriple:
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f + (float)(i * 200), 0.6f);
            furi_delay_ms(60);
            furi_hal_speaker_stop();
            if(i < 2) furi_delay_ms(40);
        }
        break;

    case RingtoneLong:
        furi_hal_speaker_start(600.0f, 0.5f);
        furi_delay_ms(400);
        furi_hal_speaker_stop();
        break;

    case RingtoneSOS:
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f, 0.6f);
            furi_delay_ms(50);
            furi_hal_speaker_stop();
            furi_delay_ms(50);
        }
        furi_delay_ms(100);
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f, 0.6f);
            furi_delay_ms(150);
            furi_hal_speaker_stop();
            furi_delay_ms(50);
        }
        furi_delay_ms(100);
        for(int i = 0; i < 3; i++) {
            furi_hal_speaker_start(800.0f, 0.6f);
            furi_delay_ms(50);
            furi_hal_speaker_stop();
            if(i < 2) furi_delay_ms(50);
        }
        break;

    case RingtoneChirp:
        for(int f = 400; f <= 1200; f += 50) {
            furi_hal_speaker_start((float)f, 0.5f);
            furi_delay_ms(15);
        }
        furi_hal_speaker_stop();
        break;

    case RingtoneNokia:
        play_nokia();
        break;

    case RingtoneDescend:
        play_descend();
        break;

    case RingtoneBounce:
        play_bounce();
        break;

    case RingtoneAlert:
        play_alert();
        break;

    case RingtonePulse:
        play_pulse();
        break;

    case RingtoneSiren:
        play_siren();
        break;

    case RingtoneBeep3:
        play_beep3();
        break;

    case RingtoneTrill:
        play_trill();
        break;

    case RingtoneMario:
        play_mario();
        break;

    case RingtoneLevelUp:
        play_levelup();
        break;

    case RingtoneMetric:
        play_metric();
        break;

    case RingtoneMinimalist:
        play_minimalist();
        break;

    default:
        break;
    }

    furi_hal_speaker_release();
}

void notify_rx_message(ZeroMeshApp* app) {
    NotificationApp* notify = furi_record_open(RECORD_NOTIFICATION);

    if(app->notify_vibro && app->notify_led) {
        notification_message(notify, &seq_vibro_led);
    } else if(app->notify_vibro) {
        notification_message(notify, &seq_vibro_short);
    } else if(app->notify_led) {
        notification_message(notify, &seq_led_flash);
    }

    furi_record_close(RECORD_NOTIFICATION);

    if(app->notify_ringtone != RingtoneNone) {
        play_ringtone(app);
    }
}