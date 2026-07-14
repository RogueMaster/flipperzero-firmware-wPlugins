#pragma once

#define NOTE_DETECT  660.0f // E5 - medium tone for tag detection
#define NOTE_SUCCESS 880.0f // A5 - higher tone for success
#define NOTE_ERROR   220.0f // A3 - lower tone for error

void vk_thermo_play_detect_sound(void* context);
void vk_thermo_play_success_sound(void* context);
void vk_thermo_play_error_sound(void* context);
void vk_thermo_stop_all_sound(void* context);
