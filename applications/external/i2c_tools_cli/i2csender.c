#include "i2csender.h"

void i2c_send(i2cSender* i2c_sender) {
    furi_hal_i2c_acquire(I2C_BUS);
    uint8_t address = i2c_sender->scanner->addresses[i2c_sender->address_idx] << 1;
    bool ok = false;
    if(i2c_sender->mode == SENDER_MODE_READ) {
        uint8_t len = i2c_sender->read_len;
        if(len < I2C_MIN_READ_LEN) len = I2C_MIN_READ_LEN;
        if(len > I2C_MAX_READ_LEN) len = I2C_MAX_READ_LEN;
        ok = furi_hal_i2c_trx(
            I2C_BUS,
            address,
            &i2c_sender->value,
            sizeof(i2c_sender->value),
            i2c_sender->recv,
            len,
            I2C_TIMEOUT);
        i2c_sender->recv_scroll = 0;
    } else {
        uint8_t tx[2] = {i2c_sender->value, i2c_sender->write_data};
        ok = furi_hal_i2c_tx(I2C_BUS, address, tx, sizeof(tx), I2C_TIMEOUT);
    }
    furi_hal_i2c_release(I2C_BUS);
    i2c_sender->error = !ok;
    i2c_sender->must_send = false;
    i2c_sender->sended = true;
}

i2cSender* i2c_sender_alloc() {
    i2cSender* i2c_sender = malloc(sizeof(i2cSender));
    i2c_sender->address_idx = 0;
    i2c_sender->value = 0;
    i2c_sender->read_len = 2;
    i2c_sender->write_data = 0;
    i2c_sender->recv_scroll = 0;
    i2c_sender->mode = SENDER_MODE_READ;
    i2c_sender->display = DISPLAY_HEX;
    i2c_sender->must_send = false;
    i2c_sender->sended = false;
    i2c_sender->error = false;
    i2c_sender->scanner = NULL;
    for(uint8_t i = 0; i < I2C_MAX_READ_LEN; i++) {
        i2c_sender->recv[i] = 0;
    }
    return i2c_sender;
}

void i2c_sender_free(i2cSender* i2c_sender) {
    furi_assert(i2c_sender);
    free(i2c_sender);
}
