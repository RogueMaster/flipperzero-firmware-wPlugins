#include <furi_hal.h>
#include <furi_hal_pwm.h>

#include <mp_flipper_modflipperzero.h>
#include <mp_flipper_runtime.h>

#include "mp_flipper_context.h"

#define NO_VALUE (-1)

inline static const FuriHalPwmOutputId decode_pin_to_output_id(uint8_t pin) {
    switch(pin) {
    case MP_FLIPPER_GPIO_PIN_PA4:
        return FuriHalPwmOutputIdLptim2PA4;
    case MP_FLIPPER_GPIO_PIN_PA7:
        return FuriHalPwmOutputIdTim1PA7;
    default:
        return NO_VALUE;
    }
}

inline bool mp_flipper_pwm_start(uint8_t raw_pin, uint32_t frequency, uint8_t duty) {
    const mp_flipper_context_t* ctx = mp_flipper_context;

    FuriHalPwmOutputId channel = decode_pin_to_output_id(raw_pin);

    if(channel == NO_VALUE) {
        return false;
    }

    if(ctx->gpio_pins[raw_pin] != MP_FLIPPER_GPIO_PIN_OFF &&
       ctx->gpio_pins[raw_pin] != MP_FLIPPER_GPIO_PIN_PWM) {
        return false;
    }

    if(ctx->gpio_pins[raw_pin] == MP_FLIPPER_GPIO_PIN_OFF) {
        furi_hal_pwm_start(channel, frequency, duty);
    } else {
        furi_hal_pwm_set_params(channel, frequency, duty);
    }

    ctx->gpio_pins[raw_pin] = MP_FLIPPER_GPIO_PIN_PWM;

    return true;
}

inline void mp_flipper_pwm_stop(uint8_t raw_pin) {
    const mp_flipper_context_t* ctx = mp_flipper_context;

    FuriHalPwmOutputId channel = decode_pin_to_output_id(raw_pin);

    if(channel == NO_VALUE) {
        return;
    }

    if(ctx->gpio_pins[raw_pin] != MP_FLIPPER_GPIO_PIN_PWM) {
        return;
    }

    furi_hal_pwm_stop(channel);

    ctx->gpio_pins[raw_pin] = MP_FLIPPER_GPIO_PIN_OFF;
}

inline bool mp_flipper_pwm_is_running(uint8_t raw_pin) {
    const mp_flipper_context_t* ctx = mp_flipper_context;

    FuriHalPwmOutputId channel = decode_pin_to_output_id(raw_pin);

    if(channel == NO_VALUE) {
        return false;
    }

    if(ctx->gpio_pins[raw_pin] != MP_FLIPPER_GPIO_PIN_PWM) {
        return false;
    }

    return furi_hal_pwm_is_running(channel);
}
