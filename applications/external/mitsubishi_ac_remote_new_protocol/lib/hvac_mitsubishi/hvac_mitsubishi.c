#include "hvac_mitsubishi.h"
#define TAG "hvac_mitsubishi"

/* 

TODO:
 * Add fan mode
*/ 

static uint8_t HVAC_MITSUBISHI_DATA[11] = {
    0x52,
    0xAE,
    0xC3,
    0x26,
    0xD9,
    0b11111101,//reverse of the next bit
    0b00000010,// 00000000 - vane horizontal | auto | moving; 00000010 - other vane positions
    0b01111111, // reverse of the next bit
    0b10000000, // 100 high fan speed /  00 vane h2(vane position depends on 6th byte and these two central bits(3rd and 4th), refer to the docs to understand how it works)
    0b11011110, // reverse of the last bit
    0b00100001 // 0010 - temp 19, 0001 - AC off, 1001 - cold, 1010 - dry, 1100 - heat, 1000 - auto
};

uint8_t* hvac_mitsubishi_init() {
    uint8_t* data = malloc(sizeof(HVAC_MITSUBISHI_DATA));
    memcpy(data, HVAC_MITSUBISHI_DATA, sizeof(HVAC_MITSUBISHI_DATA));
    return data;
}

void hvac_mitsubishi_deinit(uint8_t* data) {
    free(data);
}

void hvac_mitsubishi_power(uint8_t* data, HvacMitsubishiPower power, HvacMitsubishiMode mode) {
    switch(power) {
    case HvacMitsubishiPowerOn:
        hvac_mitsubishi_set_mode(data, mode);
        break;
    case HvacMitsubishiPowerOff:
        data[10] = (data[10] & 0b11110000) + 0b00000001;
        break;
    default:
        break;
    }
}

void hvac_mitsubishi_set_mode(uint8_t* data, HvacMitsubishiMode mode) {
    data[10] &= 0b11110000;
    switch(mode) {
    case HvacMitsubishiModeHeat:
        data[10] += 0b00001100;
        break;
    case HvacMitsubishiModeCold:
        break;
    case HvacMitsubishiModeDry:
        data[10] += 0b00001010;
        break;
    case HvacMitsubishiModeAuto:
        data[10] = 0b01111000;
        break;
    default:
        data[10] += 0b00001000;
        break;
    }
}

void hvac_mitsubishi_set_temperature(uint8_t* data, uint8_t temp) {
    uint8_t cur_temp;
    if(temp > HVAC_MITSUBISHI_TEMPERATURE_MAX - 1) {
        cur_temp = HVAC_MITSUBISHI_TEMPERATURE_MAX;
    } else if(temp < HVAC_MITSUBISHI_TEMPERATURE_MIN) {
        cur_temp = HVAC_MITSUBISHI_TEMPERATURE_MIN;
    } else {
        cur_temp = temp;
    };
    data[10] = ((cur_temp - 17) << 4) + (data[10] & 0b00001111);
}

void hvac_mitsubishi_set_fan_speed(uint8_t* data, HvacMitsubishiFanSpeed speed) {
    data[8] &= 0b00011111;
    switch(speed) {
    case HvacMitsubishiFanSpeed1:
        data[8] += 0b01000000;
        break;
    case HvacMitsubishiFanSpeed2:
        data[8] += 0b01100000;
        break;
    case HvacMitsubishiFanSpeed3:
        data[8] += 0b10000000;
        break;
    case HvacMitsubishiFanSpeed4:
        data[8] += 0b11000000;
        break;
    case HvacMitsubishiFanSpeedAuto:
        break;
    case HvacMitsubishiFanSpeedSilent:
        data[8] += 0b11100000;
        break;
    }
}

void hvac_mitsubishi_set_vane(uint8_t* data, HvacMitsubishiVane mode) {
    data[8] &= 0b11100111;
    switch(mode) {
    case HvacMitsubishiVaneAuto:
        data[6] = 0b00000000;
        break;
    case HvacMitsubishiVaneH1:
        data[6] = 0b00000000;
        data[8] += 0b00011000;
        break;
    case HvacMitsubishiVaneH2:
        data[6] = 0b00000010;
        break;
    case HvacMitsubishiVaneH3:
        data[6] = 0b00000010;
        data[8] += 0b00001000;
        break;
    case HvacMitsubishiVaneH4:
        data[6] = 0b00000010;
        data[8] += 0b00010000;
        break;
    case HvacMitsubishiVaneH5:
        data[6] = 0b00000010;
        data[8] += 0b00011000;
        break;
    case HvacMitsubishiVaneAutoMove:
        data[6] = 0b00000000;
        data[8] += 0b00010000;
        break;
    }
}

void hvac_mitsubishi_send(uint8_t* data) {
    data[5] = ~data[6];
    data[7] = ~data[8];
    data[9] = ~data[10];

    uint32_t* timings = malloc(sizeof(uint32_t) * HVAC_MITSUBISHI_SAMPLES_COUNT);

    uint32_t frequency = 38000;
    float duty_cycle = 0.33;
    size_t timings_size = 0;

    timings[timings_size] = HVAC_MITSUBISHI_HDR_MARK;
    timings_size++;

    timings[timings_size] = HVAC_MITSUBISHI_HDR_SPACE;
    timings_size++;

    char mask;

    for(int i = 0; i < 11; i ++){
        for(mask = 00000001; mask > 0; mask <<= 1) {
            if(data[i] & mask) {
                timings[timings_size] = HVAC_MITSUBISHI_BIT_MARK;
                timings_size++;
                timings[timings_size] = HVAC_MITSUBISHI_ONE_SPACE;
                timings_size++;
            } else {
                timings[timings_size] = HVAC_MITSUBISHI_BIT_MARK;
                timings_size++;
                timings[timings_size] = HVAC_MITSUBISHI_ZERO_SPACE;
                timings_size++;
            }
        }
    }

    
    timings[timings_size] = HVAC_MITSUBISHI_BIT_MARK;
    timings_size++;

    infrared_send_raw_ext(timings, timings_size, true, frequency, duty_cycle);
    free(timings);
}
