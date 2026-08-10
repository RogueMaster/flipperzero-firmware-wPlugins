#pragma once

#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "i2c_settings.h"

typedef enum {
    I2CNotifyGenuine, // rising arpeggio, green — the chip is real
    I2CNotifyBad, // falling phrase, red — fake or dead silicon
    I2CNotifyAttention, // two-tone chirp, yellow — user must fix something
    I2CNotifyNeutral, // soft blip, blue — informational
    I2CNotifyCalibrated, // BNO055 magnetometer reached level 3
    I2CNotifyCount,
} I2CNotifyKind;

// Rebuilds the playable sequences with sound, vibro and LED steps filtered
// according to the settings. Must be called once at startup and again after
// any settings change, before the next i2c_notify_play.
void i2c_notify_apply_settings(const I2CSettings* settings);

void i2c_notify_play(NotificationApp* app, I2CNotifyKind kind);
