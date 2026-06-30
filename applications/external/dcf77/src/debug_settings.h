#ifndef DCF77_DEBUG_SETTINGS_H
#define DCF77_DEBUG_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t dcf77_debug_gpio_baseband_pin_count(void);
uint8_t dcf77_debug_gpio_baseband_pin_at(size_t index);
size_t dcf77_debug_gpio_baseband_pin_index(uint8_t pin_number);
bool dcf77_debug_gpio_baseband_pin_valid(uint8_t pin_number);
uint8_t dcf77_debug_gpio_baseband_default_pin(void);
uint8_t
    dcf77_debug_gpio_baseband_step(uint8_t current_pin, int8_t direction, uint8_t reserved_pin);

size_t dcf77_debug_gpio_rf_pin_count(void);
uint8_t dcf77_debug_gpio_rf_pin_at(size_t index);
size_t dcf77_debug_gpio_rf_pin_index(uint8_t pin_number);
bool dcf77_debug_gpio_rf_pin_valid(uint8_t pin_number);
uint8_t dcf77_debug_gpio_rf_default_pin(void);
uint8_t dcf77_debug_gpio_rf_step(uint8_t current_pin, int8_t direction);

size_t dcf77_debug_gpio_rf_duty_count(void);
uint8_t dcf77_debug_gpio_rf_duty_at(size_t index);
size_t dcf77_debug_gpio_rf_duty_index(uint8_t duty_cycle);
uint8_t dcf77_debug_gpio_rf_duty_normalize(uint8_t duty_cycle);
uint8_t dcf77_debug_gpio_rf_duty_step(uint8_t duty_cycle, int8_t direction);

size_t dcf77_debug_led_color_count(void);
uint8_t dcf77_debug_led_color_at(size_t index);
size_t dcf77_debug_led_color_index(uint8_t color);
const char* dcf77_debug_led_color_label(uint8_t index);

size_t dcf77_debug_screen_mode_count(void);
const char* dcf77_debug_screen_mode_label(uint8_t index);

void dcf77_debug_format_pin_text(
    char* buffer,
    size_t buffer_size,
    uint8_t pin_number,
    bool allow_none);
void dcf77_debug_format_gpio_rf_duty_text(char* buffer, size_t buffer_size, uint8_t duty_cycle);

#endif
