#include "flippass_launch_auto.h"

#if FLIPPASS_ENABLE_TYPING_ACTIONS
static FlipPassModuleSlot flippass_output_transport_slot(FlipPassOutputTransport transport) {
    return (transport == FlipPassOutputTransportBluetooth) ? FlipPassModuleSlotOutputBle :
                                                             FlipPassModuleSlotOutputUsb;
}

static void flippass_output_plugin_progress(
    void* host_context,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    App* app = host_context;
    if(app == NULL) {
        return;
    }

    flippass_progress_update(
        app, (stage != NULL) ? stage : "Typing", (detail != NULL) ? detail : "", percent);
}

static void
    flippass_output_plugin_log(void* host_context, const char* module_name, const char* message) {
    App* app = host_context;
    if(app == NULL || module_name == NULL || message == NULL) {
        return;
    }

    FLIPPASS_LOG_EVENT(app, "%s %s", module_name, message);
}

static bool flippass_output_plugin_should_cancel(void* host_context) {
    return flippass_typing_should_cancel(host_context);
}

static FlipPassOutputPluginHostApiV1 flippass_output_transport_host_api(App* app) {
    const FlipPassOutputPluginHostApiV1 host_api = {
        .api_version = FLIPPASS_OUTPUT_PLUGIN_API_VERSION,
        .host_context = app,
        .progress = flippass_output_plugin_progress,
        .log = flippass_output_plugin_log,
        .should_cancel = flippass_output_plugin_should_cancel,
    };
    return host_api;
}

static const FlipPassOutputPluginV1*
    flippass_output_transport_plugin_loaded(const App* app, FlipPassOutputTransport transport) {
    if(app == NULL) {
        return NULL;
    }

    const FlipPassModuleInstance* instance =
        &app->module_loader.slot[flippass_output_transport_slot(transport)];
    return (instance->descriptor != NULL) ? instance->descriptor->entry_point : NULL;
}

static void flippass_output_transport_cleanup_loaded(App* app, FlipPassOutputTransport transport) {
    const FlipPassOutputPluginV1* plugin = flippass_output_transport_plugin_loaded(app, transport);
    if(plugin != NULL && plugin->cleanup != NULL) {
        const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
        plugin->cleanup(&host_api);
    }

    if(transport == FlipPassOutputTransportUsb) {
        app->usb_expect_rpc_session_close = false;
    }

    flippass_module_unload(app, flippass_output_transport_slot(transport));
}

typedef struct {
    bool armed;
    bool connected[2];
    int winner;
    uint32_t winner_tick;
} FlipPassAutoConnectionRace;

typedef struct {
    FlipPassAutoConnectionRace* race;
    unsigned transport;
} FlipPassAutoConnectionContext;

static void flippass_output_auto_connection_changed(bool connected, void* context) {
    FlipPassAutoConnectionContext* source = context;
    FURI_CRITICAL_ENTER();
    FlipPassAutoConnectionRace* race = source->race;
    const bool rising = connected && !race->connected[source->transport];
    race->connected[source->transport] = connected;
    if(race->armed && race->winner < 0 && rising) {
        race->winner = (int)source->transport;
        race->winner_tick = furi_get_tick();
    }
    FURI_CRITICAL_EXIT();
}

static int
    flippass_output_auto_winner(FlipPassAutoConnectionRace* race, bool* connected, uint32_t* tick) {
    FURI_CRITICAL_ENTER();
    const int winner = race->winner;
    *connected = winner >= 0 && race->connected[winner];
    *tick = race->winner_tick;
    FURI_CRITICAL_EXIT();
    return winner;
}

bool flippass_launch_wait_auto(App* app, FlipPassOutputTransport* selected, uint32_t timeout_ms) {
    if(app == NULL || selected == NULL) return false;
    const FlipPassOutputPluginV1* provider[2] = {NULL, NULL};
    bool pinned[2] = {false, false};
    bool watching[2] = {false, false};
    const unsigned count = FLIPPASS_ENABLE_BLUETOOTH_OUTPUT ? 2U : 1U;
    const FlipPassOutputPluginHostApiV1 host_api = flippass_output_transport_host_api(app);
    FlipPassAutoConnectionRace race = {.winner = -1};
    FlipPassAutoConnectionContext context[2] = {{&race, 0U}, {&race, 1U}};
    bool ok = false;
    int winner = -1;
    bool connected = false;
    uint32_t winner_tick = 0U;
    uint32_t started = 0U;
    const uint32_t timeout_ticks = furi_ms_to_ticks(timeout_ms);

    for(unsigned index = 0U; index < count; ++index) {
        provider[index] =
            flippass_output_transport_plugin_ensure(app, (FlipPassOutputTransport)index);
        if(provider[index] == NULL) goto finish;
        flippass_module_pin(app, flippass_output_transport_slot((FlipPassOutputTransport)index));
        pinned[index] = true;
        if(provider[index]->watch_connection == NULL || provider[index]->is_connected == NULL ||
           provider[index]->advertise == NULL)
            goto finish;
    }
    /* Watcher setters serialize detach against notification forwarding. Their
     * contexts are stack-owned and must disappear before the pins are released. */
    for(unsigned index = 0U; index < count; ++index) {
        provider[index]->watch_connection(
            flippass_output_auto_connection_changed, &context[index]);
        watching[index] = true;
    }
    {
        FURI_CRITICAL_ENTER();
        started = furi_get_tick();
        for(unsigned index = 0U; index < count; ++index) {
            race.connected[index] = provider[index]->is_connected(&host_api);
        }
        if(race.connected[0])
            race.winner = 0;
        else if(count > 1U && race.connected[1])
            race.winner = 1;
        race.winner_tick = started;
        race.armed = true;
        FURI_CRITICAL_EXIT();
    }

    /* Start both candidates without the normal advertise wrapper's teardown of
     * the other provider. Already-connected states are resolved before takeover. */
    for(unsigned index = 0U; index < count; ++index) {
        winner = flippass_output_auto_winner(&race, &connected, &winner_tick);
        if(winner >= 0 || flippass_typing_should_cancel(app) ||
           (uint32_t)(furi_get_tick() - started) >= timeout_ticks)
            break;
        if(!provider[index]->advertise(&host_api)) goto finish;
    }
    while(!flippass_typing_should_cancel(app)) {
        winner = flippass_output_auto_winner(&race, &connected, &winner_tick);
        if(winner >= 0) {
            ok = connected && (uint32_t)(winner_tick - started) <= timeout_ticks;
            break;
        }
        if((uint32_t)(furi_get_tick() - started) >= timeout_ticks) break;
        furi_delay_ms(25U);
    }

finish:
    /* Detach before any blocking cleanup, unpin or return: callbacks can never
     * retain the expired race/context objects. A disconnected winner is not
     * replaced by the other provider, which could duplicate a partial send. */
    for(unsigned index = 0U; index < count; ++index) {
        if(watching[index]) provider[index]->watch_connection(NULL, NULL);
    }
    for(unsigned index = 0U; index < count; ++index) {
        if(provider[index] != NULL && (!ok || (int)index != winner)) {
            flippass_output_transport_cleanup_loaded(app, (FlipPassOutputTransport)index);
        }
    }
    if(ok && (flippass_typing_should_cancel(app) || !provider[winner]->is_connected(&host_api))) {
        flippass_output_transport_cleanup_loaded(app, (FlipPassOutputTransport)winner);
        ok = false;
    }
    for(unsigned index = 0U; index < count; ++index) {
        if(pinned[index]) {
            flippass_module_unpin(
                app, flippass_output_transport_slot((FlipPassOutputTransport)index));
        }
        if(provider[index] != NULL && (!ok || (int)index != winner)) {
            flippass_module_unload(
                app, flippass_output_transport_slot((FlipPassOutputTransport)index));
        }
    }
    if(ok) *selected = (FlipPassOutputTransport)winner;
    return ok;
}

#endif
