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
#include "esp_stubs.h"
#include "sip.h"
#include "loader_log.h"
#include <stddef.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

static inline uint32_t port_remaining_time(const esp_loader_t *loader)
{
    return loader->_port->ops->remaining_time(loader->_port);
}

static inline esp_loader_error_t sdio_read_bytes(esp_loader_t *loader, uint32_t function,
        uint32_t addr, void *data, uint16_t size)
{
    esp_loader_error_t err = loader->_port->ops->sdio_read(loader->_port, function, addr,
                             (uint8_t *)data, size,
                             port_remaining_time(loader));
    if (err == ESP_LOADER_SUCCESS) {
        LOADER_LOGD(loader, "SDIO R fn=%" PRIu32 " addr=0x%04" PRIx32 " size=%u",
                    function, addr, (unsigned)size);
        LOADER_LOG_HEX(loader, "SDIO RX", data, size);
    }
    return err;
}

static inline esp_loader_error_t sdio_write_bytes(esp_loader_t *loader, uint32_t function,
        uint32_t addr, const void *data, uint16_t size)
{
    LOADER_LOGD(loader, "SDIO W fn=%" PRIu32 " addr=0x%04" PRIx32 " size=%u",
                function, addr, (unsigned)size);
    LOADER_LOG_HEX(loader, "SDIO TX", data, size);
    return loader->_port->ops->sdio_write(loader->_port, function, addr,
                                          (const uint8_t *)data, size,
                                          port_remaining_time(loader));
}

/* SDIO CCR registers - common to all SDIO devices */
#define SD_IO_CCCR_FN_ENABLE 0x02
#define SD_IO_CCR_FN_ENABLE_FUNC1_EN (1 << 1)
#define SD_IO_CCCR_FN_READY 0x03
#define SD_IO_CCCR_FN_ID 0x09
#define SD_IO_CCCR_FN_ID1 0x0A
#define SD_IO_CCCR_FN_ID2 0x0B

#define SD_IO_TUPLE_CODE 0x20
#define SD_IO_TUPLE_SIZE 4
#define SD_IO_ESPRESSIF_VENDOR_ID 0x0092
// Minimum size of the CIS region for Espressif devices, should contain the manufacturer and device ID.
#define SD_IO_CIS_MINIMUM_SIZE 32

#define SD_BLOCK_SIZE 512

/* Stub defines */
// CONF_W0: stub writes total packet length here before each DMA send (func 1, addr 0x6C)
#define STUB_CONF_W0_REG 0x006C
#define STUB_TOKEN_RDATA_REG 0x0044
#define STUB_INT_ST_REG 0x0058
#define STUB_INT_CLR_REG 0x00D4
#define STUB_INT_NEW_PKT (1 << 23)
#define STUB_MAX_TRANSACTION_SIZE 16647
#define STUB_BOOT_TIMEOUT 500
#define STUB_DEFAULT_TIMEOUT 200

#define SDIO_RESPONSE_MAX_SIZE (sizeof(common_response_t) + MAX_RESP_DATA_SIZE + sizeof(response_status_t))

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
    uint16_t slchost_device_id;
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
} esp_sdio_target_t;

static const esp_sdio_target_t esp_sdio_target[ESP_MAX_CHIP] = {
    // ESP8266
    {},
    // ESP32
    {
        .sdio_supported = true,
        .slchost_device_id = 0x0,
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
    {
        .sdio_supported = true,
        .slchost_device_id = 0x1017,
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
    // ESP32H2
    {},
    // ESP32C6
    {
        .sdio_supported = true,
        .slchost_device_id = 0x100D,
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
    // ESP32C61
    {
        .sdio_supported = true,
        .slchost_device_id = 0x1014,
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
};

static esp_loader_error_t slave_read_register(esp_loader_t *loader, const uint32_t addr, uint32_t *reg)
{
    assert(addr >> 2 <= 0x7F);

    uint8_t buf[4] = {0};
    buf[0] = (addr >> 2) & 0x7F;
    buf[1] = 0x80;

    RETURN_ON_ERROR(sdio_write_bytes(loader, 1,
                                     esp_sdio_target[loader->_target].slchost_win_cmd_addr,
                                     buf, sizeof(buf)));

    return sdio_read_bytes(loader, 1,
                           esp_sdio_target[loader->_target].slchost_state_w0_addr,
                           reg, sizeof(uint32_t));
}

static esp_loader_error_t slave_write_register(esp_loader_t *loader, const uint32_t addr, uint32_t reg_val)
{
    assert(addr >> 2 <= 0x7F);

    uint8_t buf[8] = {0};
    memcpy(buf, &reg_val, 4);
    buf[4] = (addr >> 2) & 0x7F;
    buf[5] = 0xC0;

    return sdio_write_bytes(loader, 1,
                            esp_sdio_target[loader->_target].slchost_conf_w5_addr,
                            buf, sizeof(buf));
}

static esp_loader_error_t slave_wait_ready(esp_loader_t *loader)
{
    uint8_t reg = 0;

    while ((reg & SD_IO_CCR_FN_ENABLE_FUNC1_EN) == 0) {
        if (port_remaining_time(loader) == 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }

        RETURN_ON_ERROR(sdio_read_bytes(loader, 0, SD_IO_CCCR_FN_READY, &reg, sizeof(reg)));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_init_card(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{

    for (int32_t trial = 0; trial < connect_args->trials; trial++) {
        loader->_port->ops->start_timer(loader->_port, connect_args->sync_timeout);

        if (loader->_port->ops->sdio_card_init(loader->_port) != ESP_LOADER_SUCCESS) {
            LOADER_LOGW(loader, "Retrying SDIO card connection...");
            loader->_port->ops->delay_ms(loader->_port, 100);
        } else {
            return ESP_LOADER_SUCCESS;
        }
    }

    return ESP_LOADER_ERROR_FAIL;
}

static esp_loader_error_t slave_init_io(esp_loader_t *loader)
{
    loader->_port->ops->start_timer(loader->_port, STUB_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(slave_wait_ready(loader));

    // Enable function 1, we will use it for upload
    uint8_t reg;
    RETURN_ON_ERROR(sdio_read_bytes(loader, 0, SD_IO_CCCR_FN_ENABLE, &reg, sizeof(reg)));

    reg |= SD_IO_CCR_FN_ENABLE_FUNC1_EN;
    uint8_t expected_val = reg;
    RETURN_ON_ERROR(sdio_write_bytes(loader, 0, SD_IO_CCCR_FN_ENABLE, &reg, sizeof(reg)));

    // Read back to verify
    RETURN_ON_ERROR(sdio_read_bytes(loader, 0, SD_IO_CCCR_FN_ENABLE, &reg, sizeof(reg)));

    if (reg != expected_val) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_detect_chip_internal(esp_loader_t *loader)
{
    loader->_port->ops->start_timer(loader->_port, STUB_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(slave_wait_ready(loader));

    // CIS region exist in 0x1000~0x17FFF of FUNC 0, get the start address of it
    // from CCCR register.
    uint8_t cis[3];
    RETURN_ON_ERROR(sdio_read_bytes(loader, 0, SD_IO_CCCR_FN_ID, cis, sizeof(cis)));
    const uint32_t cis_ptr = cis[0] | (cis[1] << 8) | (cis[2] << 16);

    uint32_t addr = cis_ptr;
    uint16_t vendor_id = 0;
    uint16_t device_id = 0;
    // Not really max, but reasonable enough for reading manufacturer and device ID,
    // which should be in the first
    const uint32_t max_addr = addr + SD_IO_CIS_MINIMUM_SIZE;
    while (addr < max_addr) {
        uint8_t header[2];
        RETURN_ON_ERROR(sdio_read_bytes(loader, 0, addr, header, sizeof(header)));
        addr += sizeof(header);
        const uint8_t tuple_code = header[0];
        const uint8_t tuple_size = header[1];
        if (tuple_code == SD_IO_TUPLE_CODE && tuple_size == SD_IO_TUPLE_SIZE) {
            uint8_t ids[SD_IO_TUPLE_SIZE];
            RETURN_ON_ERROR(sdio_read_bytes(loader, 0, addr, ids, sizeof(ids)));
            vendor_id = ids[0] | (ids[1] << 8);
            device_id = ids[2] | (ids[3] << 8);
            break;
        }
        addr += tuple_size;
    }

    if (vendor_id == SD_IO_ESPRESSIF_VENDOR_ID) {
        for (target_chip_t chip = 0; chip < ESP_MAX_CHIP; chip++) {
            if (esp_sdio_target[chip].sdio_supported && esp_sdio_target[chip].slchost_device_id == device_id) {
                loader->_target = chip;
                return ESP_LOADER_SUCCESS;
            }
        }
    }

    return ESP_LOADER_ERROR_INVALID_TARGET;
}

static esp_loader_error_t slave_init_link(esp_loader_t *loader)
{
    loader->_port->ops->start_timer(loader->_port, STUB_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(slave_wait_ready(loader));

    uint32_t reg = 0;

    // Configure stitching
    RETURN_ON_ERROR(slave_read_register(loader, esp_sdio_target[loader->_target].slc_conf1_addr, &reg));
    reg |= esp_sdio_target[loader->_target].slc_conf1_tx_stitch_en |
           esp_sdio_target[loader->_target].slc_conf1_rx_stitch_en;
    uint32_t expected_val = reg;
    RETURN_ON_ERROR(slave_write_register(loader, esp_sdio_target[loader->_target].slc_conf1_addr, reg));

    RETURN_ON_ERROR(slave_read_register(loader, esp_sdio_target[loader->_target].slc_conf1_addr, &reg));
    if (reg != expected_val) {
        return ESP_LOADER_ERROR_FAIL;
    }

    loader->_port->ops->start_timer(loader->_port, STUB_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(slave_wait_ready(loader));

    // Configure the tx packet load enable
    // This bit does not stay set, so reading it back to check for success is pointless
    RETURN_ON_ERROR(slave_read_register(loader, esp_sdio_target[loader->_target].slc_len_conf_addr, &reg));
    reg |= esp_sdio_target[loader->_target].slc_len_conf_tx_packet_load_en;
    expected_val = reg;
    RETURN_ON_ERROR(slave_write_register(loader, esp_sdio_target[loader->_target].slc_len_conf_addr, reg));

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sip_upload_ram_segment(esp_loader_t *loader, const uint32_t addr,
        const uint8_t *data, const uint32_t size)
{
    loader->_port->ops->start_timer(loader->_port, STUB_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(slave_wait_ready(loader));

    uint8_t packet[SIP_PACKET_SIZE];
    int32_t remaining = size;
    while (remaining > 0) {
        const uint32_t nondata_size = sizeof(sip_header_t) + sizeof(sip_cmd_write_memory);
        const uint32_t data_size = ROUNDUP(MIN(remaining, SIP_PACKET_SIZE - nondata_size), 4);

        const sip_header_t header = {
            .fc[0] = SIP_PACKET_TYPE_CTRL & SIP_TYPE_MASK,
            .fc[1] = 0x00,
            .len = data_size + nondata_size,
            .sequence_num = loader->_proto_ctx.sdio.sip_seq_tx,
            .u.tx_info.u.cmdid = SIP_CMD_ID_WRITE_MEMORY,
        };

        const sip_cmd_write_memory cmd = {
            .addr = addr + size - remaining,
            .len = data_size,
        };

        memcpy(&packet[0], &header, sizeof(header));
        memcpy(&packet[sizeof(header)], &cmd, sizeof(cmd));
        memcpy(
            &packet[sizeof(header) + sizeof(cmd)],
            &data[size - remaining],
            data_size
        );

        RETURN_ON_ERROR(sdio_write_bytes(loader, 1,
                                         esp_sdio_target[loader->_target].slchost_packet_space_end - header.len,
                                         packet, header.len));

        remaining -= data_size;
        loader->_proto_ctx.sdio.sip_seq_tx++;
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sip_run_ram_code(esp_loader_t *loader, const uint32_t entrypoint)
{
    loader->_port->ops->start_timer(loader->_port, STUB_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(slave_wait_ready(loader));

    const sip_header_t header = {
        .fc[0] = SIP_PACKET_TYPE_CTRL & SIP_TYPE_MASK,
        .fc[1] = SIP_HDR_F_SYNC,
        .len = sizeof(sip_header_t) + sizeof(sip_cmd_write_memory),
        .sequence_num = 0,
        .u.tx_info.u.cmdid = SIP_CMD_ID_BOOTUP,
    };

    const sip_cmd_bootup cmd = { .boot_addr = entrypoint, .discard_link = 1};
    uint8_t packet[sizeof(header) + sizeof(cmd)];

    memcpy(&packet[0], &header, sizeof(header));
    memcpy(&packet[sizeof(header)], &cmd, sizeof(cmd));

    return sdio_write_bytes(loader, 1,
                            esp_sdio_target[loader->_target].slchost_packet_space_end - header.len,
                            packet, header.len);
}

static const esp_stub_t *sdio_get_stub(target_chip_t target)
{
    extern const esp_stub_t esp_stub_esp32c5;
    extern const esp_stub_t esp_stub_esp32c6;

    switch (target) {
    case ESP32C5_CHIP:
        return &esp_stub_esp32c5;
    case ESP32C6_CHIP:
        return &esp_stub_esp32c6;
    default:
        return NULL;
    }
}

static esp_loader_error_t sdio_read_stub_packet(esp_loader_t *loader, uint8_t *dest,
        size_t max_size, size_t *recv_size)
{
    uint32_t interrupt_status = 0;
    uint32_t poll_count = 0;

    do {
        if (port_remaining_time(loader) == 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }

        RETURN_ON_ERROR(sdio_read_bytes(loader, 1, STUB_INT_ST_REG, &interrupt_status, sizeof(interrupt_status)));

        // Long-running commands such as chip erase can take seconds; avoid a hot poll loop.
        if ((interrupt_status & STUB_INT_NEW_PKT) == 0 && ++poll_count > 10000) {
            loader->_port->ops->delay_ms(loader->_port, 10);
        }
    } while ((interrupt_status & STUB_INT_NEW_PKT) == 0);

    uint32_t packet_size = 0;
    RETURN_ON_ERROR(sdio_read_bytes(loader, 1, STUB_CONF_W0_REG, &packet_size, sizeof(packet_size)));

    uint32_t clr = STUB_INT_NEW_PKT;
    if (packet_size == 0 || packet_size > max_size) {
        RETURN_ON_ERROR(sdio_write_bytes(loader, 1, STUB_INT_CLR_REG, &clr, sizeof(clr)));
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    const uint32_t packet_addr = esp_sdio_target[loader->_target].slchost_packet_space_end - packet_size;

    uint32_t bytes_read = 0;
    while (bytes_read < packet_size) {
        const uint16_t chunk_size = (uint16_t)MIN(SD_BLOCK_SIZE, packet_size - bytes_read);

        RETURN_ON_ERROR(sdio_read_bytes(loader, 1, packet_addr + bytes_read, dest + bytes_read, chunk_size));

        bytes_read += chunk_size;
    }

    RETURN_ON_ERROR(sdio_write_bytes(loader, 1, STUB_INT_CLR_REG, &clr, sizeof(clr)));

    *recv_size = packet_size;
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t slave_upload_stub(esp_loader_t *loader)
{
    loader->_port->ops->start_timer(loader->_port, STUB_DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(slave_wait_ready(loader));

    const esp_stub_t *stub = sdio_get_stub(loader->_target);
    if (stub == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
    }

    for (uint32_t seg = 0; seg < sizeof(stub->segments) / sizeof(stub->segments[0]); seg++) {
        RETURN_ON_ERROR(sip_upload_ram_segment(loader, stub->segments[seg].addr,
                                               stub->segments[seg].data,
                                               stub->segments[seg].size));
    }

    RETURN_ON_ERROR(sip_run_ram_code(loader, stub->header.entrypoint));

    // Wait for stub to signal it is ready
    static const uint8_t expected_ohai[] = {'O', 'H', 'A', 'I'};
    uint8_t buf[sizeof(expected_ohai)];
    size_t recv_size = 0;

    loader->_port->ops->start_timer(loader->_port, STUB_BOOT_TIMEOUT);
    RETURN_ON_ERROR(sdio_read_stub_packet(loader, buf, sizeof(buf), &recv_size));

    if (recv_size != sizeof(expected_ohai) || memcmp(buf, expected_ohai, sizeof(expected_ohai)) != 0) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    loader->_stub_running = true;
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t initialize_connection(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{
    RETURN_ON_ERROR(slave_init_card(loader, connect_args));

    RETURN_ON_ERROR(slave_init_io(loader));

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sdio_initialize_conn(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{

    loader->_proto_ctx.sdio.sip_seq_tx = 0;

    RETURN_ON_ERROR(initialize_connection(loader, connect_args));

    RETURN_ON_ERROR(slave_detect_chip_internal(loader));
    RETURN_ON_ERROR(slave_init_link(loader));
    RETURN_ON_ERROR(slave_upload_stub(loader));
    loader->_proto_ctx.sdio.sip_seq_tx = 0;
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sdio_wait_rx_space(esp_loader_t *loader, uint32_t transaction_size)
{
    for (;;) {
        if (port_remaining_time(loader) == 0) {
            return ESP_LOADER_ERROR_TIMEOUT;
        }

        uint32_t token_rdata = 0;
        RETURN_ON_ERROR(sdio_read_bytes(loader, 1, STUB_TOKEN_RDATA_REG, &token_rdata, sizeof(token_rdata)));

        uint32_t token1 = (token_rdata >> 16) & 0x0FFFU;
        if (token1 * SD_BLOCK_SIZE >= transaction_size) {
            return ESP_LOADER_SUCCESS;
        }

        loader->_port->ops->delay_ms(loader->_port, 1);
    }
}

static esp_loader_error_t sdio_send_stub_ack(esp_loader_t *loader, uint32_t bytes_recv)
{
    uint32_t ack = bytes_recv;

    RETURN_ON_ERROR(sdio_wait_rx_space(loader, sizeof(ack)));
    return sdio_write_bytes(loader, 1,
                            esp_sdio_target[loader->_target].slchost_packet_space_end - sizeof(ack),
                            &ack, sizeof(ack));
}

static esp_loader_error_t sdio_check_response(esp_loader_t *loader, const send_cmd_config *config)
{
    command_t command = ((const command_common_t *)config->cmd)->command;
    uint8_t response_buf[SDIO_RESPONSE_MAX_SIZE];
    size_t response_len = 0;
    RETURN_ON_ERROR(sdio_read_stub_packet(loader, response_buf, sizeof(response_buf), &response_len));

    if (response_len < sizeof(common_response_t) + sizeof(response_status_t)) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    common_response_t *response = (common_response_t *)response_buf;
    if (response->direction != READ_DIRECTION || response->command != command) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    response_status_t *status = (response_status_t *)(response_buf + response_len - sizeof(response_status_t));

    if (status->failed) {
        log_loader_internal_error(loader, status->error);
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }
    LOADER_LOGD(loader, "CMD <- %s OK", loader_command_name(command));

    if (config->reg_value != NULL) {
        *config->reg_value = response->value;
    }

    if (config->resp_data != NULL) {
        const size_t resp_data_size = response_len - sizeof(common_response_t) - sizeof(response_status_t);

        if (resp_data_size > 0) {
            memcpy(config->resp_data, response_buf + sizeof(common_response_t), resp_data_size);

            if (config->resp_data_recv_size != NULL) {
                *config->resp_data_recv_size = resp_data_size;
            }
        }
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sdio_send_cmd(esp_loader_t *loader, const send_cmd_config *config)
{
    const uint32_t total_len = config->cmd_size + config->data_size;
    LOADER_LOGD(loader, "CMD -> %s (0x%02x)",
                loader_command_name(((const command_common_t *)config->cmd)->command),
                (unsigned)((const command_common_t *)config->cmd)->command);

    if (total_len > STUB_MAX_TRANSACTION_SIZE) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }
    RETURN_ON_ERROR(sdio_wait_rx_space(loader, total_len));

    const uint32_t packet_base = esp_sdio_target[loader->_target].slchost_packet_space_end - total_len;

    uint32_t bytes_sent = 0;
    while (bytes_sent < config->cmd_size) {
        const uint16_t chunk_size = (uint16_t)MIN(SD_BLOCK_SIZE, config->cmd_size - bytes_sent);

        RETURN_ON_ERROR(sdio_write_bytes(loader, 1, packet_base + bytes_sent,
                                         (const uint8_t *)config->cmd + bytes_sent, chunk_size));

        bytes_sent += chunk_size;
    }

    if (config->data != NULL && config->data_size > 0) {
        bytes_sent = 0;
        while (bytes_sent < config->data_size) {
            const uint16_t chunk_size = (uint16_t)MIN(SD_BLOCK_SIZE, config->data_size - bytes_sent);

            RETURN_ON_ERROR(sdio_write_bytes(loader, 1,
                                             packet_base + config->cmd_size + bytes_sent,
                                             (const uint8_t *)config->data + bytes_sent,
                                             chunk_size));

            bytes_sent += chunk_size;
        }
    }

    return sdio_check_response(loader, config);
}

// Temporary functions until the new stub is ready and placed at the end of RAM (ESPTOOL-1058).
static esp_loader_error_t sdio_mem_begin_cmd(esp_loader_t *loader, uint32_t offset, uint32_t size,
        uint32_t blocks_to_write, uint32_t block_size)
{

    (void)blocks_to_write;
    (void)block_size;
    (void)size;

    // Necessary as the stub is always uploaded during initialization, but is placed at the beginning of RAM.
    // Loaded RAM app would have to be created with the stub in mind, so we just enter bootloader and reinitialize.
    // This will be fixed when the stub is placed at the end of RAM (ESPTOOL-1058).
    if (loader->_stub_running) {
        esp_loader_connect_args_t connect_config = ESP_LOADER_CONNECT_DEFAULT();
        loader->_port->ops->enter_bootloader(loader->_port);
        RETURN_ON_ERROR(initialize_connection(loader, &connect_config));
        RETURN_ON_ERROR(slave_init_link(loader));
        loader->_stub_running = false;
    }
    loader->_proto_ctx.sdio.mem_offset = offset;
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sdio_mem_data_cmd(esp_loader_t *loader, const uint8_t *data, uint32_t size)
{
    RETURN_ON_ERROR(sip_upload_ram_segment(loader, loader->_proto_ctx.sdio.mem_offset, data, size));
    loader->_proto_ctx.sdio.mem_offset += size;
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t sdio_mem_end_cmd(esp_loader_t *loader, uint32_t entrypoint)
{
    RETURN_ON_ERROR(sip_run_ram_code(loader, entrypoint));
    loader->_proto_ctx.sdio.sip_seq_tx = 0;
    return ESP_LOADER_SUCCESS;
}

const esp_loader_protocol_ops_t sdio_protocol_ops = {
    .initialize_conn   = sdio_initialize_conn,
    .send_cmd          = sdio_send_cmd,
    .spi_attach        = NULL,
    .recv_stub_packet  = sdio_read_stub_packet,
    .send_stub_ack     = sdio_send_stub_ack,
    .mem_begin_cmd     = sdio_mem_begin_cmd,
    .mem_data_cmd      = sdio_mem_data_cmd,
    .mem_end_cmd       = sdio_mem_end_cmd,
};

const esp_loader_protocol_ops_t *esp_loader_get_sdio_ops(void)
{
    return &sdio_protocol_ops;
}
