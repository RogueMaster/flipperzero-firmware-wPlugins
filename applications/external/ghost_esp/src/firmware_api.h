#pragma once
#include <furi.h>
#include <furi_hal_version.h>

// Only try to include cfw if we're building for it
#include <cfw/cfw.h>

// Check for version.h and custom name function
#if defined(__has_include)
#if __has_include(<lib/toolbox/version.h>)
#include <lib/toolbox/version.h>
// Only define if the function prototype exists
#if defined(version_get_custom_name)
#define HAS_VERSION_CUSTOM_NAME 1
#endif
#endif
#endif
