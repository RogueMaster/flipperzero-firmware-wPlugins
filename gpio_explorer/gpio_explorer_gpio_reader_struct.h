#ifndef GPIO_EXPLORER_GPIO_READER_STRUCT_H
#define GPIO_EXPLORER_GPIO_READER_STRUCT_H

#include <furi.h>

struct gpio_explorer_gpio_reader_struct {
    uint8_t curr_pin_state;
    uint8_t curr_pin_index;
} ;

#endif //GPIO_EXPLORER_GPIO_READER_STRUCT_H