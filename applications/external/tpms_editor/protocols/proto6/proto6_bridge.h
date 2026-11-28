#pragma once

#include <lib/subghz/protocols/base.h>
#include <notification/notification_messages.h>
#include "../tpms_generic.h"
#include "../../helpers/tpms_capture.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Proto6Bridge Proto6Bridge;

typedef enum {
    Proto6ModeOOK = 0,
    Proto6ModeFSK,
} Proto6Mode;

Proto6Bridge* proto6_bridge_alloc(NotificationApp* notifications);
void proto6_bridge_free(Proto6Bridge* bridge);
void proto6_bridge_reset(Proto6Bridge* bridge);
void proto6_bridge_feed(Proto6Bridge* bridge, bool level, uint32_t duration);
void proto6_bridge_rx_callback(bool level, uint32_t duration, void* context);
void proto6_bridge_set_mode(Proto6Bridge* bridge, Proto6Mode mode);
SubGhzProtocolDecoderBase* proto6_bridge_scan(Proto6Bridge* bridge);

/** Copy the exact coherent RAW signal retained for the most recent decode.
 * Samples are signed durations: positive=high, negative=low. */
size_t proto6_bridge_copy_last_capture(
    const Proto6Bridge* bridge,
    int16_t* output,
    size_t capacity,
    bool* truncated);

#ifdef __cplusplus
}
#endif
