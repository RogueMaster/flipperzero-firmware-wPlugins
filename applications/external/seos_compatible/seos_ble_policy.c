#include "seos_ble_policy.h"

SeosBleChoice seos_ble_choose_stack(bool external_enabled, SeosBleRole role) {
    if(role == SeosBleRoleCentral) {
        /* The Flipper's own radio is a peripheral only, so a central role
         * needs the dongle or nothing. */
        return external_enabled ? SeosBleChoiceExternal : SeosBleChoiceNone;
    }

    /* Either stack can advertise. The dongle wins when the user has turned it
     * on, since that is the one they went to the trouble of attaching. */
    return external_enabled ? SeosBleChoiceExternal : SeosBleChoiceNative;
}
