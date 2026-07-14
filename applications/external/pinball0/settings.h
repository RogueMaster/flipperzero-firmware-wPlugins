#pragma once

typedef struct {
    bool sound_enabled;
    bool vibrate_enabled;
    bool led_enabled;
    bool debug_mode;

    int selected_setting;
    int max_settings;
} PinballSettings;

struct PinballApp;
// Read game settings from .pinball0.conf
void pinball_load_settings(PinballApp& pb);

// Save game settings to .pinball0.conf
void pinball_save_settings(PinballApp& pb);
