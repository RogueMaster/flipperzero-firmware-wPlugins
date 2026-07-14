/* Copyright 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "protocol.h"
#include "protocol_prv.h"
#include "esp_loader_io.h"
#include "esp_stubs.h"
#include "slip.h"
#include <stddef.h>
#include <string.h>

static esp_loader_error_t check_response(const send_cmd_config *config);

esp_loader_error_t loader_initialize_conn(esp_loader_connect_args_t *connect_args)
{
    esp_loader_error_t err;
    int32_t trials = connect_args->trials;

    do {
        loader_port_start_timer(connect_args->sync_timeout);
        err = loader_sync_cmd();
        if (err == ESP_LOADER_ERROR_TIMEOUT) {
            if (--trials == 0) {
                return ESP_LOADER_ERROR_TIMEOUT;
            }
            loader_port_delay_ms(100);
        } else if (err != ESP_LOADER_SUCCESS) {
            return err;
        }
    } while (err != ESP_LOADER_SUCCESS);

    return err;
}

esp_loader_error_t loader_run_stub(target_chip_t target)
{
    esp_loader_error_t err;
    const esp_stub_t *stub = &esp_stub[target];

    // Download segments
    for (uint32_t seg = 0; seg < sizeof(stub->segments) / sizeof(stub->segments[0]); seg++) {
        err = esp_loader_mem_start(stub->segments[seg].addr, stub->segments[seg].size, ESP_RAM_BLOCK);
        if (err != ESP_LOADER_SUCCESS) {
            return err;
        }

        size_t remain_size = stub->segments[seg].size;
        uint8_t *data_pos = stub->segments[seg].data;
        while (remain_size > 0) {
            size_t data_size = MIN(ESP_RAM_BLOCK, remain_size);
            err = esp_loader_mem_write(data_pos, data_size);
            if (err != ESP_LOADER_SUCCESS) {
                return err;
            }
            data_pos += data_size;
            remain_size -= data_size;
        }
    }

    err = esp_loader_mem_finish(stub->header.entrypoint);
    if (err != ESP_LOADER_SUCCESS) {
        return err;
    }

    // stub loader sends a custom SLIP packet of the sequence OHAI
    uint8_t buff[4];
    size_t recv_size = 0;
    err = SLIP_receive_packet(buff, sizeof(buff) / sizeof(buff[0]), &recv_size);
    if (err != ESP_LOADER_SUCCESS) {
        return err;
    } else if (recv_size != sizeof(buff) || memcmp(buff, "OHAI", sizeof(buff) / sizeof(buff[0]))) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    esp_stub_set_running(true);

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t send_cmd(const send_cmd_config *config)
{
    RETURN_ON_ERROR(SLIP_send_delimiter());

    RETURN_ON_ERROR(SLIP_send((const uint8_t *)config->cmd, config->cmd_size));

    if (config->data != NULL && config->data_size != 0) {
        RETURN_ON_ERROR(SLIP_send((const uint8_t *)config->data, config->data_size));
    }

    RETURN_ON_ERROR(SLIP_send_delimiter());

    command_t command = ((const command_common_t *)config->cmd)->command;
    const uint8_t response_cnt = command == SYNC ? 8 : 1;

    for (uint8_t recv_cnt = 0; recv_cnt < response_cnt; recv_cnt++) {
        RETURN_ON_ERROR(check_response(config));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t check_response(const send_cmd_config *config)
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
        RETURN_ON_ERROR(SLIP_receive_packet(buf,
                                            sizeof(common_response_t) + sizeof(response_status_t) + config->resp_data_size,
                                            &packet_recv));
    } while ((response->direction != READ_DIRECTION) || (response->command != command) ||
             packet_recv < minimum_packet_recv);

    response_status_t *status = (response_status_t *)&buf[packet_recv - sizeof(response_status_t)];

    if (status->failed) {
        log_loader_internal_error(status->error);
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

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
