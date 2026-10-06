#include "signals.h"

// Red LED blink building block (one flash).
#define LED_BLINK \
    &message_red_255, &message_delay_100, &message_red_0, &message_delay_100

// Short silence between notes, so repeated notes are heard as separate beeps.
#define GAP &message_sound_off, &message_delay_25

// LED only: three red flashes.
static const NotificationSequence seq_led = {
    LED_BLINK, LED_BLINK, LED_BLINK, NULL,
};

// Vibro: warning = one pulse, critical = three pulses.
static const NotificationSequence seq_vibro_warn = {
    &message_red_255, &message_vibro_on, &message_delay_250, &message_vibro_off,
    &message_red_0, NULL,
};

static const NotificationSequence seq_vibro_crit = {
    &message_red_255, &message_vibro_on, &message_delay_100, &message_vibro_off,
    &message_delay_100, &message_vibro_on, &message_delay_100, &message_vibro_off,
    &message_delay_100, &message_vibro_on, &message_delay_250, &message_vibro_off,
    &message_red_0, NULL,
};

// One melody per rule (each under a second). Hot = high and fast, load = rising,
// memory = falling, disk = low thuds, battery = slow and sad, link lost = drop to low.
#define MEL_CPU_TEMP                                                                 \
    &message_note_e6, &message_delay_100, &message_note_a5, &message_delay_100,      \
        &message_note_e6, &message_delay_100, &message_note_a5, &message_delay_100,  \
        &message_note_e6, &message_delay_250
#define MEL_GPU_TEMP                                                                 \
    &message_note_g6, &message_delay_50, GAP, &message_note_g6, &message_delay_50,   \
        &message_note_d6, &message_delay_100, &message_note_g6, &message_delay_50,   \
        GAP, &message_note_g6, &message_delay_50, &message_note_d6, &message_delay_250
#define MEL_CPU_LOAD                                                                 \
    &message_note_c5, &message_delay_100, &message_note_e5, &message_delay_100,      \
        &message_note_g5, &message_delay_250
#define MEL_GPU_LOAD                                                                 \
    &message_note_a5, &message_delay_100, GAP, &message_note_a5, &message_delay_100, \
        &message_note_e6, &message_delay_250
#define MEL_RAM_LOAD                                                                 \
    &message_note_c6, &message_delay_100, &message_note_a5, &message_delay_100,      \
        &message_note_f5, &message_delay_250
#define MEL_VRAM_LOAD                                                                \
    &message_note_d6, &message_delay_100, GAP, &message_note_d6, &message_delay_100, \
        &message_note_b5, &message_delay_100, &message_note_g5, &message_delay_250
#define MEL_DISK_LOAD                                                                \
    &message_note_c4, &message_delay_100, GAP, &message_delay_50, &message_note_c4,  \
        &message_delay_100, GAP, &message_delay_50, &message_note_c4, &message_delay_250
#define MEL_BATTERY_LOW                                                              \
    &message_note_g5, &message_delay_250, &message_note_e5, &message_delay_250,      \
        &message_note_c5, &message_delay_500
#define MEL_LINK_LOST                                                                \
    &message_note_c6, &message_delay_100, &message_note_g5, &message_delay_100,      \
        &message_note_c5, &message_delay_100, &message_note_c4, &message_delay_250

#define SEQ_SOUND(name, MEL)                                                \
    static const NotificationSequence name = {                              \
        &message_red_255, MEL, &message_sound_off, &message_red_0, NULL,    \
    }
#define SEQ_BOTH(name, MEL)                                                       \
    static const NotificationSequence name = {                                    \
        &message_red_255, &message_vibro_on, MEL, &message_sound_off,             \
        &message_vibro_off, &message_red_0, NULL,                                 \
    }

SEQ_SOUND(snd_cpu_temp, MEL_CPU_TEMP);
SEQ_SOUND(snd_gpu_temp, MEL_GPU_TEMP);
SEQ_SOUND(snd_cpu_load, MEL_CPU_LOAD);
SEQ_SOUND(snd_gpu_load, MEL_GPU_LOAD);
SEQ_SOUND(snd_ram_load, MEL_RAM_LOAD);
SEQ_SOUND(snd_vram_load, MEL_VRAM_LOAD);
SEQ_SOUND(snd_disk_load, MEL_DISK_LOAD);
SEQ_SOUND(snd_battery_low, MEL_BATTERY_LOW);
SEQ_SOUND(snd_link_lost, MEL_LINK_LOST);

SEQ_BOTH(both_cpu_temp, MEL_CPU_TEMP);
SEQ_BOTH(both_gpu_temp, MEL_GPU_TEMP);
SEQ_BOTH(both_cpu_load, MEL_CPU_LOAD);
SEQ_BOTH(both_gpu_load, MEL_GPU_LOAD);
SEQ_BOTH(both_ram_load, MEL_RAM_LOAD);
SEQ_BOTH(both_vram_load, MEL_VRAM_LOAD);
SEQ_BOTH(both_disk_load, MEL_DISK_LOAD);
SEQ_BOTH(both_battery_low, MEL_BATTERY_LOW);
SEQ_BOTH(both_link_lost, MEL_LINK_LOST);

// Indexed by AlertRuleId.
static const NotificationSequence* const sound_seq[AlertRuleCount] = {
    &snd_cpu_temp, &snd_gpu_temp, &snd_cpu_load, &snd_gpu_load, &snd_ram_load,
    &snd_vram_load, &snd_disk_load, &snd_battery_low, &snd_link_lost,
};

static const NotificationSequence* const both_seq[AlertRuleCount] = {
    &both_cpu_temp, &both_gpu_temp, &both_cpu_load, &both_gpu_load, &both_ram_load,
    &both_vram_load, &both_disk_load, &both_battery_low, &both_link_lost,
};

void phr_signal_play(NotificationApp* n, AlertSignal signal, AlertRuleId rule) {
    if(rule >= AlertRuleCount) return;
    switch(signal) {
    case AlertSignalVibro:
        if(phr_signal_is_critical(rule))
            notification_message(n, &seq_vibro_crit);
        else
            notification_message(n, &seq_vibro_warn);
        break;
    case AlertSignalSound:
        notification_message(n, sound_seq[rule]);
        break;
    case AlertSignalVibroSound:
        notification_message(n, both_seq[rule]);
        break;
    case AlertSignalLed:
        notification_message(n, &seq_led);
        break;
    default:
        break;
    }
}

bool phr_signal_is_critical(AlertRuleId rule) {
    return rule == AlertCpuTemp || rule == AlertGpuTemp || rule == AlertBatteryLow ||
           rule == AlertLinkLost;
}
