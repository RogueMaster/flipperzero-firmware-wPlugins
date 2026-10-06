#pragma once

#include "application_manifest.h"
#include <storage/storage.h>

/** Read display metadata without loading an application's code or assets.
 *
 * Accepts compatible hardware with any SDK API version, so an old or new FAP
 * can still be named in a browser. The normal loader remains responsible for
 * deciding whether the application can run. This is an internal helper, not SDK.
 */
bool flipper_application_metadata_load(
    Storage* storage,
    const char* path,
    FlipperApplicationManifest* manifest);
