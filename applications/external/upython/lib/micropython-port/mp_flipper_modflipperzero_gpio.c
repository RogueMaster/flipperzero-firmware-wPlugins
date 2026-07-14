#include <furi_hal.h>

#include <mp_flipper_modflipperzero.h>
#include <mp_flipper_runtime.h>

#include "mp_flipper_context.h"

#define NO_VALUE (-1)

static const GpioPin* decode_pin(uint8_t pin) {
    switch(pin) {
    case MP_FLIPPER_GPIO_PIN_PC0:
        return &gpio_ext_pc0;
    case MP_FLIPPER_GPIO_PIN_PC1:
        return &gpio_ext_pc1;
    case MP_FLIPPER_GPIO_PIN_PC3:
        return &gpio_ext_pc3;
    case MP_FLIPPER_GPIO_PIN_PB2:
        return &gpio_ext_pb2;
    case MP_FLIPPER_GPIO_PIN_PB3:
        return &gpio_ext_pb3;
    case MP_FLIPPER_GPIO_PIN_PA4:
        return &gpio_ext_pa4;
    case MP_FLIPPER_GPIO_PIN_PA6:
        return &gpio_ext_pa6;
    case MP_FLIPPER_GPIO_PIN_PA7:
        return &gpio_ext_pa7;
    default:
        return NULL;
    }
}

static inline const GpioMode decode_mode(uint8_t mode) {
    switch(mode) {
    case MP_FLIPPER_GPIO_MODE_INPUT:
        return GpioModeInput;
    case MP_FLIPPER_GPIO_MODE_OUTPUT_PUSH_PULL:
        return GpioModeOutputPushPull;
    case MP_FLIPPER_GPIO_MODE_OUTPUT_OPEN_DRAIN:
        return GpioModeOutputOpenDrain;
    case MP_FLIPPER_GPIO_MODE_ANALOG:
        return GpioModeAnalog;
    case MP_FLIPPER_GPIO_MODE_INTERRUPT_FALL:
        return GpioModeInterruptFall;
    case MP_FLIPPER_GPIO_MODE_INTERRUPT_RISE:
        return GpioModeInterruptRise;
    }

    if((mode & MP_FLIPPER_GPIO_MODE_INTERRUPT_FALL) &&
       (mode & MP_FLIPPER_GPIO_MODE_INTERRUPT_RISE)) {
        return GpioModeInterruptRiseFall;
    }

    return NO_VALUE;
}

static inline const GpioPull decode_pull(uint8_t pull) {
    switch(pull) {
    case MP_FLIPPER_GPIO_PULL_NO:
        return GpioPullNo;
    case MP_FLIPPER_GPIO_PULL_UP:
        return GpioPullUp;
    case MP_FLIPPER_GPIO_PULL_DOWN:
        return GpioPullDown;
    default:
        return NO_VALUE;
    }
}

static inline const GpioSpeed decode_speed(uint8_t speed) {
    switch(speed) {
    case MP_FLIPPER_GPIO_SPEED_LOW:
        return GpioSpeedLow;
    case MP_FLIPPER_GPIO_SPEED_MEDIUM:
        return GpioSpeedMedium;
    case MP_FLIPPER_GPIO_SPEED_HIGH:
        return GpioSpeedHigh;
    case MP_FLIPPER_GPIO_SPEED_VERY_HIGH:
        return GpioSpeedVeryHigh;
    default:
        return NO_VALUE;
    }
}

inline bool mp_flipper_gpio_init_pin(
    uint8_t raw_pin,
    uint8_t raw_mode,
    uint8_t raw_pull,
    uint8_t raw_speed) {
    mp_flipper_context_t* ctx = mp_flipper_context;

    const GpioPin* pin = decode_pin(raw_pin);
    const GpioMode mode = decode_mode(raw_mode);
    const GpioPull pull = decode_pull(raw_pull);
    const GpioSpeed speed = decode_speed(raw_speed);

    if(pin == NULL || mode == NO_VALUE || pull == NO_VALUE || speed == NO_VALUE) {
        return false;
    }

    if(ctx->gpio_pins[raw_pin] & MP_FLIPPER_GPIO_PIN_BLOCKED) {
        return false;
    }

    furi_hal_gpio_init(pin, mode, pull, speed);

    if(raw_mode & (MP_FLIPPER_GPIO_MODE_INTERRUPT_FALL | MP_FLIPPER_GPIO_MODE_INTERRUPT_RISE)) {
        furi_hal_gpio_add_int_callback(pin, mp_flipper_on_gpio, (void*)raw_pin);
        furi_hal_gpio_enable_int_callback(pin);
    } else {
        furi_hal_gpio_disable_int_callback(pin);
        furi_hal_gpio_remove_int_callback(pin);
    }

    if(raw_mode == MP_FLIPPER_GPIO_MODE_ANALOG) {
        ctx->adc_handle = furi_hal_adc_acquire();

        furi_hal_adc_configure(ctx->adc_handle);
    }

    ctx->gpio_pins[raw_pin] = raw_mode;

    return true;
}

inline void mp_flipper_gpio_deinit_pin(uint8_t raw_pin) {
    const mp_flipper_context_t* ctx = mp_flipper_context;

    const GpioPin* pin = decode_pin(raw_pin);

    if(pin == NULL) {
        return;
    }

    if(ctx->gpio_pins[raw_pin] & (MP_FLIPPER_GPIO_PIN_BLOCKED | MP_FLIPPER_GPIO_PIN_OFF)) {
        return;
    }

    furi_hal_gpio_disable_int_callback(pin);
    furi_hal_gpio_remove_int_callback(pin);
    furi_hal_gpio_init_simple(pin, GpioModeAnalog);

    ctx->gpio_pins[raw_pin] = MP_FLIPPER_GPIO_PIN_OFF;
}

inline void mp_flipper_gpio_set_pin(uint8_t raw_pin, bool state) {
    const mp_flipper_context_t* ctx = mp_flipper_context;

    const GpioPin* pin = decode_pin(raw_pin);

    if(pin == NULL) {
        return;
    }

    if(ctx->gpio_pins[raw_pin] == MP_FLIPPER_GPIO_MODE_OUTPUT_PUSH_PULL ||
       ctx->gpio_pins[raw_pin] == MP_FLIPPER_GPIO_MODE_OUTPUT_OPEN_DRAIN) {
        furi_hal_gpio_write(pin, state);
    }
}

inline bool mp_flipper_gpio_get_pin(uint8_t raw_pin) {
    const mp_flipper_context_t* ctx = mp_flipper_context;

    const GpioPin* pin = decode_pin(raw_pin);

    if(pin == NULL) {
        return false;
    }

    if(ctx->gpio_pins[raw_pin] == MP_FLIPPER_GPIO_MODE_INPUT) {
        return furi_hal_gpio_read(pin);
    } else {
        return false;
    }
}
