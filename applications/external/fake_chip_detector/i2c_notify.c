#include "i2c_notify.h"

#include <furi.h>

// Sequences are assembled from tagged steps so the user can switch off sound,
// vibration or the LED independently and still get the remaining feedback.

typedef enum {
    StepSound, // a note, or the closing sound_off
    StepVibro,
    StepLed,
    StepDelay, // always kept: it is what gives the melody its rhythm
} StepKind;

typedef struct {
    StepKind kind;
    const NotificationMessage* message;
} Step;

#define MAX_STEPS 20

static const Step steps_genuine[] = {
    {StepLed, &message_green_255},
    {StepVibro, &message_vibro_on},
    {StepSound, &message_note_c5},
    {StepDelay, &message_delay_50},
    {StepSound, &message_note_e5},
    {StepDelay, &message_delay_50},
    {StepVibro, &message_vibro_off},
    {StepSound, &message_note_g5},
    {StepDelay, &message_delay_50},
    {StepSound, &message_note_c6},
    {StepDelay, &message_delay_100},
    {StepSound, &message_sound_off},
    {StepDelay, &message_delay_100},
};

static const Step steps_bad[] = {
    {StepLed, &message_red_255},
    {StepVibro, &message_vibro_on},
    {StepSound, &message_note_g4},
    {StepDelay, &message_delay_100},
    {StepVibro, &message_vibro_off},
    {StepSound, &message_note_ds4},
    {StepDelay, &message_delay_100},
    {StepVibro, &message_vibro_on},
    {StepSound, &message_note_c4},
    {StepDelay, &message_delay_250},
    {StepVibro, &message_vibro_off},
    {StepSound, &message_sound_off},
    {StepDelay, &message_delay_100},
};

static const Step steps_attention[] = {
    {StepLed, &message_red_255},
    {StepLed, &message_green_255}, // red + green = yellow
    {StepVibro, &message_vibro_on},
    {StepSound, &message_note_a5},
    {StepDelay, &message_delay_50},
    {StepSound, &message_note_f5},
    {StepDelay, &message_delay_50},
    {StepVibro, &message_vibro_off},
    {StepSound, &message_note_a5},
    {StepDelay, &message_delay_50},
    {StepSound, &message_note_f5},
    {StepDelay, &message_delay_100},
    {StepSound, &message_sound_off},
    {StepDelay, &message_delay_100},
};

static const Step steps_neutral[] = {
    {StepLed, &message_blue_255},
    {StepSound, &message_note_e5},
    {StepDelay, &message_delay_50},
    {StepSound, &message_sound_off},
    {StepDelay, &message_delay_50},
};

static const Step steps_calibrated[] = {
    {StepLed, &message_green_255},
    {StepVibro, &message_vibro_on},
    {StepSound, &message_note_e5},
    {StepDelay, &message_delay_50},
    {StepSound, &message_note_a5},
    {StepDelay, &message_delay_50},
    {StepVibro, &message_vibro_off},
    {StepSound, &message_note_c6},
    {StepDelay, &message_delay_100},
    {StepSound, &message_sound_off},
    {StepDelay, &message_delay_50},
};

typedef struct {
    const Step* steps;
    size_t count;
} Recipe;

static const Recipe recipes[I2CNotifyCount] = {
    [I2CNotifyGenuine] = {steps_genuine, COUNT_OF(steps_genuine)},
    [I2CNotifyBad] = {steps_bad, COUNT_OF(steps_bad)},
    [I2CNotifyAttention] = {steps_attention, COUNT_OF(steps_attention)},
    [I2CNotifyNeutral] = {steps_neutral, COUNT_OF(steps_neutral)},
    [I2CNotifyCalibrated] = {steps_calibrated, COUNT_OF(steps_calibrated)},
};

// Built once per settings change and then only read, so the async
// notification service can safely hold a pointer to them.
static const NotificationMessage* built[I2CNotifyCount][MAX_STEPS + 1];

void i2c_notify_apply_settings(const I2CSettings* settings) {
    for(size_t kind = 0; kind < I2CNotifyCount; kind++) {
        const Recipe* recipe = &recipes[kind];
        size_t out = 0;
        bool any_output = false;

        for(size_t i = 0; i < recipe->count && out < MAX_STEPS; i++) {
            const Step* step = &recipe->steps[i];
            switch(step->kind) {
            case StepSound:
                if(!settings->sound) continue;
                any_output = true;
                break;
            case StepVibro:
                if(!settings->vibro) continue;
                any_output = true;
                break;
            case StepLed:
                if(!settings->led) continue;
                any_output = true;
                break;
            case StepDelay:
                // A trailing run of delays with nothing to pace is pointless,
                // and a sequence of only delays would block the LED layer.
                if(!any_output) continue;
                break;
            }
            built[kind][out++] = step->message;
        }

        built[kind][out] = NULL; // sequences are NULL-terminated
    }
}

void i2c_notify_play(NotificationApp* app, I2CNotifyKind kind) {
    if(kind >= I2CNotifyCount) return;
    if(built[kind][0] == NULL) return; // everything switched off
    notification_message(app, (const NotificationSequence*)&built[kind]);
}
