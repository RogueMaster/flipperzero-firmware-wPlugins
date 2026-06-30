#ifndef GPIO_EXPLORER_LED_STRUCT_H
#define GPIO_EXPLORER_LED_STRUCT_H

#include <furi.h>

struct gpio_explorer_led_struct {
    uint8_t led_pin_state;
    uint8_t led_pin_index;
} ;
#endif //GPIO_EXPLORER_LED_STRUCT_H