/*
 * Where does a decoded event go?
 *
 * Deliberately free of furi and of threads: this is the policy the session
 * worker applies to every frame that arrives, and it is the part most likely
 * to be subtly wrong (a reply mistaken for a push loses the reply; a push
 * mistaken for a reply loses an incoming message). Keeping it pure means the
 * host tests can cover it without emulating a scheduler.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "meshcore_c/meshcore_companion.h"

/* Enough for the widest reply set we need: SYNC_NEXT_MESSAGE can come back as
 * a contact message, a channel message, either of their V3 variants, or
 * "no more messages". */
#define MESHCORE_CODE_SET_MAX 6

/** The set of response codes that would answer the request in flight. */
typedef struct {
    uint8_t codes[MESHCORE_CODE_SET_MAX];
    uint8_t count;
} MeshCoreCodeSet;

MeshCoreCodeSet meshcore_code_set_one(uint8_t code);
/** Builds a set from `count` codes; anything beyond MESHCORE_CODE_SET_MAX is
 *  ignored rather than silently overflowing. */
MeshCoreCodeSet meshcore_code_set(const uint8_t* codes, uint8_t count);
bool meshcore_code_set_has(const MeshCoreCodeSet* set, uint8_t code);

typedef enum {
    /** Completes the request in flight (an awaited code, or a node error). */
    MeshCoreRouteReply,
    /** Offer to the streaming collector; if it declines, treat as an event. */
    MeshCoreRouteStream,
    /** Unsolicited — hand to the application. */
    MeshCoreRouteEvent,
} MeshCoreRoute;

/**
 * @param pending     a request is in flight
 * @param want        the codes that would complete it
 * @param has_stream  the request collects a multi-frame reply
 * @param ev_code     the code just decoded
 */
MeshCoreRoute meshcore_route_event(
    bool pending,
    const MeshCoreCodeSet* want,
    bool has_stream,
    uint8_t ev_code);
