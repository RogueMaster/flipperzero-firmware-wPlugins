/**
 * CAN Commander v2.3 (ESP32-S3 official board, or your own ESP32 / ESP32-S3 hardware)
 *
 * Quick Start (USB CLI):
 * - `help` / `status` / `ver`
 * - `bus set bus=can0 bitrate=500000 mode=normal`
 * - `tool start read_all bus=both ascii=0`
 * - `tool stop`
 * - `dbc add bus=can0 id=1D5 ext=0 start=0 len=8 order=intel sign=u factor=1 offset=0 min=0 max=255 unit=raw sid=1`
 * - `tool start dbc_decode bus=can0`
 * - `tool start replay bus=can0 id=2C1`
 * - `tool config cmd=record` / `tool config cmd=stop` / `tool config cmd=replay`
 * - `led set brightness=5`
 * - Full command reference + Flipper protocol: see README.md
 *
 * Flipper UART Protocol:
 * - 921600 baud, 8N1
 * - COBS-framed packets terminated by 0x00
 * - Packet body: ver, kind, id, seq, len(u16), payload[len], crc16(ccitt-false)
 * - Query firmware version: GET_INFO (0x02) -> STATUS_EVT with [major, minor, len, string]
 */
#include "src/App.h"

App app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
