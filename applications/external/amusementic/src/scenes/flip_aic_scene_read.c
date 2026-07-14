#include "../flip_aic.h"
#include "flip_aic_scene.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>

enum PollerResult {
    PollerResultSuccess,
    PollerResultFailure,
};

static NfcCommand flip_aic_scene_read_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FelicaPollerEvent* poller_event = event.event_data;
    FURI_LOG_I(TAG, "Poller event: %d", poller_event->type);

    switch(poller_event->type) {
    case FelicaPollerEventTypeReady:
    case FelicaPollerEventTypeIncomplete:
        view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultSuccess);
        return NfcCommandStop;

    case FelicaPollerEventTypeError:
        FURI_LOG_I(TAG, "Error: %d", poller_event->data->error);
        view_dispatcher_send_custom_event(app->view_dispatcher, PollerResultFailure);
        return NfcCommandStop;

    case FelicaPollerEventTypeRequestAuthContext:
        poller_event->data->auth_context->skip_auth = true;
        return NfcCommandContinue;
    }

    return NfcCommandContinue; // why is this needed????
}

void flip_aic_scene_read_on_enter(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolFelica);

    FURI_LOG_I(TAG, "Starting poller");
    nfc_poller_start(app->nfc_poller, flip_aic_scene_read_poller_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipAICViewLoading);
}

bool flip_aic_scene_read_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    FlipAIC* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case PollerResultSuccess:
            nfc_device_set_data(
                app->nfc_device, NfcProtocolFelica, nfc_poller_get_data(app->nfc_poller));
            scene_manager_next_scene(app->scene_manager, FlipAICSceneDisplay);
            return true;

        case PollerResultFailure:
            scene_manager_next_scene(app->scene_manager, FlipAICSceneMenu);
            return true;
        }
    }

    return false;
}

void flip_aic_scene_read_on_exit(void* context) {
    furi_assert(context);
    FlipAIC* app = context;

    FURI_LOG_I(TAG, "Stopping poller");
    nfc_poller_stop(app->nfc_poller);
    nfc_poller_free(app->nfc_poller);
}
