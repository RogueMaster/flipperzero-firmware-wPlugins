/*
 * SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_loader_error.h"
#include "esp_loader_io.h"
#include "md5_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Macro which can be used to check the error code,
 * and return in case the code is not ESP_LOADER_SUCCESS.
 */
#define RETURN_ON_ERROR(x) do {         \
    esp_loader_error_t _err_ = (x);     \
    if (_err_ != ESP_LOADER_SUCCESS) {  \
        return _err_;                   \
    }                                   \
} while(0)

/**
 * @brief Supported targets
 */
typedef enum {
    ESP8266_CHIP = 0,
    ESP32_CHIP   = 1,
    ESP32S2_CHIP = 2,
    ESP32C3_CHIP = 3,
    ESP32S3_CHIP = 4,
    ESP32C2_CHIP = 5,
    ESP32C5_CHIP = 6,
    ESP32H2_CHIP = 7,
    ESP32C6_CHIP = 8,
    ESP32P4_CHIP = 9,
    ESP32C61_CHIP = 10,
    ESP_MAX_CHIP = 11,
    ESP_UNKNOWN_CHIP = 11
} target_chip_t;

/**
 * @brief Forward declarations for internal types used only as pointers inside
 * struct esp_loader.  Full definitions live in private headers.
 */
struct esp_loader_protocol_ops_s;
struct target_registers_t;

/**
 * @brief Application binary image header (8-byte common header).
 */
typedef struct {
    uint8_t magic;
    uint8_t segments;
    uint8_t flash_mode;
    uint8_t flash_size_freq;
    uint32_t entrypoint;
} esp_loader_bin_header_t;

/**
 * @brief Segment descriptor within an application binary.
 */
typedef struct {
    uint32_t addr;
    uint32_t size;
    const uint8_t *data;
} esp_loader_bin_segment_t;

typedef struct {
    target_chip_t target_chip;
    uint32_t eco_version; // Not present on ESP32-S2
    bool secure_boot_enabled;
    bool secure_boot_aggressive_revoke_enabled;
    bool secure_download_mode_enabled;
    bool secure_boot_revoked_keys[3];
    bool jtag_software_disabled;
    bool jtag_hardware_disabled;
    bool usb_disabled;
    bool flash_encryption_enabled;
    bool dcache_in_uart_download_disabled;
    bool icache_in_uart_download_disabled;
} esp_loader_target_security_info_t;

/**
 * @brief Connection arguments
 */
typedef struct {
    uint32_t sync_timeout;  /*!< Maximum time to wait for response from serial interface. */
    int32_t trials;         /*!< Number of trials to connect to target. If greater than 1,
                               100 millisecond delay is inserted after each try. */
} esp_loader_connect_args_t;

#define ESP_LOADER_CONNECT_DEFAULT() { \
  .sync_timeout = 100, \
  .trials = 10, \
}

/**
 * @brief Flash operation context.
 *
 * Allocate on the stack, fill the public fields, then pass to
 * esp_loader_flash_start() / esp_loader_flash_write() / esp_loader_flash_finish().
 * Fields inside _state are managed by the library; do not access them directly.
 */
typedef struct {
    uint32_t offset;      /*!< Flash address to write to. Must be 4-byte aligned. */
    uint32_t image_size;  /*!< Total size of the image. Must be 4-byte aligned. */
    uint32_t block_size;  /*!< Size of each block passed to esp_loader_flash_write(). */
    bool     skip_verify; /*!< When true, MD5 accumulation and verification in esp_loader_flash_finish() are skipped.
                               *   Defaults to false (i.e. verification is enabled by default). */
    struct {
        uint32_t          _sequence_number;
        struct MD5Context _md5_context;
    } _state;
} esp_loader_flash_cfg_t;

/**
 * @brief Compressed flash operation context (DEFLATE/zlib stream).
 *
 * Allocate on the stack, fill the public fields, then pass to
 * esp_loader_flash_deflate_start() / esp_loader_flash_deflate_write() / esp_loader_flash_deflate_finish().
 * Fields inside _state are managed by the library; do not access them directly.
 */
typedef struct {
    uint32_t offset;           /*!< Flash address to write to. Must be 4-byte aligned. */
    uint32_t image_size;       /*!< Size of the uncompressed image in bytes. */
    uint32_t compressed_size;  /*!< Size of the compressed data in bytes. */
    uint32_t block_size;       /*!< Size of each compressed block. */
    struct {
        uint32_t _sequence_number;
    } _state;
} esp_loader_flash_deflate_cfg_t;

/**
 * @brief RAM load operation context.
 *
 * Allocate on the stack, fill the public fields, then pass to
 * esp_loader_mem_start() / esp_loader_mem_write() / esp_loader_mem_finish().
 * Fields inside _state are managed by the library; do not access them directly.
 */
typedef struct {
    uint32_t offset;      /*!< RAM address to load to. */
    uint32_t size;        /*!< Total size of the data to load. */
    uint32_t block_size;  /*!< Size of each block passed to esp_loader_mem_write(). */
    struct {
        uint32_t _sequence_number;
    } _state;
} esp_loader_mem_cfg_t;

/**
 * @brief Protocol type stored in the loader context.
 *
 * Used internally to gate protocol-specific behaviour (e.g. baud-rate changes
 * are not available on SDIO; SPI does not support flash operations).
 */
typedef enum {
    ESP_LOADER_PROTOCOL_SERIAL, /*!< SLIP over a byte stream (UART, USB CDC-ACM, …) */
    ESP_LOADER_PROTOCOL_SPI,    /*!< SPI */
    ESP_LOADER_PROTOCOL_SDIO,   /*!< SDIO */
} esp_loader_protocol_t;

/**
 * @brief Loader context.
 *
 * Declare on the stack or as a static and pass its address to every
 * esp_loader_*() call.  All fields are prefixed with _ and must not be
 * accessed directly — they are private implementation details.
 */
typedef struct esp_loader {
    const struct esp_loader_protocol_ops_s *_protocol;
    esp_loader_port_t                      *_port;
    esp_loader_protocol_t                   _protocol_type;
    target_chip_t                           _target;
    const struct target_registers_t        *_reg;
    uint32_t  _target_flash_size;
    bool      _stub_running;
    bool      _spi_attached;
    union {
        struct {
            uint32_t sip_seq_tx;
            uint32_t mem_offset;
        } sdio;
        struct {
            uint8_t slave_seq_tx;
            uint8_t slave_seq_rx;
        } spi;
    } _proto_ctx;
} esp_loader_t;

/**
  * @brief Initialize the loader context for the serial (SLIP) protocol.
  *
  * Use with any byte-stream port: UART (@c esp32_uart_ops), USB CDC-ACM
  * (@c esp32_usb_cdc_acm_ops), Linux @c /dev/tty*, etc.  Hardware initialisation
  * is performed automatically by calling @c port->ops->init(port) inside this
  * function — no separate port init call is needed.
  *
  * @code
  *   esp32_port_t port = {
  *       .port.ops          = &esp32_uart_ops,
  *       .baud_rate         = 115200,
  *       .uart_port         = UART_NUM_1,
  *       .uart_rx_pin       = GPIO_NUM_5,
  *       .uart_tx_pin       = GPIO_NUM_4,
  *       .reset_pin         = GPIO_NUM_25,
  *       .boot_pin          = GPIO_NUM_26,
  *   };
  *   esp_loader_t loader;
  *   esp_loader_init_serial(&loader, &port.port);
  *   esp_loader_connect(&loader, &connect_args);
  *
  *   // Two devices in parallel — declare two port instances
  *   esp32_port_t port_a = { .port.ops = &esp32_uart_ops, ... };
  *   esp32_port_t port_b = { .port.ops = &esp32_uart_ops, ... };
  *   esp_loader_init_serial(&loader_a, &port_a.port);
  *   esp_loader_init_serial(&loader_b, &port_b.port);
  * @endcode
  *
  * @param loader[in]  Pointer to a caller-allocated esp_loader_t instance.
  * @param port[in]    Pointer to the port base handle (@c ops must be populated).
  *
  * @return ESP_LOADER_SUCCESS on success, or the error returned by @c ops->init.
  */
esp_loader_error_t esp_loader_init_serial(esp_loader_t *loader, esp_loader_port_t *port);

/**
  * @brief Initialize the loader context for SPI protocol.
  *
  * @param loader[in]  Pointer to a caller-allocated esp_loader_t instance.
  * @param port[in]    Pointer to the port handle.
  *
  * @return ESP_LOADER_SUCCESS on success.
  */
esp_loader_error_t esp_loader_init_spi(esp_loader_t *loader, esp_loader_port_t *port);

/**
  * @brief Initialize the loader context for SDIO protocol.
  *
  * @param loader[in]  Pointer to a caller-allocated esp_loader_t instance.
  * @param port[in]    Pointer to the port handle.
  *
  * @return ESP_LOADER_SUCCESS on success.
  */
esp_loader_error_t esp_loader_init_sdio(esp_loader_t *loader, esp_loader_port_t *port);

/**
  * @brief Deinitializes the loader and releases hardware resources.
  *
  * Calls the port's @c ops->deinit() callback (if non-NULL), which tears down
  * any peripheral drivers that were initialised by @c esp_loader_init_*(). Safe
  * to call even if @c ops->deinit is NULL — it is a no-op in that case.
  *
  * After this call the @p loader instance must not be used until re-initialized.
  *
  * @param loader[in] Pointer to an initialized loader context.
  */
void esp_loader_deinit(esp_loader_t *loader);

/**
  * @brief Connects to the target
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_connect(esp_loader_t *loader, esp_loader_connect_args_t *connect_args);

/**
  * @brief   Returns attached target chip.
  *
  * @warning This function can only be called after connection with target
  *          has been successfully established by calling esp_loader_connect().
  *
  * @param loader[in] Pointer to initialized loader context.
  *
  * @return  One of target_chip_t
  */
target_chip_t esp_loader_get_target(esp_loader_t *loader);

/**
  * @brief Connects to the target while using the flasher stub
  *
  * @note  Only supported on the serial (SLIP) interface.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_connect_with_stub(esp_loader_t *loader, esp_loader_connect_args_t *connect_args);

/**
  * @brief Connects to the target running in secure download mode
  *
  * @note  Only supported on the serial (SLIP) interface.
  * @note  Not supported on ESP8266 and ESP32.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  * @param flash_size[in]   Flash size of the target chip in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Chip does not support secure download mode
  */
esp_loader_error_t esp_loader_connect_secure_download_mode(esp_loader_t *loader,
        esp_loader_connect_args_t *connect_args,
        uint32_t flash_size);

/**
  * @brief Initiates flash operation
  *
  * @note  Must be followed by esp_loader_flash_finish() after all esp_loader_flash_write()
  *        calls, even when @p cfg->skip_verify is @c true. Skipping esp_loader_flash_finish()
  *        causes the flash-end command to never be sent and, when @p cfg->skip_verify is @c false,
  *        silently skips MD5 verification.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in,out] Flash operation context. Caller fills offset, image_size and block_size
  *                    before calling. The _state sub-struct is initialized by this function
  *                    and must not be modified by the caller.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_start(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg);

/**
  * @brief Writes supplied data to target's flash memory.
  *
  * @param loader[in,out]  Pointer to initialized loader context.
  * @param cfg[in,out]     Flash operation context initialized by esp_loader_flash_start().
  * @param payload[in]     Data to be flashed into target's memory.
  * @param size[in]        Size of payload in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_write(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg, const void *payload, uint32_t size);

/**
  * @brief Ends flash operation.
  *
  * When @p cfg->skip_verify is @c false, this function verifies flash integrity by comparing the
  * MD5 digest accumulated during esp_loader_flash_write() calls against the digest computed
  * by the target over the same flash region. Verification runs before the flash-end command
  * is issued; if it fails, @c ESP_LOADER_ERROR_INVALID_MD5 is returned immediately and the
  * flash-end command is not sent (the target stays in the loader).
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in]     Flash operation context initialized by esp_loader_flash_start().
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_INVALID_MD5 MD5 mismatch (only when skip_verify was false)
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC MD5 verify unsupported on this target/protocol
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_finish(esp_loader_t *loader, esp_loader_flash_cfg_t *cfg);

/**
  * @brief Initiates compressed flash operation (DEFLATE/zlib stream).
  *
  * @note  Only supported on the serial (SLIP) interface.
  *
  * @note  The deflate path does not accumulate an MD5 digest internally (the compressed
  *        data stream cannot be hashed to match the plaintext MD5). Use
  *        esp_loader_flash_verify_known_md5() after flashing to verify the uncompressed
  *        content.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in,out] Compressed flash operation context. Caller fills offset, image_size,
  *                    compressed_size and block_size before calling. The _state sub-struct
  *                    is initialized by this function and must not be modified by the caller.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_flash_deflate_start(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg);

/**
  * @brief Writes a compressed data block to target flash memory.
  *
  * @param loader[in,out]  Pointer to initialized loader context.
  * @param cfg[in,out]     Compressed flash context initialized by esp_loader_flash_deflate_start().
  * @param payload[in]     Data buffer containing a zlib-compressed block.
  * @param size[in]        Size of payload in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_deflate_write(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg, void *payload, uint32_t size);

/**
  * @brief Ends compressed flash operation.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in]     Compressed flash context initialized by esp_loader_flash_deflate_start().
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_deflate_finish(esp_loader_t *loader, esp_loader_flash_deflate_cfg_t *cfg);

/**
  * @brief Detects the size of the flash chip used by target
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param flash_size[out] Flash size detected in bytes
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_UNSUPPORTED_CHIP The target flash chip is not known
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_flash_detect_size(esp_loader_t *loader, uint32_t *flash_size);

/**
  * @brief Reads from the target flash.
  *
  * @note  Only supported on the serial (SLIP) interface.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param buf[out] Buffer to read into
  * @param address[in] Flash address to read from.
  * @param length[in] Read length in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_flash_read(esp_loader_t *loader, uint8_t *buf, uint32_t address, uint32_t length);

/**
  * @brief Erase the whole flash chip
  *
  * @param loader[in] Pointer to initialized loader context.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_erase(esp_loader_t *loader);

/**
  * @brief Erase a region of the flash
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param offset[in]  The offset of the region to erase (must be 4096 byte aligned)
  * @param size[in]    The size of the region to erase (must be 4096 byte aligned)
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_erase_region(esp_loader_t *loader, uint32_t offset, uint32_t size);

/**
  * @brief Get the security info of the target chip
  *
  * @note  Only supported on the serial (SLIP) interface.
  *
  * @param loader[in]        Pointer to initialized loader context.
  * @param security_info[out] The security info structure
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_get_security_info(esp_loader_t *loader,
        esp_loader_target_security_info_t *security_info);

/**
  * @brief Initiates RAM load operation.
  *
  * @param loader[in]  Pointer to initialized loader context.
  * @param cfg[in,out] RAM load context. Caller fills offset, size and block_size before
  *                    calling. The _state sub-struct is initialized by this function and
  *                    must not be modified by the caller.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_mem_start(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg);

/**
  * @brief Writes supplied data to target's RAM.
  *
  * @param loader[in,out]  Pointer to initialized loader context.
  * @param cfg[in,out]     RAM load context initialized by esp_loader_mem_start().
  * @param payload[in]     Data to be loaded into target's memory.
  * @param size[in]        Size of data in bytes.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_mem_write(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg, const void *payload, uint32_t size);

/**
  * @brief Finishes RAM load operation and transfers control to the loaded program.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param cfg[in]          RAM load context initialized by esp_loader_mem_start().
  * @param entrypoint[in]   Entry point address of the loaded program.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_mem_finish(esp_loader_t *loader, esp_loader_mem_cfg_t *cfg, uint32_t entrypoint);

/**
  * @brief Reads the MAC address of the connected chip.
  *
  * @param loader[in] Pointer to initialized loader context.
  * @param mac[out] 6 byte MAC address of the chip
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_read_mac(esp_loader_t *loader, uint8_t *mac);

/**
  * @brief Writes register.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param address[in]      Address of register.
  * @param reg_value[in]    New register value.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_write_register(esp_loader_t *loader, uint32_t address, uint32_t reg_value);

/**
  * @brief Reads register.
  *
  * @param loader[in]       Pointer to initialized loader context.
  * @param address[in]      Address of register.
  * @param reg_value[out]   Register value.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_read_register(esp_loader_t *loader, uint32_t address, uint32_t *reg_value);

/**
  * @brief Change host and target transmission rate.
  *
  * Sends a change-baud command to the ROM bootloader or flasher stub, then updates
  * the host port via @c change_transmission_rate. Works after @c esp_loader_connect()
  * or @c esp_loader_connect_with_stub().
  *
  * @note  Only supported on the serial (SLIP) interface. Not supported on ESP8266 or SDIO.
  *
  * @param loader[in]              Pointer to initialized loader context.
  * @param transmission_rate[in]   New baud rate to set.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Not supported by the protocol
  */
esp_loader_error_t esp_loader_change_transmission_rate(esp_loader_t *loader, uint32_t transmission_rate);

/**
  * @brief Verify target's flash integrity by checking with a known MD5 checksum
  * for a specified offset and length.
  *
  * @param loader[in] Pointer to initialized loader context.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_INVALID_MD5 MD5 does not match
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Unsupported on the target
  *     - ESP_LOADER_ERROR_IMAGE_SIZE Flash region specified is beyond the flash end
  */
esp_loader_error_t esp_loader_flash_verify_known_md5(esp_loader_t *loader,
        uint32_t address,
        uint32_t size,
        const uint8_t *expected_md5);

/**
  * @brief Toggles reset pin.
  *
  * @param loader[in] Pointer to initialized loader context.
  */
void esp_loader_reset_target(esp_loader_t *loader);

#ifdef __cplusplus
}
#endif
