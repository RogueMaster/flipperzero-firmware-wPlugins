#pragma once

#include <stdint.h>

#include "seos_sm_command.h"

#ifdef __cplusplus
extern "C" {
#endif

/* View dispatcher events.
 *
 * Declared apart from seos_i.h so the mapping below can be tested without the
 * GUI headers.
 */
enum SeosCustomEvent {
    // Reserve first 100 events for button types and indexes, starting from 0
    SeosCustomEventReserved = 100,

    SeosCustomEventViewExit,
    SeosCustomEventTextInputDone,
    // Read/write card events
    SeosCustomEventPollerError,
    SeosCustomEventPollerSuccess,

    SeosCustomEventHCIInit,
    // Events during emulating or reading
    SeosCustomEventScan,
    SeosCustomEventFound,
    SeosCustomEventEmulate,
    SeosCustomEventADFMatched,
    SeosCustomEventAIDSelected,
    SeosCustomEventConnected,
    SeosCustomEventAuthenticated,
    SeosCustomEventSIORequested,
    SeosCustomEventAdvertising,
    /* A reader stored a credential on us. Distinct from PollerSuccess, which
     * means we read one from a card: the two are opposite directions and the
     * scenes act on them differently. */
    SeosCustomEventSIOWritten,
};

/* Maps a secure messaging event to the view dispatcher event for it.
 *
 * Returns 0 for an event with no view of its own. */
uint32_t seos_sm_event_to_custom_event(SeosSmEvent event);

#ifdef __cplusplus
}
#endif
