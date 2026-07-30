/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_FAILURE  1
#define STATUS_SUCCESS  0

#define READ_DIRECTION  1
#define WRITE_DIRECTION 0

#define MD5_SIZE_ROM  32
#define MD5_SIZE_STUB 16

// Maximum block sized for RAM and Flash writes, respectively.
#define ESP_RAM_BLOCK 0x1800

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef ROUNDUP
#define ROUNDUP(a, b) (((a) + (b) - 1) / (b) * (b))
#endif

#define MAX_RESP_DATA_SIZE 64
#define READ_FLASH_ROM_DATA_SIZE 64

typedef enum __attribute__((packed))
{
    FLASH_BEGIN = 0x02,
    FLASH_DATA = 0x03,
    FLASH_END = 0x04,
    MEM_BEGIN = 0x05,
    MEM_END = 0x06,
    MEM_DATA = 0x07,
    SYNC = 0x08,
    WRITE_REG = 0x09,
    READ_REG = 0x0a,
    SPI_SET_PARAMS = 0x0b,
    SPI_ATTACH = 0x0d,
    READ_FLASH_ROM = 0x0e,
    CHANGE_BAUDRATE = 0x0f,
    FLASH_DEFL_BEGIN = 0x10,
    FLASH_DEFL_DATA = 0x11,
    FLASH_DEFL_END = 0x12,
    SPI_FLASH_MD5 = 0x13,
    GET_SECURITY_INFO = 0x14,
    ERASE_FLASH = 0xd0,
    ERASE_REGION = 0xd1,
    READ_FLASH_STUB = 0xd2,
} command_t;

typedef enum __attribute__((packed))
{
    RESPONSE_OK     = 0x00,
    INVALID_COMMAND = 0x05, // parameters or length field is invalid
    COMMAND_FAILED  = 0x06, // Failed to act on received message
    INVALID_CRC     = 0x07, // Invalid CRC in message
    FLASH_WRITE_ERR = 0x08, // After writing a block of data to flash, the ROM loader reads the value back and the 8-bit CRC is compared to the data read from flash. If they don't match, this error is returned.
    FLASH_READ_ERR  = 0x09, // SPI read failed
    READ_LENGTH_ERR = 0x0a, // SPI read request length is too long
    DEFLATE_ERROR   = 0x0b, // ESP32 compressed uploads only

    STUB_BAD_DATA_LEN = 0xC0,
    STUB_BAD_DATA_CHECKSUM = 0xC1,
    STUB_BAD_BLOCKSIZE = 0xC2,
    STUB_INVALID_COMMAND = 0xC3,
    STUB_FAILED_SPI_OP = 0xC4,
    STUB_FAILED_SPI_UNLOCK = 0xC5,
    STUB_NOT_IN_FLASH_MODE = 0xC6,
    STUB_INFLATE_ERROR = 0xC7,
    STUB_NOT_ENOUGH_DATA = 0xC8,
    STUB_TOO_MUCH_DATA = 0xC9,
    STUB_CMD_NOT_IMPLEMENTED = 0xFF,
} error_code_t;

typedef struct __attribute__((packed))
{
    uint8_t direction;
    uint8_t command;    // One of command_t
    uint16_t size;
    uint32_t checksum;
} command_common_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t erase_size;
    uint32_t packet_count;
    uint32_t packet_size;
    uint32_t offset;
    uint32_t encrypted;
} flash_begin_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t data_size;
    uint32_t sequence_number;
    uint32_t zero_0;
    uint32_t zero_1;
} data_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t stay_in_loader;
} flash_end_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t address;
    uint32_t size;
} flash_read_rom_cmd;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t address;
    uint32_t total_size;
    uint32_t packet_data_size;
    uint32_t max_inflight_packets;
} flash_read_stub_cmd;

typedef struct __attribute__((packed))
{
    command_common_t common;
} flash_erase_chip_cmd;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t offset;
    uint32_t size;
} flash_erase_region_cmd;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t total_size;
    uint32_t blocks;
    uint32_t block_size;
    uint32_t offset;
} mem_begin_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t stay_in_loader;
    uint32_t entry_point_address;
} mem_end_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint8_t sync_sequence[36];
} sync_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t address;
    uint32_t value;
    uint32_t mask;
    uint32_t delay_us;
} write_reg_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t address;
} read_reg_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t configuration;
    uint32_t zero; // ESP32 ROM only
} spi_attach_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t new_baudrate;
    uint32_t old_baudrate;
} change_baudrate_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t address;
    uint32_t size;
    uint32_t reserved_0;
    uint32_t reserved_1;
} spi_flash_md5_command_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
} get_security_info_command_t;

typedef struct __attribute__((packed))
{
    uint8_t direction;
    uint8_t command;    // One of command_t
    uint16_t size;
    uint32_t value;
} common_response_t;

typedef struct __attribute__((packed))
{
    uint8_t failed;
    uint8_t error;
} response_status_t;

typedef struct __attribute__((packed))
{
    command_common_t common;
    uint32_t id;
    uint32_t total_size;
    uint32_t block_size;
    uint32_t sector_size;
    uint32_t page_size;
    uint32_t status_mask;
} write_spi_command_t;

#define GET_SECURITY_INFO_SECURE_BOOT_EN (1 << 0)
#define GET_SECURITY_INFO_SECURE_BOOT_AGGRESSIVE_REVOKE (1 << 1)
#define GET_SECURITY_INFO_SECURE_DOWNLOAD_ENABLE (1 << 2)
#define GET_SECURITY_INFO_SECURE_BOOT_KEY_REVOKE0 (1 << 3)
#define GET_SECURITY_INFO_SECURE_BOOT_KEY_REVOKE1 (1 << 4)
#define GET_SECURITY_INFO_SECURE_BOOT_KEY_REVOKE2 (1 << 5)
#define GET_SECURITY_INFO_SOFT_DIS_JTAG (1 << 6)
#define GET_SECURITY_INFO_HARD_DIS_JTAG (1 << 7)
#define GET_SECURITY_INFO_DIS_USB (1 << 8)
#define GET_SECURITY_INFO_DIS_DOWNLOAD_DCACHE (1 << 9)
#define GET_SECURITY_INFO_DIS_DOWNLOAD_ICACHE (1 << 10)

typedef struct __attribute__((packed))
{
    uint32_t flags;
    uint8_t flash_crypt_cnt;
    uint8_t key_purposes[7];
    uint32_t chip_id;
    uint32_t eco_version;
} get_security_info_response_data_t;

/* All command builder functions take esp_loader_t *loader as first param */

esp_loader_error_t loader_flash_begin_cmd(esp_loader_t *loader, uint32_t *seq_num, uint32_t offset, uint32_t erase_size, uint32_t block_size, uint32_t blocks_to_write, bool encryption);

esp_loader_error_t loader_flash_data_cmd(esp_loader_t *loader, uint32_t *seq_num, const uint8_t *data, uint32_t size);

esp_loader_error_t loader_flash_end_cmd(esp_loader_t *loader, bool stay_in_loader);

esp_loader_error_t loader_flash_deflate_begin_cmd(esp_loader_t *loader, uint32_t *seq_num, uint32_t offset, uint32_t erase_size, uint32_t block_size,
        uint32_t blocks_to_write, bool encryption);

esp_loader_error_t loader_flash_deflate_data_cmd(esp_loader_t *loader, uint32_t *seq_num, const uint8_t *data, uint32_t size);

esp_loader_error_t loader_flash_deflate_end_cmd(esp_loader_t *loader, bool stay_in_loader);

esp_loader_error_t loader_md5_cmd(esp_loader_t *loader, uint32_t address, uint32_t size, uint8_t *md5_out);

esp_loader_error_t loader_spi_parameters(esp_loader_t *loader, uint32_t total_size);

esp_loader_error_t loader_flash_erase_cmd(esp_loader_t *loader);

esp_loader_error_t loader_flash_erase_region_cmd(esp_loader_t *loader, uint32_t offset, uint32_t size);

esp_loader_error_t loader_flash_read_rom_cmd(esp_loader_t *loader, uint32_t address, uint8_t *data);

esp_loader_error_t loader_flash_read_stub_cmd(esp_loader_t *loader, uint32_t address, uint32_t size, uint32_t size_per_packet);

esp_loader_error_t loader_sync_cmd(esp_loader_t *loader);

esp_loader_error_t loader_spi_attach_cmd(esp_loader_t *loader, uint32_t config);

esp_loader_error_t loader_get_security_info_cmd(esp_loader_t *loader, get_security_info_response_data_t *response,
        uint32_t *response_recv_size);

esp_loader_error_t loader_mem_begin_cmd(esp_loader_t *loader, uint32_t *seq_num, uint32_t offset, uint32_t size, uint32_t blocks_to_write, uint32_t block_size);

esp_loader_error_t loader_mem_data_cmd(esp_loader_t *loader, uint32_t *seq_num, const uint8_t *data, uint32_t size);

esp_loader_error_t loader_mem_end_cmd(esp_loader_t *loader, uint32_t entrypoint);

esp_loader_error_t loader_write_reg_cmd(esp_loader_t *loader, uint32_t address, uint32_t value, uint32_t mask, uint32_t delay_us);

esp_loader_error_t loader_read_reg_cmd(esp_loader_t *loader, uint32_t address, uint32_t *reg);

esp_loader_error_t loader_change_baudrate_cmd(esp_loader_t *loader, uint32_t new_baudrate, uint32_t old_baudrate);

#ifdef __cplusplus
}
#endif
