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

#include <unistd.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp32_usb_cdc_acm_port.h"

#define ESP_SERIAL_JTAG_PID 0x1001

static const char *TAG = "usb_cdc_acm_port";

static cdc_acm_dev_hdl_t s_acm_device;
static StreamBufferHandle_t s_rx_stream_buffer;
static bool s_is_usb_serial_jtag;
static loader_port_esp32_usb_cdc_acm_callback_t s_acm_host_error_callback;
static loader_port_esp32_usb_cdc_acm_callback_t s_device_disconnected_callback;
static loader_port_esp32_usb_cdc_acm_callback_t s_acm_host_serial_state_callback;

#if SERIAL_FLASHER_DEBUG_TRACE
static void transfer_debug_print(const uint8_t *data, const uint16_t size, const bool write)
{
    static bool write_prev = false;

    if (write_prev != write) {
        write_prev = write;
        printf("\n--- %s ---\n", write ? "WRITE" : "READ");
    }

    for (uint32_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
}
#endif

static bool handle_usb_data(const uint8_t *data, size_t data_len, void *arg)
{
    return xStreamBufferSend(s_rx_stream_buffer, data, data_len, 0) == data_len;
}

static void handle_usb_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
        if (s_acm_host_error_callback != NULL) {
            s_acm_host_error_callback();
        }
        break;

    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device disconnected");
        if (s_device_disconnected_callback != NULL) {
            s_device_disconnected_callback();
        }
        esp_loader_error_t deinit_status = loader_port_esp32_usb_cdc_acm_deinit();
        assert(deinit_status == ESP_LOADER_SUCCESS);
        break;

    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
        if (s_acm_host_serial_state_callback != NULL) {
            s_acm_host_serial_state_callback();
        }
        break;

    default:
        ESP_LOGW(TAG, "Unsupported CDC event: %i", event->type);
        break;
    }
}

static void usb_serial_jtag_reset_target(void)
{
    xStreamBufferReset(s_rx_stream_buffer);
    cdc_acm_host_set_control_line_state(s_acm_device, false, true);
    loader_port_delay_ms(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    cdc_acm_host_set_control_line_state(s_acm_device, false, false);
}

static void usb_serial_jtag_enter_booloader(void)
{
    cdc_acm_host_set_control_line_state(s_acm_device, true, false);

    loader_port_delay_ms(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);

    cdc_acm_host_set_control_line_state(s_acm_device, true, true);
    usb_serial_jtag_reset_target();
}

static void usb_serial_converter_reset_target(void)
{
    xStreamBufferReset(s_rx_stream_buffer);
    cdc_acm_host_set_control_line_state(s_acm_device, true, true);
    loader_port_delay_ms(SERIAL_FLASHER_RESET_HOLD_TIME_MS);
    cdc_acm_host_set_control_line_state(s_acm_device, true, false);
}

static void usb_serial_converter_enter_bootloader(void)
{
    cdc_acm_host_set_control_line_state(s_acm_device, true, false);

    usb_serial_converter_reset_target();

    loader_port_delay_ms(SERIAL_FLASHER_BOOT_HOLD_TIME_MS);
    cdc_acm_host_set_control_line_state(s_acm_device, false, false);
}

static uint32_t s_time_end;

esp_loader_error_t loader_port_write(const uint8_t *data, const uint16_t size,
                                     const uint32_t timeout)
{
    assert(data != NULL);
    assert(s_acm_device != NULL && s_rx_stream_buffer != NULL);

    esp_err_t err = cdc_acm_host_data_tx_blocking(s_acm_device,
                    (uint8_t *)data,
                    size,
                    timeout);

    if (err == ESP_OK) {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, size, true);
#endif
        return ESP_LOADER_SUCCESS;
    } else if (err == ESP_ERR_TIMEOUT) {
        return ESP_LOADER_ERROR_TIMEOUT;
    } else {
        return ESP_LOADER_ERROR_FAIL;
    }
}


esp_loader_error_t loader_port_read(uint8_t *data, const uint16_t size, const uint32_t timeout)
{
    assert(data != NULL);
    assert(s_acm_device != NULL && s_rx_stream_buffer != NULL);

    size_t received = xStreamBufferReceive(s_rx_stream_buffer, data, size, pdMS_TO_TICKS(timeout));

    if (received == size) {
#if SERIAL_FLASHER_DEBUG_TRACE
        transfer_debug_print(data, size, false);
#endif
        return ESP_LOADER_SUCCESS;
    } else {
        return ESP_LOADER_ERROR_TIMEOUT;
    }
}


esp_loader_error_t loader_port_esp32_usb_cdc_acm_init(const loader_esp32_usb_cdc_acm_config_t *config)
{
    s_acm_host_error_callback = config->acm_host_error_callback;
    s_device_disconnected_callback = config->device_disconnected_callback;
    s_acm_host_serial_state_callback = config->acm_host_serial_state_callback;

    /* Different reset and enter bootloader sequences are needed depending on whether the target
     * device is connected via internal USB Serial/JTAG or an USB to serial converter which
     * connects to target UART and BOOT/RST pins. See pages 1207 and 1208 of the ESP32-S3 TRM */
    s_is_usb_serial_jtag = config->device_pid == ESP_SERIAL_JTAG_PID;

    s_rx_stream_buffer = xStreamBufferCreate(1024, 1);

    if (s_rx_stream_buffer == NULL) {
        ESP_LOGE(TAG, "Could not create the stream buffer for USB data reception");
        return ESP_LOADER_ERROR_FAIL;
    }

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = config->connection_timeout_ms,
        .out_buffer_size = config->out_buffer_size,
        .in_buffer_size = 512,
        .event_cb = handle_usb_event,
        .data_cb = handle_usb_data
    };

    esp_err_t err = cdc_acm_host_open(config->device_vid,
                                      config->device_pid,
                                      0,
                                      &dev_config,
                                      &s_acm_device);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open the USB device");
        esp_loader_error_t deinit_status = loader_port_esp32_usb_cdc_acm_deinit();
        assert(deinit_status == ESP_LOADER_SUCCESS);
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}


esp_loader_error_t loader_port_esp32_usb_cdc_acm_deinit(void)
{
    s_acm_host_error_callback = NULL;
    s_device_disconnected_callback = NULL;
    s_acm_host_serial_state_callback = NULL;
    s_is_usb_serial_jtag = false;

    if (s_rx_stream_buffer != NULL) {
        vStreamBufferDelete(s_rx_stream_buffer);
        s_rx_stream_buffer = NULL;
    }

    if (s_acm_device != NULL) {
        if (cdc_acm_host_close(s_acm_device) != ESP_OK) {
            ESP_LOGE(TAG, "Could not close device");
            return ESP_LOADER_ERROR_FAIL;
        }
        s_acm_device = NULL;
    }

    return ESP_LOADER_SUCCESS;
}


void loader_port_enter_bootloader(void)
{
    assert(s_acm_device != NULL && s_rx_stream_buffer != NULL);

    if (s_is_usb_serial_jtag) {
        usb_serial_jtag_enter_booloader();
    } else {
        usb_serial_converter_enter_bootloader();
    }
}


void loader_port_reset_target(void)
{
    assert(s_acm_device != NULL && s_rx_stream_buffer != NULL);

    if (s_is_usb_serial_jtag) {
        usb_serial_jtag_reset_target();
    } else {
        usb_serial_converter_reset_target();
    }
}


void loader_port_delay_ms(const uint32_t ms)
{
    usleep(ms * 1000);
}


void loader_port_start_timer(const uint32_t ms)
{
    s_time_end = esp_timer_get_time() + ms * 1000;
}


uint32_t loader_port_remaining_time(void)
{
    int64_t remaining = (s_time_end - esp_timer_get_time()) / 1000;
    return (remaining > 0) ? (uint32_t)remaining : 0;
}


void loader_port_debug_print(const char *str)
{
    printf("DEBUG: %s\n", str);
}

esp_loader_error_t loader_port_change_transmission_rate(const uint32_t baudrate)
{
    assert(s_acm_device != NULL && s_rx_stream_buffer != NULL);

    cdc_acm_line_coding_t line_coding;
    if (cdc_acm_host_line_coding_get(s_acm_device, &line_coding) != ESP_OK) {
        return ESP_LOADER_ERROR_FAIL;
    }

    line_coding.dwDTERate = baudrate;

    if (cdc_acm_host_line_coding_set(s_acm_device, &line_coding) != ESP_OK) {
        return ESP_LOADER_ERROR_FAIL;
    }

    return ESP_LOADER_SUCCESS;
}
