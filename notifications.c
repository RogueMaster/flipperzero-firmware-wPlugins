#include "notifications.h"

static NotificationSequence success_beep = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 400, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 100 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    NULL
};

static NotificationSequence success_vibro = {
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 100 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence success_beep_and_vibro = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 400, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 100 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence failure_beep = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 200, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 500 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    NULL
};

static NotificationSequence failure_vibro = {
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 500 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence failure_beep_and_vibro = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 200, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 500 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

void stratahero_code_glyph_entry_success_notification(NotificationApp* notification, const StrataHeroSettings* settings) {
    if(settings->sound_enabled && settings->vibro_enabled) {
        notification_message(notification, &success_beep_and_vibro);
    } else if(settings->sound_enabled) {
        notification_message(notification, &success_beep);
    } else if(settings->vibro_enabled) {
        notification_message(notification, &success_vibro);
    }
}

void stratahero_code_glyph_entry_failure_notification(NotificationApp* notification, const StrataHeroSettings* settings) {
    if(settings->sound_enabled && settings->vibro_enabled) {
        notification_message(notification, &failure_beep_and_vibro);
    } else if(settings->sound_enabled) {
        notification_message(notification, &failure_beep);
    } else if(settings->vibro_enabled) {
        notification_message(notification, &failure_vibro);
    }
}

static NotificationSequence code_complete_beep = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 400, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 75 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 600, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 150 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    NULL
};

static NotificationSequence code_complete_vibro = {
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 200 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence code_complete_beep_and_vibro = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 400, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 75 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 600, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 150 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence stats_hit_beep = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 100, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    NULL
};

static NotificationSequence stats_hit_vibro = {
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence stats_hit_beep_and_vibro = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 100, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence stats_final_hit_beep = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 100, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 80 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 100, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    NULL
};

static NotificationSequence stats_final_hit_vibro = {
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 80 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

static NotificationSequence stats_final_hit_beep_and_vibro = {
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 100, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 80 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOn, { .sound = { 100, 0.5 } } },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { true } } },
    &(NotificationMessage){ NotificationMessageTypeDelay, { .delay = { 50 } } },
    &(NotificationMessage){ NotificationMessageTypeSoundOff, {} },
    &(NotificationMessage){ NotificationMessageTypeVibro, { .vibro = { false } } },
    NULL
};

void stratahero_stats_final_hit_notification(NotificationApp* notification, const StrataHeroSettings* settings) {
    if(settings->sound_enabled && settings->vibro_enabled) {
        notification_message(notification, &stats_final_hit_beep_and_vibro);
    } else if(settings->sound_enabled) {
        notification_message(notification, &stats_final_hit_beep);
    } else if(settings->vibro_enabled) {
        notification_message(notification, &stats_final_hit_vibro);
    }
}

void stratahero_stats_hit_notification(NotificationApp* notification, const StrataHeroSettings* settings) {
    if(settings->sound_enabled && settings->vibro_enabled) {
        notification_message(notification, &stats_hit_beep_and_vibro);
    } else if(settings->sound_enabled) {
        notification_message(notification, &stats_hit_beep);
    } else if(settings->vibro_enabled) {
        notification_message(notification, &stats_hit_vibro);
    }
}

void stratahero_code_complete_notification(NotificationApp* notification, const StrataHeroSettings* settings) {
    if(settings->sound_enabled && settings->vibro_enabled) {
        notification_message(notification, &code_complete_beep_and_vibro);
    } else if(settings->sound_enabled) {
        notification_message(notification, &code_complete_beep);
    } else if(settings->vibro_enabled) {
        notification_message(notification, &code_complete_vibro);
    }
}
