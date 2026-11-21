#include "z5r.h"
#include <furi.h>
#include <furi_hal.h>
#include <power/power_service/power.h>

/*
 * shortcut: assumes the Z-5R service header exposes the key EEPROM as a 24C32/24C64 on I2C
 * (32-byte pages and BaseZ5R's 0x1000/0x2000/0x4000 alias size probe point that way).
 * Not yet confirmed on hardware; if the header turns out to be an MCU protocol, replace only this file.
 * Flipper pins: 16 = SCL (PC0), 15 = SDA (PC1), 8/11/18 = GND, 9 = 3.3V, 1 = 5V (OTG).
 */
#define EEPROM_ADDR 0xA0
#define I2C_TIMEOUT 50

static const FuriHalI2cBusHandle* bus = &furi_hal_i2c_handle_external;

static void acquire(void) {
    furi_hal_i2c_acquire(bus);
    /* firmware leaves PC0/PC1 without pull-ups; ~40k internal ones are enough for a short cable */
    furi_hal_gpio_init_ex(&gpio_ext_pc0, GpioModeAltFunctionOpenDrain, GpioPullUp, GpioSpeedLow, GpioAltFn4I2C3);
    furi_hal_gpio_init_ex(&gpio_ext_pc1, GpioModeAltFunctionOpenDrain, GpioPullUp, GpioSpeedLow, GpioAltFn4I2C3);
}

bool z5r_link_present(void) {
    acquire();
    bool ok = furi_hal_i2c_is_device_ready(bus, EEPROM_ADDR, I2C_TIMEOUT);
    furi_hal_i2c_release(bus);
    return ok;
}

bool z5r_link_read(uint16_t addr, uint8_t* buf, size_t len) {
    uint8_t a[2] = {addr >> 8, addr & 0xFF};
    acquire();
    bool ok = furi_hal_i2c_trx(bus, EEPROM_ADDR, a, 2, buf, len, I2C_TIMEOUT);
    furi_hal_i2c_release(bus);
    return ok;
}

bool z5r_link_write_page(uint16_t addr, const uint8_t* page) {
    uint8_t tx[2 + Z5R_PAGE];
    tx[0] = addr >> 8;
    tx[1] = addr & 0xFF;
    memcpy(tx + 2, page, Z5R_PAGE);
    acquire();
    bool ok = furi_hal_i2c_tx(bus, EEPROM_ADDR, tx, sizeof(tx), I2C_TIMEOUT);
    if(ok) {
        ok = false; /* ACK polling: EEPROM NACKs until the internal write cycle (<=10 ms) ends */
        for(int i = 0; i < 30 && !ok; i++) {
            furi_delay_ms(1);
            ok = furi_hal_i2c_is_device_ready(bus, EEPROM_ADDR, 2);
        }
    }
    furi_hal_i2c_release(bus);
    if(!ok) return false;
    uint8_t chk[Z5R_PAGE]; /* verify: a half-written key table locks people out */
    return z5r_link_read(addr, chk, Z5R_PAGE) && !memcmp(chk, page, Z5R_PAGE);
}

void z5r_link_power(bool on5v) {
    /* the power service owns OTG and reverts direct furi_hal calls; with USB plugged in pin 1 is VBUS anyway */
    if(on5v && furi_hal_power_get_usb_voltage() > 4.5f) return; /* USB already feeds pin 1 */
    power_enable_otg(furi_record_open(RECORD_POWER), on5v);
    furi_record_close(RECORD_POWER);
}
