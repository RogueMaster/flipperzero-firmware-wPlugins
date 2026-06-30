#include "infrared_controller.h"
#include <furi.h>
#include <infrared_worker.h>
#include <infrared_signal.h>
#include <notification/notification_messages.h>
#include <furi_hal_gpio.h>
#include <furi_hal_power.h>
#include <furi_hal_infrared.h>

#define TAG "InfraredController"

const NotificationSequence sequence_hit = {
    &message_vibro_on,
    &message_note_d4,
    &message_delay_1000,
    &message_vibro_off,
    &message_sound_off,
    NULL,
};

const NotificationSequence sequence_bloop = {
    &message_note_g3,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

extern const NotificationSequence sequence_short_beep;

static bool external_board_connected = false;

static void infrared_setup_external_board(bool enable) {
    if(enable) {
        furi_hal_gpio_init(&gpio_ext_pa7, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
        furi_hal_power_enable_otg();
        furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinExtPA7);
    } else {
        furi_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        furi_hal_power_disable_otg();
        furi_hal_infrared_set_tx_output(FuriHalInfraredTxPinInternal);
    }
}

void update_infrared_board_status(InfraredController* controller) {
    if(!controller || !controller->notification) return;

    FuriHalInfraredTxPin detected_pin = furi_hal_infrared_detect_tx_output();

    if(detected_pin == FuriHalInfraredTxPinExtPA7 && !external_board_connected) {
        external_board_connected = true;
        infrared_setup_external_board(true);
        notification_message(controller->notification, &sequence_short_beep);
        FURI_LOG_I(TAG, "External infrared board connected and powered.");
    } else if(detected_pin == FuriHalInfraredTxPinInternal && external_board_connected) {
        external_board_connected = false;
        infrared_setup_external_board(false);
        notification_message(controller->notification, &sequence_bloop);
        FURI_LOG_I(TAG, "External infrared board disconnected and power disabled.");
    }
}

static void infrared_reset(void* context, uint32_t arg) {
    UNUSED(arg);
    InfraredController* controller = (InfraredController*)context;
    // furi_stream_buffer_reset(instance->stream) not exposed to the API.
    // infrared_worker_rx_stop calls it internally.
    infrared_worker_rx_stop(controller->worker);
    infrared_worker_rx_start(controller->worker);
    controller->processing_signal = false;
}

static void infrared_rx_callback(void* context, InfraredWorkerSignal* received_signal) {
    FURI_LOG_I(TAG, "RX callback triggered");

    InfraredController* controller = (InfraredController*)context;
    if(controller->processing_signal) {
        FURI_LOG_W(TAG, "Already processing a signal, skipping callback");
        return;
    }

    controller->processing_signal = true;

    if(!received_signal) {
        FURI_LOG_E(TAG, "Received signal is NULL");
        controller->processing_signal = false;
        return;
    }

    const InfraredMessage* message = infrared_worker_get_decoded_signal(received_signal);
    FURI_LOG_I(TAG, "Received signal - signal address: %p", (void*)received_signal);

    if(message) {
        FURI_LOG_I(
            TAG,
            "Received message: protocol=%d, address=0x%lx, command=0x%lx",
            message->protocol,
            (unsigned long)message->address,
            (unsigned long)message->command);

        if((controller->team == TeamRed && message->command == IR_COMMAND_BLUE_TEAM) ||
           (controller->team == TeamBlue && message->command == IR_COMMAND_RED_TEAM)) {
            controller->hit_received = true;
            FURI_LOG_I(
                TAG, "Hit detected for team: %s", controller->team == TeamRed ? "Red" : "Blue");
            notification_message_block(controller->notification, &sequence_hit);
        }
    } else {
        FURI_LOG_W(TAG, "RX callback received NULL message");
    }

    FURI_LOG_I(TAG, "RX callback completed");
    furi_timer_pending_callback(infrared_reset, controller, 0);
}

InfraredController* infrared_controller_alloc() {
    FURI_LOG_I(TAG, "Allocating InfraredController");

    InfraredController* controller = malloc(sizeof(InfraredController));
    if(!controller) {
        FURI_LOG_E(TAG, "Failed to allocate InfraredController");
        return NULL;
    }

    controller->team = TeamRed;
    controller->worker = infrared_worker_alloc();
    controller->signal = infrared_signal_alloc();
    controller->notification = furi_record_open(RECORD_NOTIFICATION);
    controller->hit_received = false;
    controller->processing_signal = false;

    if(controller->worker && controller->signal && controller->notification) {
        FURI_LOG_I(
            TAG, "InfraredWorker, InfraredSignal, and NotificationApp allocated successfully");
    } else {
        FURI_LOG_E(TAG, "Failed to allocate resources");
        free(controller);
        return NULL;
    }

    FURI_LOG_I(TAG, "Setting up RX callback");
    infrared_worker_rx_set_received_signal_callback(
        controller->worker, infrared_rx_callback, controller);

    FURI_LOG_I(TAG, "InfraredController allocated successfully");
    return controller;
}

void infrared_controller_free(InfraredController* controller) {
    FURI_LOG_I(TAG, "Freeing InfraredController");

    if(controller) {
        if(controller->worker_rx_active) {
            FURI_LOG_I(TAG, "Stopping RX worker");
            infrared_worker_rx_stop(controller->worker);
        }

        FURI_LOG_I(TAG, "Freeing InfraredWorker and InfraredSignal");
        infrared_worker_free(controller->worker);
        infrared_signal_free(controller->signal);

        FURI_LOG_I(TAG, "Closing NotificationApp");
        furi_record_close(RECORD_NOTIFICATION);

        free(controller);

        FURI_LOG_I(TAG, "InfraredController freed successfully");
    } else {
        FURI_LOG_W(TAG, "Attempted to free NULL InfraredController");
    }
}

void infrared_controller_set_team(InfraredController* controller, LaserTagTeam team) {
    FURI_LOG_I(TAG, "Setting team to %s", team == TeamRed ? "Red" : "Blue");
    controller->team = team;
}

void infrared_controller_send(InfraredController* controller) {
    FURI_LOG_I(TAG, "Preparing to send infrared signal");

    InfraredMessage message = {
        .protocol = InfraredProtocolNEC,
        .address = 0x42,
        .command = (controller->team == TeamRed) ? IR_COMMAND_RED_TEAM : IR_COMMAND_BLUE_TEAM};

    FURI_LOG_I(
        TAG,
        "Prepared message: protocol=%d, address=0x%lx, command=0x%lx",
        message.protocol,
        (unsigned long)message.address,
        (unsigned long)message.command);

    if(controller->worker_rx_active) {
        FURI_LOG_I(TAG, "Stopping RX worker");
        infrared_worker_rx_stop(controller->worker);
        controller->worker_rx_active = false;
    }

    FURI_LOG_I(TAG, "Setting message for infrared signal");
    infrared_signal_set_message(controller->signal, &message);

    FURI_LOG_I(TAG, "Starting infrared signal transmission");
    infrared_signal_transmit(controller->signal);

    if(!controller->worker_rx_active) {
        infrared_worker_rx_start(controller->worker);
        controller->worker_rx_active = true;
    }

    FURI_LOG_I(TAG, "Infrared signal transmission completed");
}

bool infrared_controller_receive(InfraredController* controller) {
    FURI_LOG_I(TAG, "Starting infrared signal reception");

    if(controller->processing_signal) {
        FURI_LOG_W(TAG, "Cannot start reception, another signal is still being processed");
        return false;
    }

    if(!controller->worker_rx_active) {
        infrared_worker_rx_start(controller->worker);
        controller->worker_rx_active = true;
        furi_delay_ms(250);
    }

    bool hit = controller->hit_received;

    FURI_LOG_I(TAG, "Signal reception complete, hit received: %s", hit ? "true" : "false");

    controller->hit_received = false;

    return hit;
}

void infrared_controller_pause(InfraredController* controller) {
    if(controller->worker_rx_active) {
        FURI_LOG_I(TAG, "Stopping RX worker");
        infrared_worker_rx_stop(controller->worker);
        controller->worker_rx_active = false;
    }
}

void infrared_controller_resume(InfraredController* controller) {
    if(!controller->worker_rx_active) {
        FURI_LOG_I(TAG, "Starting RX worker");
        infrared_worker_rx_start(controller->worker);
        controller->worker_rx_active = true;
    }
}
