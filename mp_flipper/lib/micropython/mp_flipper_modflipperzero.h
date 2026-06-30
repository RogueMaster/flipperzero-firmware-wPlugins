#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MP_FLIPPER_LED_RED (1 << 0)
#define MP_FLIPPER_LED_GREEN (1 << 1)
#define MP_FLIPPER_LED_BLUE (1 << 2)
#define MP_FLIPPER_LED_BACKLIGHT (1 << 3)

#define MP_FLIPPER_COLOR_BLACK (1 << 0)
#define MP_FLIPPER_COLOR_WHITE (1 << 1)

#define MP_FLIPPER_INPUT_BUTTON_UP (1 << 0)
#define MP_FLIPPER_INPUT_BUTTON_DOWN (1 << 1)
#define MP_FLIPPER_INPUT_BUTTON_RIGHT (1 << 2)
#define MP_FLIPPER_INPUT_BUTTON_LEFT (1 << 3)
#define MP_FLIPPER_INPUT_BUTTON_OK (1 << 4)
#define MP_FLIPPER_INPUT_BUTTON_BACK (1 << 5)
#define MP_FLIPPER_INPUT_BUTTON ((1 << 6) - 1)

#define MP_FLIPPER_INPUT_TYPE_PRESS (1 << 6)
#define MP_FLIPPER_INPUT_TYPE_RELEASE (1 << 7)
#define MP_FLIPPER_INPUT_TYPE_SHORT (1 << 8)
#define MP_FLIPPER_INPUT_TYPE_LONG (1 << 9)
#define MP_FLIPPER_INPUT_TYPE_REPEAT (1 << 10)
#define MP_FLIPPER_INPUT_TYPE ((1 << 11) - 1 - MP_FLIPPER_INPUT_BUTTON)

#define MP_FLIPPER_ALIGN_BEGIN (1 << 0)
#define MP_FLIPPER_ALIGN_CENTER (1 << 1)
#define MP_FLIPPER_ALIGN_END (1 << 2)

#define MP_FLIPPER_FONT_PRIMARY (1 << 0)
#define MP_FLIPPER_FONT_SECONDARY (1 << 1)

void mp_flipper_light_set(uint8_t raw_light, uint8_t brightness);
void mp_flipper_light_blink_start(uint8_t raw_light, uint8_t brightness, uint16_t on_time, uint16_t period);
void mp_flipper_light_blink_set_color(uint8_t raw_light);
void mp_flipper_light_blink_stop();

void mp_flipper_vibro(bool state);

#define MP_FLIPPER_SPEAKER_VOLUME_MIN MICROPY_FLOAT_CONST(0.0)
#define MP_FLIPPER_SPEAKER_VOLUME_MAX MICROPY_FLOAT_CONST(1.0)

bool mp_flipper_speaker_start(float frequency, float volume);
bool mp_flipper_speaker_set_volume(float volume);
bool mp_flipper_speaker_stop();

uint8_t mp_flipper_canvas_width();
uint8_t mp_flipper_canvas_height();
uint8_t mp_flipper_canvas_text_width(const char* text);
uint8_t mp_flipper_canvas_text_height();
void mp_flipper_canvas_draw_dot(uint8_t x, uint8_t y);
void mp_flipper_canvas_draw_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r);
void mp_flipper_canvas_draw_frame(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t r);
void mp_flipper_canvas_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void mp_flipper_canvas_draw_circle(uint8_t x, uint8_t y, uint8_t r);
void mp_flipper_canvas_draw_disc(uint8_t x, uint8_t y, uint8_t r);
void mp_flipper_canvas_set_font(uint8_t font);
void mp_flipper_canvas_set_color(uint8_t color);
void mp_flipper_canvas_set_text(uint8_t x, uint8_t y, const char* text);
void mp_flipper_canvas_set_text_align(uint8_t x, uint8_t y);
void mp_flipper_canvas_update();
void mp_flipper_canvas_clear();

void mp_flipper_on_input(uint16_t button, uint16_t type);

void mp_flipper_dialog_message_set_text(const char* text, uint8_t x, uint8_t y, uint8_t h, uint8_t v);
void mp_flipper_dialog_message_set_header(const char* text, uint8_t x, uint8_t y, uint8_t h, uint8_t v);
void mp_flipper_dialog_message_set_button(const char* text, uint8_t button);
uint8_t mp_flipper_dialog_message_show();
void mp_flipper_dialog_message_clear();

#define MP_FLIPPER_GPIO_PIN_PC0 (0)
#define MP_FLIPPER_GPIO_PIN_PC1 (1)
#define MP_FLIPPER_GPIO_PIN_PC3 (2)
#define MP_FLIPPER_GPIO_PIN_PB2 (3)
#define MP_FLIPPER_GPIO_PIN_PB3 (4)
#define MP_FLIPPER_GPIO_PIN_PA4 (5)
#define MP_FLIPPER_GPIO_PIN_PA6 (6)
#define MP_FLIPPER_GPIO_PIN_PA7 (7)

#define MP_FLIPPER_GPIO_PINS (8)

#define MP_FLIPPER_GPIO_MODE_INPUT (1 << 0)
#define MP_FLIPPER_GPIO_MODE_OUTPUT_PUSH_PULL (1 << 1)
#define MP_FLIPPER_GPIO_MODE_OUTPUT_OPEN_DRAIN (1 << 2)
#define MP_FLIPPER_GPIO_MODE_ANALOG (1 << 3)
#define MP_FLIPPER_GPIO_MODE_INTERRUPT_RISE (1 << 4)
#define MP_FLIPPER_GPIO_MODE_INTERRUPT_FALL (1 << 5)

#define MP_FLIPPER_GPIO_PULL_NO (0)
#define MP_FLIPPER_GPIO_PULL_UP (1)
#define MP_FLIPPER_GPIO_PULL_DOWN (2)

#define MP_FLIPPER_GPIO_SPEED_LOW (0)
#define MP_FLIPPER_GPIO_SPEED_MEDIUM (1)
#define MP_FLIPPER_GPIO_SPEED_HIGH (2)
#define MP_FLIPPER_GPIO_SPEED_VERY_HIGH (3)

bool mp_flipper_gpio_init_pin(uint8_t raw_pin, uint8_t raw_mode, uint8_t raw_pull, uint8_t raw_speed);
void mp_flipper_gpio_deinit_pin(uint8_t raw_pin);
void mp_flipper_gpio_set_pin(uint8_t raw_pin, bool state);
bool mp_flipper_gpio_get_pin(uint8_t raw_pin);
void mp_flipper_on_gpio(void* ctx);

uint16_t mp_flipper_adc_read_pin(uint8_t raw_pin);
float mp_flipper_adc_convert_to_voltage(uint16_t value);

bool mp_flipper_pwm_start(uint8_t raw_pin, uint32_t frequency, uint8_t duty);
void mp_flipper_pwm_stop(uint8_t raw_pin);
bool mp_flipper_pwm_is_running(uint8_t raw_pin);

#define MP_FLIPPER_INFRARED_RX_DEFAULT_TIMEOUT (1000000)
#define MP_FLIPPER_INFRARED_TX_DEFAULT_FREQUENCY (38000)
#define MP_FLIPPER_INFRARED_TX_DEFAULT_DUTY_CYCLE (0.33)

typedef uint32_t (*mp_flipper_infrared_signal_tx_provider)(void* signal, const size_t index);

uint32_t* mp_flipper_infrared_receive(uint32_t timeout, size_t* length);
bool mp_flipper_infrared_transmit(
    void* signal,
    size_t length,
    mp_flipper_infrared_signal_tx_provider callback,
    uint32_t repeat,
    uint32_t frequency,
    float duty,
    bool use_external_pin);
bool mp_flipper_infrared_is_busy();

#define MP_FLIPPER_UART_MODE_USART (0)
#define MP_FLIPPER_UART_MODE_LPUART (1)

void* mp_flipper_uart_open(uint8_t raw_mode, uint32_t baud_rate);
bool mp_flipper_uart_close(void* handle);
bool mp_flipper_uart_sync(void* handle);
size_t mp_flipper_uart_read(void* handle, void* buffer, size_t size, int* errcode);
size_t mp_flipper_uart_write(void* handle, const void* buffer, size_t size, int* errcode);
