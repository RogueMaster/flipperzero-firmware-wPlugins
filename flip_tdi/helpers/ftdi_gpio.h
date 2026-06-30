#pragma once
#include "ftdi.h"

void ftdi_gpio_set_direction(uint8_t gpio_mask);
void ftdi_gpio_init(uint8_t gpio_mask);
void ftdi_gpio_deinit();

static inline uint8_t ftdi_gpio_get_b0(void) { // gpio_ext_pa7
    return (GPIOA->IDR & LL_GPIO_PIN_7) ? 0b00000001 : 0;
}

static inline uint8_t ftdi_gpio_get_b1(void) { // gpio_ext_pa6
    return (GPIOA->IDR & LL_GPIO_PIN_6) ? 0b00000010 : 0;
}

static inline uint8_t ftdi_gpio_get_b2(void) { // gpio_ext_pa4
    return (GPIOA->IDR & LL_GPIO_PIN_4) ? 0b00000100 : 0;
}

static inline uint8_t ftdi_gpio_get_b3(void) { // gpio_ext_pb3
    return (GPIOB->IDR & LL_GPIO_PIN_3) ? 0b00001000 : 0;
}

static inline uint8_t ftdi_gpio_get_b4(void) { // gpio_ext_pb2
    return (GPIOB->IDR & LL_GPIO_PIN_2) ? 0b00010000 : 0;
}

static inline uint8_t ftdi_gpio_get_b5(void) { // gpio_ext_pc3
    return (GPIOC->IDR & LL_GPIO_PIN_3) ? 0b00100000 : 0;
}

static inline uint8_t ftdi_gpio_get_b6(void) { // gpio_ext_pc1
    return (GPIOC->IDR & LL_GPIO_PIN_1) ? 0b01000000 : 0;
}

static inline uint8_t ftdi_gpio_get_b7(void) { // gpio_ext_pc0
    return (GPIOC->IDR & LL_GPIO_PIN_0) ? 0b10000000 : 0;
}

static inline void ftdi_gpio_set_noop(uint8_t state) { // gpio_ext_pa7
    UNUSED(state);
}

static inline void ftdi_gpio_set_b0(uint8_t state) { // gpio_ext_pa7
    if(state) {
        GPIOA->BSRR = LL_GPIO_PIN_7;
    } else {
        GPIOA->BRR = LL_GPIO_PIN_7;
    }
}

static inline void ftdi_gpio_set_b1(uint8_t state) { // gpio_ext_pa6
    if(state) {
        GPIOA->BSRR = LL_GPIO_PIN_6;
    } else {
        GPIOA->BRR = LL_GPIO_PIN_6;
    }
}

static inline void ftdi_gpio_set_b2(uint8_t state) { // gpio_ext_pa4
    if(state) {
        GPIOA->BSRR = LL_GPIO_PIN_4;
    } else {
        GPIOA->BRR = LL_GPIO_PIN_4;
    }
}

static inline void ftdi_gpio_set_b3(uint8_t state) { // gpio_ext_pb3
    if(state) {
        GPIOB->BSRR = LL_GPIO_PIN_3;
    } else {
        GPIOB->BRR = LL_GPIO_PIN_3;
    }
}

static inline void ftdi_gpio_set_b4(uint8_t state) { // gpio_ext_pb2
    if(state) {
        GPIOB->BSRR = LL_GPIO_PIN_2;
    } else {
        GPIOB->BRR = LL_GPIO_PIN_2;
    }
}

static inline void ftdi_gpio_set_b5(uint8_t state) { // gpio_ext_pc3
    if(state) {
        GPIOC->BSRR = LL_GPIO_PIN_3;
    } else {
        GPIOC->BRR = LL_GPIO_PIN_3;
    }
}

static inline void ftdi_gpio_set_b6(uint8_t state) { // gpio_ext_pc1
    if(state) {
        GPIOC->BSRR = LL_GPIO_PIN_1;
    } else {
        GPIOC->BRR = LL_GPIO_PIN_1;
    }
}

static inline void ftdi_gpio_set_b7(uint8_t state) { // gpio_ext_pc0
    if(state) {
        GPIOC->BSRR = LL_GPIO_PIN_0;
    } else {
        GPIOC->BRR = LL_GPIO_PIN_0;
    }
}
