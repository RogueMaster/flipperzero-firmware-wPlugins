/* Reduced variant of the Flipper Zero SubGhz Class */

#include "subghz_i.h"
#include "../../helpers/meal_pager_custom_event.h"
#include "../../helpers/meal_pager_led.h"
//#include "../meal_pager_storage.h"

SubGhz* subghz_alloc() {
    SubGhz* subghz = calloc(1, sizeof(SubGhz));
    furi_check(subghz);

    subghz->file_path = furi_string_alloc();

    subghz->txrx = subghz_txrx_alloc();

    return subghz;
}

void subghz_free(SubGhz* subghz) {
    //TxRx
    subghz_txrx_free(subghz->txrx);

    // Furi strings
    furi_string_free(subghz->file_path);

    // The rest
    free(subghz);
}

bool subghz_send(void* context) {
    Meal_Pager* app = context;
    furi_assert(app);
    app->state_notifications = SubGhzNotificationStateIDLE;
    if(!subghz_load_protocol_from_file(app->subghz)) {
        FURI_LOG_E(TAG, "Failed to load protocol from temp file");
        return false;
    }
    SubGhzTxRxStartTxState tx_state =
        subghz_txrx_tx_start(app->subghz->txrx, subghz_txrx_get_fff_data(app->subghz->txrx));
    if(tx_state != SubGhzTxRxStartTxStateOk) {
        FURI_LOG_E(TAG, "Start TX failed with state %d", (int)tx_state);
        subghz_txrx_stop(app->subghz->txrx);
        return false;
    }
    // Poll hardware completion in the scene, after the entire RAW file has drained.
    app->state_notifications = SubGhzNotificationStateTx;
    return true;
}
