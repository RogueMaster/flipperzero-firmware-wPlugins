#include "transmit.h"

#include <furi.h>
#include <furi_hal.h>
#include <lib/subghz/devices/cc1101_configs.h>

#include <stdlib.h>

#define TX_GAP_US 10000 // 10ms gap between repeats

typedef enum {
    TxCmdNone,
    TxCmdStart,
    TxCmdStop,
    TxCmdExit,
} TxCommand;

struct OpenshockTx {
    FuriThread* thread;
    FuriMessageQueue* cmd_queue;

    // Pulse data (written by main thread before sending TxCmdStart)
    OokPulse pulses[OPENSHOCK_MAX_PULSES];
    size_t count;

    // Async TX state (only accessed by TX thread + callback)
    size_t pulse_index;
    bool phase_high;
    volatile bool stop_requested;
    volatile bool active;
};

static LevelDuration tx_callback(void* context) {
    OpenshockTx* tx = context;

    if(tx->stop_requested) {
        return level_duration_reset();
    }

    if(tx->pulse_index >= tx->count) {
        tx->pulse_index = 0;
        tx->phase_high = true;
        return level_duration_make(false, TX_GAP_US);
    }

    const OokPulse* p = &tx->pulses[tx->pulse_index];

    if(tx->phase_high) {
        tx->phase_high = false;
        return level_duration_make(true, p->high_us);
    } else {
        tx->phase_high = true;
        tx->pulse_index++;
        return level_duration_make(false, p->low_us);
    }
}

static void tx_do_start(OpenshockTx* tx) {
    tx->pulse_index = 0;
    tx->phase_high = true;
    tx->stop_requested = false;

    // Use direct HAL for CC1101 control
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    furi_hal_subghz_set_frequency_and_path(OPENSHOCK_FREQUENCY);

    furi_hal_power_suppress_charge_enter();

    if(!furi_hal_subghz_start_async_tx(tx_callback, tx)) {
        furi_hal_subghz_sleep();
        furi_hal_power_suppress_charge_exit();
        tx->active = false;
        return;
    }

    tx->active = true;

    // Wait for stop or exit command
    TxCommand cmd;
    while(true) {
        if(furi_message_queue_get(tx->cmd_queue, &cmd, 5) == FuriStatusOk) {
            if(cmd == TxCmdStop || cmd == TxCmdExit) break;
        }
    }

    // Teardown
    tx->stop_requested = true;
    while(!furi_hal_subghz_is_async_tx_complete()) {
        furi_delay_ms(1);
    }
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_sleep();
    furi_hal_power_suppress_charge_exit();

    tx->active = false;

    if(cmd == TxCmdExit) {
        furi_message_queue_put(tx->cmd_queue, &cmd, 0);
    }
}

static int32_t tx_thread(void* context) {
    OpenshockTx* tx = context;
    TxCommand cmd;

    while(true) {
        if(furi_message_queue_get(tx->cmd_queue, &cmd, FuriWaitForever) != FuriStatusOk) {
            continue;
        }
        if(cmd == TxCmdExit) break;
        if(cmd == TxCmdStart) {
            tx_do_start(tx);
        }
    }

    return 0;
}

OpenshockTx* openshock_tx_alloc(void) {
    OpenshockTx* tx = malloc(sizeof(OpenshockTx));
    if(!tx) return NULL;

    memset(tx, 0, sizeof(OpenshockTx));
    tx->cmd_queue = furi_message_queue_alloc(4, sizeof(TxCommand));
    tx->thread = furi_thread_alloc_ex("OpenshockTx", 2048, tx_thread, tx);
    furi_thread_start(tx->thread);

    return tx;
}

void openshock_tx_free(OpenshockTx* tx) {
    if(!tx) return;

    TxCommand cmd = TxCmdExit;
    furi_message_queue_put(tx->cmd_queue, &cmd, FuriWaitForever);
    furi_thread_join(tx->thread);
    furi_thread_free(tx->thread);
    furi_message_queue_free(tx->cmd_queue);
    free(tx);
}

bool openshock_tx_start(OpenshockTx* tx, const OokPulse* pulses, size_t count) {
    if(!tx || !pulses || count == 0 || count > OPENSHOCK_MAX_PULSES) return false;

    memcpy(tx->pulses, pulses, count * sizeof(OokPulse));
    tx->count = count;

    TxCommand cmd = TxCmdStart;
    return furi_message_queue_put(tx->cmd_queue, &cmd, 100) == FuriStatusOk;
}

void openshock_tx_stop(OpenshockTx* tx) {
    if(!tx) return;
    TxCommand cmd = TxCmdStop;
    furi_message_queue_put(tx->cmd_queue, &cmd, 100);
}

bool openshock_tx_is_active(OpenshockTx* tx) {
    return tx && tx->active;
}
