#include "../air_stats_i.h"

/* ---- Helper: find CO2 value from active sensor ---- */

static float get_co2_value(void) {
    for(uint8_t i = 0; i < app->sensors_count; i++) {
        Sensor* s = app->sensors[i];
        if((s->type->datatype & UT_CO2) && s->status != UT_SENSORSTATUS_INACTIVE && s->co2 > 0.0f)
            return s->co2;
    }
    return -1.0f;
}

/* ---- LED sequences via notification API (do_not_reset = persists through sound) ---- */

/* green:  G=255 */
static const NotificationSequence _seq_led_green = {
    &message_green_255,
    &message_do_not_reset,
    NULL};
/* yellow: R=255 + G=255 */
static const NotificationSequence _seq_led_yellow =
    {&message_red_255, &message_green_255, &message_do_not_reset, NULL};
/* orange: R=255 + G=80 (custom) */
static const NotificationMessage _led_g_80 = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 80};
static const NotificationSequence _seq_led_orange =
    {&message_red_255, &_led_g_80, &message_do_not_reset, NULL};
/* red:    R=255 */
static const NotificationSequence _seq_led_red = {&message_red_255, &message_do_not_reset, NULL};
/* off */
static const NotificationSequence _seq_led_off =
    {&message_red_0, &message_green_0, &message_blue_0, NULL};

/* ---- LED: update color based on CO2 level, only when level changes ---- */

void air_stats_update_led(void) {
    static uint8_t last_level = 0xFF; /* 0=off, 1=green, 2=yellow, 3=orange, 4=red */

    if(!app->settings.led_notify) {
        if(last_level != 0) {
            notification_message(app->notifications, &_seq_led_off);
            last_level = 0;
        }
        return;
    }

    float co2 = get_co2_value();
    if(co2 < 0.0f) return; /* no data — don't touch LED */

    uint8_t level;
    if(co2 < 800.0f)
        level = 1;
    else if(co2 < 1000.0f)
        level = 2;
    else if(co2 < 1400.0f)
        level = 3;
    else
        level = 4;

    if(level == last_level) return;
    last_level = level;

    const NotificationSequence* seq;
    switch(level) {
    case 1:
        seq = &_seq_led_green;
        break;
    case 2:
        seq = &_seq_led_yellow;
        break;
    case 3:
        seq = &_seq_led_orange;
        break;
    case 4:
        seq = &_seq_led_red;
        break;
    default:
        seq = &_seq_led_off;
        break;
    }
    notification_message(app->notifications, seq);
}

/* ---- Sound: CO2 alert with cooldown and hysteresis ---- */

#define CO2_HYSTERESIS_PPM 50u
#define CO2_COOLDOWN_TICKS furi_ms_to_ticks(60000)

static NotificationMessage _co2_vol_msg = {
    .type = NotificationMessageTypeForceSpeakerVolumeSetting,
    .data.forced_settings.speaker_volume = 0.5f,
};

/* Sound 1 — alarm (OK→BAD): two rising notes */
static const NotificationMessage _alarm_note1 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 880.0f, .volume = 1.0f},
};
static const NotificationMessage _alarm_note2 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 1174.7f, .volume = 1.0f},
};
static const NotificationMessage _delay120 = {
    .type = NotificationMessageTypeDelay,
    .data.delay.length = 120,
};
static const NotificationMessage _soff = {
    .type = NotificationMessageTypeSoundOff,
};
static const NotificationSequence _seq_alarm = {
    &_co2_vol_msg,
    &_alarm_note1,
    &_delay120,
    &_alarm_note2,
    &_delay120,
    &_soff,
    &message_do_not_reset,
    NULL,
};

/* Sound 2 — relief (BAD→OK): two falling notes */
static const NotificationMessage _relief_note1 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 1174.7f, .volume = 1.0f},
};
static const NotificationMessage _relief_note2 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 880.0f, .volume = 1.0f},
};
static const NotificationSequence _seq_relief = {
    &_co2_vol_msg,
    &_relief_note1,
    &_delay120,
    &_relief_note2,
    &_delay120,
    &_soff,
    &message_do_not_reset,
    NULL,
};

static bool cooldown_expired(void) {
    return furi_get_tick() - app->last_alert_tick >= CO2_COOLDOWN_TICKS;
}

static void play_alert(const NotificationSequence* seq) {
    _co2_vol_msg.data.forced_settings.speaker_volume = app->settings.sound_volume / 10.0f;
    notification_message(app->notifications, seq);
    app->last_alert_tick = furi_get_tick();
}

void air_stats_check_sound_alert(void) {
    if(!app->settings.sound_notify) return;
    float co2 = get_co2_value();
    if(co2 <= 0.0f) return;

    bool above = (co2 >= (float)app->settings.co2_alert_threshold);
    bool way_below = (co2 < (float)(app->settings.co2_alert_threshold - CO2_HYSTERESIS_PPM));

    /* OK → BAD transition */
    if(above && !app->co2_was_above) {
        if(cooldown_expired()) {
            play_alert(&_seq_alarm);
        }
        app->co2_was_above = true;
    }

    /* BAD → OK transition (with hysteresis) */
    if(way_below && app->co2_was_above) {
        if(cooldown_expired()) {
            play_alert(&_seq_relief);
        }
        app->co2_was_above = false;
    }
}
