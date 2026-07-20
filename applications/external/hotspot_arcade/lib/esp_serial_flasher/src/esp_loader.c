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
#include "esp_loader_io.h"
#include "esp_loader.h"
#include "esp_stubs.h"
#include "esp_targets.h"
#include "md5_hash.h"
#include "slip.h"
#include <string.h>
#include <assert.h>

#define SHORT_TIMEOUT 100
#define DEFAULT_TIMEOUT 1000
#define DEFAULT_FLASH_TIMEOUT 3000
#define LOAD_RAM_TIMEOUT_PER_MB 2000000
#define MD5_TIMEOUT_PER_MB 8000
#define ERASE_FLASH_TIMEOUT_PER_MB 10000

#define FLASH_SECTOR_SIZE 4096
#define ROM_FLASH_BLOCK_SIZE 1024

typedef enum {
    SPI_FLASH_READ_ID = 0x9F
} spi_flash_cmd_t;

static const target_registers_t *s_reg = NULL;
static target_chip_t s_target = ESP_UNKNOWN_CHIP;

#ifndef SERIAL_FLASHER_INTERFACE_SPI
#define DEFAULT_FLASH_SIZE 2 * 1024 * 1024
static uint32_t s_flash_write_size = 0;
static uint32_t s_target_flash_size = 0;
#endif

#if MD5_ENABLED

static struct MD5Context s_md5_context;
static uint32_t s_start_address;
static uint32_t s_image_size;

static inline void init_md5(uint32_t address, uint32_t size)
{
    s_start_address = address;
    s_image_size = size;
    MD5Init(&s_md5_context);
}

static inline void md5_update(const uint8_t *data, uint32_t size)
{
    MD5Update(&s_md5_context, data, size);
}

static inline void md5_final(uint8_t digets[16])
{
    MD5Final(digets, &s_md5_context);
}

#endif

static uint32_t timeout_per_mb(uint32_t size_bytes, uint32_t time_per_mb)
{
    uint32_t timeout = time_per_mb * (size_bytes / 1e6);
    return MAX(timeout, DEFAULT_FLASH_TIMEOUT);
}

esp_loader_error_t esp_loader_connect(esp_loader_connect_args_t *connect_args)
{
    loader_port_enter_bootloader();

    RETURN_ON_ERROR(loader_initialize_conn(connect_args));

    RETURN_ON_ERROR(loader_detect_chip(&s_target, &s_reg));

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
    s_target_flash_size = 0;

    if (s_target == ESP8266_CHIP) {
        loader_port_start_timer(DEFAULT_TIMEOUT);
        return loader_flash_begin_cmd(0, 0, 0, 0, s_target);
    } else {
        uint32_t spi_config;
        RETURN_ON_ERROR( loader_read_spi_config(s_target, &spi_config) );
        loader_port_start_timer(DEFAULT_TIMEOUT);
        return loader_spi_attach_cmd(spi_config);
    }
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

    return ESP_LOADER_SUCCESS;
}

target_chip_t esp_loader_get_target(void)
{
    return s_target;
}

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
esp_loader_error_t esp_loader_connect_with_stub(esp_loader_connect_args_t *connect_args)
{
    s_target_flash_size = 0;

    loader_port_enter_bootloader();

    RETURN_ON_ERROR(loader_initialize_conn(connect_args));

    RETURN_ON_ERROR(loader_detect_chip(&s_target, &s_reg));

    if (s_target == ESP32P4_CHIP || s_target == ESP32C5_CHIP) {
        return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
    }

    RETURN_ON_ERROR(loader_run_stub(s_target));

    return ESP_LOADER_SUCCESS;
}

#ifdef SERIAL_FLASHER_INTERFACE_UART
esp_loader_error_t esp_loader_connect_secure_download_mode(esp_loader_connect_args_t *connect_args,
        const uint32_t flash_size, const target_chip_t target_chip)
{
    s_target_flash_size = flash_size;
    s_target = target_chip;

    loader_port_enter_bootloader();

    RETURN_ON_ERROR(loader_initialize_conn(connect_args));

    if (s_target == ESP_UNKNOWN_CHIP) {
        RETURN_ON_ERROR(loader_detect_chip(&s_target, &s_reg));
    }

    if (s_target == ESP8266_CHIP) {
        loader_port_start_timer(DEFAULT_TIMEOUT);
        return loader_flash_begin_cmd(0, 0, 0, 0, s_target);
    } else {
        loader_port_start_timer(DEFAULT_TIMEOUT);
        return loader_spi_attach_cmd(0);
    }

    return ESP_LOADER_SUCCESS;
}
#endif /* SERIAL_FLASHER_INTERFACE_UART */
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

#ifndef SERIAL_FLASHER_INTERFACE_SPI
static esp_loader_error_t spi_set_data_lengths(size_t mosi_bits, size_t miso_bits)
{
    if (mosi_bits > 0) {
        RETURN_ON_ERROR( esp_loader_write_register(s_reg->mosi_dlen, mosi_bits - 1) );
    }
    if (miso_bits > 0) {
        RETURN_ON_ERROR( esp_loader_write_register(s_reg->miso_dlen, miso_bits - 1) );
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t spi_set_data_lengths_8266(size_t mosi_bits, size_t miso_bits)
{
    uint32_t mosi_mask = (mosi_bits == 0) ? 0 : mosi_bits - 1;
    uint32_t miso_mask = (miso_bits == 0) ? 0 : miso_bits - 1;
    return esp_loader_write_register(s_reg->usr1, (miso_mask << 8) | (mosi_mask << 17));
}

static esp_loader_error_t spi_flash_command(spi_flash_cmd_t cmd, void *data_tx, size_t tx_size, void *data_rx, size_t rx_size)
{
    assert(rx_size <= 32); // Reading more than 32 bits back from a SPI flash operation is unsupported
    assert(tx_size <= 64); // Writing more than 64 bytes of data with one SPI command is unsupported

    uint32_t SPI_USR_CMD  = (1 << 31);
    uint32_t SPI_USR_MISO = (1 << 28);
    uint32_t SPI_USR_MOSI = (1 << 27);
    uint32_t SPI_CMD_USR  = (1 << 18);
    uint32_t CMD_LEN_SHIFT = 28;

    // Save SPI configuration
    uint32_t old_spi_usr;
    uint32_t old_spi_usr2;
    RETURN_ON_ERROR( esp_loader_read_register(s_reg->usr, &old_spi_usr) );
    RETURN_ON_ERROR( esp_loader_read_register(s_reg->usr2, &old_spi_usr2) );

    if (s_target == ESP8266_CHIP) {
        RETURN_ON_ERROR( spi_set_data_lengths_8266(tx_size, rx_size) );
    } else {
        RETURN_ON_ERROR( spi_set_data_lengths(tx_size, rx_size) );
    }

    uint32_t usr_reg_2 = (7 << CMD_LEN_SHIFT) | cmd;
    uint32_t usr_reg = SPI_USR_CMD;
    if (rx_size > 0) {
        usr_reg |= SPI_USR_MISO;
    }
    if (tx_size > 0) {
        usr_reg |= SPI_USR_MOSI;
    }

    RETURN_ON_ERROR( esp_loader_write_register(s_reg->usr, usr_reg) );
    RETURN_ON_ERROR( esp_loader_write_register(s_reg->usr2, usr_reg_2 ) );

    if (tx_size == 0) {
        // clear data register before we read it
        RETURN_ON_ERROR( esp_loader_write_register(s_reg->w0, 0) );
    } else {
        uint32_t *data = (uint32_t *)data_tx;
        uint32_t words_to_write = (tx_size + 31) / (8 * 4);
        uint32_t data_reg_addr = s_reg->w0;

        while (words_to_write--) {
            uint32_t word = *data++;
            RETURN_ON_ERROR( esp_loader_write_register(data_reg_addr, word) );
            data_reg_addr += 4;
        }
    }

    RETURN_ON_ERROR( esp_loader_write_register(s_reg->cmd, SPI_CMD_USR) );

    uint32_t trials = 10;
    while (trials--) {
        uint32_t cmd_reg;
        RETURN_ON_ERROR( esp_loader_read_register(s_reg->cmd, &cmd_reg) );
        if ((cmd_reg & SPI_CMD_USR) == 0) {
            break;
        }
    }

    if (trials == 0) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    RETURN_ON_ERROR( esp_loader_read_register(s_reg->w0, data_rx) );

    // Restore SPI configuration
    RETURN_ON_ERROR( esp_loader_write_register(s_reg->usr, old_spi_usr) );
    RETURN_ON_ERROR( esp_loader_write_register(s_reg->usr2, old_spi_usr2) );

    return ESP_LOADER_SUCCESS;
}

static uint32_t calc_erase_size(const target_chip_t target, const uint32_t offset,
                                const uint32_t image_size)
{
    if (target != ESP8266_CHIP || esp_stub_get_running()) {
        return image_size;
    } else {
        /* Needed to fix a bug in the ESP8266 ROM */
        const uint32_t sectors_per_block = 16U;
        const uint32_t sector_size = 4096U;

        const uint32_t num_sectors = (image_size + sector_size - 1) / sector_size;
        const uint32_t start_sector = offset / sector_size;

        uint32_t head_sectors = sectors_per_block - (start_sector % sectors_per_block);

        /* The ROM bug deletes extra num_sectors if we don't cross the block boundary
           and extra head_sectors if we do */
        if (num_sectors <= head_sectors) {
            return ((num_sectors + 1) / 2) * sector_size;
        } else {
            return (num_sectors - head_sectors) * sector_size;
        }
    }
}

esp_loader_error_t esp_loader_flash_detect_size(uint32_t *flash_size)
{
    typedef struct {
        uint8_t id;
        uint32_t size;
    } size_id_size_pair_t;

    /* There is no rule manufacturers have to follow for assigning these parts of the flash ID,
       these constants have been taken from esptool source code. */
    static const size_id_size_pair_t size_mapping[] = {
        { 0x12, 256 * 1024 },
        { 0x13, 512 * 1024 },
        { 0x14, 1 * 1024 * 1024 },
        { 0x15, 2 * 1024 * 1024 },
        { 0x16, 4 * 1024 * 1024 },
        { 0x17, 8 * 1024 * 1024 },
        { 0x18, 16 * 1024 * 1024 },
        { 0x19, 32 * 1024 * 1024 },
        { 0x1A, 64 * 1024 * 1024 },
        { 0x1B, 128 * 1024 * 1024 },
        { 0x1C, 256 * 1024 * 1024 },
        { 0x20, 64 * 1024 * 1024 },
        { 0x21, 128 * 1024 * 1024 },
        { 0x22, 256 * 1024 * 1024 },
        { 0x32, 256 * 1024 },
        { 0x33, 512 * 1024 },
        { 0x34, 1 * 1024 * 1024 },
        { 0x35, 2 * 1024 * 1024 },
        { 0x36, 4 * 1024 * 1024 },
        { 0x37, 8 * 1024 * 1024 },
        { 0x38, 16 * 1024 * 1024 },
        { 0x39, 32 * 1024 * 1024 },
        { 0x3A, 64 * 1024 * 1024 },
    };

    uint32_t flash_id = 0;
    RETURN_ON_ERROR( spi_flash_command(SPI_FLASH_READ_ID, NULL, 0, &flash_id, 24) );
    uint8_t size_id = flash_id >> 16;

    // Try finding the size id within supported size ids
    for (size_t i = 0; i < sizeof(size_mapping) / sizeof(size_mapping[0]); i++) {
        if (size_id == size_mapping[i].id) {
            *flash_size = size_mapping[i].size;
            return ESP_LOADER_SUCCESS;
        }
    }

    return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
}

static esp_loader_error_t init_flash_params(void)
{
    /* Flash size will be known in advance if we're in secure download mode or we already read it*/
    if (s_target_flash_size == 0) {
        if (esp_loader_flash_detect_size(&s_target_flash_size) != ESP_LOADER_SUCCESS) {
            loader_port_debug_print("Flash size detection failed, falling back to default");
            s_target_flash_size = DEFAULT_FLASH_SIZE;
        }
    }

#ifndef SERIAL_FLASHER_INTERFACE_SDIO
    loader_port_start_timer(DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(loader_spi_parameters(s_target_flash_size));
#endif

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_start(uint32_t offset, uint32_t image_size, uint32_t block_size)
{
    s_flash_write_size = block_size;

    // Both the address and image size must be aligned to 4 bytes
    if (offset % 4 != 0 || image_size % 4 != 0) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    RETURN_ON_ERROR(init_flash_params());
    if (image_size + offset > s_target_flash_size) {
        return ESP_LOADER_ERROR_IMAGE_SIZE;
    }

#if MD5_ENABLED
    init_md5(offset, image_size);
#endif

    bool encryption_in_cmd = encryption_in_begin_flash_cmd(s_target) && !esp_stub_get_running();
    const uint32_t erase_size = calc_erase_size(esp_loader_get_target(), offset, image_size);
    const uint32_t blocks_to_write = (image_size + block_size - 1) / block_size;

    loader_port_start_timer(timeout_per_mb(erase_size, ERASE_FLASH_TIMEOUT_PER_MB));
    return loader_flash_begin_cmd(offset, erase_size, block_size, blocks_to_write, encryption_in_cmd);
}


esp_loader_error_t esp_loader_flash_write(void *payload, uint32_t size)
{
    uint32_t padding_bytes = s_flash_write_size - size;
    uint8_t *data = (uint8_t *)payload;
    uint32_t padding_index = size;

    if (size > s_flash_write_size) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    const uint8_t padding_pattern = 0xFF;
    while (padding_bytes--) {
        data[padding_index++] = padding_pattern;
    }

#if MD5_ENABLED
    md5_update(payload, (size + 3) & ~3);
#endif

    unsigned int attempt = 0;
    esp_loader_error_t result = ESP_LOADER_ERROR_FAIL;
    do {
        loader_port_start_timer(DEFAULT_TIMEOUT);
        result = loader_flash_data_cmd(data, s_flash_write_size);
        attempt++;
    } while (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES);

    return result;
}


esp_loader_error_t esp_loader_flash_finish(bool reboot)
{
    loader_port_start_timer(DEFAULT_TIMEOUT);

    return loader_flash_end_cmd(!reboot);
}

esp_loader_error_t esp_loader_flash_erase(void)
{
    if (esp_stub_get_running()) {
        RETURN_ON_ERROR(init_flash_params());

        loader_port_start_timer(timeout_per_mb(s_target_flash_size, ERASE_FLASH_TIMEOUT_PER_MB));
        RETURN_ON_ERROR(loader_flash_erase_cmd());
    } else {
        // erase using flash begin
        uint32_t flash_size = 0;
        RETURN_ON_ERROR(esp_loader_flash_detect_size(&flash_size));
        RETURN_ON_ERROR(esp_loader_flash_start(0, flash_size, ROM_FLASH_BLOCK_SIZE));
    }
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_erase_region(uint32_t offset, uint32_t size)
{
    // Both offset and size must be aligned to flash sector size.
    if (offset % FLASH_SECTOR_SIZE != 0 || size % FLASH_SECTOR_SIZE != 0) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    if (esp_stub_get_running()) {
        RETURN_ON_ERROR(init_flash_params());

        loader_port_start_timer(timeout_per_mb(size, ERASE_FLASH_TIMEOUT_PER_MB));
        RETURN_ON_ERROR(loader_flash_erase_region_cmd(offset, size));
    } else {
        // erase using flash begin
        uint32_t flash_size = 0;
        RETURN_ON_ERROR(esp_loader_flash_detect_size(&flash_size));
        if (offset + size > flash_size) {
            return ESP_LOADER_ERROR_FAIL;
        }
        RETURN_ON_ERROR(esp_loader_flash_start(offset, size, ROM_FLASH_BLOCK_SIZE));
    }
    return ESP_LOADER_SUCCESS;
}
#endif /* SERIAL_FLASHER_INTERFACE_SPI */

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
esp_loader_error_t esp_loader_change_transmission_rate_stub(const uint32_t old_transmission_rate,
        const uint32_t new_transmission_rate)
{
    if (s_target == ESP8266_CHIP || !esp_stub_get_running()) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    loader_port_start_timer(DEFAULT_TIMEOUT);

    esp_loader_error_t err = loader_change_baudrate_cmd(new_transmission_rate, old_transmission_rate);

    // Wait for the stub to be ready to receive data.
    if (err == ESP_LOADER_SUCCESS) {
        loader_port_delay_ms(25);
    }

    return err;
}

static uint32_t byte_popcnt(uint8_t byte)
{
    uint32_t cnt = 0;
    for (uint32_t bit = 0; bit < 8; bit++) {
        cnt += byte & 0x01;
        byte >>= 1;
    }

    return cnt;
}

esp_loader_error_t esp_loader_get_security_info(esp_loader_target_security_info_t *security_info)
{
    loader_port_start_timer(SHORT_TIMEOUT);

    get_security_info_response_data_t resp;
    uint32_t response_received_size = 0;
    RETURN_ON_ERROR(loader_get_security_info_cmd(&resp, &response_received_size));

    if (response_received_size == sizeof(get_security_info_response_data_t)) {
        security_info->target_chip = target_from_chip_id(resp.chip_id);
        security_info->eco_version = resp.eco_version;
    } else if (response_received_size == sizeof(get_security_info_response_data_t) - 8) {
        security_info->target_chip = ESP32S2_CHIP;
        security_info->eco_version = 0;
    } else {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    security_info->secure_boot_enabled =
        (resp.flags & GET_SECURITY_INFO_SECURE_BOOT_EN) != 0;
    security_info->secure_boot_aggressive_revoke_enabled =
        (resp.flags & GET_SECURITY_INFO_SECURE_BOOT_AGGRESSIVE_REVOKE) != 0;
    security_info->secure_download_mode_enabled =
        (resp.flags & GET_SECURITY_INFO_SECURE_DOWNLOAD_ENABLE) != 0;
    security_info->secure_boot_revoked_keys[0] =
        (resp.flags & GET_SECURITY_INFO_SECURE_BOOT_KEY_REVOKE0) != 0;
    security_info->secure_boot_revoked_keys[1] =
        (resp.flags & GET_SECURITY_INFO_SECURE_BOOT_KEY_REVOKE1) != 0;
    security_info->secure_boot_revoked_keys[2] =
        (resp.flags & GET_SECURITY_INFO_SECURE_BOOT_KEY_REVOKE2) != 0;
    security_info->jtag_software_disabled =
        (resp.flags & GET_SECURITY_INFO_SOFT_DIS_JTAG) != 0;
    security_info->jtag_hardware_disabled =
        (resp.flags & GET_SECURITY_INFO_HARD_DIS_JTAG) != 0;
    security_info->usb_disabled = (resp.flags & GET_SECURITY_INFO_DIS_USB) != 0;

    // If the number of set bits in key_purposes is odd, flash is encrypted
    uint32_t key_purposes_bit_cnt = 0;
    for (size_t byte = 0; byte < sizeof(resp.key_purposes); byte++) {
        key_purposes_bit_cnt += byte_popcnt(resp.key_purposes[byte]);
    }
    security_info->flash_encryption_enabled = key_purposes_bit_cnt % 2;

    security_info->dcache_in_uart_download_disabled =
        (resp.flags & GET_SECURITY_INFO_DIS_DOWNLOAD_DCACHE) != 0;
    security_info->icache_in_uart_download_disabled =
        (resp.flags & GET_SECURITY_INFO_DIS_DOWNLOAD_ICACHE) != 0;

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t flash_read_stub(uint8_t *dest, uint32_t address, uint32_t length)
{
    uint8_t buf[256]; // Hardcoded for now, decent tradeoff between speed and stack usage
    size_t recv_size = 0;
    struct MD5Context md5_context;
    MD5Init(&md5_context);

    // The flasher stub requires reads to be aligned to 4 bytes.
    // The solution is to read more than is needed and discard the unecessary bytes.
    const uint32_t seek_back_len = address % 4;
    address -= seek_back_len;
    length += seek_back_len;

    const uint32_t overread_len = ROUNDUP(length, 4) - length;
    length += overread_len;

    loader_port_start_timer(DEFAULT_TIMEOUT);
    loader_flash_read_stub_cmd(address, length, sizeof(buf));

    uint32_t copy_dest_start = 0;
    int32_t remaining = length;
    while (remaining > 0) {
        loader_port_start_timer(DEFAULT_TIMEOUT);
        const uint32_t to_receive = MIN(remaining, sizeof(buf));
        RETURN_ON_ERROR(SLIP_receive_packet(buf, to_receive, &recv_size));

        if (recv_size != to_receive) {
            return ESP_LOADER_ERROR_INVALID_RESPONSE;
        }

        MD5Update(&md5_context, buf, recv_size);

        // Handle seek back and overread.
        uint32_t copy_start = 0;
        uint32_t copy_length = recv_size;

        const bool first_read = remaining == length;
        if (first_read) {
            copy_start += seek_back_len;
            copy_length -= seek_back_len;
        }

        const bool last_read = remaining - recv_size <= 0;
        if (last_read) {
            copy_length -= overread_len;
        }

        memcpy(&dest[copy_dest_start], &buf[copy_start], copy_length);
        copy_dest_start += copy_length;

        remaining -= recv_size;

        // Ack by sending back total received byte count
        const uint32_t bytes_recv = length - remaining;
        loader_port_start_timer(DEFAULT_TIMEOUT);
        RETURN_ON_ERROR(SLIP_send_delimiter());
        RETURN_ON_ERROR(SLIP_send((const uint8_t *)&bytes_recv, sizeof(bytes_recv)));
        RETURN_ON_ERROR(SLIP_send_delimiter());
    }

    uint8_t md5_calc[16];
    MD5Final(md5_calc, &md5_context);

    loader_port_start_timer(DEFAULT_TIMEOUT);
    uint8_t md5_recv[16];
    RETURN_ON_ERROR(SLIP_receive_packet(md5_recv, sizeof(md5_recv), &recv_size));

    if (recv_size != sizeof(md5_recv) || memcmp(md5_calc, md5_recv, sizeof(md5_calc))) {
        return ESP_LOADER_ERROR_INVALID_MD5;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_read(uint8_t *dest, uint32_t address, uint32_t length)
{
    RETURN_ON_ERROR(init_flash_params());
    if (address + length >= s_target_flash_size) {
        return ESP_LOADER_ERROR_IMAGE_SIZE;
    }

    if (esp_stub_get_running()) {
        RETURN_ON_ERROR(flash_read_stub(dest, address, length));
    } else {
        // We read from the ROM in 64B chunks, if we want to read anything in the last 64B
        // we need to ensure that the read is aligned to 64B, so we read more than necessary.
        const uint32_t seek_back_len = address % READ_FLASH_ROM_DATA_SIZE;
        address -= seek_back_len;
        length += seek_back_len;

        uint32_t copy_dest_start = 0;
        int32_t remaining = length;
        while (remaining > 0) {
            uint8_t buf[READ_FLASH_ROM_DATA_SIZE];

            loader_port_start_timer(DEFAULT_TIMEOUT);
            RETURN_ON_ERROR(loader_flash_read_rom_cmd(address + length - remaining, buf));

            const bool first_read = remaining == length;
            size_t to_read = MIN(remaining, sizeof(buf));
            if (first_read) {
                to_read -= seek_back_len;
                memcpy(&dest[0], &buf[seek_back_len], to_read);
            } else {
                memcpy(&dest[copy_dest_start], &buf[0], to_read);
            }

            remaining -= READ_FLASH_ROM_DATA_SIZE;
            copy_dest_start += to_read;
        }
    }

    return ESP_LOADER_SUCCESS;
}
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

esp_loader_error_t esp_loader_mem_start(uint32_t offset, uint32_t size, uint32_t block_size)
{
#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
    if (esp_stub_get_running()) {
        const esp_stub_t *stub = &esp_stub[s_target];

        // check we're not going to overwrite a running stub with this data
        const uint32_t load_start = offset;
        const uint32_t load_end = offset + size;
        for (uint32_t seg = 0; seg < sizeof(stub->segments) / sizeof(stub->segments[0]); seg++) {
            const uint32_t stub_start = stub->segments[seg].addr;
            const uint32_t stub_end = stub->segments[seg].addr + stub->segments[seg].size;
            if (load_start < stub_end && load_end > stub_start) {
                loader_port_debug_print("Software loader is resident at the requested address, can't load binary at overlapping address range");
                return ESP_LOADER_ERROR_INVALID_PARAM;
            }
        }
    }
#endif

    uint32_t blocks_to_write = ROUNDUP(size, block_size);
    loader_port_start_timer(timeout_per_mb(size, LOAD_RAM_TIMEOUT_PER_MB));
    return loader_mem_begin_cmd(offset, size, blocks_to_write, block_size);
}


esp_loader_error_t esp_loader_mem_write(const void *payload, uint32_t size)
{
    const uint8_t *data = (const uint8_t *)payload;

    unsigned int attempt = 0;
    esp_loader_error_t result = ESP_LOADER_ERROR_FAIL;
    do {
        loader_port_start_timer(timeout_per_mb(size, LOAD_RAM_TIMEOUT_PER_MB));
        result = loader_mem_data_cmd(data, size);
        attempt++;
    } while (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES);

    return result;
}


esp_loader_error_t esp_loader_mem_finish(uint32_t entrypoint)
{
    loader_port_start_timer(DEFAULT_TIMEOUT);
    return loader_mem_end_cmd(entrypoint);
}

esp_loader_error_t esp_loader_read_mac(uint8_t *mac)
{
    if (s_target == ESP8266_CHIP) {
        return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
    }

    return loader_read_mac(s_target, mac);
}

esp_loader_error_t esp_loader_read_register(uint32_t address, uint32_t *reg_value)
{
    loader_port_start_timer(DEFAULT_TIMEOUT);

    return loader_read_reg_cmd(address, reg_value);
}

esp_loader_error_t esp_loader_write_register(uint32_t address, uint32_t reg_value)
{
    loader_port_start_timer(DEFAULT_TIMEOUT);

    return loader_write_reg_cmd(address, reg_value, 0xFFFFFFFF, 0);
}

#ifndef SERIAL_FLASHER_INTERFACE_SDIO
esp_loader_error_t esp_loader_change_transmission_rate(uint32_t transmission_rate)
{
    if (s_target == ESP8266_CHIP || esp_stub_get_running()) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    loader_port_start_timer(DEFAULT_TIMEOUT);

    return loader_change_baudrate_cmd(transmission_rate, 0);
}
#endif /* SERIAL_FLASHER_INTERFACE_SDIO */

#if MD5_ENABLED
static void hexify(const uint8_t raw_md5[16], uint8_t hex_md5_out[32])
{
    static const uint8_t dec_to_hex[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    for (int i = 0; i < 16; i++) {
        *hex_md5_out++ = dec_to_hex[raw_md5[i] >> 4];
        *hex_md5_out++ = dec_to_hex[raw_md5[i] & 0xF];
    }
}

esp_loader_error_t esp_loader_flash_verify_known_md5(uint32_t address,
        uint32_t size,
        const uint8_t *expected_md5)
{
    if (s_target == ESP8266_CHIP && !esp_stub_get_running()) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    RETURN_ON_ERROR(init_flash_params());

    if (address + size >= s_target_flash_size) {
        return ESP_LOADER_ERROR_IMAGE_SIZE;
    }

    /* Zero termination require 1 byte */
    uint8_t received_md5[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};

    loader_port_start_timer(timeout_per_mb(size, MD5_TIMEOUT_PER_MB));

    RETURN_ON_ERROR(loader_md5_cmd(address, size, received_md5));

    if (esp_stub_get_running()) {
        // Convert the received MD5 to hex, because stub returns it as 16 raw data bytes
        uint8_t rec_md5_hex[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};
        hexify(received_md5, rec_md5_hex);
        memcpy(received_md5, rec_md5_hex, MD5_SIZE_ROM);
    }

    bool md5_match = memcmp(expected_md5, received_md5, MD5_SIZE_ROM) == 0;
    if (!md5_match) {
        loader_port_debug_print("Error: MD5 checksum does not match");
        loader_port_debug_print("Expected:");
        loader_port_debug_print((char *)expected_md5);
        loader_port_debug_print("Actual:");
        loader_port_debug_print((char *)received_md5);

        return ESP_LOADER_ERROR_INVALID_MD5;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_verify(void)
{
    uint8_t raw_md5[16] = {0};
    /* Zero termination require 1 byte */
    uint8_t hex_md5[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};
    md5_final(raw_md5);
    hexify(raw_md5, hex_md5);

    return esp_loader_flash_verify_known_md5(s_start_address, s_image_size, hex_md5);
}
#endif /* MD5_ENABLED */

void esp_loader_reset_target(void)
{
    esp_stub_set_running(false);
    loader_port_reset_target();
}
