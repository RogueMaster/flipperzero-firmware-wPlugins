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
#include <stddef.h>
#include <string.h>

#define CMD_SIZE(cmd) ( sizeof(cmd) - sizeof(command_common_t) )

static uint32_t s_sequence_number = 0;

static uint8_t compute_checksum(const uint8_t *data, uint32_t size)
{
    uint8_t checksum = 0xEF;

    while (size--) {
        checksum ^= *data++;
    }

    return checksum;
}

void log_loader_internal_error(error_code_t error)
{
    switch (error) {
    case INVALID_CRC:               loader_port_debug_print("Error: INVALID_CRC"); break;
    case INVALID_COMMAND:           loader_port_debug_print("Error: INVALID_COMMAND"); break;
    case COMMAND_FAILED:            loader_port_debug_print("Error: COMMAND_FAILED"); break;
    case FLASH_WRITE_ERR:           loader_port_debug_print("Error: FLASH_WRITE_ERR"); break;
    case FLASH_READ_ERR:            loader_port_debug_print("Error: FLASH_READ_ERR"); break;
    case READ_LENGTH_ERR:           loader_port_debug_print("Error: READ_LENGTH_ERR"); break;
    case DEFLATE_ERROR:             loader_port_debug_print("Error: DEFLATE_ERROR"); break;

    case STUB_BAD_DATA_LEN:         loader_port_debug_print("Error: BAD_DATA_LEN"); break;
    case STUB_BAD_DATA_CHECKSUM:    loader_port_debug_print("Error: BAD_DATA_CHECKSUM"); break;
    case STUB_BAD_BLOCKSIZE:        loader_port_debug_print("Error: BAD_BLOCKSIZE"); break;
    case STUB_INVALID_COMMAND:      loader_port_debug_print("Error: INVALID_COMMAND"); break;
    case STUB_FAILED_SPI_OP:        loader_port_debug_print("Error: FAILED_SPI_OP"); break;
    case STUB_FAILED_SPI_UNLOCK:    loader_port_debug_print("Error: FAILED_SPI_UNLOCK"); break;
    case STUB_NOT_IN_FLASH_MODE:    loader_port_debug_print("Error: NOT_IN_FLASH_MODE"); break;
    case STUB_INFLATE_ERROR:        loader_port_debug_print("Error: INFLATE_ERROR"); break;
    case STUB_NOT_ENOUGH_DATA:      loader_port_debug_print("Error: NOT_ENOUGH_DATA"); break;
    case STUB_TOO_MUCH_DATA:        loader_port_debug_print("Error: TOO_MUCH_DATA"); break;
    case STUB_CMD_NOT_IMPLEMENTED:  loader_port_debug_print("Error: CMD_NOT_IMPLEMENTED"); break;

    default:                        loader_port_debug_print("Error: UNKNOWN ERROR"); break;
    }
}

esp_loader_error_t loader_flash_begin_cmd(uint32_t offset,
        uint32_t erase_size,
        uint32_t block_size,
        uint32_t blocks_to_write,
        bool encryption)
{
    flash_begin_command_t flash_begin_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = FLASH_BEGIN,
            .size = CMD_SIZE(flash_begin_cmd) - (encryption ? 0 : sizeof(uint32_t)),
            .checksum = 0
        },
        .erase_size = erase_size,
        .packet_count = blocks_to_write,
        .packet_size = block_size,
        .offset = offset,
        .encrypted = 0
    };

    s_sequence_number = 0;

    const send_cmd_config cmd_config = {
        .cmd = &flash_begin_cmd,
        .cmd_size = sizeof(flash_begin_cmd) - (encryption ? 0 : sizeof(uint32_t)),
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_flash_data_cmd(const uint8_t *data, uint32_t size)
{
    data_command_t data_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = FLASH_DATA,
            .size = CMD_SIZE(data_cmd) + size,
            .checksum = compute_checksum(data, size)
        },
        .data_size = size,
        .sequence_number = s_sequence_number++,
    };

    const send_cmd_config cmd_config = {
        .cmd = &data_cmd,
        .cmd_size = sizeof(data_cmd),
        .data = data,
        .data_size = size,
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_flash_end_cmd(bool stay_in_loader)
{
    flash_end_command_t end_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = FLASH_END,
            .size = CMD_SIZE(end_cmd),
            .checksum = 0
        },
        .stay_in_loader = stay_in_loader
    };

    const send_cmd_config cmd_config = {
        .cmd = &end_cmd,
        .cmd_size = sizeof(end_cmd)
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_flash_read_rom_cmd(const uint32_t address, uint8_t *data)
{
    const flash_read_rom_cmd flash_read_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = READ_FLASH_ROM,
            .size = CMD_SIZE(flash_read_cmd),
            .checksum = 0
        },
        .address = address,
        .size = READ_FLASH_ROM_DATA_SIZE,
    };

    const send_cmd_config cmd_config = {
        .cmd = &flash_read_cmd,
        .cmd_size = sizeof(flash_read_cmd),
        .resp_data = data,
        .resp_data_size = READ_FLASH_ROM_DATA_SIZE,
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_flash_read_stub_cmd(const uint32_t address, const uint32_t size,
        const uint32_t size_per_packet)
{
    const flash_read_stub_cmd flash_read_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = READ_FLASH_STUB,
            .size = CMD_SIZE(flash_read_cmd),
            .checksum = 0
        },
        .address = address,
        .total_size = size,
        .packet_data_size = size_per_packet,
        .max_inflight_packets = 1,
    };

    const send_cmd_config cmd_config = {
        .cmd = &flash_read_cmd,
        .cmd_size = sizeof(flash_read_cmd),
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_flash_erase_cmd(void)
{
    const flash_erase_chip_cmd erase_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = ERASE_FLASH,
            .size = CMD_SIZE(erase_cmd),
            .checksum = 0
        },
    };

    const send_cmd_config cmd_config = {
        .cmd = &erase_cmd,
        .cmd_size = sizeof(erase_cmd),
    };

    return send_cmd(&cmd_config);
}

esp_loader_error_t loader_flash_erase_region_cmd(uint32_t offset, uint32_t size)
{
    const flash_erase_region_cmd erase_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = ERASE_REGION,
            .size = CMD_SIZE(erase_cmd),
            .checksum = 0
        },
        .offset = offset,
        .size = size,
    };

    const send_cmd_config cmd_config = {
        .cmd = &erase_cmd,
        .cmd_size = sizeof(erase_cmd),
    };

    return send_cmd(&cmd_config);
}

esp_loader_error_t loader_sync_cmd(void)
{
    sync_command_t sync_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = SYNC,
            .size = CMD_SIZE(sync_cmd),
            .checksum = 0
        },
        .sync_sequence = {
            0x07, 0x07, 0x12, 0x20,
            0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
            0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
            0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
            0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
        }
    };

    const send_cmd_config cmd_config = {
        .cmd = &sync_cmd,
        .cmd_size = sizeof(sync_cmd)
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_spi_attach_cmd(uint32_t config)
{
    spi_attach_command_t attach_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = SPI_ATTACH,
            .size = CMD_SIZE(attach_cmd),
            .checksum = 0
        },
        .configuration = config,
        .zero = 0
    };

    const send_cmd_config cmd_config = {
        .cmd = &attach_cmd,
        .cmd_size = esp_stub_get_running() ? sizeof(attach_cmd) - sizeof(attach_cmd.zero) : sizeof(attach_cmd),
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_md5_cmd(uint32_t address, uint32_t size, uint8_t *md5_out)
{
    spi_flash_md5_command_t md5_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = SPI_FLASH_MD5,
            .size = CMD_SIZE(md5_cmd),
            .checksum = 0
        },
        .address = address,
        .size = size,
        .reserved_0 = 0,
        .reserved_1 = 0
    };

    const send_cmd_config cmd_config = {
        .cmd = &md5_cmd,
        .cmd_size = sizeof(md5_cmd),
        .resp_data = md5_out,
        .resp_data_size = esp_stub_get_running() ? MD5_SIZE_STUB : MD5_SIZE_ROM,
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_spi_parameters(uint32_t total_size)
{
    write_spi_command_t spi_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = SPI_SET_PARAMS,
            .size = 24,
            .checksum = 0
        },
        .id = 0,
        .total_size = total_size,
        .block_size = 64 * 1024,
        .sector_size = 4 * 1024,
        .page_size = 0x100,
        .status_mask = 0xFFFF,
    };

    const send_cmd_config cmd_config = {
        .cmd = &spi_cmd,
        .cmd_size = sizeof(spi_cmd),
    };

    return send_cmd(&cmd_config);
}


#ifndef SERIAL_FLASHER_INTERFACE_SDIO
esp_loader_error_t loader_mem_begin_cmd(uint32_t offset, uint32_t size, uint32_t blocks_to_write, uint32_t block_size)
{

    mem_begin_command_t mem_begin_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = MEM_BEGIN,
            .size = CMD_SIZE(mem_begin_cmd),
            .checksum = 0
        },
        .total_size = size,
        .blocks = blocks_to_write,
        .block_size = block_size,
        .offset = offset
    };

    s_sequence_number = 0;

    const send_cmd_config cmd_config = {
        .cmd = &mem_begin_cmd,
        .cmd_size = sizeof(mem_begin_cmd),
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_mem_data_cmd(const uint8_t *data, uint32_t size)
{
    data_command_t data_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = MEM_DATA,
            .size = CMD_SIZE(data_cmd) + size,
            .checksum = compute_checksum(data, size)
        },
        .data_size = size,
        .sequence_number = s_sequence_number++,
    };

    const send_cmd_config cmd_config = {
        .cmd = &data_cmd,
        .cmd_size = sizeof(data_cmd),
        .data = data,
        .data_size = size,
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_mem_end_cmd(uint32_t entrypoint)
{
    mem_end_command_t end_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = MEM_END,
            .size = CMD_SIZE(end_cmd),
        },
        .stay_in_loader = (entrypoint == 0),
        .entry_point_address = entrypoint
    };

    const send_cmd_config cmd_config = {
        .cmd = &end_cmd,
        .cmd_size = sizeof(end_cmd),
    };

    return send_cmd(&cmd_config);
}
#endif /* SERIAL_FLASHER_INTERFACE_SDIO */


esp_loader_error_t loader_write_reg_cmd(uint32_t address, uint32_t value,
                                        uint32_t mask, uint32_t delay_us)
{
    write_reg_command_t write_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = WRITE_REG,
            .size = CMD_SIZE(write_cmd),
            .checksum = 0
        },
        .address = address,
        .value = value,
        .mask = mask,
        .delay_us = delay_us
    };

    const send_cmd_config cmd_config = {
        .cmd = &write_cmd,
        .cmd_size = sizeof(write_cmd),
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_read_reg_cmd(uint32_t address, uint32_t *reg)
{
    read_reg_command_t read_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = READ_REG,
            .size = CMD_SIZE(read_cmd),
            .checksum = 0
        },
        .address = address,
    };

    const send_cmd_config cmd_config = {
        .cmd = &read_cmd,
        .cmd_size = sizeof(read_cmd),
        .reg_value = reg,
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_change_baudrate_cmd(uint32_t new_baudrate, uint32_t old_baudrate)
{
    change_baudrate_command_t baudrate_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = CHANGE_BAUDRATE,
            .size = CMD_SIZE(baudrate_cmd),
            .checksum = 0
        },
        .new_baudrate = new_baudrate,
        .old_baudrate = old_baudrate
    };

    const send_cmd_config cmd_config = {
        .cmd = &baudrate_cmd,
        .cmd_size = sizeof(baudrate_cmd),
    };

    return send_cmd(&cmd_config);
}


esp_loader_error_t loader_get_security_info_cmd(get_security_info_response_data_t *response,
        uint32_t *response_recv_size)
{
    const get_security_info_command_t get_security_info_cmd = {
        .common = {
            .direction = WRITE_DIRECTION,
            .command = GET_SECURITY_INFO,
            .size = CMD_SIZE(get_security_info_cmd),
            .checksum = 0
        },
    };

    const send_cmd_config cmd_config = {
        .cmd = &get_security_info_cmd,
        .cmd_size = sizeof(get_security_info_cmd),
        .resp_data = response,
        .resp_data_size = sizeof(get_security_info_response_data_t),
        .resp_data_recv_size = response_recv_size,
    };

    return send_cmd(&cmd_config);
}


__attribute__ ((weak)) void loader_port_debug_print(const char *str)
{
    (void) str;
}
