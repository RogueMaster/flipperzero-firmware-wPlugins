#include "ui_feedback.h"

static const NotificationSequence sequence_level_up_feedback = {
    &message_force_vibro_setting_on,
    &message_force_speaker_volume_setting_1f,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_50,
    &message_vibro_off,
    &message_sound_off,
    &message_delay_50,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_50,
    &message_vibro_off,
    &message_sound_off,
    &message_delay_50,
    &message_vibro_on,
    &message_note_g5,
    &message_delay_50,
    &message_vibro_off,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_low_hp_feedback = {
    &message_force_vibro_setting_on,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_secret_found_feedback = {
    &message_force_speaker_volume_setting_1f,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_snare_feedback = {
    &message_force_vibro_setting_on,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_trap_spotted_feedback = {
    &message_force_vibro_setting_on,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    &message_delay_25,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static NotificationMessage message_led_red = {
    .type = NotificationMessageTypeLedRed,
    .data.led.value = 0,
};

static NotificationMessage message_led_green = {
    .type = NotificationMessageTypeLedGreen,
    .data.led.value = 0,
};

static NotificationMessage message_led_blue = {
    .type = NotificationMessageTypeLedBlue,
    .data.led.value = 0,
};

static const NotificationSequence sequence_led_rgb = {
    &message_led_red,
    &message_led_green,
    &message_led_blue,
    NULL,
};

static void consume_game_event(AppContext* app);

void feedback_reset_led(AppContext* app, bool force) {
    if(!app->notifications) return;
    if(!force && app->led_state == 0) return;
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_reset_rgb);
    app->led_state = 0;
    app->hp_low = false;
}

static bool hp_is_low(const FrGame* game) {
    if(game->player.max_hp == 0) return false;
    return game->player.hp == 0 || (uint16_t)game->player.hp * 10u <= (uint16_t)game->player.max_hp * 3u;
}

static void feedback_send_led(
    AppContext* app,
    bool force,
    uint8_t state,
    uint8_t red,
    uint8_t green,
    uint8_t blue) {
    if(!force && state == app->led_state) return;
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_reset_rgb);
    message_led_red.data.led.value = red;
    message_led_green.data.led.value = green;
    message_led_blue.data.led.value = blue;
    notification_message(app->notifications, &sequence_led_rgb);
    app->led_state = state;
}

void feedback_update_led(AppContext* app, bool force) {
    if(!app->notifications || app->game->mode == FR_MODE_QUIT) return;
    if(app->game->mode != FR_MODE_PLAYING || app->screen == UI_TITLE || app->screen == UI_CLASS_SELECT ||
       app->screen == UI_SCORES || app->screen == UI_HELP_MENU || app->screen == UI_HELP_PAGE) {
        feedback_reset_led(app, force);
        return;
    }
    bool low = hp_is_low(app->game);
    uint8_t effects = app->game->player.effects;
    bool burning = (effects & FR_FX_BURNING) != 0;
    bool poisoned = (effects & FR_FX_POISONED) != 0;
    bool starving = fr_hunger_state(app->game) == FR_HUNGER_STARVING;
    uint8_t phase = (uint8_t)((furi_get_tick() / 250u) & 1u);
    uint8_t slow_phase = (uint8_t)((furi_get_tick() / 1000u) & 1u);
    uint8_t state = 1;
    uint8_t red = 0;
    uint8_t green = 255;
    uint8_t blue = 0;

    if(low && burning) {
        state = (uint8_t)(10u + slow_phase);
        red = 255;
        green = slow_phase ? 80 : 0;
        blue = 0;
    } else if(low && poisoned) {
        state = (uint8_t)(12u + slow_phase);
        red = 255;
        green = 0;
        blue = slow_phase ? 180 : 0;
    } else if(low && starving) {
        state = (uint8_t)(14u + slow_phase);
        red = slow_phase ? 160 : 255;
        green = slow_phase ? 70 : 0;
        blue = 0;
    } else if(low) {
        state = (uint8_t)(16u + slow_phase);
        red = slow_phase ? 255 : 90;
        green = 0;
        blue = 0;
    } else if(burning) {
        state = (uint8_t)(18u + phase);
        red = 255;
        green = phase ? 80 : 0;
        blue = 0;
    } else if(poisoned) {
        state = (uint8_t)(20u + phase);
        red = phase ? 180 : 0;
        green = 0;
        blue = 255;
    } else if(starving) {
        state = 22;
        red = 160;
        green = 70;
        blue = 0;
    } else if((effects & FR_FX_SLOWED) != 0) {
        state = (uint8_t)(23u + phase);
        red = phase ? 70 : 0;
        green = phase ? 0 : 180;
        blue = 255;
    } else if((effects & FR_FX_AFRAID) != 0) {
        state = (uint8_t)(25u + phase);
        red = 255;
        green = phase ? 70 : 0;
        blue = 0;
    } else if((effects & FR_FX_CONFUSED) != 0) {
        state = (uint8_t)(27u + phase);
        red = phase ? 180 : 0;
        green = phase ? 0 : 180;
        blue = phase ? 255 : 0;
    } else if((effects & FR_FX_STUNNED) != 0) {
        state = 29;
        red = 180;
        green = 160;
        blue = 80;
    } else if((effects & FR_FX_BLIND) != 0) {
        state = 30;
        red = 0;
        green = 0;
        blue = 60;
    }

    feedback_send_led(app, force, state, red, green, blue);
    app->hp_low = low;
}
FeedbackBefore feedback_capture(AppContext* app) {
    return (FeedbackBefore){
        app->game->player.hp,
        app->game->player.level,
        hp_is_low(app->game),
    };
}

static void feedback_after_action(AppContext* app, FeedbackBefore before, FrActionResult result) {
    if(!app->notifications) return;
    bool did_hit = result.kind == FR_ACTION_ATTACK || result.kind == FR_ACTION_RANGED || result.kind == FR_ACTION_ZAP;
    bool was_hit = app->game->player.hp < before.hp;
    if(did_hit || was_hit) notification_message(app->notifications, &sequence_single_vibro);
    if(app->sound_enabled && app->game->player.level > before.level) {
        notification_message(app->notifications, &sequence_level_up_feedback);
    }
    bool low = hp_is_low(app->game);
    if(!before.low_hp && low) notification_message(app->notifications, &sequence_low_hp_feedback);
    feedback_update_led(app, false);
}

static void maybe_open_perk_choice(AppContext* app) {
    if(app->game->mode == FR_MODE_PLAYING && app->game->player.pending_perks > 0) {
        app->screen = UI_PERK_CHOICE;
        app->selection = 0;
    }
}

void finish_action(AppContext* app, FeedbackBefore before, FrActionResult result) {
    feedback_after_action(app, before, result);
    consume_game_event(app);
    maybe_open_perk_choice(app);
}

static void consume_game_event(AppContext* app) {
    if(app->game->last_event == FR_EVENT_DEW) {
        app->dew_x = app->game->event_x;
        app->dew_y = app->game->event_y;
        app->dew_phase = 1;
        app->dew_ticks = DEW_PHASE_TICKS;
        app->game->last_event = FR_EVENT_NONE;
    } else if(app->game->last_event == FR_EVENT_SECRET) {
        app->flash_x = app->game->event_x;
        app->flash_y = app->game->event_y;
        app->flash_ticks = 4;
        if(app->sound_enabled && app->notifications) {
            notification_message(app->notifications, &sequence_secret_found_feedback);
        }
        app->game->last_event = FR_EVENT_NONE;
    } else if(app->game->last_event == FR_EVENT_MON_PROJECTILE) {
        int8_t dx = (int8_t)((int16_t)app->game->event_tx - (int16_t)app->game->event_x);
        int8_t dy = (int8_t)((int16_t)app->game->event_ty - (int16_t)app->game->event_y);
        dx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
        dy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
        app->fx_active = true;
        app->fx_x = app->game->event_x;
        app->fx_y = app->game->event_y;
        app->fx_tx = app->game->event_tx;
        app->fx_ty = app->game->event_ty;
        app->fx_dx = dx;
        app->fx_dy = dy;
        app->fx_glyph = app->game->event_glyph ? app->game->event_glyph : '-';
        app->flash_x = app->game->event_tx;
        app->flash_y = app->game->event_ty;
        app->flash_ticks = 4;
        app->game->last_event = FR_EVENT_NONE;
    } else if(app->game->last_event == FR_EVENT_SNARE) {
        if(app->notifications) notification_message(app->notifications, &sequence_snare_feedback);
        app->game->last_event = FR_EVENT_NONE;
    } else if(app->game->last_event == FR_EVENT_TRAP_SPOTTED) {
        app->flash_x = app->game->event_x;
        app->flash_y = app->game->event_y;
        app->flash_ticks = 3;
        if(app->notifications) notification_message(app->notifications, &sequence_trap_spotted_feedback);
        app->game->last_event = FR_EVENT_NONE;
    }
}
