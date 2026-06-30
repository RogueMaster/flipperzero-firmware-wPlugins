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
#include "esp_loader.h"
#include "esp_targets.h"
#include "sip.h"
#include "esp_stubs.h"
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

/* SDIO CCR registers - common to all SDIO devices */
#define SD_IO_CCCR_FN_ENABLE 0x02
#define SD_IO_CCR_FN_ENABLE_FUNC1_EN (1 << 1)
#define SD_IO_CCCR_FN_READY 0x03
#define SD_BLOCK_SIZE 512

/* Stub defines */
#define STUB_CMD_REG 0x006C
#define STUB_INT_ST_REG 0x0058
#define STUB_INT_CLR_REG 0x00D4
#define STUB_PKT_LEN_REG 0x0060
#define STUB_INT_BIT0 (1 << 0)
#define STUB_INT_NEW_PKT (1 << 23)
#define STUB_MAX_TRANSACTION_SIZE 4096
#define RX_BYTE_MASK 0xFFFFF
#define STUB_BOOT_TIMEOUT 500
#define STUB_DEFAULT_TIMEOUT 100

typedef enum {
    STUB_BUSY = 0,
    STUB_READY = 1,
} stub_status_t;

typedef struct {
    bool sdio_supported;
    /* SLC Host Registers. Some of these are reserved for host/slave communication without
       on-target concurrency issues */
    /* The host writes into CONF registers to send data to the slave, and reads back from STATE registers */
    /* Uses a different target base address from SLC slave registers, read or write with regular transactions */
    uint32_t slchost_date_addr;
    uint32_t slchost_date_expected_val;
    uint32_t slchost_state_w0_addr;
    uint32_t slchost_conf_w5_addr;
    uint32_t slchost_win_cmd_addr;
    uint32_t slchost_packet_space_end;

    /* SLC Registers */
    /* Use host registers for writing/reading to these using slave_read/write_register() */
    uint32_t slc_conf1_addr;
    uint32_t slc_len_conf_addr;
    uint32_t slc_conf1_tx_stitch_en;
    uint32_t slc_conf1_rx_stitch_en;
    uint32_t slc_len_conf_tx_packet_load_en;
} esp_target_t;

typedef struct {
    esp_loader_bin_header_t header;
    esp_loader_bin_segment_t segments[3];
} esp_stub_t;

extern const esp_stub_t esp_stub[ESP_MAX_CHIP];

static const esp_target_t esp_target[ESP_MAX_CHIP] = {

    // ESP8266
    {},

    // ESP32
    {
        .sdio_supported = true,
        .slchost_date_addr = 0x178,
        .slchost_date_expected_val = 0x16022500,
        .slchost_state_w0_addr = 0x64,
        .slchost_conf_w5_addr = 0x80,
        .slchost_win_cmd_addr = 0x84,
        .slchost_packet_space_end = 0x1f800,
        .slc_conf1_addr = 0x60,
        .slc_len_conf_addr = 0xE4,
        .slc_conf1_tx_stitch_en = (1 << 5),
        .slc_conf1_rx_stitch_en = (1 << 6),
        .slc_len_conf_tx_packet_load_en = (1 << 24),
    },

    // ESP32S2
    {},

    // ESP32C3
    {},

    // ESP32S3
    {},

    // ESP32C2
    {},

    // ESP32C5
    {},

    // ESP32H2
    {},

    // ESP32C6
    {
        .sdio_supported = true,
        .slchost_date_addr = 0x178,
        .slchost_date_expected_val = 0x21060700,
        .slchost_state_w0_addr = 0x64,
        .slchost_conf_w5_addr = 0x80,
        .slchost_win_cmd_addr = 0x84,
        .slchost_packet_space_end = 0x1f800,
        .slc_conf1_addr = 0x70,
        .slc_len_conf_addr = 0xF4,
        .slc_conf1_tx_stitch_en = (1 << 5),
        .slc_conf1_rx_stitch_en = (1 << 6),
        .slc_len_conf_tx_packet_load_en = (1 << 24),
    },

    // ESP32P4
    {},
};

static uint8_t s_block_buf[SD_BLOCK_SIZE] __attribute__((aligned(4)));
static uint32_t s_sip_seq_tx;
static target_chip_t s_target_chip = ESP_UNKNOWN_CHIP;

static esp_loader_error_t slave_read_register(const uint32_t addr, uint32_t *reg)
{
    assert(addr >> 2 <= 0x7F);

    uint8_t buf[4] __attribute__((aligned(4))) = {0};
    buf[0] = (addr >> 2) & 0x7F;
    buf[1] = 0x80;

    RETURN_ON_ERROR(loader_port_write(1,
                                      esp_target[s_target_chip].slchost_win_cmd_addr,
                                      buf,
                                      sizeof(buf),
                                      loader_port_remaining_time()));

    return loader_port_read(1,
                            esp_target[s_target_chip].slchost_state_w0_addr,
                            (uint8_t *)reg,
                            sizeof(uint32_t),
                            loader_port_remaining_time());
}

static esp_loader_error_t slave_write_register(const uint32_t addr, uint32_t reg_val)
{
    assert(addr >> 2 <= 0x7F);

    uint8_t buf[8] __attribute__((aligned(4))) = {0};
    memcpy(buf, &reg_val, 4);
    buf[4] = (addr >> 2) & 0x7F;
    buf[5] = 0xC0;

    return loader_port_write(1,
                             esp_target[s_target_chip].slchost_conf_w5_addr,
                             buf,
                             sizeof(buf),
                             loader_port_remaining_time());
}

static esp_loader_error_t slave_wait_ready(const uint32_t timeout)
{
    uint8_t reg __attribute__((aligned(4))) = 0;

    loader_port_start_timer(timeout);
    while ((reg & SD_IO_CCR_FN_ENABLE_FUNC1_EN) == 0) {
        if (loader_port_remaining_time() == 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }

        RETURN_ON_ERROR(loader_port_read(0, SD_IO_CCCR_FN_READY,
                                         &reg, sizeof(reg),
                                         loader_port_remaining_time()));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_init_card(uint32_t trials)
{
    for (uint8_t trial = 0; trial < trials; trial++) {

        if (loader_port_sdio_card_init() != ESP_LOADER_SUCCESS) {
            loader_port_debug_print("Retrying card connection...");
            loader_port_delay_ms(100);
        } else {
            return ESP_LOADER_SUCCESS;
        }
    }

    return ESP_LOADER_ERROR_FAIL;
}

static esp_loader_error_t slave_init_io(void)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    // Enable function 1, we will use it for upload
    // The alignment requirement comes from the esp port DMA requirements
    uint8_t reg __attribute__((aligned(4)));
    RETURN_ON_ERROR(loader_port_read(0, SD_IO_CCCR_FN_ENABLE,
                                     &reg, sizeof(reg),
                                     loader_port_remaining_time()));

    reg |= SD_IO_CCR_FN_ENABLE_FUNC1_EN;
    uint8_t expected_val = reg;
    RETURN_ON_ERROR(loader_port_write(0, SD_IO_CCCR_FN_ENABLE,
                                      &reg, sizeof(reg),
                                      loader_port_remaining_time()));

    // Read back to verify
    RETURN_ON_ERROR(loader_port_read(0, SD_IO_CCCR_FN_ENABLE,
                                     &reg, sizeof(reg),
                                     loader_port_remaining_time()));

    if (reg != expected_val) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_detect_chip(void)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    for (int chip = 0; chip < ESP_MAX_CHIP; chip++) {
        if (!esp_target[chip].sdio_supported) {
            continue;
        }

        uint32_t reg;
        RETURN_ON_ERROR(loader_port_read(1,
                                         esp_target[chip].slchost_date_addr,
                                         (uint8_t *)&reg,
                                         sizeof(uint32_t),
                                         loader_port_remaining_time()));

        if (reg == esp_target[chip].slchost_date_expected_val) {
            s_target_chip = (target_chip_t)chip;
            return ESP_LOADER_SUCCESS;
        }
    }

    return ESP_LOADER_ERROR_INVALID_TARGET;
}

static esp_loader_error_t slave_init_link(void)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    uint32_t reg = 0;

    // Configure stitching
    RETURN_ON_ERROR(slave_read_register(esp_target[s_target_chip].slc_conf1_addr, &reg));
    reg |= esp_target[s_target_chip].slc_conf1_tx_stitch_en |
           esp_target[s_target_chip].slc_conf1_rx_stitch_en;
    uint32_t expected_val = reg;
    RETURN_ON_ERROR(slave_write_register(esp_target[s_target_chip].slc_conf1_addr, reg));

    RETURN_ON_ERROR(slave_read_register(esp_target[s_target_chip].slc_conf1_addr, &reg));
    if (reg != expected_val) {
        return ESP_LOADER_ERROR_FAIL;
    }

    RETURN_ON_ERROR(slave_wait_ready(100));

    // Configure the tx packet load enable
    // This bit does not stay set, so reading it back to check for success is pointless
    RETURN_ON_ERROR(slave_read_register(esp_target[s_target_chip].slc_len_conf_addr, &reg));
    reg |= esp_target[s_target_chip].slc_len_conf_tx_packet_load_en;
    expected_val = reg;
    RETURN_ON_ERROR(slave_write_register(esp_target[s_target_chip].slc_len_conf_addr, reg));

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sip_upload_ram_segment(const uint32_t addr, const uint8_t *data,
        const uint32_t size)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    int32_t remaining = size;
    while (remaining > 0) {
        const uint32_t nondata_size = sizeof(sip_header_t) + sizeof(sip_cmd_write_memory);
        const uint32_t data_size = ROUNDUP(MIN(remaining, SIP_PACKET_SIZE - nondata_size), 4);

        const sip_header_t header = {
            .fc[0] = SIP_PACKET_TYPE_CTRL & SIP_TYPE_MASK,
            .fc[1] = 0x00,
            .len = data_size + nondata_size,
            .sequence_num = s_sip_seq_tx,
            .u.tx_info.u.cmdid = SIP_CMD_ID_WRITE_MEMORY,
        };

        const sip_cmd_write_memory cmd = {
            .addr = addr + size - remaining,
            .len = data_size,
        };

        memcpy(&s_block_buf[0], &header, sizeof(header));
        memcpy(&s_block_buf[sizeof(header)], &cmd, sizeof(cmd));
        memcpy(
            &s_block_buf[sizeof(header) + sizeof(cmd)],
            &data[size - remaining],
            data_size
        );

        RETURN_ON_ERROR(loader_port_write(1,
                                          esp_target[s_target_chip].slchost_packet_space_end - header.len,
                                          s_block_buf,
                                          header.len,
                                          loader_port_remaining_time()));

        remaining -= data_size;
        s_sip_seq_tx++;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sip_run_ram_code(const uint32_t entrypoint)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    const sip_header_t header = {
        .fc[0] = SIP_PACKET_TYPE_CTRL & SIP_TYPE_MASK,
        .fc[1] = SIP_HDR_F_SYNC,
        .len = sizeof(sip_header_t) + sizeof(sip_cmd_write_memory),
        .sequence_num = 0,
        .u.tx_info.u.cmdid = SIP_CMD_ID_BOOTUP,
    };

    const sip_cmd_bootup cmd = { .boot_addr = entrypoint, .discard_link = 1};

    memcpy(&s_block_buf[0], &header, sizeof(header));
    memcpy(&s_block_buf[sizeof(header)], &cmd, sizeof(cmd));

    return loader_port_write(1,
                             esp_target[s_target_chip].slchost_packet_space_end - header.len,
                             s_block_buf,
                             header.len,
                             loader_port_remaining_time()
                            );

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_upload_stub(void)
{
    RETURN_ON_ERROR(slave_wait_ready(100));

    const esp_stub_t *stub = &esp_stub[s_target_chip];

    for (uint32_t seg = 0; seg < sizeof(stub->segments) / sizeof(stub->segments[0]); seg++) {
        RETURN_ON_ERROR(sip_upload_ram_segment(stub->segments[seg].addr,
                                               stub->segments[seg].data,
                                               stub->segments[seg].size));
    }

    return sip_run_ram_code(stub->header.entrypoint);
}

static esp_loader_error_t stub_wait_ready(int32_t timeout, bool clear_flag)
{
    uint8_t reg __attribute__((aligned(4))) = 0;
    loader_port_start_timer(timeout);
    do {
        RETURN_ON_ERROR(loader_port_read(1, STUB_INT_ST_REG, &reg, sizeof(reg), loader_port_remaining_time()));
    } while ((reg & STUB_INT_BIT0) == 0);

    if (clear_flag) {
        reg = STUB_INT_BIT0;
        RETURN_ON_ERROR(loader_port_write(1, STUB_INT_CLR_REG, &reg, sizeof(reg), loader_port_remaining_time()));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t initialize_connection(esp_loader_connect_args_t *connect_args)
{
    RETURN_ON_ERROR(slave_init_card(connect_args->trials));

    RETURN_ON_ERROR(slave_init_io());

    RETURN_ON_ERROR(slave_detect_chip());

    RETURN_ON_ERROR(slave_init_link());

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_initialize_conn(esp_loader_connect_args_t *connect_args)
{
    s_sip_seq_tx = 0;

    RETURN_ON_ERROR(initialize_connection(connect_args));

    RETURN_ON_ERROR(slave_upload_stub());

    loader_port_delay_ms(STUB_BOOT_TIMEOUT);

    RETURN_ON_ERROR(initialize_connection(connect_args));

    RETURN_ON_ERROR(stub_wait_ready(STUB_DEFAULT_TIMEOUT, false));

    esp_stub_set_running(true);

    s_sip_seq_tx = 0;

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t check_response(const send_cmd_config *config)
{
    uint32_t reg __attribute__((aligned(4))) = 0;
    do {
        RETURN_ON_ERROR(loader_port_read(1, STUB_INT_ST_REG, (uint8_t *)&reg, sizeof(reg), loader_port_remaining_time()));
    } while ((reg & STUB_INT_NEW_PKT) == 0);

    // The STUB_PKT_LEN_REG contains all the bytes received, so we need to subtract the bytes we already got
    static uint32_t s_got_bytes_latest = 0;
    uint32_t packet_recv __attribute__((aligned(4))) = 0;
    while (packet_recv == 0 || packet_recv == s_got_bytes_latest) {
        RETURN_ON_ERROR(loader_port_read(1, STUB_PKT_LEN_REG, (uint8_t *)&packet_recv, sizeof(packet_recv), loader_port_remaining_time()));
        // The packet_recv is 24 bits, so we need to mask it
        packet_recv &= RX_BYTE_MASK;
    }

    packet_recv -= s_got_bytes_latest;
    s_got_bytes_latest += packet_recv;

    if (packet_recv > sizeof(s_block_buf)) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    RETURN_ON_ERROR(loader_port_read(1, esp_target[s_target_chip].slchost_packet_space_end - packet_recv, s_block_buf, packet_recv, loader_port_remaining_time()));

    if (packet_recv < (sizeof(common_response_t) + sizeof(response_status_t))) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    common_response_t *response = (common_response_t *)&s_block_buf[0];
    command_t command = ((const command_common_t *)config->cmd)->command;

    // Check if response matches request
    if (response->direction != READ_DIRECTION || response->command != command) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    // Check status
    response_status_t *status = (response_status_t *)&s_block_buf[packet_recv - sizeof(response_status_t)];

    if (status->failed) {
        log_loader_internal_error(status->error);
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    // Extract register value if requested
    if (config->reg_value != NULL) {
        *config->reg_value = response->value;
    }

    // Extract response data if requested
    if (config->resp_data != NULL) {
        const size_t resp_data_size = packet_recv - sizeof(common_response_t) - sizeof(response_status_t);

        if (resp_data_size > 0) {
            memcpy(config->resp_data, &s_block_buf[sizeof(common_response_t)], resp_data_size);

            if (config->resp_data_recv_size != NULL) {
                *config->resp_data_recv_size = resp_data_size;
            }
        }
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t send_cmd(const send_cmd_config *config)
{
    if (config->cmd_size + config->data_size > STUB_MAX_TRANSACTION_SIZE) { // TODO: check it
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }
    RETURN_ON_ERROR(stub_wait_ready(STUB_DEFAULT_TIMEOUT, true));

    memcpy(s_block_buf, config->cmd, config->cmd_size);

    // If there's data and it can fit in the first block with the command, include it
    uint32_t first_block_data_bytes = 0;
    if (config->data != NULL && config->data_size > 0) {
        first_block_data_bytes = MIN(SD_BLOCK_SIZE - config->cmd_size, config->data_size);
        memcpy(s_block_buf + config->cmd_size, config->data, first_block_data_bytes);
    }

    // Send first chunk (containing command and possibly some data)
    uint32_t first_chunk_size = config->cmd_size + first_block_data_bytes;
    RETURN_ON_ERROR(loader_port_write(1,
                                      esp_target[s_target_chip].slchost_packet_space_end - (config->cmd_size + config->data_size),
                                      s_block_buf,
                                      first_chunk_size,
                                      loader_port_remaining_time()));

    // If we have more data to send
    if (config->data != NULL && first_block_data_bytes < config->data_size) {
        uint32_t bytes_sent = first_block_data_bytes;
        uint32_t total_data = config->data_size;

        // Simple loop for remaining data chunks - no conditionals needed
        while (bytes_sent < total_data) {
            uint32_t chunk_size = MIN(SD_BLOCK_SIZE, total_data - bytes_sent);
            memcpy(s_block_buf, config->data + bytes_sent, chunk_size);

            RETURN_ON_ERROR(loader_port_write(1,
                                              esp_target[s_target_chip].slchost_packet_space_end -
                                              (config->cmd_size + config->data_size) +
                                              config->cmd_size + bytes_sent,
                                              s_block_buf,
                                              chunk_size,
                                              loader_port_remaining_time()));

            bytes_sent += chunk_size;
        }
    }

    return check_response(config);
}


// Temporary functions until the new stub is ready and placed at the end of RAM (ESPTOOL-1058).
static uint32_t s_mem_offset = 0;
esp_loader_error_t loader_mem_begin_cmd(uint32_t offset, uint32_t size, uint32_t blocks_to_write, uint32_t block_size)
{
    (void)blocks_to_write;
    (void)block_size;
    (void)size;

    // Necessary as the stub is always uploaded during initialization, but is placed at the beginning of RAM.
    // Loaded RAM app would have to be created with the stub in mind, so we just enter bootloader and reinitialize.
    // This will be fixed when the stub is placed at the end of RAM (ESPTOOL-1058).
    if (esp_stub_get_running()) {
        esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();
        loader_port_enter_bootloader();
        RETURN_ON_ERROR(initialize_connection(&connect_config));
        esp_stub_set_running(false);
    }
    s_mem_offset = offset;
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_mem_data_cmd(const uint8_t *data, uint32_t size)
{
    RETURN_ON_ERROR(sip_upload_ram_segment(s_mem_offset, data, size));
    s_mem_offset += size;
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_mem_end_cmd(uint32_t entrypoint)
{
    RETURN_ON_ERROR(sip_run_ram_code(entrypoint));
    s_sip_seq_tx = 0;
    return ESP_LOADER_SUCCESS;
}
