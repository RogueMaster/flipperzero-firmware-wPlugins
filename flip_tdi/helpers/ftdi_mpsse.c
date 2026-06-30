#include "ftdi_mpsse.h"
#include "furi.h"
#include <furi_hal.h>
#include "ftdi_gpio.h"
#include "ftdi_latency_timer.h"
#include "ftdi_mpsse_data.h"

#define TAG "FTDI_MPSSE"

typedef void (*FtdiMpsseGpioO)(uint8_t state);

#define FTDI_MPSSE_TIMEOUT 5000
#define FTDI_MPSSE_TX_RX_SIZE (4096UL)

typedef enum {
    FtdiMpsseErrorNone = 0,
    FtdiMpsseErrorTimeout,
    FtdiMpsseErrorTxOverflow,
} FtdiMpsseError;

struct FtdiMpsse {
    Ftdi* ftdi;
    uint8_t gpio_state;
    uint8_t gpio_mask;
    uint16_t data_size;
    bool is_loopback;
    bool is_div5;
    bool is_clk3phase;
    bool is_adaptive;
    FtdiMpsseError error;

    FtdiMpsseGpioO gpio_o[8];
    uint8_t gpio_mask_old;

    FtdiMpsseCallbackImmediate callback_immediate;
    void* context_immediate;

    uint8_t* data_buf;
    uint16_t data_buf_count_byte;
};

void ftdi_mpsse_gpio_set_callback(
    FtdiMpsse* ftdi_mpsse,
    FtdiMpsseCallbackImmediate callback,
    void* context) {
    ftdi_mpsse->callback_immediate = callback;
    ftdi_mpsse->context_immediate = context;
}

void ftdi_mpsse_gpio_set_direction(FtdiMpsse* ftdi_mpsse) {
    ftdi_gpio_set_direction(ftdi_mpsse->gpio_mask);
    if(ftdi_mpsse->gpio_mask & 0b00000001) {
        ftdi_mpsse->gpio_o[0] = ftdi_gpio_set_b0;
    } else {
        ftdi_mpsse->gpio_o[0] = ftdi_gpio_set_noop;
    }

    if(ftdi_mpsse->gpio_mask & 0b00000010) {
        ftdi_mpsse->gpio_o[1] = ftdi_gpio_set_b1;
    } else {
        ftdi_mpsse->gpio_o[1] = ftdi_gpio_set_noop;
    }

    if(ftdi_mpsse->gpio_mask & 0b00000100) {
        ftdi_mpsse->gpio_o[2] = ftdi_gpio_set_b2;
    } else {
        ftdi_mpsse->gpio_o[2] = ftdi_gpio_set_noop;
    }

    if(ftdi_mpsse->gpio_mask & 0b00001000) {
        ftdi_mpsse->gpio_o[3] = ftdi_gpio_set_b3;
    } else {
        ftdi_mpsse->gpio_o[3] = ftdi_gpio_set_noop;
    }

    if(ftdi_mpsse->gpio_mask & 0b00010000) {
        ftdi_mpsse->gpio_o[4] = ftdi_gpio_set_b4;
    } else {
        ftdi_mpsse->gpio_o[4] = ftdi_gpio_set_noop;
    }

    if(ftdi_mpsse->gpio_mask & 0b00100000) {
        ftdi_mpsse->gpio_o[5] = ftdi_gpio_set_b5;
    } else {
        ftdi_mpsse->gpio_o[5] = ftdi_gpio_set_noop;
    }

    if(ftdi_mpsse->gpio_mask & 0b01000000) {
        ftdi_mpsse->gpio_o[6] = ftdi_gpio_set_b6;
    } else {
        ftdi_mpsse->gpio_o[6] = ftdi_gpio_set_noop;
    }

    if(ftdi_mpsse->gpio_mask & 0b10000000) {
        ftdi_mpsse->gpio_o[7] = ftdi_gpio_set_b7;
    } else {
        ftdi_mpsse->gpio_o[7] = ftdi_gpio_set_noop;
    }
}

void ftdi_mpsse_gpio_init(FtdiMpsse* ftdi_mpsse) {
    ftdi_gpio_init(ftdi_mpsse->gpio_mask);
    ftdi_mpsse_gpio_set_direction(ftdi_mpsse);
}

static inline void ftdi_mpsse_gpio_set(FtdiMpsse* ftdi_mpsse) {
    ftdi_mpsse->gpio_o[0](ftdi_mpsse->gpio_state & 0b00000001);
    ftdi_mpsse->gpio_o[1](ftdi_mpsse->gpio_state & 0b00000010);
    ftdi_mpsse->gpio_o[2](ftdi_mpsse->gpio_state & 0b00000100);
    ftdi_mpsse->gpio_o[3](ftdi_mpsse->gpio_state & 0b00001000);
    ftdi_mpsse->gpio_o[4](ftdi_mpsse->gpio_state & 0b00010000);
    ftdi_mpsse->gpio_o[5](ftdi_mpsse->gpio_state & 0b00100000);
    ftdi_mpsse->gpio_o[6](ftdi_mpsse->gpio_state & 0b01000000);
    ftdi_mpsse->gpio_o[7](ftdi_mpsse->gpio_state & 0b10000000);
}

static inline uint8_t ftdi_mpsse_gpio_get(void) {
    uint8_t gpio_data = 0;
    gpio_data |= ftdi_gpio_get_b0();
    gpio_data |= ftdi_gpio_get_b1();
    gpio_data |= ftdi_gpio_get_b2();
    gpio_data |= ftdi_gpio_get_b3();
    gpio_data |= ftdi_gpio_get_b4();
    gpio_data |= ftdi_gpio_get_b5();
    gpio_data |= ftdi_gpio_get_b6();
    gpio_data |= ftdi_gpio_get_b7();
    return gpio_data;
}

FtdiMpsse* ftdi_mpsse_alloc(Ftdi* ftdi) {
    FtdiMpsse* ftdi_mpsse = malloc(sizeof(FtdiMpsse));
    ftdi_mpsse->ftdi = ftdi;
    ftdi_mpsse->gpio_state = 0;
    ftdi_mpsse->gpio_mask = 0;
    ftdi_mpsse->data_size = 0;
    ftdi_mpsse->is_loopback = false;
    ftdi_mpsse->is_div5 = false;
    ftdi_mpsse->is_clk3phase = false;
    ftdi_mpsse->is_adaptive = false;

    ftdi_mpsse->error = FtdiMpsseErrorNone;
    ftdi_mpsse->data_buf = malloc(FTDI_MPSSE_TX_RX_SIZE * sizeof(uint8_t));
    ftdi_mpsse->data_buf_count_byte = 0;

    ftdi_mpsse_gpio_init(ftdi_mpsse);

    return ftdi_mpsse;
}

void ftdi_mpsse_free(FtdiMpsse* ftdi_mpsse) {
    if(!ftdi_mpsse) return;
    free(ftdi_mpsse->data_buf);
    ftdi_gpio_deinit();
    free(ftdi_mpsse);
    ftdi_mpsse = NULL;
}

uint8_t ftdi_mpsse_get_data_stream(FtdiMpsse* ftdi_mpsse) {
    uint8_t data = 0;
    if(ftdi_get_rx_buf(ftdi_mpsse->ftdi, &data, 1, FTDI_MPSSE_TIMEOUT) != 1) {
        FURI_LOG_E(TAG, "Timeout");
        ftdi_mpsse->error = FtdiMpsseErrorTimeout;
    }
#ifdef FTDI_DEBUG
    FURI_LOG_RAW_I("0x%02X ", data);
#endif
    return data;
}

void ftdi_mpssse_set_data_stream(FtdiMpsse* ftdi_mpsse, uint8_t* data, uint16_t size) {
    ftdi_set_tx_buf(ftdi_mpsse->ftdi, data, size);
}

static inline void ftdi_mpsse_skeep_data(FtdiMpsse* ftdi_mpsse) {
    ftdi_mpsse->data_size++;
    while(ftdi_mpsse->data_size--) {
        if(ftdi_mpsse->error != FtdiMpsseErrorNone) return;
        ftdi_mpsse_get_data_stream(ftdi_mpsse);
    }
}

void ftdi_mpsse_get_data(FtdiMpsse* ftdi_mpsse) {
    //todo add support for tx buffer, data_size_max = 0xFF00
    ftdi_mpsse->data_size++;
    if(ftdi_mpsse->data_size > FTDI_MPSSE_TX_RX_SIZE) {
        ftdi_mpsse->error = FtdiMpsseErrorTxOverflow;
        FURI_LOG_E(TAG, "Tx buffer overflow");
        ftdi_mpsse_skeep_data(ftdi_mpsse);
        ftdi_mpsse->data_size = 0;
    }

    if(ftdi_get_rx_buf(
           ftdi_mpsse->ftdi, ftdi_mpsse->data_buf, ftdi_mpsse->data_size, FTDI_MPSSE_TIMEOUT) !=
       ftdi_mpsse->data_size) {
        FURI_LOG_E(TAG, "Timeout");
        ftdi_mpsse->error = FtdiMpsseErrorTimeout;
    }
    ftdi_mpsse->data_buf_count_byte = ftdi_mpsse->data_size;
}

static uint16_t ftdi_mpsse_get_data_size(FtdiMpsse* ftdi_mpsse) {
    return ftdi_mpsse_get_data_stream(ftdi_mpsse) |
           (uint16_t)ftdi_mpsse_get_data_stream(ftdi_mpsse) << 8;
}

static inline void ftdi_mpsse_immediate(FtdiMpsse* ftdi_mpsse) {
    if(ftdi_mpsse->callback_immediate) {
        ftdi_mpsse->callback_immediate(ftdi_mpsse->context_immediate);
    }
}
void ftdi_mpsse_state_machine(FtdiMpsse* ftdi_mpsse) {
    ftdi_mpsse->error = FtdiMpsseErrorNone;
    uint8_t data = ftdi_mpsse_get_data_stream(ftdi_mpsse);
    uint8_t gpio_state_io = 0xFF;
    switch(data) {
    case FtdiMpsseCommandsSetBitsLow: // 0x80  Change LSB GPIO output */
        ftdi_mpsse->gpio_state = ftdi_mpsse_get_data_stream(ftdi_mpsse);
        ftdi_mpsse->gpio_mask = ftdi_mpsse_get_data_stream(ftdi_mpsse);
        if(ftdi_mpsse->gpio_mask_old != ftdi_mpsse->gpio_mask) {
            ftdi_mpsse_gpio_set_direction(ftdi_mpsse);
            ftdi_mpsse->gpio_mask_old = ftdi_mpsse->gpio_mask;
        }
        ftdi_mpsse_gpio_set(ftdi_mpsse);
        //Write to GPIO
        break;
    case FtdiMpsseCommandsSetBitsHigh: // 0x82  Change MSB GPIO output */
        //Todo not supported
        ftdi_mpsse_get_data_stream(ftdi_mpsse);
        ftdi_mpsse_get_data_stream(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsGetBitsLow: // 0x81  Get LSB GPIO output */
        //Read GPIO
        gpio_state_io = ftdi_mpsse_gpio_get();
        ftdi_mpssse_set_data_stream(ftdi_mpsse, &gpio_state_io, 1);
        break;
    case FtdiMpsseCommandsGetBitsHigh: // 0x83  Get MSB GPIO output */
        //Todo not supported
        gpio_state_io = 0xFF;
        ftdi_mpssse_set_data_stream(ftdi_mpsse, &gpio_state_io, 1);
        break;
    case FtdiMpsseCommandsSendImmediate: // 0x87  Send immediate */
        //tx data to host callback
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsWriteBytesPveMsb: // 0x10  Write bytes with positive edge clock, MSB first */
        //spi mode 1,3
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_get_data(ftdi_mpsse);
        ftdi_mpsse_data_write_bytes_pve_msb(ftdi_mpsse->data_buf, ftdi_mpsse->data_buf_count_byte);
        break;
    case FtdiMpsseCommandsWriteBytesNveMsb: // 0x11  Write bytes with negative edge clock, MSB first */
        //spi mode 0,2
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_get_data(ftdi_mpsse);
        ftdi_mpsse_data_write_bytes_nve_msb(ftdi_mpsse->data_buf, ftdi_mpsse->data_buf_count_byte);
        break;
    case FtdiMpsseCommandsWriteBitsPveMsb: // 0x12  Write bits with positive edge clock, MSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_skeep_data(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsWriteBitsNveMsb: // 0x13  Write bits with negative edge clock, MSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_skeep_data(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsWriteBytesPveLsb: // 0x18  Write bytes with positive edge clock, LSB first */
        //spi mode 1,3
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_get_data(ftdi_mpsse);
        ftdi_mpsse_data_write_bytes_pve_lsb(ftdi_mpsse->data_buf, ftdi_mpsse->data_buf_count_byte);
        break;
    case FtdiMpsseCommandsWriteBytesNveLsb: // 0x19  Write bytes with negative edge clock, LSB first */
        //spi mode 0,2
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_get_data(ftdi_mpsse);
        ftdi_mpsse_data_write_bytes_nve_lsb(ftdi_mpsse->data_buf, ftdi_mpsse->data_buf_count_byte);
        break;
    case FtdiMpsseCommandsWriteBitsPveLsb: // 0x1a  Write bits with positive edge clock, LSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_skeep_data(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsWriteBitsNveLsb: // 0x1b  Write bits with negative edge clock, LSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        ftdi_mpsse_skeep_data(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsReadBytesPveMsb: // 0x20  Read bytes with positive edge clock, MSB first */
        //spi mode 1,3
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        if(ftdi_mpsse->data_size >= FTDI_MPSSE_TX_RX_SIZE) {
            ftdi_mpsse->error = FtdiMpsseErrorTxOverflow;
            FURI_LOG_E(TAG, "Tx buffer overflow");
        } else {
            ftdi_mpsse->data_size++;
            ftdi_mpsse_data_read_bytes_pve_msb(ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpssse_set_data_stream(ftdi_mpsse, ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpsse_immediate(ftdi_mpsse);
        }
        break;
    case FtdiMpsseCommandsReadBytesNveMsb: // 0x24  Read bytes with negative edge clock, MSB first */
        //spi mode 0,2
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        if(ftdi_mpsse->data_size >= FTDI_MPSSE_TX_RX_SIZE) {
            ftdi_mpsse->error = FtdiMpsseErrorTxOverflow;
            FURI_LOG_E(TAG, "Tx buffer overflow");
        } else {
            ftdi_mpsse->data_size++;
            ftdi_mpsse_data_read_bytes_nve_msb(ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpssse_set_data_stream(ftdi_mpsse, ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpsse_immediate(ftdi_mpsse);
        }

        //write data
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsReadBitsPveMsb: // 0x22  Read bits with positive edge clock, MSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //write data
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsReadBitsNveMsb: // 0x26  Read bits with negative edge clock, MSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //write data
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsReadBytesPveLsb: // 0x28  Read bytes with positive edge clock, LSB first */
        //spi mode 1,3
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        if(ftdi_mpsse->data_size >= FTDI_MPSSE_TX_RX_SIZE) {
            ftdi_mpsse->error = FtdiMpsseErrorTxOverflow;
            FURI_LOG_E(TAG, "Tx buffer overflow");
        } else {
            ftdi_mpsse->data_size++;
            ftdi_mpsse_data_read_bytes_pve_lsb(ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpssse_set_data_stream(ftdi_mpsse, ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpsse_immediate(ftdi_mpsse);
        }
        break;
    case FtdiMpsseCommandsReadBytesNveLsb: // 0x2c  Read bytes with negative edge clock, LSB first */
        //spi mode 0,2
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        if(ftdi_mpsse->data_size >= FTDI_MPSSE_TX_RX_SIZE) {
            ftdi_mpsse->error = FtdiMpsseErrorTxOverflow;
            FURI_LOG_E(TAG, "Tx buffer overflow");
        } else {
            ftdi_mpsse->data_size++;
            ftdi_mpsse_data_read_bytes_nve_lsb(ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpssse_set_data_stream(ftdi_mpsse, ftdi_mpsse->data_buf, ftdi_mpsse->data_size);
            ftdi_mpsse_immediate(ftdi_mpsse);
        }
        break;
    case FtdiMpsseCommandsReadBitsPveLsb: // 0x2a  Read bits with positive edge clock, LSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //write data
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsReadBitsNveLsb: // 0x2e  Read bits with negative edge clock, LSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //write data
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBytesPveNveMsb: // 0x31  Read/Write bytes with positive edge clock, MSB first */
        //spi mode 1,3
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_get_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBytesNvePveMsb: // 0x34  Read/Write bytes with negative edge clock, MSB first */
        //spi mode 0,2
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_get_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBitsPveNveMsb: // 0x33  Read/Write bits with positive edge clock, MSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_skeep_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBitsNvePveMsb: // 0x36  Read/Write bits with negative edge clock, MSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_skeep_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBytesPveNveLsb: // 0x39  Read/Write bytes with positive edge clock, LSB first */
        //spi mode 1,3
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_get_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBytesNvePveLsb: // 0x3c  Read/Write bytes with negative edge clock, LSB first */
        //spi mode 0,2
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_get_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBitsPveNveLsb: // 0x3b  Read/Write bits with positive edge clock, LSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_skeep_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsRwBitsNvePveLsb: // 0x3e  Read/Write bits with negative edge clock, LSB first */
        //not supported
        ftdi_mpsse->data_size = ftdi_mpsse_get_data_size(ftdi_mpsse);
        //read data
        //ftdi_mpsse_skeep_data(ftdi_mpsse);
        //ftdi_mpssse_set_data_stream(ftdi_mpsse, 0xFF, ftdi_mpsse->data_size);
        break;
    case FtdiMpsseCommandsWriteBitsTmsPve: // 0x4a  Write bits with TMS, positive edge clock */
        //not supported
        break;
    case FtdiMpsseCommandsWriteBitsTmsNve: // 0x4b  Write bits with TMS, negative edge clock */
        //not supported
        break;
    case FtdiMpsseCommandsRwBitsTmsPvePve: // 0x6a  Read/Write bits with TMS, positive edge clock, MSB first */
        //not supported
        break;
    case FtdiMpsseCommandsRwBitsTmsPveNve: // 0x6b  Read/Write bits with TMS, positive edge clock, MSB first */
        //not supported
        break;
    case FtdiMpsseCommandsRwBitsTmsNvePve: // 0x6e  Read/Write bits with TMS, negative edge clock, MSB first */
        //not supported
        break;
    case FtdiMpsseCommandsRwBitsTmsNveNve: // 0x6f  Read/Write bits with TMS, negative edge clock, MSB first */
        //not supported
        break;
    case FtdiMpsseCommandsLoopbackStart: // 0x84  Enable loopback */
        ftdi_mpsse->is_loopback = true;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsLoopbackEnd: // 0x85  Disable loopback */
        ftdi_mpsse->is_loopback = false;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsSetTckDivisor: // 0x86  Set clock */
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsDisDiv5: // 0x8a  Disable divide by 5 */
        ftdi_mpsse->is_div5 = false;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsEnDiv5: // 0x8b  Enable divide by 5 */
        ftdi_mpsse->is_div5 = true;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsEnableClk3Phase: // 0x8c  Enable 3-phase data clocking (I2C) */
        ftdi_mpsse->is_clk3phase = true;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsDisableClk3Phase: // 0x8d  Disable 3-phase data clocking */
        ftdi_mpsse->is_clk3phase = false;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsClkBitsNoData: // 0x8e  Allows JTAG clock to be output w/o data */
        //not supported
        break;
    case FtdiMpsseCommandsClkBytesNoData: // 0x8f  Allows JTAG clock to be output w/o data */
        //not supported
        break;
    case FtdiMpsseCommandsClkWaitOnHigh: // 0x94  Clock until GPIOL1 is high */
        //not supported
        break;
    case FtdiMpsseCommandsClkWaitOnLow: // 0x95  Clock until GPIOL1 is low */
        //not supported
        break;
    case FtdiMpsseCommandsEnableClkAdaptive: // 0x96  Enable JTAG adaptive clock for ARM */
        ftdi_mpsse->is_adaptive = true;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsDisableClkAdaptive: // 0x97  Disable JTAG adaptive clock */
        ftdi_mpsse->is_adaptive = false;
        ftdi_mpsse_immediate(ftdi_mpsse);
        break;
    case FtdiMpsseCommandsClkCountWaitOnHigh: // 0x9c  Clock byte cycles until GPIOL1 is high */
        //not supported
        break;
    case FtdiMpsseCommandsClkCountWaitOnLow: // 0x9d  Clock byte cycles until GPIOL1 is low */
        //not supported
        break;
    case FtdiMpsseCommandsDriveZero: // 0x9e  Drive-zero mode */
        //not supported
        break;
    case FtdiMpsseCommandsWaitOnHigh: // 0x88  Wait until GPIOL1 is high */
        //not supported
        break;
    case FtdiMpsseCommandsWaitOnLow: // 0x89  Wait until GPIOL1 is low */
        //not supported
        break;
    case FtdiMpsseCommandsReadShort: // 0x90  Read short */
        //not supported
        break;
    case FtdiMpsseCommandsReadExtended: // 0x91  Read extended */
        //not supported
        break;
    case FtdiMpsseCommandsWriteShort: // 0x92  Write short */
        //not supported
        break;
    case FtdiMpsseCommandsWriteExtended: // 0x93  Write extended */
        //not supported
        break;

    default:
        break;
    }
}
