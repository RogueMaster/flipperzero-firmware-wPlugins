#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_power.h>

#include <lib/subghz/subghz_tx_rx_worker.h>

#include <applications/services/power/power_service/power.h>    // otg
#include <applications/services/notification/notification.h>    // NotificationApp
#include <notification/notification_messages.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_configs.h>

#include <toolbox/pipe.h>

#include "subghz_share.h"
#include "flipper_share.h"

#define TAG "SubGhzShare"

#define SUBGHZ_MESSAGE_LEN_MAX 60

#define SUBGHZ_DEVICE_CC1101_INT_NAME "cc1101_int"
#define RECORD_POWER "power"
typedef struct Power Power;

struct SubGhzChatWorker {
    FuriThread* thread;
    SubGhzTxRxWorker* subghz_txrx;

    volatile bool worker_running;
    volatile bool worker_stoping;
    FuriMessageQueue* event_queue;
    uint32_t last_time_rx_data;

    // PipeSide* pipe;
};

SubGhzChatWorker* subghz_chat = NULL;

void subghz_cli_radio_device_power_on(void) {
    Power* power = furi_record_open(RECORD_POWER);
    power_enable_otg(power, true);
    furi_record_close(RECORD_POWER);
}

void subghz_cli_radio_device_power_off(void) {
    Power* power = furi_record_open(RECORD_POWER);
    power_enable_otg(power, false);
    furi_record_close(RECORD_POWER);
}

uint8_t subghz_share_send(uint8_t* data, size_t size) {
    // FURI_LOG_I(TAG, "subghz_share_send: start");
    furi_assert(subghz_chat);
    if(!subghz_chat) {
        FURI_LOG_E(TAG, "subghz_share_send: subghz_chat is NULL");
        return 1;
    }
    if(!subghz_chat->subghz_txrx) {
        FURI_LOG_E(TAG, "subghz_share_send: subghz_chat or subghz_txrx is NULL");
        return 2;
    }
    if(!subghz_tx_rx_worker_write(subghz_chat->subghz_txrx, data, size)) {
        FURI_LOG_W(TAG, "subghz_share_send: write failed");
        return 3;
    }

    return 0;
}

uint8_t ss_subghz_deinit() {
    FURI_LOG_T(TAG, "ss_subghz_deinit: start");

    if(!subghz_chat) {
        FURI_LOG_I(TAG, "ss_subghz_deinit: already deinitialized");
        return 0;
    }

    /* stop worker if running */
    if(subghz_chat->subghz_txrx && subghz_tx_rx_worker_is_running(subghz_chat->subghz_txrx)) {
        subghz_tx_rx_worker_stop(subghz_chat->subghz_txrx);
    }

    subghz_chat->worker_running = false;

    /* try to deinit devices (safe to call repeatedly) */
    subghz_devices_deinit();

    /* restore power mode */
    furi_hal_power_suppress_charge_exit();

    /* free queue if allocated */
    if(subghz_chat->event_queue) {
        furi_message_queue_free(subghz_chat->event_queue);
        subghz_chat->event_queue = NULL;
    }

    /* free txrx worker if allocated */
    if(subghz_chat->subghz_txrx) {
        subghz_tx_rx_worker_free(subghz_chat->subghz_txrx);
        subghz_chat->subghz_txrx = NULL;
    }

    /* free thread if allocated */
    if(subghz_chat->thread) {
        // if thread is joinable, join if needed; otherwise free
        // furi_thread_join(subghz_chat->thread); // optional
        furi_thread_free(subghz_chat->thread);
        subghz_chat->thread = NULL;
    }

    free(subghz_chat);
    subghz_chat = NULL;

    FURI_LOG_T(TAG, "ss_subghz_deinit: done");
    return 0;
}

static void ss_subghz_rx_callback(void* ctx) {
    uint8_t buffer[SUBGHZ_MESSAGE_LEN_MAX];
    size_t len = subghz_tx_rx_worker_read(subghz_chat->subghz_txrx, buffer, sizeof(buffer));
    if(len > 0) {
        // FURI_LOG_I(TAG, "ss_subghz_rx_callback: received %zu bytes", len);
        // FURI_LOG_I(TAG, "ss_subghz_rx_callback: received %zu bytes: %.*s", len, (int)len, buffer);
        // if (fs_receive_callback(buffer, len) != 0) {
        //     FURI_LOG_E(TAG, "ss_subghz_rx_callback: failed to add packet to decoder");
        // }
        fs_receive_callback(buffer, len);
    }
    furi_assert(ctx);
}

uint8_t ss_subghz_init() {
    FURI_LOG_I(TAG, "ss_subghz_init: start");

    /* already initialized? */
    if(subghz_chat) {
        FURI_LOG_W(TAG, "ss_subghz_init: already initialized");
        return 0;
    }

    subghz_chat = malloc(sizeof(SubGhzChatWorker));
    furi_assert(subghz_chat);

    uint32_t frequency = 433920000; // TODO: define / make configurable?

    // TODO: check if allowed in this region?

    // FURI_LOG_I(TAG, "ss_subghz_init: subghz_devices_init");
    subghz_devices_init();
    
    // FURI_LOG_I(TAG, "ss_subghz_init: subghz_devices_get_by_name");
    const SubGhzDevice* device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    // FURI_LOG_I(TAG, "ss_subghz_init: check if the device is connected");
    if(!subghz_devices_is_connect(device)) {
        subghz_cli_radio_device_power_off();
        device = subghz_devices_get_by_name(SUBGHZ_DEVICE_CC1101_INT_NAME);
    }
    furi_assert(device);

    // FURI_LOG_I(TAG, "ss_subghz_init: furi_message_queue_alloc");
    subghz_chat->event_queue = furi_message_queue_alloc(80, sizeof(SubGhzChatEvent));
    
    // FURI_LOG_I(TAG, "ss_subghz_init: subghz_tx_rx_worker_alloc");
    furi_assert(subghz_chat);
    subghz_chat->subghz_txrx = subghz_tx_rx_worker_alloc();
    furi_assert(subghz_chat->subghz_txrx);

    // FURI_LOG_I(TAG, "ss_subghz_init: subghz_tx_rx_worker_start");
    if(subghz_tx_rx_worker_start(subghz_chat->subghz_txrx, device, frequency)) {
        furi_message_queue_reset(subghz_chat->event_queue);
        subghz_tx_rx_worker_set_callback_have_read(
            subghz_chat->subghz_txrx, ss_subghz_rx_callback, subghz_chat);
    }
    
    furi_hal_power_suppress_charge_enter();
    
    FURI_LOG_I(TAG, "ss_subghz_init: SubGhz init done");
    return 0;
}
