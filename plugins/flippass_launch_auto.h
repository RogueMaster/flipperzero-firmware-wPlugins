#pragma once
#include "../flippass.h"
#include "flippass_output_plugin.h"

/* Root validates/maps providers. The cold Launch coordinator owns the race
 * and detaches every observer before returning through its pinned caller. */
const FlipPassOutputPluginV1*
    flippass_output_transport_plugin_ensure(App* app, FlipPassOutputTransport transport);
bool flippass_launch_wait_auto(App* app, FlipPassOutputTransport* selected, uint32_t timeout_ms);
