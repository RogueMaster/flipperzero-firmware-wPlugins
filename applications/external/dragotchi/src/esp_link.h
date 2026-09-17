#ifndef __esp_link_h__
#define __esp_link_h__
#include <stdint.h>
#include <stdbool.h>

/* WiFi devboard link over the Flipper's GPIO expansion UART.
 *
 * Probes for an attached ESP32 devboard running the Dragotchi reporter
 * firmware, which emits lines of the form:
 *     DRAGO wifi=<count> rssi=<-dbm>
 * on the UART at ESP_LINK_BAUD. Receive-only from the Flipper's side.
 *
 * Returns true and fills the outputs when a valid report is received within
 * ESP_PROBE_MS; false when the board is absent or silent (game falls back to
 * pure sub-GHz hunting). Safe to call when no board is attached. */
bool esp_probe(uint8_t *wifi_count_out, int8_t *rssi_out);

#endif
