#include "uart_link.h"

#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>
#include <string.h>
#include <stdlib.h>

#define UART_BAUD 115200
#define UART_RX_STREAM_SIZE 512
#define UART_LINE_MAX 96
#define UART_WORKER_STACK 1536

#define WORKER_FLAG_STOP (1u << 0)
#define WORKER_FLAG_DATA (1u << 1)

struct UartLink {
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial;
    Expansion* expansion;
    volatile bool running;

    UartLinkRxCallback rx_cb;
    UartLinkStatusCallback status_cb;
    void* cb_context;
};

UartLink* uart_link_alloc(void) {
    UartLink* link = malloc(sizeof(UartLink));
    memset(link, 0, sizeof(UartLink));
    return link;
}

void uart_link_free(UartLink* link) {
    furi_assert(link);
    uart_link_stop(link);
    free(link);
}

void uart_link_set_callbacks(
    UartLink* link,
    UartLinkRxCallback rx_cb,
    UartLinkStatusCallback status_cb,
    void* context) {
    furi_assert(link);
    link->rx_cb = rx_cb;
    link->status_cb = status_cb;
    link->cb_context = context;
}

static uint8_t hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0xFF;
}

static bool parse_mac(const char* s, uint8_t mac[6]) {
    for(int i = 0; i < 6; i++) {
        uint8_t hi = hex_nibble(s[i * 2]);
        uint8_t lo = hex_nibble(s[i * 2 + 1]);
        if(hi == 0xFF || lo == 0xFF) return false;
        mac[i] = (hi << 4) | lo;
    }
    return true;
}

static void uart_link_parse_line(UartLink* link, char* line) {
    if(strncmp(line, "GT1,", 4) == 0) {
        // GT1,<mac>,<rssi>,<type>,<name>  (manual tokenize - newlib has no strsep)
        char* p = line + 4;
        char* c1 = strchr(p, ',');
        if(!c1) return;
        *c1 = '\0';
        char* mac_s = p;
        p = c1 + 1;

        char* c2 = strchr(p, ',');
        if(!c2) return;
        *c2 = '\0';
        char* rssi_s = p;
        p = c2 + 1;

        char* type_s;
        char* name_s;
        char* c3 = strchr(p, ','); // name is optional
        if(c3) {
            *c3 = '\0';
            type_s = p;
            name_s = c3 + 1;
        } else {
            type_s = p;
            name_s = "";
        }
        if(strlen(mac_s) < 12) return;

        uint8_t mac[6];
        if(!parse_mac(mac_s, mac)) return;
        int rssi = atoi(rssi_s);
        TrackerType type = tracker_type_from_code((uint8_t)atoi(type_s));
        const char* name = (name_s && name_s[0]) ? name_s : "";

        if(link->rx_cb) {
            link->rx_cb(link->cb_context, mac, type, (int8_t)rssi, name);
        }
    } else if(strncmp(line, "GTHELLO,", 8) == 0) {
        if(link->status_cb) link->status_cb(link->cb_context, true, line + 8);
    }
}

static int32_t uart_link_worker(void* context) {
    UartLink* link = context;
    char line[UART_LINE_MAX];
    size_t pos = 0;
    uint8_t buf[32];

    while(link->running) {
        size_t n = furi_stream_buffer_receive(link->rx_stream, buf, sizeof(buf), 50);
        for(size_t i = 0; i < n; i++) {
            char c = (char)buf[i];
            if(c == '\n' || c == '\r') {
                if(pos > 0) {
                    line[pos] = '\0';
                    uart_link_parse_line(link, line);
                    pos = 0;
                }
            } else if(pos < sizeof(line) - 1) {
                line[pos++] = c;
            } else {
                pos = 0; // overflow - drop the malformed line
            }
        }
    }
    return 0;
}

static void uart_link_rx_isr(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context) {
    UartLink* link = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(link->rx_stream, &data, 1, 0);
    }
}

void uart_link_start(UartLink* link) {
    furi_assert(link);
    if(link->running) return;

    // The Expansion service squats on the USART by default - hand it over to us.
    link->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(link->expansion);

    link->rx_stream = furi_stream_buffer_alloc(UART_RX_STREAM_SIZE, 1);
    link->running = true;

    link->thread = furi_thread_alloc_ex("GhostTagUart", UART_WORKER_STACK, uart_link_worker, link);
    furi_thread_start(link->thread);

    link->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    furi_check(link->serial);
    furi_hal_serial_init(link->serial, UART_BAUD);
    furi_hal_serial_async_rx_start(link->serial, uart_link_rx_isr, link, false);
}

void uart_link_stop(UartLink* link) {
    furi_assert(link);
    if(!link->running) return;

    link->running = false;

    if(link->serial) {
        furi_hal_serial_async_rx_stop(link->serial);
        furi_hal_serial_deinit(link->serial);
        furi_hal_serial_control_release(link->serial);
        link->serial = NULL;
    }

    if(link->thread) {
        furi_thread_join(link->thread);
        furi_thread_free(link->thread);
        link->thread = NULL;
    }

    if(link->rx_stream) {
        furi_stream_buffer_free(link->rx_stream);
        link->rx_stream = NULL;
    }

    if(link->expansion) {
        expansion_enable(link->expansion);
        furi_record_close(RECORD_EXPANSION);
        link->expansion = NULL;
    }
}

bool uart_link_is_running(UartLink* link) {
    furi_assert(link);
    return link->running;
}

void uart_link_send_command(UartLink* link, const char* cmd) {
    furi_assert(link);
    if(!link->running || !link->serial) return;
    furi_hal_serial_tx(link->serial, (const uint8_t*)cmd, strlen(cmd));
}
