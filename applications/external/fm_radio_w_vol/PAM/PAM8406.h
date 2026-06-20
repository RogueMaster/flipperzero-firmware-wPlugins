#pragma once

#include <stdbool.h>

typedef struct {
    bool powered;
    bool muted;
    bool class_d_mode;
} PAM8406State;

void pam8406_init(void);
void pam8406_apply_state(const PAM8406State* state);
void pam8406_shutdown(void);
