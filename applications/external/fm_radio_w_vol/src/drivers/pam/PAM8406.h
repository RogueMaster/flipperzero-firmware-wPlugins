/**
 * @file PAM8406.h
 * @brief PAM8406 Class-D amplifier GPIO control (enable, mute, mode).
 */
#pragma once

#include <stdbool.h>

typedef struct {
    bool powered;
    bool muted;
    bool class_d_mode;
} PAM8406State;

/** Configure amplifier control GPIO pins. */
void pam8406_init(void);

/** Drive power, mute, and Class-D mode from @p state. */
void pam8406_apply_state(const PAM8406State* state);

/** Power down and mute the amplifier. */
void pam8406_shutdown(void);
