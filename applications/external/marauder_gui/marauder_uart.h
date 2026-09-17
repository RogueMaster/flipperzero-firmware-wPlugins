#pragma once

#include <furi.h>

typedef struct MarauderUart MarauderUart;

MarauderUart* marauder_uart_alloc(void);
void marauder_uart_free(MarauderUart* uart);

/* Send raw bytes as-is (no newline appended) */
void marauder_uart_send(MarauderUart* uart, const char* data);

/* Send a CLI command followed by '\n', matching ESP32 Marauder's line-delimited protocol */
void marauder_uart_send_line(MarauderUart* uart, const char* line);

/* Non-blocking read of whatever text bytes have arrived since the last call */
size_t marauder_uart_receive(MarauderUart* uart, uint8_t* buffer, size_t max_len);

/* Non-blocking read of raw PCAP bytes - the payload Marauder streams (when a sniff/wardrive
   command is sent with "-serial") wrapped in [BUF/BEGIN]/[BUF/CLOSE] markers. Those markers are
   demuxed out of the text stream in the RX callback and their contents routed here instead, so
   the caller can write them straight to a .pcap file on the Flipper's SD card. */
size_t marauder_uart_receive_pcap(MarauderUart* uart, uint8_t* buffer, size_t max_len);

/* Reset the PCAP demux back to "text mode" and empty both stream buffers. Call this around a
   capture so a missed [BUF/CLOSE] (dropped bytes at 115200) can't leave the demux stuck inside a
   pcap region, which would otherwise silently swallow the next scene's text output. */
void marauder_uart_reset_capture(MarauderUart* uart);
