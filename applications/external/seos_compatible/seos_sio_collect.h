#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/toolbox/bit_buffer.h>

#include "seos_sm_command.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Collecting an answer that arrives in more than one frame.
 *
 * A card that cannot fit its answer in one frame ends each piece with 61xx and
 * waits to be asked for the rest. The whole answer is one protected message,
 * so the pieces are collected before anything is unwrapped.
 *
 * A step rather than a loop: the NFC reader drives it from a poller loop, the
 * BLE readers from notifications, and only the caller knows how to send.
 */

typedef enum {
    SeosSioCollectSend, /* send what is in `next` and step again with the answer */
    SeosSioCollectComplete, /* the assembled buffer holds the whole message */
    SeosSioCollectFailed,
} SeosSioCollectResult;

typedef struct {
    BitBuffer* assembled;
    /* The command as it went out. A card asking for a different length is sent
     * these bytes again rather than a freshly wrapped command, because
     * wrapping steps the sequence counter and the card would refuse it. */
    uint8_t sent[SEOS_SM_RESPONSE_MAX];
    size_t sent_len;
    bool already_resent;
    unsigned frames;
} SeosSioCollector;

/* Starts a collection. `assembled` is reset and holds the result. */
void seos_sio_collect_begin(
    SeosSioCollector* collector,
    BitBuffer* assembled,
    const uint8_t* sent,
    size_t sent_len);

/* Takes one received frame, status word included. */
SeosSioCollectResult seos_sio_collect_step(
    SeosSioCollector* collector,
    const uint8_t* rx,
    size_t rx_len,
    BitBuffer* next);

#ifdef __cplusplus
}
#endif
