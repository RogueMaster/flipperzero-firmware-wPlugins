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
#include "slip.h"
#include "loader_log.h"
#include <stddef.h>
#include <string.h>

static esp_loader_error_t uart_check_response(esp_loader_t *loader, const send_cmd_config *config);

static esp_loader_error_t uart_initialize_conn(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{
    esp_loader_error_t err;
    int32_t trials = connect_args->trials;

    do {
        loader->_port->ops->start_timer(loader->_port, connect_args->sync_timeout);
        err = loader_sync_cmd(loader);
        if (err == ESP_LOADER_ERROR_TIMEOUT) {
            if (--trials == 0) {
                return ESP_LOADER_ERROR_TIMEOUT;
            }
            loader->_port->ops->delay_ms(loader->_port, 100);
        } else if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    } while (err != ESP_LOADER_SUCCESS);

    return err;
}

static esp_loader_error_t uart_spi_attach(esp_loader_t *loader, uint32_t config)
{
    return loader_spi_attach_cmd(loader, config);
}

static esp_loader_error_t uart_send_cmd(esp_loader_t *loader, const send_cmd_config *config)
{
    command_t command = ((const command_common_t *)config->cmd)->command;
    LOADER_LOGD(loader, "CMD -> %s (0x%02x)", loader_command_name(command), (unsigned)command);

    RETURN_ON_ERROR(SLIP_send_delimiter(loader));

    RETURN_ON_ERROR(SLIP_send(loader, (const uint8_t *)config->cmd, config->cmd_size));

    if (config->data != NULL && config->data_size != 0) {
        RETURN_ON_ERROR(SLIP_send(loader, (const uint8_t *)config->data, config->data_size));
    }

    RETURN_ON_ERROR(SLIP_send_delimiter(loader));

    const uint8_t response_cnt = command == SYNC ? 8 : 1;

    for (uint8_t recv_cnt = 0; recv_cnt < response_cnt; recv_cnt++) {
        RETURN_ON_ERROR(uart_check_response(loader, config));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t uart_check_response(esp_loader_t *loader, const send_cmd_config *config)
{
    uint8_t buf[sizeof(common_response_t) + sizeof(response_status_t) + MAX_RESP_DATA_SIZE];

    common_response_t *response = (common_response_t *)&buf[0];
    command_t command = ((const command_common_t *)config->cmd)->command;

    // If the command has fixed response data size, require all of it to be received
    uint32_t minimum_packet_recv = sizeof(common_response_t) + sizeof(response_status_t);
    if (config->resp_data_recv_size == NULL) {
        minimum_packet_recv += config->resp_data_size;
    }

    size_t packet_recv = 0;
    do {
        RETURN_ON_ERROR(SLIP_receive_packet(loader, buf,
                                            sizeof(common_response_t) + sizeof(response_status_t) + config->resp_data_size,
                                            &packet_recv));
    } while ((response->direction != READ_DIRECTION) || (response->command != command) ||
             packet_recv < minimum_packet_recv);

    response_status_t *status = (response_status_t *)&buf[packet_recv - sizeof(response_status_t)];

    if (status->failed) {
        log_loader_internal_error(loader, status->error);
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }
    LOADER_LOGD(loader, "CMD <- %s OK", loader_command_name(command));

    if (config->reg_value != NULL) {
        *config->reg_value = response->value;
    }

    if (config->resp_data != NULL) {
        const size_t resp_data_size = packet_recv - sizeof(common_response_t) - sizeof(response_status_t);

        memcpy(config->resp_data, &buf[sizeof(common_response_t)], resp_data_size);

        if (config->resp_data_recv_size != NULL) {
            *config->resp_data_recv_size = resp_data_size;
        }
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t uart_recv_stub_packet(esp_loader_t *loader, uint8_t *dest,
        size_t max_size, size_t *recv_size)
{
    return SLIP_receive_packet(loader, dest, max_size, recv_size);
}

static esp_loader_error_t uart_send_stub_ack(esp_loader_t *loader, uint32_t bytes_recv)
{
    RETURN_ON_ERROR(SLIP_send_delimiter(loader));
    RETURN_ON_ERROR(SLIP_send(loader, (const uint8_t *)&bytes_recv, sizeof(bytes_recv)));
    RETURN_ON_ERROR(SLIP_send_delimiter(loader));
    return ESP_LOADER_SUCCESS;
}


static esp_loader_error_t uart_mem_begin_cmd(esp_loader_t *loader, uint32_t offset,
        uint32_t size, uint32_t blocks_to_write,
        uint32_t block_size)
{
    if (loader->_stub_running) {
        esp_loader_connect_args_t connect_args = ESP_LOADER_CONNECT_DEFAULT();
        loader->_port->ops->enter_bootloader(loader->_port);
        RETURN_ON_ERROR(loader->_protocol->initialize_conn(loader, &connect_args));
        loader->_stub_running = false;
    }

    uint32_t seq = 0;
    return loader_mem_begin_cmd(loader, &seq, offset, size, blocks_to_write, block_size);
}

const esp_loader_protocol_ops_t uart_protocol_ops = {
    .initialize_conn   = uart_initialize_conn,
    .send_cmd          = uart_send_cmd,
    .spi_attach        = uart_spi_attach,
    .recv_stub_packet  = uart_recv_stub_packet,
    .send_stub_ack     = uart_send_stub_ack,
    .mem_begin_cmd     = uart_mem_begin_cmd,
    .mem_data_cmd      = NULL,
    .mem_end_cmd       = NULL,
};

const esp_loader_protocol_ops_t *esp_loader_get_serial_ops(void)
{
    return &uart_protocol_ops;
}
