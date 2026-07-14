/* Copyright 2020-2023 Espressif Systems (Shanghai) CO LTD
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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Used for backwards compatibility with the previous API */
#define esp_loader_change_baudrate esp_loader_change_transmission_rate

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
 * @brief Error codes
 */
typedef enum {
    ESP_LOADER_SUCCESS,                /*!< Success */
    ESP_LOADER_ERROR_FAIL,             /*!< Unspecified error */
    ESP_LOADER_ERROR_TIMEOUT,          /*!< Timeout elapsed */
    ESP_LOADER_ERROR_IMAGE_SIZE,       /*!< Image size to flash is larger than flash size */
    ESP_LOADER_ERROR_INVALID_MD5,      /*!< Computed and received MD5 does not match */
    ESP_LOADER_ERROR_INVALID_PARAM,    /*!< Invalid parameter passed to function */
    ESP_LOADER_ERROR_INVALID_TARGET,   /*!< Connected target is invalid */
    ESP_LOADER_ERROR_UNSUPPORTED_CHIP, /*!< Attached chip is not supported */
    ESP_LOADER_ERROR_UNSUPPORTED_FUNC, /*!< Function is not supported on attached target */
    ESP_LOADER_ERROR_INVALID_RESPONSE  /*!< Internal error */
} esp_loader_error_t;

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
    ESP_MAX_CHIP = 10,
    ESP_UNKNOWN_CHIP = 10
} target_chip_t;

/**
 * @brief Application binary header
 */
typedef struct {
    uint8_t magic;
    uint8_t segments;
    uint8_t flash_mode;
    uint8_t flash_size_freq;
    uint32_t entrypoint;
} esp_loader_bin_header_t;

/**
 * @brief Segment binary header
 */
typedef struct {
    uint32_t addr;
    uint32_t size;
    uint8_t *data;
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
  * @brief Connects to the target
  *
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_connect(esp_loader_connect_args_t *connect_args);

/**
  * @brief   Returns attached target chip.
  *
  * @warning This function can only be called after connection with target
  *          has been successfully established by calling esp_loader_connect().
  *
  * @return  One of target_chip_t
  */
target_chip_t esp_loader_get_target(void);


#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
/**
  * @brief Connects to the target while using the flasher stub
  *
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_connect_with_stub(esp_loader_connect_args_t *connect_args);

#ifdef SERIAL_FLASHER_INTERFACE_UART
/**
  * @brief Connects to the target running in secure download mode
  *
  * Secure download mode is a special mode in which the commands accepted by the boot ROM
  * are limited to a safe subset. It is enabled by burning an efuse on the target.
  * Read more about it here:
  * https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html#config-secure-uart-rom-dl-mode
  *
  * @param connect_args[in] Timing parameters to be used for connecting to target.
  * @param flash_size Flash size of the target chip.
  * @param target_chip Target chip. Used for the ESP32 and ESP8266, which do not support the
  *                    GET_SECURITY_INFO command required to identify the target in secure
  *                    download mode. Leave as ESP_UNKNOWN_CHIP for autodetection of newer chips.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_connect_secure_download_mode(esp_loader_connect_args_t *connect_args,
        uint32_t flash_size, target_chip_t target_chip);
#endif /* SERIAL_FLASHER_INTERFACE_UART */
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */

#ifndef SERIAL_FLASHER_INTERFACE_SPI
/**
  * @brief Initiates flash operation
  *
  * @param offset[in] Address from which flash operation will be performed. Must be 4 byte aligned.
  * @param image_size[in] Size of the whole binary to be loaded into flash. Must be 4 byte aligned.
  * @param block_size[in] Size of buffer used in subsequent calls to esp_loader_flash_write.
  *
  * @note  image_size is size of the whole image, whereas, block_size is chunk of data sent
  *        to the target, each time esp_loader_flash_write function is called.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_start(uint32_t offset, uint32_t image_size, uint32_t block_size);

/**
  * @brief Writes supplied data to target's flash memory.
  *
  * @param payload[in]      Data to be flashed into target's memory.
  * @param size[in]         Size of payload in bytes.
  *
  * @note  size must not be greater that block_size supplied to previously called
  *        esp_loader_flash_start function. If size is less than block_size,
  *        remaining bytes of payload buffer will be padded with 0xff.
  *        Therefore, size of payload buffer has to be equal or greater than block_size.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_write(void *payload, uint32_t size);

/**
  * @brief Ends flash operation.
  *
  * @param reboot[in]       reboot the target if true.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_finish(bool reboot);

/**
  * @brief Detects the size of the flash chip used by target
  *
  * @param flash_size[out] Flash size detected in bytes
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_UNSUPPORTED_CHIP The target flash chip is not known
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target chip is running in secure download mode
  */
esp_loader_error_t esp_loader_flash_detect_size(uint32_t *flash_size);
#endif /* SERIAL_FLASHER_INTERFACE_SPI */

#if (defined SERIAL_FLASHER_INTERFACE_UART) || (defined SERIAL_FLASHER_INTERFACE_USB)
/**
  * @brief Reads from the target flash.
  *
  * @param buf[out] Buffer to read into
  * @param address[in] Flash address to read from.
  * @param length[in] Read length in bytes.
  *
  * @note Higher read speeds can be achieved by using the flasher stub.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_UNSUPPORTED_CHIP The target flash chip is not known
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target chip is running in secure download mode
  */
esp_loader_error_t esp_loader_flash_read(uint8_t *buf, uint32_t address, uint32_t length);

/**
  * @brief Erase the whole flash chip
  *
  * @note When using ROM-based approach (without stub), this function is experimental
  *       and not fully tested in all scenarios. Use with caution. When using the stub,
  *       this function is fully supported.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  */
esp_loader_error_t esp_loader_flash_erase(void);

/**
  * @brief Erase a region of the flash
  *
  * @note When using ROM-based approach (without stub), this function is experimental
  *       and not fully tested in all scenarios. Use with caution. When using the stub,
  *       this function is fully supported.
  *
  * @param offset[in] The offset of the region to erase (must be 4096 byte aligned)
  * @param size[in] The size of the region to erase (must be 4096 byte aligned)
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_INVALID_PARAM Invalid parameter
  */
esp_loader_error_t esp_loader_flash_erase_region(uint32_t offset, uint32_t size);

/**
  * @brief Change baud rate of the stub running on the target
  *
  * @note  Baud rate has to be also adjusted accordingly on host MCU, as
  *        target's baud rate is changed upon return from this function.
  *
  * @param old_transmission_rate[in] The baudrate to be changed
  * @param new_transmission_rate[in] The new baud rate to be set.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The stub is not running
  */
esp_loader_error_t esp_loader_change_transmission_rate_stub(uint32_t old_transmission_rate,
        uint32_t new_transmission_rate);

/**
  * @brief Get the security info of the target chip
  *
  * @note  The ESP32 and ESP8266 do not support this command.
  *
  * @param security_info[out] The security info structure
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Either a timeout event or the target chip responded with
  *                                a different command code, due to not supporting the command.
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE The target reply is malformed.
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target chip does not support this command.
  */
esp_loader_error_t esp_loader_get_security_info(esp_loader_target_security_info_t *security_info);
#endif /* SERIAL_FLASHER_INTERFACE_UART || SERIAL_FLASHER_INTERFACE_USB */


/**
  * @brief Initiates mem operation, initiates loading for program into target RAM
  *
  * @param offset[in]       Address from which mem operation will be performed.
  * @param size[in]         Size of the whole binary to be loaded into mem.
  * @param block_size[in]   Size of buffer used in subsequent calls to esp_loader_mem_write.
  *
  * @note  image_size is size of the whole image, whereas, block_size is chunk of data sent
  *        to the target, each time esp_mem_flash_write function is called.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target is running in secure download mode
  */
esp_loader_error_t esp_loader_mem_start(uint32_t offset, uint32_t size, uint32_t block_size);


/**
  * @brief Writes supplied data to target's mem memory.
  *
  * @param payload[in]      Data to be loaded into target's memory.
  * @param size[in]         Size of data in bytes.
  *
  * @note  size must not be greater that block_size supplied to previously called
  *        esp_loader_mem_start function.
  *        Therefore, size of data buffer has to be equal or greater than block_size.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target is running in secure download mode
  */
esp_loader_error_t esp_loader_mem_write(const void *payload, uint32_t size);


/**
  * @brief Ends mem operation, finish loading for program into target RAM
  *        and send the entrypoint of ram_loadable app
  *
  * @param entrypoint[in]       entrypoint of ram program.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target is running in secure download mode
  */
esp_loader_error_t esp_loader_mem_finish(uint32_t entrypoint);

/**
  * @brief Reads te MAC of the connected chip.
  *
  * @param mac[out] 6 byte MAC address of the chip
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target is running in secure download mode
  */
esp_loader_error_t esp_loader_read_mac(uint8_t *mac);

/**
  * @brief Writes register.
  *
  * @param address[in]      Address of register.
  * @param reg_value[in]    New register value.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target is running in secure download mode
  */
esp_loader_error_t esp_loader_write_register(uint32_t address, uint32_t reg_value);

/**
  * @brief Reads register.
  *
  * @param address[in]      Address of register.
  * @param reg_value[out]   Register value.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC The target is running in secure download mode
  */
esp_loader_error_t esp_loader_read_register(uint32_t address, uint32_t *reg_value);

#ifndef SERIAL_FLASHER_INTERFACE_SDIO
/**
  * @brief Change baud rate.
  *
  * @note  Baud rate has to be also adjusted accordingly on host MCU, as
  *        target's baud rate is changed upon return from this function.
  *
  * @param transmission_rate[in]     new baud rate to be set.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Either the target is running in secure download
  *       mode or the stub is running on the target.
  */
esp_loader_error_t esp_loader_change_transmission_rate(uint32_t transmission_rate);
#endif /* SERIAL_FLASHER_INTERFACE_SDIO */

#if MD5_ENABLED
/**
  * @brief Verify target's flash integrity by checking with a known MD5 checksum
  * for a specified offset and length.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_INVALID_MD5 MD5 does not match
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Unsupported on the target
  *     - ESP_LOADER_ERROR_IMAGE_SIZE Flash region specified is beyond the flash end
  */
esp_loader_error_t esp_loader_flash_verify_known_md5(uint32_t address,
        uint32_t size,
        const uint8_t *expected_md5);

/**
  * @brief Verify target's flash integrity by checking MD5.
  *        MD5 checksum is computed from data pushed to target's memory by calling
  *        esp_loader_flash_write() function and compared against target's MD5.
  *        Target computes checksum based on offset and image_size passed to
  *        esp_loader_flash_start() function.
  *
  * @note  This function is only available if MD5_ENABLED is set.
  *
  * @return
  *     - ESP_LOADER_SUCCESS Success
  *     - ESP_LOADER_ERROR_INVALID_MD5 MD5 does not match
  *     - ESP_LOADER_ERROR_TIMEOUT Timeout
  *     - ESP_LOADER_ERROR_INVALID_RESPONSE Internal error
  *     - ESP_LOADER_ERROR_UNSUPPORTED_FUNC Unsupported on the target
  *     - ESP_LOADER_ERROR_IMAGE_SIZE Flash region specified is beyond the flash end
  */
esp_loader_error_t esp_loader_flash_verify(void);
#endif /* MD5_ENABLED */

/**
  * @brief Toggles reset pin.
  */
void esp_loader_reset_target(void);



#ifdef __cplusplus
}
#endif
