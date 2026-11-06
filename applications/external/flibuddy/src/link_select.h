/* The hello handshake over several transports at once (Connection = Auto),
 * as a pure state machine on top of host_link.h.
 *
 * Each transport is a slot with its own HostLink. While no host is up,
 * every port that opens is greeted with our hello as on a single
 * transport; the first slot whose host says hello becomes the active one
 * and carries the session. The other ports are left alone: their links go
 * back to closed, a hello there is ignored and not answered, and a port
 * that opens meanwhile is not greeted, so a second host sees a Flipper
 * that does not answer. When the active port closes the host is lost and
 * every other open port is greeted again; a host waiting there answers
 * and connects.
 *
 * Two hosts that open their ports at the same moment both get our first
 * hello; the one that answers first wins, the other believes it is
 * connected but never hears from the Flipper until it is greeted again.
 *
 * With one slot in use this is host_link. Times are as in host_link.h.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "host_link.h"

#define LINK_SLOTS 2
#define LINK_NONE  0xFF /* no active slot */

typedef struct {
    uint8_t send_hello; /* bit n: send our hello on slot n */
    bool host_connected; /* over the active slot: feed AppEventHostConnected */
    bool host_lost; /* feed AppEventHostLost */
    bool accepted; /* host_hello: the hello is the active host's (its app name counts) */
} LinkOutput;

typedef struct {
    HostLink link[LINK_SLOTS];
    bool open[LINK_SLOTS];
    uint8_t active; /* slot, or LINK_NONE */
} LinkSelect;

void link_select_init(LinkSelect* ls);

LinkOutput link_select_port_opened(LinkSelect* ls, uint8_t slot, uint32_t now_ms);
LinkOutput link_select_port_closed(LinkSelect* ls, uint8_t slot, uint32_t now_ms);
LinkOutput link_select_host_hello(LinkSelect* ls, uint8_t slot, uint32_t now_ms);
LinkOutput link_select_tick(LinkSelect* ls, uint32_t now_ms);

/* The slot that carries the session, LINK_NONE without a host. */
uint8_t link_select_active(const LinkSelect* ls);

/* Whether a host on this slot is being listened to: our hello went out
 * there and was not superseded, or its host is the active one. */
bool link_select_listening(const LinkSelect* ls, uint8_t slot);
