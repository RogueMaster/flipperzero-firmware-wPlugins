/*
 * Framed UART link - implementation.
 *
 * UART config: UART1 by default on the Flipper WiFi Dev Board. The numbers
 * are picked to match the FAP-side defaults; if you change them on one side
 * you must change them on the other.
 *
 *   pin TX = GPIO17, pin RX = GPIO18, baud 230400, no flow control.
 *
 * RX runs in a dedicated task that re-syncs on the 0xAA 0x55 SOF whenever
 * a CRC mismatch or oversize frame is seen.
 */
#include "wifi_link.h"
#include "tt_wifi_proto.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"

#include <string.h>
#include <stdio.h>

/* The Flipper Wi-Fi Devboard wires the ESP32-S2's UART0 (default
 * IO_MUX pins U0TXD=GPIO43, U0RXD=GPIO44) to the Flipper. We claim
 * UART0 for our framed binary protocol; the IDF console is silenced
 * via CONFIG_ESP_CONSOLE_NONE=y so the two never collide. */
#define LINK_UART_NUM UART_NUM_0
#define LINK_PIN_TX   UART_PIN_NO_CHANGE
#define LINK_PIN_RX   UART_PIN_NO_CHANGE
#define LINK_BAUD     230400
#define LINK_RXBUF    4096
#define LINK_TXBUF    1024

static const char* TAG = "link";

static WifiLinkRxFn s_rx_cb;
static void* s_rx_user;
static SemaphoreHandle_t s_tx_lock;

bool wifi_link_send(uint8_t type, const uint8_t* payload, uint16_t len) {
    if(len > TT_FRAME_MAX_PAYLOAD) {
        ESP_LOGE(TAG, "tx: payload %u over limit", (unsigned)len);
        return false;
    }
    /* Frame buffer: 2(SOF) + 1(type) + 2(len) + payload + 2(crc). */
    uint8_t hdr[5];
    hdr[0] = TT_FRAME_SOF0;
    hdr[1] = TT_FRAME_SOF1;
    hdr[2] = type;
    hdr[3] = (uint8_t)(len & 0xFFU);
    hdr[4] = (uint8_t)(len >> 8);

    /* CRC over [type, len_lo, len_hi, payload...]. */
    uint16_t crc = 0xFFFFU;
    {
        for(int i = 2; i < 5; i++) {
            crc ^= (uint16_t)hdr[i] << 8;
            for(int b = 0; b < 8; b++)
                crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
        for(uint16_t i = 0; i < len; i++) {
            crc ^= (uint16_t)payload[i] << 8;
            for(int b = 0; b < 8; b++)
                crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    uint8_t tail[2] = {(uint8_t)(crc >> 8), (uint8_t)(crc & 0xFFU)};

    xSemaphoreTake(s_tx_lock, portMAX_DELAY);
    int ok = uart_write_bytes(LINK_UART_NUM, (const char*)hdr, sizeof(hdr));
    if(ok > 0 && len) ok = uart_write_bytes(LINK_UART_NUM, (const char*)payload, len);
    if(ok > 0) ok = uart_write_bytes(LINK_UART_NUM, (const char*)tail, sizeof(tail));
    xSemaphoreGive(s_tx_lock);
    return ok > 0;
}

bool wifi_link_send_progress(uint8_t percent, const char* msg) {
    uint8_t buf[1 + 1 + 64];
    if(!msg) msg = "";
    size_t mlen = strnlen(msg, sizeof(buf) - 2);
    buf[0] = percent;
    buf[1] = (uint8_t)mlen;
    memcpy(&buf[2], msg, mlen);
    return wifi_link_send(TT_FRAME_PROGRESS, buf, (uint16_t)(2 + mlen));
}

bool wifi_link_send_error(const char* msg) {
    uint8_t buf[1 + 96];
    if(!msg) msg = "";
    size_t mlen = strnlen(msg, sizeof(buf) - 1);
    buf[0] = (uint8_t)mlen;
    memcpy(&buf[1], msg, mlen);
    return wifi_link_send(TT_FRAME_ERROR, buf, (uint16_t)(1 + mlen));
}

/* ---- RX task ----------------------------------------------------------- */

static inline uint16_t crc16_step(uint16_t crc, uint8_t b) {
    crc ^= (uint16_t)b << 8;
    for(int i = 0; i < 8; i++)
        crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    return crc;
}

static void rx_task(void* arg) {
    (void)arg;
    enum {
        S_SOF0,
        S_SOF1,
        S_TYPE,
        S_LEN_LO,
        S_LEN_HI,
        S_PAYLOAD,
        S_CRC_HI,
        S_CRC_LO
    } st = S_SOF0;
    uint8_t type = 0;
    uint16_t len = 0, idx = 0;
    uint16_t crc_calc = 0xFFFFU;
    uint16_t crc_recv = 0;
    static uint8_t payload[TT_FRAME_MAX_PAYLOAD];
#define crc_step(B) (crc_calc = crc16_step(crc_calc, (B)))

    while(1) {
        uint8_t b;
        int n = uart_read_bytes(LINK_UART_NUM, &b, 1, pdMS_TO_TICKS(1000));
        if(n != 1) continue;

        switch(st) {
        case S_SOF0:
            if(b == TT_FRAME_SOF0) st = S_SOF1;
            break;
        case S_SOF1:
            st = (b == TT_FRAME_SOF1) ? S_TYPE : S_SOF0;
            break;
        case S_TYPE:
            type = b;
            crc_calc = 0xFFFFU;
            crc_step(b);
            st = S_LEN_LO;
            break;
        case S_LEN_LO:
            len = b;
            crc_step(b);
            st = S_LEN_HI;
            break;
        case S_LEN_HI:
            len |= (uint16_t)b << 8;
            crc_step(b);
            if(len > TT_FRAME_MAX_PAYLOAD) {
                st = S_SOF0;
                break;
            }
            idx = 0;
            st = (len == 0) ? S_CRC_HI : S_PAYLOAD;
            break;
        case S_PAYLOAD:
            payload[idx++] = b;
            crc_step(b);
            if(idx >= len) st = S_CRC_HI;
            break;
        case S_CRC_HI:
            crc_recv = (uint16_t)b << 8;
            st = S_CRC_LO;
            break;
        case S_CRC_LO:
            crc_recv |= b;
            if(crc_recv == crc_calc) {
                if(s_rx_cb) s_rx_cb(type, payload, len, s_rx_user);
            } else {
                ESP_LOGW(TAG, "CRC mismatch type=0x%02X len=%u", type, len);
            }
            st = S_SOF0;
            break;
        }
    }
}

void wifi_link_init(WifiLinkRxFn cb, void* user) {
    s_rx_cb = cb;
    s_rx_user = user;
    s_tx_lock = xSemaphoreCreateMutex();

    uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(LINK_UART_NUM, LINK_RXBUF, LINK_TXBUF, 0, NULL, 0);
    uart_param_config(LINK_UART_NUM, &cfg);
    uart_set_pin(LINK_UART_NUM, LINK_PIN_TX, LINK_PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* RX task also runs the frame dispatch + cloud_client_render(). The
     * mbedTLS handshake allocates ~6-8 KB on the stack during ECDHE +
     * cert chain validation, so 16 KB gives a comfortable margin. The
     * old 4 KB stack silently overflowed and locked the task. */
    xTaskCreate(rx_task, "link_rx", 16384, NULL, 6, NULL);
}
