#pragma once

/*
 * Plugin interface for LAN Tester category plugins.
 *
 * Each tool category (Port Info, Scan, Diagnostics, ...) is compiled into its
 * own embedded .fal plugin. The host loads a category's plugin only while that
 * category is in use and frees it on the way out, so the tool code is resident
 * in RAM only when needed. Plugins reach the shared W5500/ioLibrary and app
 * helpers through the host's private API resolver (see api/).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LAN_TESTER_PLUGIN_APP_ID      "lan_tester_category"
#define LAN_TESTER_PLUGIN_API_VERSION 1

typedef struct LanTesterApp LanTesterApp;

typedef struct {
    const char* name; /* category name, for logging */
    /* Run one tool. op is the LanTesterMenuItem selected by the user; the
       plugin fills app->tool_text / drives its views just like the old
       in-app lan_tester_do_* functions did. */
    void (*run)(LanTesterApp* app, uint32_t op);
} LanTesterCategoryPlugin;

#ifdef __cplusplus
}
#endif
