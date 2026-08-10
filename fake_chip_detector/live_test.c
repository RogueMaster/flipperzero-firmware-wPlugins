#include "live_test.h"
#include "live_adxl345.h"
#include "live_aht.h"
#include "live_apds9960.h"
#include "live_bh1750.h"
#include "live_bno055.h"
#include "live_ds3231.h"
#include "live_mlx90614.h"
#include "live_mpu6050.h"
#include "live_sht.h"
#include "live_ssd1306.h"
#include "live_vl6180x.h"

#include <furi.h>
#include <string.h>

// The registry. One line per part. Order is display order in any future list;
// lookup is by chip name, so it does not otherwise matter.
static const LiveTest* const live_tests[] = {
    &live_test_adxl345,
    &live_test_aht,
    &live_test_apds9960,
    &live_test_bh1750,
    &live_test_bno055,
    &live_test_ds3231,
    &live_test_mlx90614,
    &live_test_mpu6050,
    &live_test_mpu6500,
    &live_test_mpu9250,
    &live_test_sht,
    &live_test_ssd1306,
    &live_test_vl6180x,
};

const LiveTest* live_test_for_chip(const char* chip_name) {
    if(!chip_name) return NULL;
    for(size_t i = 0; i < COUNT_OF(live_tests); i++) {
        if(strcmp(live_tests[i]->chip, chip_name) == 0) return live_tests[i];
    }
    return NULL;
}

size_t live_test_count(void) {
    return COUNT_OF(live_tests);
}

const LiveTest* live_test_get(size_t index) {
    if(index >= COUNT_OF(live_tests)) return NULL;
    return live_tests[index];
}
