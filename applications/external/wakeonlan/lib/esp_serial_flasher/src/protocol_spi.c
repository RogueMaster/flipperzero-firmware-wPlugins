/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protocol.h"
#include "protocol_prv.h"
#include "esp_loader.h"
#include "esp_loader_protocol.h"
#include "esp_targets.h"
#include "loader_log.h"
#include <stddef.h>
#include <assert.h>

static inline uint32_t port_remaining_time(const esp_loader_t *loader)
{
    return loader->_port->ops->remaining_time(loader->_port);
}

static esp_loader_error_t spi_write(esp_loader_t *loader, const uint8_t *data, uint16_t size)
{
    LOADER_LOG_HEX(loader, "SPI TX", data, size);
    return loader->_port->ops->write(loader->_port, data, size, port_remaining_time(loader));
}

static esp_loader_error_t spi_read(esp_loader_t *loader, uint8_t *data, uint16_t size)
{
    esp_loader_error_t err =
        loader->_port->ops->read(loader->_port, data, size, port_remaining_time(loader));
    if (err == ESP_LOADER_SUCCESS) {
        LOADER_LOG_HEX(loader, "SPI RX", data, size);
    }
    return err;
}

typedef struct __attribute__((packed))
{
    uint8_t cmd;
    uint8_t addr;
    uint8_t dummy;
} transaction_preamble_t;

typedef enum {
    TRANS_CMD_WRBUF = 0x01,
    TRANS_CMD_RDBUF = 0x02,
    TRANS_CMD_WRDMA = 0x03,
    TRANS_CMD_RDDMA = 0x04,
    TRANS_CMD_SEG_DONE = 0x05,
    TRANS_CMD_ENQPI = 0x06,
    TRANS_CMD_WR_DONE = 0x07,
    TRANS_CMD_CMD8 = 0x08,
    TRANS_CMD_CMD9 = 0x09,
    TRANS_CMD_CMDA = 0x0A,
    TRANS_CMD_EXQPI = 0xDD,
} transaction_cmd_t;

/* Slave protocol registers */
typedef enum {
    SLAVE_REGISTER_VER = 0,
    SLAVE_REGISTER_RXSTA = 4,
    SLAVE_REGISTER_TXSTA = 8,
    SLAVE_REGISTER_CMD = 12,
} slave_register_addr_t;

#define SLAVE_STA_TOGGLE_BIT (0x01U << 0)
#define SLAVE_STA_INIT_BIT (0x01U << 1)
#define SLAVE_STA_BUF_LENGTH_POS 2U

typedef enum {
    SLAVE_STATE_INIT = SLAVE_STA_TOGGLE_BIT | SLAVE_STA_INIT_BIT,
    SLAVE_STATE_FIRST_PACKET = SLAVE_STA_INIT_BIT,
} slave_state_t;

typedef enum {
    /* Target to host */
    SLAVE_CMD_IDLE = 0xAA,
    SLAVE_CMD_READY = 0xA5,
    /* Host to target */
    SLAVE_CMD_REBOOT = 0xFE,
    SLAVE_CMD_COMM_REINIT = 0x5A,
    SLAVE_CMD_DONE = 0x55,
} slave_cmd_t;

static esp_loader_error_t write_slave_reg(esp_loader_t *loader, const uint8_t *data, const uint32_t addr,
        const uint8_t size);
static esp_loader_error_t read_slave_reg(esp_loader_t *loader, uint8_t *out_data, const uint32_t addr,
        const uint8_t size);
static esp_loader_error_t handle_slave_state(esp_loader_t *loader, const uint32_t status_reg_addr,
        uint8_t *seq_state, bool *slave_ready, uint32_t *buf_size);
static esp_loader_error_t spi_check_response(esp_loader_t *loader, command_t cmd, uint32_t *reg_value);

static esp_loader_error_t spi_initialize_conn(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{

    for (uint8_t trial = 0; trial < connect_args->trials; trial++) {
        uint8_t slave_ready_flag __attribute__((aligned(4)));
        RETURN_ON_ERROR(read_slave_reg(loader, &slave_ready_flag, SLAVE_REGISTER_CMD,
                                       sizeof(slave_ready_flag)));

        if (slave_ready_flag != SLAVE_CMD_IDLE) {
            LOADER_LOGW(loader, "Waiting for SPI slave to be idle...");
            loader->_port->ops->delay_ms(loader->_port, 100);
        } else {
            break;
        }
    }

    const uint8_t reg_val = SLAVE_CMD_READY;
    RETURN_ON_ERROR(write_slave_reg(loader, &reg_val, SLAVE_REGISTER_CMD, sizeof(reg_val)));

    for (uint8_t trial = 0; trial < connect_args->trials; trial++) {
        uint8_t slave_ready_flag __attribute__((aligned(4)));
        RETURN_ON_ERROR(read_slave_reg(loader, &slave_ready_flag, SLAVE_REGISTER_CMD,
                                       sizeof(slave_ready_flag)));

        if (slave_ready_flag != SLAVE_CMD_READY) {
            LOADER_LOGW(loader, "Waiting for SPI slave to be ready...");
            loader->_port->ops->delay_ms(loader->_port, 100);
        } else {
            break;
        }
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t spi_send_cmd(esp_loader_t *loader, const send_cmd_config *config)
{
    command_t command = ((const command_common_t *)config->cmd)->command;
    LOADER_LOGD(loader, "CMD -> %s (0x%02x)", loader_command_name(command), (unsigned)command);

    if (config->resp_data != NULL) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    uint32_t target_buf_size;
    bool slave_ready = false;
    while (!slave_ready) {
        RETURN_ON_ERROR(handle_slave_state(loader, SLAVE_REGISTER_RXSTA, &loader->_proto_ctx.spi.slave_seq_rx,
                                           &slave_ready, &target_buf_size));
    }

    if (config->cmd_size + config->data_size > target_buf_size) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    transaction_preamble_t preamble = {.cmd = TRANS_CMD_WRDMA};

    loader->_port->ops->spi_set_cs(loader->_port, 0);
    RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)&preamble, sizeof(preamble)));
    RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)config->cmd, config->cmd_size));
    if (config->data != NULL && config->data_size != 0) {
        RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)config->data, config->data_size));
    }

    loader->_port->ops->spi_set_cs(loader->_port, 1);

    /* Terminate the write */
    loader->_port->ops->spi_set_cs(loader->_port, 0);
    preamble.cmd = TRANS_CMD_WR_DONE;
    RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)&preamble, sizeof(preamble)));
    loader->_port->ops->spi_set_cs(loader->_port, 1);

    return spi_check_response(loader, command, config->reg_value);
}


static esp_loader_error_t read_slave_reg(esp_loader_t *loader, uint8_t *out_data, const uint32_t addr,
        const uint8_t size)
{
    transaction_preamble_t preamble = {
        .cmd = TRANS_CMD_RDBUF,
        .addr = addr,
    };

    loader->_port->ops->spi_set_cs(loader->_port, 0);
    RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)&preamble, sizeof(preamble)));
    RETURN_ON_ERROR(spi_read(loader, out_data, size));
    loader->_port->ops->spi_set_cs(loader->_port, 1);

    return ESP_LOADER_SUCCESS;
}


static esp_loader_error_t write_slave_reg(esp_loader_t *loader, const uint8_t *data, const uint32_t addr,
        const uint8_t size)
{
    transaction_preamble_t preamble = {
        .cmd = TRANS_CMD_WRBUF,
        .addr = addr,
    };

    loader->_port->ops->spi_set_cs(loader->_port, 0);
    RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)&preamble, sizeof(preamble)));
    RETURN_ON_ERROR(spi_write(loader, data, size));
    loader->_port->ops->spi_set_cs(loader->_port, 1);

    return ESP_LOADER_SUCCESS;
}


static esp_loader_error_t handle_slave_state(esp_loader_t *loader, const uint32_t status_reg_addr,
        uint8_t *seq_state, bool *slave_ready, uint32_t *buf_size)
{
    uint32_t status_reg;
    RETURN_ON_ERROR(read_slave_reg(loader, (uint8_t *)&status_reg, status_reg_addr,
                                   sizeof(status_reg)));
    const slave_state_t state = status_reg & (SLAVE_STA_TOGGLE_BIT | SLAVE_STA_INIT_BIT);

    switch (state) {
    case SLAVE_STATE_INIT: {
        const uint32_t initial = 0U;
        RETURN_ON_ERROR(write_slave_reg(loader, (uint8_t *)&initial, status_reg_addr, sizeof(initial)));
        break;
    }

    case SLAVE_STATE_FIRST_PACKET: {
        *seq_state = state & SLAVE_STA_TOGGLE_BIT;
        *buf_size = status_reg >> SLAVE_STA_BUF_LENGTH_POS;
        *slave_ready = true;
        break;
    }

    default: {
        const uint8_t new_seq = state & SLAVE_STA_TOGGLE_BIT;
        if (new_seq != *seq_state) {
            *seq_state = new_seq;
            *buf_size = status_reg >> SLAVE_STA_BUF_LENGTH_POS;
            *slave_ready = true;
        }
        break;
    }
    }

    return ESP_LOADER_SUCCESS;
}


static esp_loader_error_t spi_check_response(esp_loader_t *loader, const command_t cmd, uint32_t *reg_value)
{
    uint8_t buf[sizeof(common_response_t) + sizeof(response_status_t)] __attribute__((aligned(4)));

    uint32_t target_buf_size;
    bool slave_ready = false;
    while (!slave_ready) {
        RETURN_ON_ERROR(handle_slave_state(loader, SLAVE_REGISTER_TXSTA, &loader->_proto_ctx.spi.slave_seq_tx,
                                           &slave_ready, &target_buf_size));
    }

    if (sizeof(buf) > target_buf_size) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    transaction_preamble_t preamble = {
        .cmd = TRANS_CMD_RDDMA,
    };

    loader->_port->ops->spi_set_cs(loader->_port, 0);
    RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)&preamble, sizeof(preamble)));
    RETURN_ON_ERROR(spi_read(loader, buf, sizeof(buf)));

    loader->_port->ops->spi_set_cs(loader->_port, 1);

    /* Terminate the read */
    loader->_port->ops->spi_set_cs(loader->_port, 0);
    preamble.cmd = TRANS_CMD_CMD8;
    RETURN_ON_ERROR(spi_write(loader, (const uint8_t *)&preamble, sizeof(preamble)));
    loader->_port->ops->spi_set_cs(loader->_port, 1);

    common_response_t *common = (common_response_t *)&buf[0];
    if ((common->direction != READ_DIRECTION) || (common->command != cmd)) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    response_status_t *status = (response_status_t *)&buf[sizeof(buf) - sizeof(response_status_t)];
    if (status->failed) {
        log_loader_internal_error(loader, status->error);
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }
    LOADER_LOGD(loader, "CMD <- %s OK", loader_command_name(cmd));

    if (reg_value != NULL) {
        *reg_value = common->value;
    }

    return ESP_LOADER_SUCCESS;
}

const esp_loader_protocol_ops_t spi_protocol_ops = {
    .initialize_conn   = spi_initialize_conn,
    .send_cmd          = spi_send_cmd,
    .spi_attach        = NULL,
    .recv_stub_packet  = NULL,
    .send_stub_ack     = NULL,
    .mem_begin_cmd     = NULL,
    .mem_data_cmd      = NULL,
    .mem_end_cmd       = NULL,
};

const esp_loader_protocol_ops_t *esp_loader_get_spi_ops(void)
{
    return &spi_protocol_ops;
}
