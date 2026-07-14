#include "debug_settings.h"

#include <stdio.h>

static const uint8_t dcf77_debug_gpio_baseband_pins[] = {
    3U,
    4U,
    5U,
    6U,
    7U,
    15U,
    16U,
    0U,
};

static const uint8_t dcf77_debug_gpio_rf_pins[] = {
    0U,
    2U,
};

static const char* const dcf77_debug_led_color_labels[] = {
    "None",
    "Orange",
    "Red",
    "Yellow",
    "Green",
    "Cyan",
    "Blue",
    "Violet",
    "Magenta",
    "Pink",
    "White",
};

static const uint8_t dcf77_debug_led_color_values[] = {
    0U,
    2U,
    1U,
    5U,
    3U,
    6U,
    4U,
    9U,
    7U,
    10U,
    8U,
};

static const char* const dcf77_debug_screen_mode_labels[] = {
    "Debug",
    "Basic",
};

size_t dcf77_debug_gpio_baseband_pin_count(void) {
    return sizeof(dcf77_debug_gpio_baseband_pins) / sizeof(dcf77_debug_gpio_baseband_pins[0]);
}

uint8_t dcf77_debug_gpio_baseband_pin_at(size_t index) {
    if(index >= dcf77_debug_gpio_baseband_pin_count()) {
        index = 0;
    }

    return dcf77_debug_gpio_baseband_pins[index];
}

size_t dcf77_debug_gpio_baseband_pin_index(uint8_t pin_number) {
    for(size_t i = 0; i < dcf77_debug_gpio_baseband_pin_count(); i++) {
        if(dcf77_debug_gpio_baseband_pins[i] == pin_number) {
            return i;
        }
    }

    return dcf77_debug_gpio_baseband_pin_index(dcf77_debug_gpio_baseband_default_pin());
}

bool dcf77_debug_gpio_baseband_pin_valid(uint8_t pin_number) {
    for(size_t i = 0; i < dcf77_debug_gpio_baseband_pin_count(); i++) {
        if(dcf77_debug_gpio_baseband_pins[i] == pin_number) {
            return true;
        }
    }

    return false;
}

uint8_t dcf77_debug_gpio_baseband_default_pin(void) {
    return 7U;
}

uint8_t
    dcf77_debug_gpio_baseband_step(uint8_t current_pin, int8_t direction, uint8_t reserved_pin) {
    const size_t count = dcf77_debug_gpio_baseband_pin_count();
    size_t index = dcf77_debug_gpio_baseband_pin_index(current_pin);

    for(size_t attempt = 0; attempt < count; attempt++) {
        if(direction > 0) {
            index = (index + 1U) % count;
        } else if(direction < 0) {
            index = (index == 0U) ? (count - 1U) : (index - 1U);
        } else {
            break;
        }

        const uint8_t candidate_pin = dcf77_debug_gpio_baseband_pin_at(index);
        if(candidate_pin == 0U || candidate_pin != reserved_pin) {
            return candidate_pin;
        }
    }

    return current_pin;
}

size_t dcf77_debug_gpio_rf_pin_count(void) {
    return sizeof(dcf77_debug_gpio_rf_pins) / sizeof(dcf77_debug_gpio_rf_pins[0]);
}

uint8_t dcf77_debug_gpio_rf_pin_at(size_t index) {
    if(index >= dcf77_debug_gpio_rf_pin_count()) {
        index = 0;
    }

    return dcf77_debug_gpio_rf_pins[index];
}

size_t dcf77_debug_gpio_rf_pin_index(uint8_t pin_number) {
    for(size_t i = 0; i < dcf77_debug_gpio_rf_pin_count(); i++) {
        if(dcf77_debug_gpio_rf_pins[i] == pin_number) {
            return i;
        }
    }

    return 0;
}

bool dcf77_debug_gpio_rf_pin_valid(uint8_t pin_number) {
    for(size_t i = 0; i < dcf77_debug_gpio_rf_pin_count(); i++) {
        if(dcf77_debug_gpio_rf_pins[i] == pin_number) {
            return true;
        }
    }

    return false;
}

uint8_t dcf77_debug_gpio_rf_default_pin(void) {
    return 0U;
}

uint8_t dcf77_debug_gpio_rf_step(uint8_t current_pin, int8_t direction) {
    const size_t count = dcf77_debug_gpio_rf_pin_count();
    size_t index = dcf77_debug_gpio_rf_pin_index(current_pin);

    if(direction > 0) {
        index = (index + 1U) % count;
    } else if(direction < 0) {
        index = (index == 0U) ? (count - 1U) : (index - 1U);
    }

    return dcf77_debug_gpio_rf_pin_at(index);
}

size_t dcf77_debug_gpio_rf_duty_count(void) {
    return 17U;
}

uint8_t dcf77_debug_gpio_rf_duty_at(size_t index) {
    if(index >= dcf77_debug_gpio_rf_duty_count()) {
        index = dcf77_debug_gpio_rf_duty_count() - 1U;
    }

    return (uint8_t)(10U + (index * 5U));
}

size_t dcf77_debug_gpio_rf_duty_index(uint8_t duty_cycle) {
    duty_cycle = dcf77_debug_gpio_rf_duty_normalize(duty_cycle);
    return (size_t)((duty_cycle - 10U) / 5U);
}

uint8_t dcf77_debug_gpio_rf_duty_normalize(uint8_t duty_cycle) {
    if(duty_cycle <= 10U) {
        return 10U;
    }

    if(duty_cycle >= 90U) {
        return 90U;
    }

    return (uint8_t)(10U + (((duty_cycle - 10U) / 5U) * 5U));
}

uint8_t dcf77_debug_gpio_rf_duty_step(uint8_t duty_cycle, int8_t direction) {
    duty_cycle = dcf77_debug_gpio_rf_duty_normalize(duty_cycle);

    if(direction > 0 && duty_cycle < 90U) {
        duty_cycle += 5U;
    } else if(direction < 0 && duty_cycle > 10U) {
        duty_cycle -= 5U;
    }

    return duty_cycle;
}

size_t dcf77_debug_led_color_count(void) {
    return sizeof(dcf77_debug_led_color_labels) / sizeof(dcf77_debug_led_color_labels[0]);
}

uint8_t dcf77_debug_led_color_at(size_t index) {
    if(index >= dcf77_debug_led_color_count()) {
        index = 0;
    }

    return dcf77_debug_led_color_values[index];
}

size_t dcf77_debug_led_color_index(uint8_t color) {
    for(size_t i = 0; i < dcf77_debug_led_color_count(); i++) {
        if(dcf77_debug_led_color_values[i] == color) {
            return i;
        }
    }

    return 2U;
}

const char* dcf77_debug_led_color_label(uint8_t index) {
    if(index >= dcf77_debug_led_color_count()) {
        index = 0;
    }

    return dcf77_debug_led_color_labels[index];
}

size_t dcf77_debug_screen_mode_count(void) {
    return sizeof(dcf77_debug_screen_mode_labels) / sizeof(dcf77_debug_screen_mode_labels[0]);
}

const char* dcf77_debug_screen_mode_label(uint8_t index) {
    if(index >= dcf77_debug_screen_mode_count()) {
        index = 0;
    }

    return dcf77_debug_screen_mode_labels[index];
}

void dcf77_debug_format_pin_text(
    char* buffer,
    size_t buffer_size,
    uint8_t pin_number,
    bool allow_none) {
    if(pin_number == 0U && allow_none) {
        snprintf(buffer, buffer_size, "None");
        return;
    }

    snprintf(buffer, buffer_size, "P%u", pin_number);
}

void dcf77_debug_format_gpio_rf_duty_text(char* buffer, size_t buffer_size, uint8_t duty_cycle) {
    snprintf(
        buffer, buffer_size, "%u%%", (unsigned int)dcf77_debug_gpio_rf_duty_normalize(duty_cycle));
}
