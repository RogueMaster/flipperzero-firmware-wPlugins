#include "../../flipper_i2c.h"
#include <furi.h>
#include <furi_hal.h>
#include <string.h>

#define TAG "I2CDriver"
#define I2C_RECOVERY_CLOCKS 9
#define I2C_RECOVERY_HALF_PERIOD_US 10

static bool bms_i2c_recover_bus(void) {
    furi_hal_gpio_write(&gpio_ext_pc0, true);
    furi_hal_gpio_write(&gpio_ext_pc1, true);
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_delay_us(I2C_RECOVERY_HALF_PERIOD_US);

    if(!furi_hal_gpio_read(&gpio_ext_pc0)) {
        FURI_LOG_E(TAG, "Bus recovery aborted: SCL held LOW");
        goto cleanup;
    }

    for(uint8_t pulse = 0; pulse < I2C_RECOVERY_CLOCKS && !furi_hal_gpio_read(&gpio_ext_pc1);
        pulse++) {
        furi_hal_gpio_write(&gpio_ext_pc0, false);
        furi_delay_us(I2C_RECOVERY_HALF_PERIOD_US);
        furi_hal_gpio_write(&gpio_ext_pc0, true);
        furi_delay_us(I2C_RECOVERY_HALF_PERIOD_US);

        if(!furi_hal_gpio_read(&gpio_ext_pc0)) {
            FURI_LOG_E(TAG, "Bus recovery stopped: SCL did not return HIGH");
            goto cleanup;
        }
        FURI_LOG_D(TAG, "Bus recovery clock %u, SDA=%u", pulse + 1, furi_hal_gpio_read(&gpio_ext_pc1));
    }

    // Generate STOP: SDA rises while SCL is high.
    furi_hal_gpio_write(&gpio_ext_pc1, false);
    furi_delay_us(I2C_RECOVERY_HALF_PERIOD_US);
    furi_hal_gpio_write(&gpio_ext_pc0, true);
    furi_delay_us(I2C_RECOVERY_HALF_PERIOD_US);
    furi_hal_gpio_write(&gpio_ext_pc1, true);
    furi_delay_us(I2C_RECOVERY_HALF_PERIOD_US);

cleanup:
    bool recovered =
        furi_hal_gpio_read(&gpio_ext_pc0) && furi_hal_gpio_read(&gpio_ext_pc1);
    FURI_LOG_I(
        TAG,
        "Bus recovery result: SCL=%u SDA=%u",
        furi_hal_gpio_read(&gpio_ext_pc0),
        furi_hal_gpio_read(&gpio_ext_pc1));
    furi_hal_gpio_init(&gpio_ext_pc0, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_ext_pc1, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    return recovered;
}

uint8_t crc8_ccitt_byte(uint8_t crc, uint8_t dt) {
    uint8_t ncrc = crc ^ dt;
    for(uint8_t i = 0; i < 8; i++) {
        if((ncrc & 0x80) != 0) {
            ncrc <<= 1;
            ncrc ^= 0x07;
        } else {
            ncrc <<= 1;
        }
    }
    return ncrc & 0xFF;
}

uint8_t crc8_ccitt_compute(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for(size_t i = 0; i < len; i++) {
        crc = crc8_ccitt_byte(crc, data[i]);
    }
    return crc;
}

void bms_i2c_init(BMSI2C* i2c, uint8_t addr) {
    FURI_LOG_I(TAG, "bms_i2c_init addr=0x%02X", addr);
    i2c->dev_addr = addr;
    i2c->use_pec = false;
}

void bms_i2c_set_pec(BMSI2C* i2c, bool enable) {
    FURI_LOG_I(TAG, "bms_i2c_set_pec enable=%d", enable);
    i2c->use_pec = enable;
}

I2CResult bms_i2c_read_word(BMSI2C* i2c, uint8_t cmd, uint16_t* value) {
    if(!i2c || !value) return I2C_ERROR_INVALID;
    FURI_LOG_D(TAG, "read_word cmd=0x%02X addr=0x%02X", cmd, i2c->dev_addr);

    uint8_t address = i2c->dev_addr << 1;
    uint8_t data[3];
    size_t read_length = i2c->use_pec ? 3U : 2U;

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok;
    if(i2c->use_pec) {
        ok = furi_hal_i2c_trx(
            &furi_hal_i2c_handle_external,
            address,
            &cmd,
            1,
            data,
            read_length,
            I2C_TIMEOUT_MS);
    } else {
        ok = furi_hal_i2c_read_mem(
            &furi_hal_i2c_handle_external,
            address,
            cmd,
            data,
            read_length,
            I2C_TIMEOUT_MS);
    }
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(!ok) return I2C_ERROR_NACK;

    if(i2c->use_pec) {
        uint8_t crc_data[5] = {address, cmd, address | 1U, data[0], data[1]};
        if(crc8_ccitt_compute(crc_data, sizeof(crc_data)) != data[2]) {
            FURI_LOG_E(TAG, "read_word PEC mismatch cmd=0x%02X", cmd);
            return I2C_ERROR_INVALID;
        }
    }

    *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    return I2C_OK;
}

I2CResult bms_i2c_write_byte(BMSI2C* i2c, uint8_t cmd, uint8_t value) {
    FURI_LOG_D(TAG, "write_byte cmd=0x%02X val=0x%02X", cmd, value);
    
    uint8_t tx_buf[2] = {cmd, value};
    
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external,
        i2c->dev_addr << 1,
        tx_buf, 2,
        I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    
    FURI_LOG_D(TAG, "write_byte result=%d", ok);
    
    if(!ok) return I2C_ERROR_NACK;
    return I2C_OK;
}

I2CResult bms_i2c_write_word(BMSI2C* i2c, uint8_t cmd, uint16_t value) {
    FURI_LOG_D(TAG, "write_word cmd=0x%02X val=0x%04X", cmd, value);

    uint8_t data[2] = {(uint8_t)(value & 0xFF), (uint8_t)(value >> 8)};

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok;
    if(i2c->use_pec) {
        uint8_t tx_buf[4] = {cmd, data[0], data[1], 0};
        uint8_t crc_data[4] = {i2c->dev_addr << 1, cmd, data[0], data[1]};
        tx_buf[3] = crc8_ccitt_compute(crc_data, sizeof(crc_data));
        ok = furi_hal_i2c_tx(
            &furi_hal_i2c_handle_external,
            i2c->dev_addr << 1,
            tx_buf,
            sizeof(tx_buf),
            I2C_TIMEOUT_MS);
        FURI_LOG_D(TAG, "write_word PEC=0x%02X", tx_buf[3]);
    } else {
        ok = furi_hal_i2c_write_mem(
            &furi_hal_i2c_handle_external,
            i2c->dev_addr << 1,
            cmd,
            data,
            sizeof(data),
            I2C_TIMEOUT_MS);
    }
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(!ok) return I2C_ERROR_NACK;
    return I2C_OK;
}

I2CResult bms_i2c_write_word_raw(BMSI2C* i2c, uint8_t cmd, uint16_t value) {
    FURI_LOG_D(TAG, "write_word_raw cmd=0x%02X val=0x%04X", cmd, value);
    
    uint8_t data[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
    
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_write_mem(
        &furi_hal_i2c_handle_external,
        i2c->dev_addr << 1,
        cmd, data, 2,
        I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    
    if(!ok) return I2C_ERROR_NACK;
    return I2C_OK;
}

I2CResult bms_i2c_read_block(BMSI2C* i2c, uint8_t cmd, uint8_t* buf, uint8_t* len, uint8_t max_len) {
    if(!i2c || !buf || !len || max_len == 0 || max_len > 32) return I2C_ERROR_INVALID;

    FURI_LOG_D(TAG, "read_block cmd=0x%02X max_len=%d", cmd, max_len);

    uint8_t address = i2c->dev_addr << 1;
    uint8_t count = 0;
    uint8_t rx_buf[33];
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool command_sent = furi_hal_i2c_tx_ext(
        &furi_hal_i2c_handle_external,
        address,
        false,
        &cmd,
        1,
        FuriHalI2cBeginStart,
        FuriHalI2cEndAwaitRestart,
        I2C_TIMEOUT_MS);
    bool count_read = false;
    if(command_sent) {
        count_read = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            address,
            false,
            &count,
            1,
            FuriHalI2cBeginRestart,
            FuriHalI2cEndPause,
            I2C_TIMEOUT_MS);
    }
    bool wire_length_valid = count > 0 && count <= 32;
    bool output_length_valid = wire_length_valid && count <= max_len;
    size_t rx_len = count + (i2c->use_pec ? 1U : 0U);
    bool transfer_ok = false;
    bool stop_completed = false;
    if(count_read && wire_length_valid) {
        transfer_ok = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            address,
            false,
            rx_buf,
            rx_len,
            FuriHalI2cBeginResume,
            FuriHalI2cEndStop,
            I2C_TIMEOUT_MS);
        stop_completed = transfer_ok;
    } else if(command_sent) {
        uint8_t discard;
        stop_completed = furi_hal_i2c_rx_ext(
            &furi_hal_i2c_handle_external,
            address,
            false,
            &discard,
            1,
            count_read ? FuriHalI2cBeginResume : FuriHalI2cBeginRestart,
            FuriHalI2cEndStop,
            I2C_TIMEOUT_MS);
    }
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    if(command_sent && !stop_completed) {
        FURI_LOG_W(TAG, "read_block STOP failed, recovering bus");
        bms_i2c_recover_bus();
    }
    if(!command_sent || !count_read || !transfer_ok) {
        return wire_length_valid ? I2C_ERROR_NACK : I2C_ERROR_INVALID;
    }
    if(!output_length_valid) {
        FURI_LOG_E(TAG, "read_block invalid count=%u max=%u", count, max_len);
        return I2C_ERROR_INVALID;
    }

    if(i2c->use_pec) {
        uint8_t crc_data[36];
        crc_data[0] = address;
        crc_data[1] = cmd;
        crc_data[2] = address | 1U;
        crc_data[3] = count;
        memcpy(&crc_data[4], rx_buf, count);
        if(crc8_ccitt_compute(crc_data, count + 4) != rx_buf[count]) {
            FURI_LOG_E(TAG, "read_block PEC mismatch");
            return I2C_ERROR_INVALID;
        }
    }

    memcpy(buf, rx_buf, count);
    *len = count;
    FURI_LOG_D(TAG, "read_block: len=%d", *len);
    return I2C_OK;
}

I2CResult bms_i2c_write_block(BMSI2C* i2c, uint8_t cmd, uint8_t* data, uint8_t len) {
    if(!i2c || !data || len == 0 || len > 32) return I2C_ERROR_INVALID;

    uint8_t tx_buf[35];
    tx_buf[0] = cmd;
    tx_buf[1] = len;
    memcpy(&tx_buf[2], data, len);
    size_t tx_len = len + 2;

    if(i2c->use_pec) {
        uint8_t crc_data[35];
        crc_data[0] = i2c->dev_addr << 1;
        memcpy(&crc_data[1], tx_buf, tx_len);
        tx_buf[tx_len] = crc8_ccitt_compute(crc_data, tx_len + 1);
        tx_len++;
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_tx(
        &furi_hal_i2c_handle_external,
        i2c->dev_addr << 1,
        tx_buf,
        tx_len,
        I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    FURI_LOG_D(
        TAG,
        "write_block cmd=0x%02X len=%u PEC=%u result=%u",
        cmd,
        len,
        i2c->use_pec,
        ok);
    return ok ? I2C_OK : I2C_ERROR_NACK;
}

I2CResult bms_i2c_scan(uint8_t* found_addresses, size_t capacity, size_t* found_count) {
    if(!found_addresses || !found_count || capacity == 0) return I2C_ERROR_INVALID;

    *found_count = 0;
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    furi_delay_ms(1);

    bool scl_high = furi_hal_gpio_read(&gpio_ext_pc0);
    bool sda_high = furi_hal_gpio_read(&gpio_ext_pc1);
    FURI_LOG_I(TAG, "Bus idle after acquire: SCL=%u SDA=%u", scl_high, sda_high);
    if(scl_high && !sda_high) {
        FURI_LOG_W(TAG, "SDA held LOW, attempting safe 9-clock bus recovery");
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        bms_i2c_recover_bus();
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
        furi_delay_ms(1);
        scl_high = furi_hal_gpio_read(&gpio_ext_pc0);
        sda_high = furi_hal_gpio_read(&gpio_ext_pc1);
        FURI_LOG_I(TAG, "Bus idle after recovery: SCL=%u SDA=%u", scl_high, sda_high);
    }
    if(!scl_high || !sda_high) {
        FURI_LOG_E(
            TAG,
            "I2C bus stuck: %s%s held LOW",
            scl_high ? "" : "SCL ",
            sda_high ? "" : "SDA");
        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
        return I2C_ERROR_BUSY;
    }

    bool sbs_found = false;
    uint8_t voltage_data[2];
    for(uint8_t attempt = 0; attempt < SBS_WAKE_ATTEMPTS; attempt++) {
        if(furi_hal_i2c_read_mem(
               &furi_hal_i2c_handle_external,
               SBS_I2C_ADDRESS << 1,
               0x09,
               voltage_data,
               sizeof(voltage_data),
               SBS_WAKE_TIMEOUT_MS)) {
            found_addresses[(*found_count)++] = SBS_I2C_ADDRESS;
            sbs_found = true;
            uint16_t voltage =
                (uint16_t)voltage_data[0] | ((uint16_t)voltage_data[1] << 8);
            FURI_LOG_I(
                TAG,
                "SBS wake probe found 0x%02X, Voltage response: %u mV",
                SBS_I2C_ADDRESS,
                voltage);
            break;
        }

        FURI_LOG_D(TAG, "SBS wake probe attempt %u failed", attempt + 1);
        furi_delay_ms(20);
    }

    for(uint8_t addr = I2C_SCAN_FIRST_ADDRESS; addr <= I2C_SCAN_LAST_ADDRESS; addr++) {
        if(sbs_found && addr == SBS_I2C_ADDRESS) continue;
        if(furi_hal_i2c_is_device_ready(
               &furi_hal_i2c_handle_external, addr << 1, I2C_SCAN_TIMEOUT_MS)) {
            if(*found_count < capacity) {
                found_addresses[*found_count] = addr;
                (*found_count)++;
            }
            FURI_LOG_I(TAG, "Found device at 7-bit address 0x%02X", addr);
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return I2C_OK;
}

I2CResult bms_i2c_read_register(uint8_t address, uint8_t reg, uint8_t* value) {
    if(!value || address < I2C_SCAN_FIRST_ADDRESS || address > I2C_SCAN_LAST_ADDRESS) {
        return I2C_ERROR_INVALID;
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    bool ok = furi_hal_i2c_read_mem(
        &furi_hal_i2c_handle_external,
        address << 1,
        reg,
        value,
        1,
        I2C_TIMEOUT_MS);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    return ok ? I2C_OK : I2C_ERROR_NACK;
}

I2CResult bms_i2c_read_after_write(BMSI2C* i2c, uint8_t cmd, uint8_t* buf, uint8_t* len, uint8_t max_len) {
    UNUSED(i2c);
    UNUSED(buf);
    UNUSED(len);
    UNUSED(max_len);
    UNUSED(cmd);
    return I2C_ERROR_INVALID;
}
