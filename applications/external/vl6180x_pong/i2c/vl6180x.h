#pragma once

#include <stdbool.h>
#include <stdint.h>

#define VL6180X_NO_DEVICE_FOUND_ADDRESS 0xFF

// Note: 0xFE should not be a possible returned value from the VL6180X. Using it as a way to know that it failed to read.
#define VL6180X_FAILED_DISTANCE 0xFE

uint8_t find_vl6180x_address();
void configure_vl6180x(uint8_t vl6180x_address);
uint8_t read_vl6180x_range(uint8_t vl6180x_address);
