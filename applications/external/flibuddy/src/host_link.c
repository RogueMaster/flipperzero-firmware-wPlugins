#include "host_link.h"

static const HostLinkOutput nothing = {false, false, false};

static bool hello_expired(const HostLink* link, uint32_t now_ms) {
    return (uint32_t)(now_ms - link->hello_sent_at) >= HOST_LINK_HELLO_TIMEOUT_MS;
}

void host_link_init(HostLink* link) {
    link->state = HostLinkStateClosed;
    link->hello_sent_at = 0;
}

HostLinkOutput host_link_port_opened(HostLink* link, uint32_t now_ms) {
    if(link->state != HostLinkStateClosed) return nothing;
    link->state = HostLinkStateWaiting;
    link->hello_sent_at = now_ms;
    return (HostLinkOutput){.send_hello = true};
}

HostLinkOutput host_link_port_closed(HostLink* link) {
    bool was_up = link->state == HostLinkStateUp;
    link->state = HostLinkStateClosed;
    return (HostLinkOutput){.host_lost = was_up};
}

HostLinkOutput host_link_host_hello(HostLink* link, uint32_t now_ms) {
    if(link->state == HostLinkStateClosed) return nothing;
    bool answers_ours = link->state == HostLinkStateWaiting && !hello_expired(link, now_ms);
    bool was_up = link->state == HostLinkStateUp;
    link->state = HostLinkStateUp;
    return (HostLinkOutput){.send_hello = !answers_ours, .host_connected = !was_up};
}

HostLinkOutput host_link_tick(HostLink* link, uint32_t now_ms) {
    if(link->state == HostLinkStateWaiting && hello_expired(link, now_ms)) {
        link->state = HostLinkStateNoHost;
    }
    return nothing;
}

bool host_link_is_up(const HostLink* link) {
    return link->state == HostLinkStateUp;
}
