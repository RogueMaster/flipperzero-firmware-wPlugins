/* Reduced variant of the Flipper Zero SubGhz Class */

#include "subghz_i.h"
#include "../../helpers/xremote_custom_event.h"
#include "../../helpers/xremote_led.h"

#include <lib/subghz/blocks/generic.h>

SubGhz* subghz_alloc() {
    SubGhz* subghz = malloc(sizeof(SubGhz));

    subghz->file_path = furi_string_alloc();
    subghz->txrx = subghz_txrx_alloc();
    subghz_txrx_set_need_save_callback(subghz->txrx, subghz_save_to_file, subghz);
    subghz->dialogs = furi_record_open(RECORD_DIALOGS);

    return subghz;
}

void subghz_free(SubGhz* subghz) {
    subghz_txrx_free(subghz->txrx);
    furi_string_free(subghz->file_path);
    furi_record_close(RECORD_DIALOGS);

    // The rest
    free(subghz);
}

void subghz_send(void* context, const char* path) {
    XRemote* app = context;

    if(!subghz_load_protocol_from_file(app->subghz, path)) {
        xremote_cross_remote_set_transmitting(app->cross_remote, XRemoteTransmittingStop);
        app->transmitting = false;
        return;
    }

    // Send like a single button press in Sub-GHz app: protocols TX as if the button
    // was held for sg_timing and then finish with their own repeats,
    // RAW is sent once to the end of file. subghz_send_tick() catches the end of TX
    subghz_block_generic_global.endless_tx = subghz_get_load_type_file(app->subghz) !=
                                             SubGhzLoadTypeFileRaw;
    if(!subghz_tx_start(app->subghz, subghz_txrx_get_fff_data(app->subghz->txrx))) {
        subghz_block_generic_global.endless_tx = false;
        xremote_cross_remote_set_transmitting(app->cross_remote, XRemoteTransmittingStop);
        app->transmitting = false;
        return;
    }
    app->subghz_tx_running = true;
    app->subghz_tx_start = furi_get_tick();
    app->state_notifications = SubGhzNotificationStateTx;
}

void subghz_send_tick(void* context) {
    XRemote* app = context;
    if(!app->subghz_tx_running) return;

    if(subghz_block_generic_global.endless_tx &&
       furi_get_tick() - app->subghz_tx_start >= furi_ms_to_ticks(app->sg_timing)) {
        // "Release" the button
        subghz_block_generic_global.endless_tx = false;
    }
    if(subghz_txrx_tx_is_complete(app->subghz->txrx)) {
        app->subghz_tx_running = false;
        xremote_cross_remote_set_transmitting(app->cross_remote, XRemoteTransmittingStopSubghz);
    }
}

void subghz_send_stop(void* context) {
    XRemote* app = context;
    subghz_block_generic_global.endless_tx = false;
    app->subghz_tx_running = false;
    // This will call need_save_callback, if necessary
    subghz_txrx_stop(app->subghz->txrx);
    subghz_txrx_reset_dynamic_and_custom_btns(app->subghz->txrx);
}
