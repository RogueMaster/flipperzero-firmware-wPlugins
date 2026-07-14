#include "nrf24.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <assert.h>
#include <string.h>

uint8_t nrf24_spi_users_count = 0;  

void nrf24_init(nrf24_device_t* device) {
    if(device->initialized) return;

    furi_hal_gpio_init(device->cs_pin, GpioModeOutputPushPull, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_write(device->cs_pin, true);

    furi_hal_gpio_init(device->ce_pin, GpioModeOutputPushPull, GpioPullUp, GpioSpeedVeryHigh);
    furi_hal_gpio_write(device->ce_pin, false);

    if(nrf24_spi_users_count == 0) {
        furi_hal_spi_bus_handle_init(device->spi_handle);
        furi_hal_spi_acquire(device->spi_handle);
    }
    nrf24_spi_users_count++;
    
    device->initialized = true;
}

void nrf24_deinit(nrf24_device_t* device) {
    if(!device->initialized) return;

    furi_hal_gpio_write(device->ce_pin, false);

    nrf24_spi_users_count--;
    if(nrf24_spi_users_count == 0) {
        furi_hal_spi_release(device->spi_handle);
        furi_hal_spi_bus_handle_deinit(device->spi_handle);
    }

    furi_hal_gpio_init(device->ce_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(device->cs_pin, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    
    device->initialized = false;
}

void nrf24_spi_trx(
    nrf24_device_t* device,
    uint8_t* tx,
    uint8_t* rx,
    uint8_t size,
    uint32_t timeout) {
    UNUSED(timeout);
    furi_hal_gpio_write(device->cs_pin, false);
    furi_hal_spi_bus_trx(device->spi_handle, tx, rx, size, nrf24_TIMEOUT);
    furi_hal_gpio_write(device->cs_pin, true);
}

uint8_t nrf24_write_reg(nrf24_device_t* device, uint8_t reg, uint8_t data) {
    uint8_t tx[2] = {W_REGISTER | (REGISTER_MASK & reg), data};
    uint8_t rx[2] = {0};
    nrf24_spi_trx(device, tx, rx, 2, nrf24_TIMEOUT);
    return rx[0];
}

uint8_t nrf24_write_buf_reg(nrf24_device_t* device, uint8_t reg, uint8_t* data, uint8_t size) {
    uint8_t tx[size + 1];
    uint8_t rx[size + 1];
    memset(rx, 0, size + 1);
    tx[0] = W_REGISTER | (REGISTER_MASK & reg);
    memcpy(&tx[1], data, size);
    nrf24_spi_trx(device, tx, rx, size + 1, nrf24_TIMEOUT);
    return rx[0];
}

uint8_t nrf24_read_reg(nrf24_device_t* device, uint8_t reg, uint8_t* data, uint8_t size) {
    uint8_t tx[size + 1];
    uint8_t rx[size + 1];
    memset(rx, 0, size + 1);
    tx[0] = R_REGISTER | (REGISTER_MASK & reg);
    memset(&tx[1], 0, size);
    nrf24_spi_trx(device, tx, rx, size + 1, nrf24_TIMEOUT);
    memcpy(data, &rx[1], size);
    return rx[0];
}

uint8_t nrf24_flush_rx(nrf24_device_t* device) {
    uint8_t tx[] = {FLUSH_RX};
    uint8_t rx[] = {0};
    nrf24_spi_trx(device, tx, rx, 1, nrf24_TIMEOUT);
    return rx[0];
}

uint8_t nrf24_flush_tx(nrf24_device_t* device) {
    uint8_t tx[] = {FLUSH_TX};
    uint8_t rx[] = {0};
    nrf24_spi_trx(device, tx, rx, 1, nrf24_TIMEOUT);
    return rx[0];
}

uint8_t nrf24_get_maclen(nrf24_device_t* device) {
    uint8_t maclen;
    nrf24_read_reg(device, REG_SETUP_AW, &maclen, 1);
    maclen &= 3;
    return maclen + 2;
}

uint8_t nrf24_set_maclen(nrf24_device_t* device, uint8_t maclen) {
    assert(maclen > 1 && maclen < 6);
    uint8_t status = 0;
    status = nrf24_write_reg(device, REG_SETUP_AW, maclen - 2);
    return status;
}

uint8_t nrf24_status(nrf24_device_t* device) {
    uint8_t status;
    uint8_t tx[] = {R_REGISTER | (REGISTER_MASK & REG_STATUS)};
    nrf24_spi_trx(device, tx, &status, 1, nrf24_TIMEOUT);
    return status;
}

uint32_t nrf24_get_rate(nrf24_device_t* device) {
    uint8_t setup = 0;
    uint32_t rate = 0;
    nrf24_read_reg(device, REG_RF_SETUP, &setup, 1);
    setup &= 0x28;
    if(setup == 0x20)
        rate = 250000; // 250kbps
    else if(setup == 0x08)
        rate = 2000000; // 2Mbps
    else if(setup == 0x00)
        rate = 1000000; // 1Mbps

    return rate;
}

uint8_t nrf24_set_rate(nrf24_device_t* device, uint32_t rate) {
    uint8_t r6 = 0;
    uint8_t status = 0;
    if(!rate) rate = 2000000;

    nrf24_read_reg(device, REG_RF_SETUP, &r6, 1); // RF_SETUP register
    r6 = r6 & (~0x28); // Clear rate fields.
    if(rate == 2000000)
        r6 = r6 | 0x08;
    else if(rate == 1000000)
        r6 = r6;
    else if(rate == 250000)
        r6 = r6 | 0x20;

    status = nrf24_write_reg(device, REG_RF_SETUP, r6); // Write new rate.
    return status;
}

uint8_t nrf24_set_txpower(nrf24_device_t* device, uint8_t level) {
    uint8_t setup;
    nrf24_read_reg(device, REG_RF_SETUP, &setup, 1);
    setup = (setup & 0xF8) | level;
    nrf24_write_reg(device, REG_RF_SETUP, setup);
    return setup;
}

void nrf24_startConstCarrier(nrf24_device_t* device, uint8_t level, uint8_t channel) {
    nrf24_deinit(device);
    nrf24_init(device);

    nrf24_set_tx_mode(device);

    nrf24_write_reg(device, REG_RF_CH, channel);

    uint8_t setup = nrf24_set_txpower(device, level);
    //nrf24_read_reg(device, REG_RF_SETUP, &setup, 1);
    //setup = (setup & 0xF8) | ((level & 0x3) << 1);
    //nrf24_write_reg(device, REG_RF_SETUP, setup);

    setup |= NRF24_CONT_WAVE | NRF24_PLL_LOCK;
    nrf24_write_reg(device, REG_RF_SETUP, setup);

    nrf24_write_reg(device, REG_EN_AA, 0x00);

    uint8_t config;
    nrf24_read_reg(device, REG_CONFIG, &config, 1);
    config &= ~NRF24_EN_CRC;
    nrf24_write_reg(device, REG_CONFIG, config);

    uint8_t dummy_payload[32];
    memset(dummy_payload, 0xFF, sizeof(dummy_payload));
    
    uint8_t tx[33];
    tx[0] = W_TX_PAYLOAD;
    memcpy(&tx[1], dummy_payload, 32);
    nrf24_spi_trx(device, tx, NULL, 33, nrf24_TIMEOUT);

    nrf24_set_tx_mode(device);
}

void nrf24_stopConstCarrier(nrf24_device_t* device) {
    uint8_t setup;
    nrf24_read_reg(device, REG_RF_SETUP, &setup, 1);
    setup &= ~(NRF24_CONT_WAVE | NRF24_PLL_LOCK);
    nrf24_write_reg(device, REG_RF_SETUP, setup);

    uint8_t config;
    nrf24_read_reg(device, REG_CONFIG, &config, 1);
    config |= NRF24_EN_CRC;
    nrf24_write_reg(device, REG_CONFIG, config);
}

uint8_t nrf24_get_chan(nrf24_device_t* device) {
    uint8_t channel = 0;
    nrf24_read_reg(device, REG_RF_CH, &channel, 1);
    return channel;
}

uint8_t nrf24_set_chan(nrf24_device_t* device, uint8_t chan) {
    uint8_t status;
    status = nrf24_write_reg(device, REG_RF_CH, chan);
    return status;
}

uint8_t nrf24_get_src_mac(nrf24_device_t* device, uint8_t* mac) {
    uint8_t size = 0;
    uint8_t status = 0;
    size = nrf24_get_maclen(device);
    status = nrf24_read_reg(device, REG_RX_ADDR_P0, mac, size);
    return status;
}

uint8_t nrf24_set_src_mac(nrf24_device_t* device, uint8_t* mac, uint8_t size) {
    uint8_t status = 0;
    uint8_t clearmac[] = {0, 0, 0, 0, 0};
    nrf24_set_maclen(device, size);
    nrf24_write_buf_reg(device, REG_RX_ADDR_P0, clearmac, 5);
    status = nrf24_write_buf_reg(device, REG_RX_ADDR_P0, mac, size);
    return status;
}

uint8_t nrf24_get_dst_mac(nrf24_device_t* device, uint8_t* mac) {
    uint8_t size = 0;
    uint8_t status = 0;
    size = nrf24_get_maclen(device);
    status = nrf24_read_reg(device, REG_TX_ADDR, mac, size);
    return status;
}

uint8_t nrf24_set_dst_mac(nrf24_device_t* device, uint8_t* mac, uint8_t size) {
    uint8_t status = 0;
    uint8_t clearmac[] = {0, 0, 0, 0, 0};
    nrf24_set_maclen(device, size);
    nrf24_write_buf_reg(device, REG_TX_ADDR, clearmac, 5);
    status = nrf24_write_buf_reg(device, REG_TX_ADDR, mac, size);
    return status;
}

uint8_t nrf24_get_packetlen(nrf24_device_t* device) {
    uint8_t len = 0;
    nrf24_read_reg(device, RX_PW_P0, &len, 1);
    return len;
}

uint8_t nrf24_set_packetlen(nrf24_device_t* device, uint8_t len) {
    uint8_t status = 0;
    status = nrf24_write_reg(device, RX_PW_P0, len);
    return status;
}

uint8_t nrf24_rxpacket(nrf24_device_t* device, uint8_t* packet, uint8_t* packetsize, bool full) {
    uint8_t status = 0;
    uint8_t size = 0;
    uint8_t tx_pl_wid[] = {R_RX_PL_WID, 0};
    uint8_t rx_pl_wid[] = {0, 0};
    uint8_t tx_cmd[33] = {0}; // 32 max payload size + 1 for command
    uint8_t tmp_packet[33] = {0};

    status = nrf24_status(device);

    if(status & RX_DR) {
        if(full)
            size = nrf24_get_packetlen(device);
        else {
            nrf24_spi_trx(device, tx_pl_wid, rx_pl_wid, 2, nrf24_TIMEOUT);
            size = rx_pl_wid[1];
        }

        tx_cmd[0] = R_RX_PAYLOAD;
        nrf24_spi_trx(device, tx_cmd, tmp_packet, size + 1, nrf24_TIMEOUT);
        nrf24_write_reg(device, REG_STATUS, RX_DR); // clear bit.
        memcpy(packet, &tmp_packet[1], size);
    } else if(status == 0) {
        nrf24_flush_rx(device);
        nrf24_write_reg(device, REG_STATUS, RX_DR); // clear bit.
    }

    *packetsize = size;
    return status;
}

uint8_t nrf24_txpacket(nrf24_device_t* device, uint8_t* payload, uint8_t size, bool ack) {
    uint8_t status = 0;
    uint8_t tx[size + 1];
    uint8_t rx[size + 1];
    memset(tx, 0, size + 1);
    memset(rx, 0, size + 1);

    if(!ack)
        tx[0] = W_TX_PAYLOAD_NOACK;
    else
        tx[0] = W_TX_PAYLOAD;

    memcpy(&tx[1], payload, size);
    nrf24_spi_trx(device, tx, rx, size + 1, nrf24_TIMEOUT);
    nrf24_set_tx_mode(device);

    while(!(status & (TX_DS | MAX_RT))) status = nrf24_status(device);

    if(status & MAX_RT) nrf24_flush_tx(device);

    nrf24_set_idle(device);
    nrf24_write_reg(device, REG_STATUS, TX_DS | MAX_RT);
    return status & TX_DS;
}

uint8_t nrf24_power_up(nrf24_device_t* device) {
    uint8_t status = 0;
    uint8_t cfg = 0;
    nrf24_read_reg(device, REG_CONFIG, &cfg, 1);
    cfg = cfg | 2;
    status = nrf24_write_reg(device, REG_CONFIG, cfg);
    furi_delay_ms(5000);
    return status;
}

uint8_t nrf24_set_idle(nrf24_device_t* device) {
    uint8_t status = 0;
    uint8_t cfg = 0;
    nrf24_read_reg(device, REG_CONFIG, &cfg, 1);
    cfg &= 0xfc; // clear bottom two bits to power down the radio
    status = nrf24_write_reg(device, REG_CONFIG, cfg);
    //nr204_write_reg(device, REG_EN_RXADDR, 0x0);
    furi_hal_gpio_write(device->ce_pin, false);
    return status;
}

uint8_t nrf24_set_rx_mode(nrf24_device_t* device) {
    uint8_t status = 0;
    uint8_t cfg = 0;
    //status = nrf24_write_reg(device, REG_CONFIG, 0x0F); // enable 2-byte CRC, PWR_UP, and PRIM_RX
    nrf24_read_reg(device, REG_CONFIG, &cfg, 1);
    cfg |= 0x03; // PWR_UP, and PRIM_RX
    status = nrf24_write_reg(device, REG_CONFIG, cfg);
    //nr204_write_reg(REG_EN_RXADDR, 0x03) // Set RX Pipe 0 and 1
    furi_hal_gpio_write(device->ce_pin, true);
    furi_delay_ms(2000);
    return status;
}

uint8_t nrf24_set_tx_mode(nrf24_device_t* device) {
    uint8_t status = 0;
    uint8_t cfg = 0;
    furi_hal_gpio_write(device->ce_pin, false);
    nrf24_write_reg(device, REG_STATUS, 0x30);
    //status = nrf24_write_reg(device, REG_CONFIG, 0x0E); // enable 2-byte CRC, PWR_UP
    nrf24_read_reg(device, REG_CONFIG, &cfg, 1);
    cfg &= 0xfe; // disable PRIM_RX
    cfg |= 0x02; // PWR_UP
    status = nrf24_write_reg(device, REG_CONFIG, cfg);
    furi_hal_gpio_write(device->ce_pin, true);
    // no need to do any delay, just fire and forget
    return status;
}

void nrf24_configure(
    nrf24_device_t* device,
    uint8_t rate,
    uint8_t* srcmac,
    uint8_t* dstmac,
    uint8_t maclen,
    uint8_t channel,
    bool noack,
    bool disable_aa) {
    assert(channel <= 125);
    assert(rate == 1 || rate == 2);
    if(rate == 2)
        rate = 8; // 2Mbps
    else
        rate = 0; // 1Mbps

    nrf24_write_reg(device, REG_CONFIG, 0x00); // Stop nRF
    nrf24_set_idle(device);
    nrf24_write_reg(device, REG_STATUS, 0x1c); // clear interrupts
    if(disable_aa)
        nrf24_write_reg(device, REG_EN_AA, 0x00); // Disable Shockburst
    else
        nrf24_write_reg(device, REG_EN_AA, 0x1F); // Enable Shockburst

    nrf24_write_reg(device, REG_DYNPD, 0x3F); // enable dynamic payload length on all pipes
    if(noack)
        nrf24_write_reg(device, REG_FEATURE, 0x05); // disable payload-with-ack, enable noack
    else {
        nrf24_write_reg(device, REG_CONFIG, 0x0C); // 2 byte CRC
        nrf24_write_reg(device, REG_FEATURE, 0x07); // enable dyn payload and ack
        nrf24_write_reg(
            device, REG_SETUP_RETR, 0x1f); // 15 retries for AA, 500us auto retransmit delay
    }

    nrf24_set_idle(device);
    nrf24_flush_rx(device);
    nrf24_flush_tx(device);

    if(maclen) nrf24_set_maclen(device, maclen);
    if(srcmac) nrf24_set_src_mac(device, srcmac, maclen);
    if(dstmac) nrf24_set_dst_mac(device, dstmac, maclen);

    nrf24_write_reg(device, REG_RF_CH, channel);
    nrf24_write_reg(device, REG_RF_SETUP, rate);
    furi_delay_ms(200);
}

void nrf24_init_promisc_mode(nrf24_device_t* device, uint8_t channel, uint8_t rate) {
    //uint8_t preamble[] = {0x55, 0x00}; // little endian
    uint8_t preamble[] = {0xAA, 0x00}; // little endian
    //uint8_t preamble[] = {0x00, 0x55}; // little endian
    //uint8_t preamble[] = {0x00, 0xAA}; // little endian
    nrf24_write_reg(device, REG_CONFIG, 0x00); // Stop nRF
    nrf24_write_reg(device, REG_STATUS, 0x1c); // clear interrupts
    nrf24_write_reg(device, REG_DYNPD, 0x0); // disable shockburst
    nrf24_write_reg(device, REG_EN_AA, 0x00); // Disable Shockburst
    nrf24_write_reg(device, REG_FEATURE, 0x05); // disable payload-with-ack, enable noack
    nrf24_set_maclen(device, 2); // shortest address
    nrf24_set_src_mac(device, preamble, 2); // set src mac to preamble bits to catch everything
    nrf24_set_packetlen(device, 32); // set max packet length
    nrf24_set_idle(device);
    nrf24_flush_rx(device);
    nrf24_flush_tx(device);
    nrf24_write_reg(device, REG_RF_CH, channel);
    nrf24_write_reg(device, REG_RF_SETUP, rate);

    // prime for RX, no checksum
    nrf24_write_reg(device, REG_CONFIG, 0x03); // PWR_UP and PRIM_RX, disable AA and CRC
    furi_hal_gpio_write(device->ce_pin, true);
    furi_delay_ms(100);
}

void hexlify(uint8_t* in, uint8_t size, char* out) {
    memset(out, 0, size * 2);
    for(int i = 0; i < size; i++)
        snprintf(out + strlen(out), sizeof(out + strlen(out)), "%02X", in[i]);
}

uint64_t bytes_to_int64(uint8_t* bytes, uint8_t size, bool bigendian) {
    uint64_t ret = 0;
    for(int i = 0; i < size; i++)
        if(bigendian)
            ret |= bytes[i] << ((size - 1 - i) * 8);
        else
            ret |= bytes[i] << (i * 8);

    return ret;
}

void int64_to_bytes(uint64_t val, uint8_t* out, bool bigendian) {
    for(int i = 0; i < 8; i++) {
        if(bigendian)
            out[i] = (val >> ((7 - i) * 8)) & 0xff;
        else
            out[i] = (val >> (i * 8)) & 0xff;
    }
}

uint32_t bytes_to_int32(uint8_t* bytes, bool bigendian) {
    uint32_t ret = 0;
    for(int i = 0; i < 4; i++)
        if(bigendian)
            ret |= bytes[i] << ((3 - i) * 8);
        else
            ret |= bytes[i] << (i * 8);

    return ret;
}

void int32_to_bytes(uint32_t val, uint8_t* out, bool bigendian) {
    for(int i = 0; i < 4; i++) {
        if(bigendian)
            out[i] = (val >> ((3 - i) * 8)) & 0xff;
        else
            out[i] = (val >> (i * 8)) & 0xff;
    }
}

uint64_t bytes_to_int16(uint8_t* bytes, bool bigendian) {
    uint16_t ret = 0;
    for(int i = 0; i < 2; i++)
        if(bigendian)
            ret |= bytes[i] << ((1 - i) * 8);
        else
            ret |= bytes[i] << (i * 8);

    return ret;
}

void int16_to_bytes(uint16_t val, uint8_t* out, bool bigendian) {
    for(int i = 0; i < 2; i++) {
        if(bigendian)
            out[i] = (val >> ((1 - i) * 8)) & 0xff;
        else
            out[i] = (val >> (i * 8)) & 0xff;
    }
}

// handle iffyness with preamble processing sometimes being a bit (literally) off
void alt_address_old(uint8_t* packet, uint8_t* altaddr) {
    uint8_t macmess_hi_b[4];
    uint8_t macmess_lo_b[2];
    uint32_t macmess_hi;
    uint16_t macmess_lo;
    uint8_t preserved;

    // get first 6 bytes into 32-bit and 16-bit variables
    memcpy(macmess_hi_b, packet, 4);
    memcpy(macmess_lo_b, packet + 4, 2);

    macmess_hi = bytes_to_int32(macmess_hi_b, true);

    //preserve least 7 bits from hi that will be shifted down to lo
    preserved = macmess_hi & 0x7f;
    macmess_hi >>= 7;

    macmess_lo = bytes_to_int16(macmess_lo_b, true);
    macmess_lo >>= 7;
    macmess_lo = (preserved << 9) | macmess_lo;
    int32_to_bytes(macmess_hi, macmess_hi_b, true);
    int16_to_bytes(macmess_lo, macmess_lo_b, true);
    memcpy(altaddr, &macmess_hi_b[1], 3);
    memcpy(altaddr + 3, macmess_lo_b, 2);
}

bool validate_address(uint8_t* addr) {
    uint8_t bad[][3] = {{0x55, 0x55}, {0xAA, 0xAA}, {0x00, 0x00}, {0xFF, 0xFF}};
    for(int i = 0; i < 4; i++)
        for(int j = 0; j < 2; j++)
            if(!memcmp(addr + j * 2, bad[i], 2)) return false;

    return true;
}

bool nrf24_sniff_address(nrf24_device_t* device, uint8_t maclen, uint8_t* address) {
    bool found = false;
    uint8_t packet[32] = {0};
    uint8_t packetsize;
    //char printit[65];
    uint8_t status = 0;
    status = nrf24_rxpacket(device, packet, &packetsize, true);
    if(status & RX_DR) {
        if(validate_address(packet)) {
            for(int i = 0; i < maclen; i++) address[i] = packet[maclen - 1 - i];

            /*
            alt_address(packet, packet);

            for(i = 0; i < maclen; i++)
                address[i + 5] = packet[maclen - 1 - i];
            */

            //memcpy(address, packet, maclen);
            //hexlify(packet, packetsize, printit);
            found = true;
        }
    }

    return found;
}

uint8_t nrf24_find_channel(
    nrf24_device_t* device,
    uint8_t* srcmac,
    uint8_t* dstmac,
    uint8_t maclen,
    uint8_t rate,
    uint8_t min_channel,
    uint8_t max_channel,
    bool autoinit) {
    uint8_t ping_packet[] = {0x0f, 0x0f, 0x0f, 0x0f}; // this can be anything, we just need an ack
    uint8_t ch = max_channel + 1; // means fail
    nrf24_configure(device, rate, srcmac, dstmac, maclen, 2, false, false);
    for(ch = min_channel; ch <= max_channel + 1; ch++) {
        nrf24_write_reg(device, REG_RF_CH, ch);
        if(nrf24_txpacket(device, ping_packet, 4, true)) break;
    }

    if(autoinit) {
        FURI_LOG_D("nrf24", "initializing radio for channel %d", ch);
        nrf24_configure(device, rate, srcmac, dstmac, maclen, ch, false, false);
        return ch;
    }

    return ch;
}

bool nrf24_check_connected(nrf24_device_t* device) {
    uint8_t status = nrf24_status(device);

    if(status != 0x00) {
        return true;
    } else {
        return false;
    }
}

uint8_t nrf24_set_mac(nrf24_device_t* device, uint8_t mac_addr, uint8_t *mac, uint8_t mlen) {
    uint8_t addr[5];
    for(int i = 0; i < mlen; i++) addr[i] = mac[mlen - i - 1];
    return nrf24_write_buf_reg(device, mac_addr, addr, mlen);
}
