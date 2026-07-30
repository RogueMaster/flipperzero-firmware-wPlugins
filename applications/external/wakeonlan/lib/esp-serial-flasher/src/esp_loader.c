/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protocol.h"
#include "esp_loader.h"
#include "esp_loader_protocol.h"
#include "esp_stubs.h"
#include "esp_targets.h"
#include "md5_hash.h"
#include "slip.h"
#include "loader_log.h"
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#if SERIAL_FLASHER_LOG_LEVEL >= ESP_LOADER_LOG_INFO
static const char *target_chip_name(target_chip_t chip)
{
    static const char *const names[ESP_MAX_CHIP] = {
        [ESP8266_CHIP] = "ESP8266",
        [ESP32_CHIP] = "ESP32",
        [ESP32S2_CHIP] = "ESP32-S2",
        [ESP32C3_CHIP] = "ESP32-C3",
        [ESP32S3_CHIP] = "ESP32-S3",
        [ESP32C2_CHIP] = "ESP32-C2",
        [ESP32C5_CHIP] = "ESP32-C5",
        [ESP32H2_CHIP] = "ESP32-H2",
        [ESP32C6_CHIP] = "ESP32-C6",
        [ESP32P4_CHIP] = "ESP32-P4",
        [ESP32C61_CHIP] = "ESP32-C61",
    };
    if ((unsigned)chip < sizeof(names) / sizeof(names[0])) {
        return names[chip];
    }
    return "UNKNOWN";
}
#endif

#ifndef SERIAL_FLASHER_WRITE_BLOCK_RETRIES
#define SERIAL_FLASHER_WRITE_BLOCK_RETRIES 3
#endif

#define SHORT_TIMEOUT 100
#define DEFAULT_TIMEOUT 1000
#define DEFAULT_FLASH_TIMEOUT 3000
#define LOAD_RAM_TIMEOUT_PER_MB 2000
#define MD5_TIMEOUT_PER_MB 8000
#define ERASE_FLASH_TIMEOUT_PER_MB 10000

#define INITIAL_UART_BAUDRATE 115200

#define FLASH_SECTOR_SIZE 4096
#define ROM_FLASH_BLOCK_SIZE 1024

#define DEFAULT_FLASH_SIZE (2 * 1024 * 1024)
#define MAX_ROM_FLASH_SIZE (16 * 1024 * 1024)
#define FLASH_READ_STUB_PACKET_SIZE 4096U

typedef enum {
    SPI_FLASH_READ_ID = 0x9F
} spi_flash_cmd_t;


static inline void init_md5(esp_loader_flash_cfg_t *cfg)
{
    MD5Init(&cfg->_state._md5_context);
}

static inline void md5_update(esp_loader_flash_cfg_t *cfg, const uint8_t *data, uint32_t size)
{
    MD5Update(&cfg->_state._md5_context, data, size);
}

static inline void md5_final(esp_loader_flash_cfg_t *cfg, uint8_t digest[16])
{
    MD5Final(digest, &cfg->_state._md5_context);
}

static uint32_t timeout_per_mb(uint32_t size_bytes, uint32_t time_per_mb)
{
    uint32_t timeout = (uint32_t)((uint64_t)time_per_mb * size_bytes / 1000000UL);
    return MAX(timeout, DEFAULT_FLASH_TIMEOUT);
}


static esp_loader_error_t loader_init_common(esp_loader_t *loader,
        esp_loader_protocol_t protocol,
        const esp_loader_protocol_ops_t *proto_ops,
        esp_loader_port_t *port)
{
    memset(loader, 0, sizeof(*loader));
    loader->_protocol_type = protocol;
    loader->_protocol      = proto_ops;
    loader->_port          = port;
    loader->_target        = ESP_UNKNOWN_CHIP;

    if (port->ops->init) {
        return port->ops->init(port);
    }
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_init_serial(esp_loader_t *loader, esp_loader_port_t *port)
{
    return loader_init_common(loader, ESP_LOADER_PROTOCOL_SERIAL, esp_loader_get_serial_ops(), port);
}

esp_loader_error_t esp_loader_init_spi(esp_loader_t *loader, esp_loader_port_t *port)
{
    return loader_init_common(loader, ESP_LOADER_PROTOCOL_SPI, esp_loader_get_spi_ops(), port);
}

esp_loader_error_t esp_loader_init_sdio(esp_loader_t *loader, esp_loader_port_t *port)
{
    return loader_init_common(loader, ESP_LOADER_PROTOCOL_SDIO, esp_loader_get_sdio_ops(), port);
}

void esp_loader_deinit(esp_loader_t *loader)
{
    if (loader->_port && loader->_port->ops->deinit) {
        loader->_port->ops->deinit(loader->_port);
    }
    memset(loader, 0, sizeof(*loader));
}

/*
 * Attaches the SPI flash to the ROM bootloader targets if not already attached.
 * Must be called before any flash operation (handled via init_flash_params).
 * Safe to call multiple times — it is a no-op once already attached.
 */
static esp_loader_error_t loader_ensure_spi_attached(esp_loader_t *loader)
{
    if (loader->_spi_attached || loader->_protocol->spi_attach == NULL) {
        return ESP_LOADER_SUCCESS;
    }

    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);

    if (loader->_target == ESP8266_CHIP) {
        uint32_t sequence_number = 0;
        RETURN_ON_ERROR(loader_flash_begin_cmd(loader, &sequence_number, 0, 0, 0, 0, false));
    } else {
        uint32_t spi_config;
        RETURN_ON_ERROR(loader_read_spi_config(loader, loader->_target, &spi_config));
        RETURN_ON_ERROR(loader->_protocol->spi_attach(loader, spi_config));
    }

    loader->_spi_attached = true;
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_connect(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{
    loader->_port->ops->enter_bootloader(loader->_port);

    RETURN_ON_ERROR(loader->_protocol->initialize_conn(loader, connect_args));

    RETURN_ON_ERROR(loader_detect_chip(loader));

    LOADER_LOGI(loader, "Connected - target: %s", target_chip_name(loader->_target));

    return ESP_LOADER_SUCCESS;
}

target_chip_t esp_loader_get_target(esp_loader_t *loader)
{
    return loader->_target;
}

esp_loader_error_t esp_loader_connect_with_stub(esp_loader_t *loader, esp_loader_connect_args_t *connect_args)
{
    if (loader->_protocol_type != ESP_LOADER_PROTOCOL_SERIAL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    loader->_target_flash_size = 0;

    loader->_port->ops->enter_bootloader(loader->_port);

    RETURN_ON_ERROR(loader->_protocol->initialize_conn(loader, connect_args));

    RETURN_ON_ERROR(loader_detect_chip(loader));

    const esp_stub_t *stub;
    if (loader->_target == ESP32P4_CHIP) {
        esp_loader_target_security_info_t info;
        bool got_info = (esp_loader_get_security_info(loader, &info) == ESP_LOADER_SUCCESS);
        if (got_info && info.eco_version >= ESP32P4_ECO_REV3_MIN) {
            stub = esp_stub[ESP32P4_CHIP];   // ECO5+
        } else {
            stub = &esp_stub_esp32p4rev1;    // ECO < 5 or revision unknown
        }
    } else {
        stub = esp_stub[loader->_target];
        if (stub == NULL) {
            return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
        }
    }

    LOADER_LOGI(loader, "Connected - target: %s", target_chip_name(loader->_target));

    esp_loader_mem_cfg_t mem_cfg = {0};

    for (uint32_t seg = 0; seg < sizeof(stub->segments) / sizeof(stub->segments[0]); seg++) {
        if (stub->segments[seg].size == 0) {
            continue;
        }
        mem_cfg = (esp_loader_mem_cfg_t) {
            .offset = stub->segments[seg].addr,
            .size = stub->segments[seg].size,
            .block_size = ESP_RAM_BLOCK,
        };
        RETURN_ON_ERROR(esp_loader_mem_start(loader, &mem_cfg));

        size_t remain_size = stub->segments[seg].size;
        const uint8_t *data_pos = stub->segments[seg].data;
        while (remain_size > 0) {
            size_t data_size = MIN(ESP_RAM_BLOCK, remain_size);
            RETURN_ON_ERROR(esp_loader_mem_write(loader, &mem_cfg, data_pos, data_size));
            data_pos += data_size;
            remain_size -= data_size;
        }
    }

    RETURN_ON_ERROR(esp_loader_mem_finish(loader, &mem_cfg, stub->header.entrypoint));

    uint8_t buff[4];
    size_t recv_size = 0;
    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
    esp_loader_error_t err = SLIP_receive_packet(loader, buff, sizeof(buff) / sizeof(buff[0]), &recv_size);
    if (err != ESP_LOADER_SUCCESS) {
        return err;
    } else if (recv_size != sizeof(buff) || memcmp(buff, "OHAI", sizeof(buff) / sizeof(buff[0]))) {
        return ESP_LOADER_ERROR_INVALID_RESPONSE;
    }

    loader->_stub_running = true;

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_connect_secure_download_mode(esp_loader_t *loader,
        esp_loader_connect_args_t *connect_args,
        const uint32_t flash_size)
{
    if (loader->_protocol->spi_attach == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    loader->_target_flash_size = flash_size;

    loader->_port->ops->enter_bootloader(loader->_port);

    RETURN_ON_ERROR(loader->_protocol->initialize_conn(loader, connect_args));

    RETURN_ON_ERROR(loader_detect_chip(loader));

    if (loader->_target == ESP8266_CHIP || loader->_target == ESP32_CHIP) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(loader->_protocol->spi_attach(loader, 0));

    LOADER_LOGI(loader, "Connected - target: %s", target_chip_name(loader->_target));

    /* Mark as attached so loader_ensure_spi_attached() does not repeat the call.
     * spi_config is 0 here because efuse registers are not readable in secure
     * download mode; this differs from the normal attach path. */
    loader->_spi_attached = true;
    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t spi_set_data_lengths(esp_loader_t *loader, size_t mosi_bits, size_t miso_bits)
{

    if (mosi_bits > 0) {
        RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->mosi_dlen, mosi_bits - 1));
    }
    if (miso_bits > 0) {
        RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->miso_dlen, miso_bits - 1));
    }

    return ESP_LOADER_SUCCESS;
}

static esp_loader_error_t spi_set_data_lengths_8266(esp_loader_t *loader, size_t mosi_bits, size_t miso_bits)
{

    uint32_t mosi_mask = (mosi_bits == 0) ? 0 : mosi_bits - 1;
    uint32_t miso_mask = (miso_bits == 0) ? 0 : miso_bits - 1;
    return esp_loader_write_register(loader, loader->_reg->usr1, (miso_mask << 8) | (mosi_mask << 17));
}

static esp_loader_error_t spi_flash_command(esp_loader_t *loader, spi_flash_cmd_t cmd, void *data_tx, size_t tx_size, void *data_rx, size_t rx_size)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    assert(rx_size <= 32);
    assert(tx_size <= 64);

    uint32_t SPI_USR_CMD  = (1 << 31);
    uint32_t SPI_USR_MISO = (1 << 28);
    uint32_t SPI_USR_MOSI = (1 << 27);
    uint32_t SPI_CMD_USR  = (1 << 18);
    uint32_t CMD_LEN_SHIFT = 28;

    uint32_t old_spi_usr;
    uint32_t old_spi_usr2;
    RETURN_ON_ERROR(esp_loader_read_register(loader, loader->_reg->usr, &old_spi_usr));
    RETURN_ON_ERROR(esp_loader_read_register(loader, loader->_reg->usr2, &old_spi_usr2));

    if (loader->_target == ESP8266_CHIP) {
        RETURN_ON_ERROR(spi_set_data_lengths_8266(loader, tx_size, rx_size));
    } else {
        RETURN_ON_ERROR(spi_set_data_lengths(loader, tx_size, rx_size));
    }

    uint32_t usr_reg_2 = (7 << CMD_LEN_SHIFT) | cmd;
    uint32_t usr_reg = SPI_USR_CMD;
    if (rx_size > 0) {
        usr_reg |= SPI_USR_MISO;
    }
    if (tx_size > 0) {
        usr_reg |= SPI_USR_MOSI;
    }

    RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->usr, usr_reg));
    RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->usr2, usr_reg_2));

    if (tx_size == 0) {
        RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->w0, 0));
    } else {
        uint32_t *data = (uint32_t *)data_tx;
        uint32_t words_to_write = (tx_size + 31) / (8 * 4);
        uint32_t data_reg_addr = loader->_reg->w0;

        while (words_to_write--) {
            uint32_t word = *data++;
            RETURN_ON_ERROR(esp_loader_write_register(loader, data_reg_addr, word));
            data_reg_addr += 4;
        }
    }

    RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->cmd, SPI_CMD_USR));

    uint32_t trials = 10;
    while (trials--) {
        uint32_t cmd_reg;
        RETURN_ON_ERROR(esp_loader_read_register(loader, loader->_reg->cmd, &cmd_reg));
        if ((cmd_reg & SPI_CMD_USR) == 0) {
            break;
        }
    }

    if (trials == 0) {
        return ESP_LOADER_ERROR_TIMEOUT;
    }

    RETURN_ON_ERROR(esp_loader_read_register(loader, loader->_reg->w0, data_rx));

    RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->usr, old_spi_usr));
    RETURN_ON_ERROR(esp_loader_write_register(loader, loader->_reg->usr2, old_spi_usr2));

    return ESP_LOADER_SUCCESS;
}

static uint32_t calc_erase_size(const target_chip_t target, bool stub_running,
                                const uint32_t offset, const uint32_t image_size)
{
    if (target != ESP8266_CHIP || stub_running) {
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

esp_loader_error_t esp_loader_flash_detect_size(esp_loader_t *loader, uint32_t *flash_size)
{
    RETURN_ON_ERROR(loader_ensure_spi_attached(loader));

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
    RETURN_ON_ERROR(spi_flash_command(loader, SPI_FLASH_READ_ID, NULL, 0, &flash_id, 24));
    uint8_t size_id = flash_id >> 16;

    for (size_t i = 0; i < sizeof(size_mapping) / sizeof(size_mapping[0]); i++) {
        if (size_id == size_mapping[i].id) {
            *flash_size = size_mapping[i].size;
            return ESP_LOADER_SUCCESS;
        }
    }

    return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
}

static esp_loader_error_t init_flash_params(esp_loader_t *loader)
{
    RETURN_ON_ERROR(loader_ensure_spi_attached(loader));

    /* Flash size will be known in advance if we're in secure download mode or we already read it*/
    if (loader->_target_flash_size == 0) {
        if (esp_loader_flash_detect_size(loader, &loader->_target_flash_size) != ESP_LOADER_SUCCESS) {
            LOADER_LOGW(loader, "Flash size detection failed, falling back to default");
            loader->_target_flash_size = DEFAULT_FLASH_SIZE;
        } else {
            LOADER_LOGI(loader, "Flash size: %" PRIu32 " MB",
                        loader->_target_flash_size / (1024u * 1024u));
        }
    }

    // SDIO doesn't need to set SPI parameters as the temporary stub already does it
    // TODO: Should be removed when the temporary stub is removed ESF-221
    if (loader->_protocol_type != ESP_LOADER_PROTOCOL_SDIO) {
        loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
        RETURN_ON_ERROR(loader_spi_parameters(loader, loader->_target_flash_size));
    }
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_start(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    if (cfg->offset % 4 != 0 || cfg->image_size % 4 != 0) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    /* ROM bootloader uses 24-bit SPI addressing — addresses >= 16 MB silently
     * wrap around to 0. Reject such writes early to prevent silent corruption. */
    if (!loader->_stub_running && cfg->offset >= MAX_ROM_FLASH_SIZE) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    LOADER_LOGI(loader, "Flash write start - offset: 0x%08" PRIx32 "  size: %" PRIu32 " bytes",
                cfg->offset, cfg->image_size);

    RETURN_ON_ERROR(init_flash_params(loader));
    if (cfg->image_size + cfg->offset > loader->_target_flash_size) {
        return ESP_LOADER_ERROR_IMAGE_SIZE;
    }

    if (!cfg->skip_verify) {
        if (loader->_target == ESP8266_CHIP && !loader->_stub_running) {
            return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
        }
        init_md5(cfg);
    }

    bool encryption_in_cmd = encryption_in_begin_flash_cmd(loader->_target) && !loader->_stub_running;
    const uint32_t blocks_to_write = (cfg->image_size + cfg->block_size - 1) / cfg->block_size;
    const uint32_t erase_size = calc_erase_size(loader->_target, loader->_stub_running, cfg->offset, cfg->image_size);
    loader->_port->ops->start_timer(loader->_port, timeout_per_mb(erase_size, ERASE_FLASH_TIMEOUT_PER_MB));
    return loader_flash_begin_cmd(loader, &cfg->_state._sequence_number, cfg->offset, erase_size, cfg->block_size, blocks_to_write, encryption_in_cmd);
}


esp_loader_error_t esp_loader_flash_write(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg, const void *payload, uint32_t size)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    if (size > cfg->block_size) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    if (!cfg->skip_verify) {
        md5_update(cfg, payload, size);
    }

    unsigned int attempt = 0;
    esp_loader_error_t result = ESP_LOADER_ERROR_FAIL;
    uint32_t saved_seq = cfg->_state._sequence_number;
    do {
        cfg->_state._sequence_number = saved_seq;
        loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
        result = loader_flash_data_cmd(loader, &cfg->_state._sequence_number, payload, size);
        attempt++;
        if (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES) {
            LOADER_LOGW(loader, "Flash write failed (attempt %u/%u), retrying",
                        attempt, (unsigned)SERIAL_FLASHER_WRITE_BLOCK_RETRIES);
        }
    } while (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES);

    return result;
}

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

esp_loader_error_t esp_loader_flash_finish(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg)
{
    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    if (!cfg->skip_verify) {
        uint8_t raw_md5[16] = {0};
        uint8_t hex_md5[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};
        md5_final(cfg, raw_md5);
        hexify(raw_md5, hex_md5);
        RETURN_ON_ERROR(esp_loader_flash_verify_known_md5(loader,
                        cfg->offset, cfg->image_size, hex_md5));
    }

    // ROM does not respond to next commands after flash end, even when stay_in_loader is set to true.
    // If set to false, the bootloader is rebooted, but reading of strapping pins is not done again, so it boots
    // into the bootloader again.
    if (loader->_stub_running) {
        loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
        RETURN_ON_ERROR(loader_flash_end_cmd(loader, true));
    }
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_deflate_start(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    if (cfg->offset % 4 != 0) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    if (loader->_target == ESP8266_CHIP && !loader->_stub_running) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    LOADER_LOGI(loader,
                "Compressed flash write start - offset: 0x%08" PRIx32 "  size: %" PRIu32 " bytes  compressed: %" PRIu32 " bytes",
                cfg->offset, cfg->image_size, cfg->compressed_size);

    RETURN_ON_ERROR(init_flash_params(loader));
    if (cfg->image_size + cfg->offset > loader->_target_flash_size) {
        return ESP_LOADER_ERROR_IMAGE_SIZE;
    }

    bool encryption_in_cmd = encryption_in_begin_flash_cmd(loader->_target) && !loader->_stub_running;
    const uint32_t erase_size = calc_erase_size(loader->_target, loader->_stub_running, cfg->offset, cfg->image_size);
    const uint32_t blocks_to_write = (cfg->compressed_size + cfg->block_size - 1) / cfg->block_size;

    loader->_port->ops->start_timer(loader->_port, timeout_per_mb(erase_size, ERASE_FLASH_TIMEOUT_PER_MB));
    return loader_flash_deflate_begin_cmd(loader, &cfg->_state._sequence_number, cfg->offset, erase_size, cfg->block_size, blocks_to_write, encryption_in_cmd);
}


esp_loader_error_t esp_loader_flash_deflate_write(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg, void *payload, uint32_t size)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    if (size > cfg->block_size) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    unsigned int attempt = 0;
    esp_loader_error_t result = ESP_LOADER_ERROR_FAIL;
    uint32_t saved_seq = cfg->_state._sequence_number;
    do {
        cfg->_state._sequence_number = saved_seq;
        loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
        result = loader_flash_deflate_data_cmd(loader, &cfg->_state._sequence_number, payload, size);
        attempt++;
        if (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES) {
            LOADER_LOGW(loader, "Compressed flash write failed (attempt %u/%u), retrying",
                        attempt, (unsigned)SERIAL_FLASHER_WRITE_BLOCK_RETRIES);
        }
    } while (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES);

    return result;
}


esp_loader_error_t esp_loader_flash_deflate_finish(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg)
{
    (void)cfg;

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    if (!loader->_stub_running) {
        return ESP_LOADER_SUCCESS;
    }

    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
    return loader_flash_deflate_end_cmd(loader, true);
}

esp_loader_error_t esp_loader_flash_erase(esp_loader_t *loader)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    LOADER_LOGI(loader, "Flash erase start");

    if (loader->_stub_running) {
        RETURN_ON_ERROR(init_flash_params(loader));

        loader->_port->ops->start_timer(loader->_port, timeout_per_mb(loader->_target_flash_size, ERASE_FLASH_TIMEOUT_PER_MB));
        RETURN_ON_ERROR(loader_flash_erase_cmd(loader));
    } else {
        uint32_t flash_size = 0;
        RETURN_ON_ERROR(esp_loader_flash_detect_size(loader, &flash_size));
        esp_loader_flash_cfg_t cfg = {
            .offset = 0,
            .image_size = flash_size,
            .block_size = ROM_FLASH_BLOCK_SIZE,
            .skip_verify = true,
        };
        RETURN_ON_ERROR(esp_loader_flash_start(loader, &cfg));
    }
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_erase_region(esp_loader_t *loader, uint32_t offset, uint32_t size)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    if (offset % FLASH_SECTOR_SIZE != 0 || size % FLASH_SECTOR_SIZE != 0) {
        return ESP_LOADER_ERROR_INVALID_PARAM;
    }

    LOADER_LOGI(loader, "Flash erase region start - offset: 0x%08" PRIx32 "  size: %" PRIu32 " bytes",
                offset, size);

    if (loader->_stub_running) {
        RETURN_ON_ERROR(init_flash_params(loader));

        loader->_port->ops->start_timer(loader->_port, timeout_per_mb(size, ERASE_FLASH_TIMEOUT_PER_MB));
        RETURN_ON_ERROR(loader_flash_erase_region_cmd(loader, offset, size));
    } else {
        uint32_t flash_size = 0;
        RETURN_ON_ERROR(esp_loader_flash_detect_size(loader, &flash_size));
        if (offset + size > flash_size) {
            return ESP_LOADER_ERROR_FAIL;
        }
        esp_loader_flash_cfg_t cfg = {
            .offset = offset,
            .image_size = size,
            .block_size = ROM_FLASH_BLOCK_SIZE,
            .skip_verify = true,
        };
        RETURN_ON_ERROR(esp_loader_flash_start(loader, &cfg));
    }
    return ESP_LOADER_SUCCESS;
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

esp_loader_error_t esp_loader_get_security_info(esp_loader_t *loader,
        esp_loader_target_security_info_t *security_info)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    loader->_port->ops->start_timer(loader->_port, SHORT_TIMEOUT);

    get_security_info_response_data_t resp;
    uint32_t response_received_size = 0;
    RETURN_ON_ERROR(loader_get_security_info_cmd(loader, &resp, &response_received_size));

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

static esp_loader_error_t loader_flash_read_stub(esp_loader_t *loader, uint8_t *dest,
        uint32_t address, uint32_t length)
{
    if (loader->_protocol->recv_stub_packet == NULL || loader->_protocol->send_stub_ack == NULL) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    struct MD5Context md5_context;
    MD5Init(&md5_context);

    const uint32_t packet_size = MIN(length, FLASH_READ_STUB_PACKET_SIZE);

    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(loader_flash_read_stub_cmd(loader, address, length, packet_size));

    uint32_t remaining = length;
    while (remaining > 0) {
        const uint32_t offset = length - remaining;
        const uint32_t to_receive = MIN(remaining, packet_size);
        size_t recv_size = 0;

        loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
        RETURN_ON_ERROR(loader->_protocol->recv_stub_packet(loader, &dest[offset], to_receive, &recv_size));

        if (recv_size != to_receive) {
            return ESP_LOADER_ERROR_INVALID_RESPONSE;
        }

        MD5Update(&md5_context, &dest[offset], recv_size);
        remaining -= recv_size;

        loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
        RETURN_ON_ERROR(loader->_protocol->send_stub_ack(loader, length - remaining));
    }

    uint8_t md5_calc[16];
    MD5Final(md5_calc, &md5_context);

    uint8_t md5_recv[16];
    size_t recv_size = 0;
    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
    RETURN_ON_ERROR(loader->_protocol->recv_stub_packet(loader, md5_recv, sizeof(md5_recv), &recv_size));

    if (recv_size != sizeof(md5_recv) || memcmp(md5_calc, md5_recv, sizeof(md5_calc))) {
        return ESP_LOADER_ERROR_INVALID_MD5;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_flash_read(esp_loader_t *loader, uint8_t *dest, uint32_t address, uint32_t length)
{

    if (loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    RETURN_ON_ERROR(init_flash_params(loader));
    if (address + length > loader->_target_flash_size) {
        return ESP_LOADER_ERROR_IMAGE_SIZE;
    }

    if (loader->_stub_running) {
        RETURN_ON_ERROR(loader_flash_read_stub(loader, dest, address, length));
    } else {
        const uint32_t seek_back_len = address % READ_FLASH_ROM_DATA_SIZE;
        address -= seek_back_len;
        length += seek_back_len;

        uint32_t copy_dest_start = 0;
        int32_t remaining = length;
        while (remaining > 0) {
            uint8_t buf[READ_FLASH_ROM_DATA_SIZE];

            loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);
            RETURN_ON_ERROR(loader_flash_read_rom_cmd(loader, address + length - remaining, buf));

            const bool first_read = remaining == (int32_t)length;
            size_t to_read = MIN(remaining, (int32_t)sizeof(buf));
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

esp_loader_error_t esp_loader_mem_start(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg)
{

    uint32_t blocks_to_write = ROUNDUP(cfg->size, cfg->block_size);

    loader->_port->ops->start_timer(loader->_port, timeout_per_mb(cfg->size, LOAD_RAM_TIMEOUT_PER_MB));

    if (loader->_protocol->mem_begin_cmd) {
        cfg->_state._sequence_number = 0;
        return loader->_protocol->mem_begin_cmd(loader, cfg->offset, cfg->size, blocks_to_write, cfg->block_size);
    }

    return loader_mem_begin_cmd(loader, &cfg->_state._sequence_number, cfg->offset, cfg->size, blocks_to_write, cfg->block_size);
}


esp_loader_error_t esp_loader_mem_write(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg, const void *payload, uint32_t size)
{
    const uint8_t *data = (const uint8_t *)payload;

    unsigned int attempt = 0;
    esp_loader_error_t result = ESP_LOADER_ERROR_FAIL;
    do {
        loader->_port->ops->start_timer(loader->_port, timeout_per_mb(size, LOAD_RAM_TIMEOUT_PER_MB));
        if (loader->_protocol->mem_data_cmd) {
            result = loader->_protocol->mem_data_cmd(loader, data, size);
        } else {
            result = loader_mem_data_cmd(loader, &cfg->_state._sequence_number, data, size);
        }
        attempt++;
        if (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES) {
            LOADER_LOGW(loader, "RAM write failed (attempt %u/%u), retrying",
                        attempt, (unsigned)SERIAL_FLASHER_WRITE_BLOCK_RETRIES);
        }
    } while (result != ESP_LOADER_SUCCESS && attempt < SERIAL_FLASHER_WRITE_BLOCK_RETRIES);

    return result;
}


esp_loader_error_t esp_loader_mem_finish(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg, uint32_t entrypoint)
{
    (void)cfg;

    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);

    if (loader->_protocol->mem_end_cmd) {
        return loader->_protocol->mem_end_cmd(loader, entrypoint);
    }

    return loader_mem_end_cmd(loader, entrypoint);
}

esp_loader_error_t esp_loader_read_mac(esp_loader_t *loader, uint8_t *mac)
{

    if (loader->_target == ESP8266_CHIP) {
        return ESP_LOADER_ERROR_UNSUPPORTED_CHIP;
    }

    return loader_read_mac(loader, loader->_target, mac);
}

esp_loader_error_t esp_loader_read_register(esp_loader_t *loader, uint32_t address, uint32_t *reg_value)
{
    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);

    return loader_read_reg_cmd(loader, address, reg_value);
}

esp_loader_error_t esp_loader_write_register(esp_loader_t *loader, uint32_t address, uint32_t reg_value)
{
    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);

    return loader_write_reg_cmd(loader, address, reg_value, 0xFFFFFFFF, 0);
}

static esp_loader_error_t get_crystal_frequency_esp32c2(esp_loader_t *loader, uint32_t *frequency)
{
    /*
    There is a bug in the ESP32-C2 ROM that causes it to think it has a 40 MHz crystal,
    even though it might be 26 MHz. That is why we need to check frequency and adjust
    the transmission rate accordingly.

    The logic here is:
    - We know that our baud rate and the target's UART baud rate are roughly the same,
    or we couldn't communicate
    - We can read the UART clock divider register to know how the ESP derives this
    from the APB bus frequency
    - Multiplying these two together gives us the bus frequency which is either
    the crystal frequency or multiple of the crystal frequency (for some chips).
    */

    // ESP32-C2 supported crystal frequencies
    const uint32_t ESP32C2_CRYSTAL_26MHZ = 26;
    const uint32_t ESP32C2_CRYSTAL_40MHZ = 40;

    const uint32_t CRYSTAL_FREQ_THRESHOLD = 33;

    // UART clock divider register address and mask
    const uint32_t UART_CLK_DIV_REG = 0x60000014;
    const uint32_t UART_CLK_DIV_REG_MASK = 0xFFFFF;

    *frequency = 0;
    uint32_t est_freq;
    RETURN_ON_ERROR(esp_loader_read_register(loader, UART_CLK_DIV_REG, &est_freq));
    est_freq &= UART_CLK_DIV_REG_MASK;

    est_freq = (INITIAL_UART_BAUDRATE * est_freq) / 1000000U;

    if (est_freq > CRYSTAL_FREQ_THRESHOLD) {
        *frequency = ESP32C2_CRYSTAL_40MHZ;
    } else {
        *frequency = ESP32C2_CRYSTAL_26MHZ;
    }

    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t esp_loader_change_transmission_rate(esp_loader_t *loader, uint32_t transmission_rate)
{

    if (loader->_target == ESP8266_CHIP || loader->_protocol_type == ESP_LOADER_PROTOCOL_SDIO) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }
    if (loader->_target == ESP32C2_CHIP) {
        const uint32_t ESP32C2_CRYSTAL_26MHZ = 26;
        const uint32_t ESP32C2_CRYSTAL_40MHZ = 40;

        uint32_t frequency;
        // The ESP32-C2 still thinks it has 40 MHz crystal, even though it might be 26 MHz.
        // So we need to adjust the transmission rate accordingly.
        RETURN_ON_ERROR(get_crystal_frequency_esp32c2(loader, &frequency));
        if (frequency == ESP32C2_CRYSTAL_26MHZ) {
            transmission_rate = transmission_rate * ESP32C2_CRYSTAL_40MHZ / ESP32C2_CRYSTAL_26MHZ;
        }
    }

    loader->_port->ops->start_timer(loader->_port, DEFAULT_TIMEOUT);

    esp_loader_error_t err = loader_change_baudrate_cmd(loader, transmission_rate, 0);
    if (err == ESP_LOADER_SUCCESS) {
        if (loader->_stub_running) {
            loader->_port->ops->delay_ms(loader->_port, 25);
        }
        if (loader->_port->ops->change_transmission_rate != NULL) {
            err = loader->_port->ops->change_transmission_rate(loader->_port, transmission_rate);
        }
    }
    return err;
}

esp_loader_error_t esp_loader_flash_verify_known_md5(esp_loader_t *loader,
        uint32_t address,
        uint32_t size,
        const uint8_t *expected_md5)
{

    if ((loader->_target == ESP8266_CHIP && !loader->_stub_running) || loader->_protocol_type == ESP_LOADER_PROTOCOL_SPI) {
        return ESP_LOADER_ERROR_UNSUPPORTED_FUNC;
    }

    RETURN_ON_ERROR(init_flash_params(loader));

    if (address + size > loader->_target_flash_size) {
        return ESP_LOADER_ERROR_IMAGE_SIZE;
    }

    uint8_t received_md5[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};

    loader->_port->ops->start_timer(loader->_port, timeout_per_mb(size, MD5_TIMEOUT_PER_MB));

    RETURN_ON_ERROR(loader_md5_cmd(loader, address, size, received_md5));

    if (loader->_stub_running) {
        uint8_t rec_md5_hex[MAX(MD5_SIZE_ROM, MD5_SIZE_STUB) + 1] = {0};
        hexify(received_md5, rec_md5_hex);
        memcpy(received_md5, rec_md5_hex, MD5_SIZE_ROM);
    }

    bool md5_match = memcmp(expected_md5, received_md5, MD5_SIZE_ROM) == 0;
    if (!md5_match) {
        LOADER_LOGE(loader, "MD5 mismatch - expected: %.*s  actual: %.*s",
                    (int)MD5_SIZE_ROM, (const char *)expected_md5,
                    (int)MD5_SIZE_ROM, (const char *)received_md5);
        return ESP_LOADER_ERROR_INVALID_MD5;
    }

    return ESP_LOADER_SUCCESS;
}

void esp_loader_reset_target(esp_loader_t *loader)
{
    loader->_stub_running = false;
    loader->_spi_attached = false;
    loader->_port->ops->reset_target(loader->_port);
}
