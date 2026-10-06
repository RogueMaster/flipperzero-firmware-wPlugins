#pragma once
// Thread-safe link state shared between the transport threads and the UI thread.
#include <furi.h>
#include "protocol.h"

#define PHR_LINK_LOST_MS 5000
#define PHR_HELLO_FAST_MS 1000
#define PHR_HELLO_SLOW_MS 10000

typedef enum {
    PhrLinkWaiting, // never received telemetry in this session
    PhrLinkActive,
    PhrLinkLost, // had telemetry, then 5 s of silence
} PhrLinkState;

typedef struct {
    PhrLinkState state;
    PhrTelemetry telemetry; // last valid frame (valid if have_any)
    bool have_any;
    uint32_t frames; // total valid frames, changes on every new frame
    uint32_t silence_ms; // since last valid frame (0 if none yet)
    bool connected; // transport level: BLE connected / USB port open
} PhrLinkSnapshot;

typedef struct PhrLink PhrLink;

PhrLink* phr_link_alloc(void);
void phr_link_free(PhrLink* link);
void phr_link_reset(PhrLink* link);

/** Called from transport threads with raw received bytes. */
void phr_link_feed(PhrLink* link, const uint8_t* data, size_t len);
/** Transport-level connection state (forces a prompt HELLO when it becomes true). */
void phr_link_set_connected(PhrLink* link, bool connected);
void phr_link_snapshot(PhrLink* link, PhrLinkSnapshot* out);

/**
 * Returns true if a HELLO should be sent now (and marks it as sent).
 * Rule: every 1 s until telemetry, then every 10 s; every 1 s again while the link is lost.
 */
bool phr_link_hello_due(PhrLink* link);
/** Builds the next HELLO frame (rolling seq). Returns length. */
size_t phr_link_build_hello(PhrLink* link, uint8_t transport, uint8_t* buf);
