#pragma once

#include "seos_ble_plugin.h"

/* Which BLE stack serves a role, given what the user has turned on.
 *
 * Kept apart from the loading itself so the rule can be read and tested on
 * its own: only the dongle can act as a central, either stack can act as a
 * peripheral, and the dongle is only ever used when the user has asked for
 * it.
 */

typedef enum {
    SeosBleRolePeripheral, /* advertising as a credential or a reader */
    SeosBleRoleCentral, /* scanning for, and connecting to, other devices */
} SeosBleRole;

typedef enum {
    SeosBleChoiceNone, /* nothing available can serve this role */
    SeosBleChoiceExternal,
    SeosBleChoiceNative,
} SeosBleChoice;

SeosBleChoice seos_ble_choose_stack(bool external_enabled, SeosBleRole role);
