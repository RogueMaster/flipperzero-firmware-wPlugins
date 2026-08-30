#pragma once

#include "seos.h"
#include "seos_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Interface between the app and a BLE stack loaded at runtime.
 *
 * The BLE stacks are most of this app's image and most sessions never use
 * one, so they are built as plugins and loaded only when a scene needs them.
 * Everything they need from the app is resolved through the app's own symbol
 * table, so the sources move across unchanged.
 */

#define SEOS_BLE_PLUGIN_APP_ID      "seos"
#define SEOS_BLE_PLUGIN_API_VERSION 1

/* Chunk framing, shared by both stacks. A message is split into chunks of
 * this many bytes, each behind a flag byte saying where it sits. */
#define BLE_CHUNK_SIZE 19

#define BLE_FLAG_SOM 0x80
#define BLE_FLAG_EOM 0x40
#define BLE_FLAG_ERR 0x20

/* Which stack a plugin drives. */
typedef enum {
    SeosBleStackExternal, /* an nRF52840 dongle over the serial port */
    SeosBleStackNative, /* the Flipper's own radio */
} SeosBleStack;

/* What a plugin offers.
 *
 * A plugin is stateful, so a context is allocated and handed back on every
 * later call. Teardown order matters: stop, then free, and only then may the
 * plugin be unloaded -- a callback re-entering unmapped code is a hard fault.
 */
typedef struct {
    const char* name;
    SeosBleStack stack;
    bool central; /* whether it can also drive the central role */

    void* (*alloc)(Seos* seos);
    void (*start)(void* context, FlowMode mode);
    void (*stop)(void* context);
    void (*free)(void* context);
} SeosBlePlugin;

#ifdef __cplusplus
}
#endif
