#include "ftdi_latency_timer.h"
#include "furi.h"
#include <furi_hal.h>

#include <furi_hal_resources.h>

#define TAG "FTDI_GPIO"

#define gpio_b0 (gpio_ext_pa7)
#define gpio_b1 (gpio_ext_pa6)
#define gpio_b2 (gpio_ext_pa4)
#define gpio_b3 (gpio_ext_pb3)
#define gpio_b4 (gpio_ext_pb2)
#define gpio_b5 (gpio_ext_pc3)
#define gpio_b6 (gpio_ext_pc1)
#define gpio_b7 (gpio_ext_pc0)

void ftdi_gpio_set_direction(uint8_t gpio_mask) {
    // gpio_ext_pa7
    if(gpio_mask & 0b00000001) {
        LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_7, LL_GPIO_MODE_INPUT);
    }
    // gpio_ext_pa6
    if(gpio_mask & 0b00000010) {
        LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_6, LL_GPIO_MODE_INPUT);
    }
    // gpio_ext_pa4
    if(gpio_mask & 0b00000100) {
        LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_INPUT);
    }
    // gpio_ext_pb3
    if(gpio_mask & 0b00001000) {
        LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_3, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_3, LL_GPIO_MODE_INPUT);
    }
    // gpio_ext_pb2
    if(gpio_mask & 0b00010000) {
        LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_2, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_2, LL_GPIO_MODE_INPUT);
    }
    // gpio_ext_pc3
    if(gpio_mask & 0b00100000) {
        LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_3, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_3, LL_GPIO_MODE_INPUT);
    }
    // gpio_ext_pc1
    if(gpio_mask & 0b01000000) {
        LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_1, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_1, LL_GPIO_MODE_INPUT);
    }
    // gpio_ext_pc0
    if(gpio_mask & 0b10000000) {
        LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_0, LL_GPIO_MODE_OUTPUT);
    } else {
        LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_0, LL_GPIO_MODE_INPUT);
    }
}

void ftdi_gpio_init(uint8_t gpio_mask) {
    furi_hal_gpio_init(&gpio_b0, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_b1, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_b2, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_b3, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_b4, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_b5, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_b6, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);
    furi_hal_gpio_init(&gpio_b7, GpioModeInput, GpioPullDown, GpioSpeedVeryHigh);

    ftdi_gpio_set_direction(gpio_mask);
}

void ftdi_gpio_deinit() {
    furi_hal_gpio_init(&gpio_b0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_b1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_b2, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_b3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_b4, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_b5, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_b6, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_b7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}
