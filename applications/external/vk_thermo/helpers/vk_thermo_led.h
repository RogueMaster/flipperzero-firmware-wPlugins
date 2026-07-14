#pragma once

void vk_thermo_led_set_rgb(void* context, int red, int green, int blue);

void vk_thermo_led_reset(void* context);

// Convenience functions
void vk_thermo_led_detect(void* context); // Blue flash (tag found)
void vk_thermo_led_success(void* context); // Green flash
void vk_thermo_led_error(void* context); // Red flash
