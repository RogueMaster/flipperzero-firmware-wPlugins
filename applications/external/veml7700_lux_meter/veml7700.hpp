#pragma once

#include <stdint.h>

class VEML7700 {
public:
    VEML7700(uint8_t i2c_address_7bit);

    bool init();

    bool readLux(float& lux_value);

private:
    uint8_t _i2c_addr_8bit;
};
