/*
 * Framed UART link to the Flipper.
 *
 * The link is a simple two-way pipe of TT_FRAME_* records (see
 * shared/tt_wifi_proto.h). All TX is funnelled through wifi_link_send_*; all
 * RX is delivered to a callback registered with wifi_link_set_handler().
 */
#ifndef TT_WIFI_LINK_H
#define TT_WIFI_LINK_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef void (*WifiLinkRxFn)(uint8_t type, const uint8_t* payload, uint16_t len, void* user);

void wifi_link_init(WifiLinkRxFn cb, void* user);

/* Send a fully-formed frame. Returns false if the payload is oversized or
 * the UART driver couldn't accept it. */
bool wifi_link_send(uint8_t type, const uint8_t* payload, uint16_t len);

/* Convenience helpers for the most common frames. */
bool wifi_link_send_progress(uint8_t percent, const char* msg);
bool wifi_link_send_error(const char* msg);

#endif /* TT_WIFI_LINK_H */
