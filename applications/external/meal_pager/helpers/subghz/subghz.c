/* Reduced variant of the Flipper Zero SubGhz Class */

#include "subghz_i.h"
#include "../../helpers/meal_pager_custom_event.h"
#include "../../helpers/meal_pager_led.h"
//#include "../meal_pager_storage.h"

SubGhz* subghz_alloc() {
    SubGhz* subghz = malloc(sizeof(SubGhz));

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
    if(!subghz_load_protocol_from_file(app->subghz)) return false;
    if(subghz_txrx_tx_start(app->subghz->txrx, subghz_txrx_get_fff_data(app->subghz->txrx)) !=
       SubGhzTxRxStartTxStateOk) {
        return false;
    }
    // Poll hardware completion in the scene, after the entire RAW file has drained.
    app->state_notifications = SubGhzNotificationStateTx;
    return true;
}
