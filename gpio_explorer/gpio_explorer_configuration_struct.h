#ifndef GPIO_EXPLORER_CONFIGURATION_STRUCT_H
#define GPIO_EXPLORER_CONFIGURATION_STRUCT_H

#include <furi.h>

struct gpio_explorer_configure_struct {
    uint8_t rgb_pin_r_index;
    uint8_t rgb_pin_g_index;
    uint8_t rgb_pin_b_index;
    uint8_t led_pin_index;
} ;

#endif //GPIO_EXPLORER_CONFIGURATION_STRUCT_H