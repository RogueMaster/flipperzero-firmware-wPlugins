#include "notifications.h"
#include "pinball0.h"

static const NotificationMessage* nm_list[32];

// static FuriMutex* nm_mutex;

// void notify_init() {
//     nm_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
// }
// void notify_free() {
//     furi_mutex_free(nm_mutex);
// }

void notify_ball_released(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.vibrate_enabled) {
        nm_list[n++] = &message_vibro_on;
    }
    nm_list[n++] = &message_delay_100;
    if(app->settings.vibrate_enabled) {
        nm_list[n++] = &message_vibro_off;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_table_bump(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.vibrate_enabled) {
        nm_list[n++] = &message_vibro_on;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_red_255;
    }
    nm_list[n++] = &message_delay_100;
    if(app->settings.vibrate_enabled) {
        nm_list[n++] = &message_vibro_off;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_red_0;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_table_tilted(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;

    for(int i = 0; i < 2; i++) {
        nm_list[n++] = &message_display_backlight_off;
        if(app->settings.vibrate_enabled) {
            nm_list[n++] = &message_vibro_on;
        }
        if(app->settings.led_enabled) {
            nm_list[n++] = &message_red_255;
        }
        nm_list[n++] = &message_delay_500;

        nm_list[n++] = &message_display_backlight_on;
        if(app->settings.vibrate_enabled) {
            nm_list[n++] = &message_vibro_off;
        }
        if(app->settings.led_enabled) {
            nm_list[n++] = &message_red_0;
        }
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_error_message(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_c6;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_sound_off;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_c5;
        nm_list[n++] = &message_delay_250;
        nm_list[n++] = &message_sound_off;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_game_over(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_delay_500;
        nm_list[n++] = &message_note_b5;
        nm_list[n++] = &message_delay_250;
        nm_list[n++] = &message_note_f6;
        nm_list[n++] = &message_delay_250;
        nm_list[n++] = &message_sound_off;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_f6;
        nm_list[n++] = &message_delay_100;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_f6;
        nm_list[n++] = &message_delay_100;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_e6;
        nm_list[n++] = &message_delay_100;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_d6;
        nm_list[n++] = &message_delay_100;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_c6;
        nm_list[n++] = &message_delay_1000;
        nm_list[n++] = &message_sound_off;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_bumper_hit(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_blue_255;
    }
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_f4;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_note_f5;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_sound_off;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_blue_0;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_rail_hit(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_d4;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_note_d5;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_sound_off;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_portal(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_blue_255;
        nm_list[n++] = &message_red_255;
    }
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_c4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_e4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_b4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_c5;
        nm_list[n++] = &message_delay_50;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_blue_255;
        nm_list[n++] = &message_red_0;
    }
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_e4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_g4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_c5;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_e5;
        nm_list[n++] = &message_delay_50;

        nm_list[n++] = &message_sound_off;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_blue_0;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

void notify_lost_life(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_red_255;
        nm_list[n++] = &message_green_255;
    }
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_c5;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_c4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_b4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_b3;
        nm_list[n++] = &message_delay_50;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_green_0;
    }
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_as4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_as3;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_a4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_a3;
        nm_list[n++] = &message_delay_50;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_green_255;
    }
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_gs4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_gs3;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_g4;
        nm_list[n++] = &message_delay_50;
        nm_list[n++] = &message_note_g4;
        nm_list[n++] = &message_delay_50;

        nm_list[n++] = &message_sound_off;
    }
    if(app->settings.led_enabled) {
        nm_list[n++] = &message_red_0;
        nm_list[n++] = &message_green_0;
    }
    furi_assert(n < 32);
    nm_list[n] = NULL;
    notification_message_block(app->notify, &nm_list);
}

void notify_flipper(void* ctx) {
    PinballApp* app = (PinballApp*)ctx;
    int n = 0;
    if(app->settings.sound_enabled) {
        nm_list[n++] = &message_note_c4;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_note_cs4;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_sound_off;
    }
    nm_list[n] = NULL;
    notification_message(app->notify, &nm_list);
}

/*
Mario coin sound - ish
        nm_list[n++] = &message_note_b5;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_delay_10;
        nm_list[n++] = &message_sound_off;
        nm_list[n++] = &message_note_e6;
        nm_list[n++] = &message_delay_250;
        nm_list[n++] = &message_delay_100;

*/
