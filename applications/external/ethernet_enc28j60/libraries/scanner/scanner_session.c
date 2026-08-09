#include "scanner_session.h"
#include "../chip/enc28j60.h"
#include "../chip/rx_dispatch.h"
#include "../../modules/arp_module.h"
#include "../protocol_tools/ethernet_protocol.h"
#include "../protocol_tools/arp.h"

void scanner_session_init(scanner_session_t* s, App* app) {
    furi_assert(s);
    furi_assert(app);
    furi_assert(app->ethernet);

    s->ethernet = app->ethernet;
    s->view_dispatcher = app->view_dispatcher;
    s->ip_gateway = app->ip_gateway;
    s->mac_gateway = app->mac_gateway;
    s->subnet_mask = app->ethernet->subnet_mask;
    s->cache_next = 0;
    for(uint8_t i = 0; i < SCANNER_RESOLVE_CACHE_ENTRIES; i++) {
        s->cache[i].valid = false;
    }
}

void scanner_session_deinit(scanner_session_t* s) {
    UNUSED(s);
    // No-op for now. F0.4 will add RX handler unsubscription here.
}

void scanner_send_packet_trigger(void* ctx) {
    scanner_send_trigger_ctx_t* c = (scanner_send_trigger_ctx_t*)ctx;
    send_packet(c->eth, c->buf, c->len);
}

static bool same_subnet(const uint8_t a[4], const uint8_t b[4], const uint8_t mask[4]) {
    // F0.5e — byte-wise compare. Pre-fix this cast uint8_t[4] to
    // uint32_t* and OR'd word-aligned. The arrays it sees come from
    // enc28j60_t.{ip_address,subnet_mask} which sit at non-word-
    // aligned offsets in the struct; ARM Cortex-M4 tolerates unaligned
    // word loads at runtime by default, but the cast violates strict
    // aliasing and would HardFault if Furi flipped UNALIGN_TRP.
    for(uint8_t i = 0; i < 4; i++) {
        if((a[i] & mask[i]) != (b[i] & mask[i])) return false;
    }
    return true;
}

static bool cache_lookup(scanner_session_t* s, const uint8_t ip[4], uint8_t mac_out[6]) {
    for(uint8_t i = 0; i < SCANNER_RESOLVE_CACHE_ENTRIES; i++) {
        if(s->cache[i].valid && memcmp(s->cache[i].ip, ip, 4) == 0) {
            memcpy(mac_out, s->cache[i].mac, 6);
            return true;
        }
    }
    return false;
}

static void cache_insert(scanner_session_t* s, const uint8_t ip[4], const uint8_t mac[6]) {
    uint8_t slot = s->cache_next;
    memcpy(s->cache[slot].ip, ip, 4);
    memcpy(s->cache[slot].mac, mac, 6);
    s->cache[slot].valid = true;
    s->cache_next = (slot + 1) % SCANNER_RESOLVE_CACHE_ENTRIES;
}

bool scanner_resolve_next_hop(scanner_session_t* s, const uint8_t target_ip[4], uint8_t mac_out[6]) {
    furi_assert(s);
    furi_assert(target_ip);
    furi_assert(mac_out);

    const uint8_t* resolve_ip;
    if(same_subnet(target_ip, s->ethernet->ip_address, s->subnet_mask)) {
        resolve_ip = target_ip;
    } else {
        resolve_ip = s->ip_gateway;
    }

    if(cache_lookup(s, resolve_ip, mac_out)) {
        return true;
    }

    // F0.4f — replaces arp_get_specific_mac (which had its own inline
    // receive_packet poll) with the scanner_wait_for_packet primitive.
    // Now the chip is read exclusively by rx_dispatch; we just send
    // the ARP request and let the registered handler signal us when a
    // matching reply arrives. No more rx_dispatch_pause/resume needed.
    //
    // F0.5d — send via the trigger param so the rx handler is registered
    // before the request goes out (closes the send→register race).
    //
    // Retry budget mirrors legacy arp_get_specific_mac (10 attempts ×
    // 2 s/wait = up to 20 s worst case) so timing-sensitive callers
    // (e.g., port scan over the gateway) keep the same behavior.
    arp_set_my_mac_address(s->ethernet->mac_address);
    arp_set_my_ip_address(s->ethernet->ip_address);

    arp_reply_match_ctx_t pred_ctx = {
        .target_ip = (uint8_t*)resolve_ip,
        .target_mac = mac_out,
    };

    uint8_t request_buf[ETHERNET_HEADER_LEN + ARP_LEN] = {0};
    uint16_t request_len = 0;
    if(!set_arp_request(request_buf, &request_len, (uint8_t*)resolve_ip)) return false;

    scanner_send_trigger_ctx_t trigger_ctx = {s->ethernet, request_buf, request_len};

    bool ok = false;
    for(uint8_t attempt = 0; attempt < 10 && !ok; attempt++) {
        if(scanner_cancel_requested(s)) break;

        uint16_t got = 0;
        ok = scanner_wait_for_packet(
            s,
            arp_reply_match_predicate,
            &pred_ctx,
            scanner_send_packet_trigger,
            &trigger_ctx,
            &got,
            2000);
    }

    if(ok) {
        cache_insert(s, resolve_ip, mac_out);
        // Backward-compat: keep app->mac_gateway in sync when the
        // resolved hop IS the gateway.
        if(resolve_ip == s->ip_gateway) {
            memcpy(s->mac_gateway, mac_out, 6);
        }
    }
    return ok;
}

// State for the rx_dispatch handler that scanner_wait_for_packet registers
// for the duration of a single wait. Lives on the stack of the caller of
// scanner_wait_for_packet; the handler runs in the rx_dispatch thread.
typedef struct {
    scanner_packet_predicate_fn user_pred;
    void* user_ctx;
    FuriSemaphore* signal;
    volatile uint16_t matched_len;
    volatile bool matched;
} scanner_wait_state_t;

static bool wait_predicate(const uint8_t* frame, uint16_t len, void* ctx) {
    scanner_wait_state_t* w = (scanner_wait_state_t*)ctx;
    if(w->matched) return false;
    if(w->user_pred(frame, len, w->user_ctx)) {
        w->matched_len = len;
        return true;
    }
    return false;
}

static void wait_signal_handler(const uint8_t* frame, uint16_t len, void* ctx) {
    UNUSED(frame);
    UNUSED(len);
    scanner_wait_state_t* w = (scanner_wait_state_t*)ctx;
    if(!w->matched) {
        w->matched = true;
        furi_semaphore_release(w->signal);
    }
}

bool scanner_wait_for_packet(
    scanner_session_t* s,
    scanner_packet_predicate_fn pred,
    void* pred_ctx,
    scanner_trigger_fn trigger_fn,
    void* trigger_ctx,
    uint16_t* len_out,
    uint32_t timeout_ms) {
    furi_assert(s);
    furi_assert(pred);
    furi_assert(len_out);
    UNUSED(s);

    *len_out = 0;

    scanner_wait_state_t state = {
        .user_pred = pred,
        .user_ctx = pred_ctx,
        .signal = furi_semaphore_alloc(1, 0),
        .matched_len = 0,
        .matched = false,
    };
    if(!state.signal) return false;

    rx_handle_t* handle = rx_register(wait_predicate, wait_signal_handler, &state);
    if(!handle) {
        furi_semaphore_free(state.signal);
        return false;
    }

    // F0.5d — fire the trigger only after the handler is in the registry,
    // so a sub-millisecond reply to whatever the trigger sends still hits
    // an armed predicate.
    if(trigger_fn) trigger_fn(trigger_ctx);

    // Sleep on the semaphore in short slices so we can also poll the back
    // button (cancel). 50 ms per slice keeps cancel latency under 50 ms
    // without busy-polling.
    bool got = false;
    uint32_t start = furi_get_tick();
    while((furi_get_tick() - start) < timeout_ms) {
        if(scanner_cancel_requested(s)) break;
        uint32_t slice = timeout_ms - (furi_get_tick() - start);
        if(slice > 50) slice = 50;
        if(furi_semaphore_acquire(state.signal, slice) == FuriStatusOk) {
            got = true;
            *len_out = state.matched_len;
            break;
        }
    }

    rx_unregister(handle);
    furi_semaphore_free(state.signal);
    return got;
}

bool scanner_cancel_requested(scanner_session_t* s) {
    UNUSED(s);
    return !furi_hal_gpio_read(&gpio_button_back);
}
