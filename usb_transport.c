#include "usb_transport.h"

#include "config.h"

#include <furi.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_cdc.h>

#include <stdlib.h>
#include <string.h>

#define TAG "FibUsb"

typedef enum {
    UsbTransportWorkerFlagStop = (1UL << 0),
    UsbTransportWorkerFlagRx = (1UL << 1),
    UsbTransportWorkerFlagState = (1UL << 2),
    UsbTransportWorkerFlagControlLine = (1UL << 3),
    UsbTransportWorkerFlagOverflow = (1UL << 4),
} UsbTransportWorkerFlag;

struct UsbTransport {
    UsbTransportReceiveCallback receive_callback;
    UsbTransportEventCallback event_callback;
    void* callback_context;

    FuriThread* worker;
    FuriStreamBuffer* rx_stream;
    FuriMutex* state_mutex;
    FuriMutex* tx_mutex;
    FuriSemaphore* tx_semaphore;
    CdcCallbacks cdc_callbacks;
    uint8_t tx_packet[CDC_DATA_SZ];

    FuriHalUsbInterface* previous_usb_config;
    bool usb_config_changed;
    bool started;
    /* Read by USB callbacks and written by the application thread. */
    volatile bool accepting_callbacks;

    volatile bool callback_usb_connected;
    volatile uint8_t callback_control_lines;
    bool usb_connected;
    bool port_open;
};

static void usb_transport_signal_worker(UsbTransport* transport, uint32_t flag) {
    if(!transport || !transport->worker) return;
    furi_thread_flags_set(furi_thread_get_id(transport->worker), flag);
}

static void usb_transport_cdc_tx_complete(void* context) {
    UsbTransport* transport = context;
    if(!transport || !transport->accepting_callbacks) return;
    furi_semaphore_release(transport->tx_semaphore);
}

static void usb_transport_cdc_rx(void* context) {
    UsbTransport* transport = context;
    if(!transport || !transport->accepting_callbacks) return;

    uint8_t packet[CDC_DATA_SZ];
    const int32_t received = furi_hal_cdc_receive(FIB_CDC_INTERFACE, packet, sizeof(packet));
    if(received <= 0) return;

    const size_t queued =
        furi_stream_buffer_send(transport->rx_stream, packet, (size_t)received, 0U);
    if(queued != (size_t)received) {
        usb_transport_signal_worker(transport, UsbTransportWorkerFlagOverflow);
    } else {
        usb_transport_signal_worker(transport, UsbTransportWorkerFlagRx);
    }
}

static void usb_transport_cdc_state(void* context, CdcState state) {
    UsbTransport* transport = context;
    if(!transport || !transport->accepting_callbacks) return;
    const bool connected = (state == CdcStateConnected);
    transport->callback_usb_connected = connected;
    if(!connected) {
        transport->callback_control_lines = 0U;
        /* A synchronous sender may be running on the worker itself. */
        furi_semaphore_release(transport->tx_semaphore);
    }
    usb_transport_signal_worker(transport, UsbTransportWorkerFlagState);
}

static void usb_transport_cdc_control_line(void* context, CdcCtrlLine control_lines) {
    UsbTransport* transport = context;
    if(!transport || !transport->accepting_callbacks) return;
    transport->callback_control_lines = (uint8_t)control_lines;
    if((control_lines & CdcCtrlLineDTR) == 0U) {
        furi_semaphore_release(transport->tx_semaphore);
    }
    usb_transport_signal_worker(transport, UsbTransportWorkerFlagControlLine);
}

static void usb_transport_notify(UsbTransport* transport, UsbTransportEvent event) {
    if(transport->event_callback) {
        transport->event_callback(event, transport->callback_context);
    }
}

static void usb_transport_update_state(UsbTransport* transport) {
    const bool connected = transport->callback_usb_connected;
    bool notify_usb = false;
    bool notify_port = false;
    UsbTransportEvent usb_event = UsbTransportEventUsbDisconnected;
    UsbTransportEvent port_event = UsbTransportEventPortClosed;

    furi_mutex_acquire(transport->state_mutex, FuriWaitForever);
    if(transport->usb_connected != connected) {
        transport->usb_connected = connected;
        notify_usb = true;
        usb_event = connected ? UsbTransportEventUsbConnected : UsbTransportEventUsbDisconnected;
    }

    const bool port_open = connected &&
                           ((transport->callback_control_lines & CdcCtrlLineDTR) != 0U);
    if(transport->port_open != port_open) {
        transport->port_open = port_open;
        notify_port = true;
        port_event = port_open ? UsbTransportEventPortOpened : UsbTransportEventPortClosed;
    }
    furi_mutex_release(transport->state_mutex);

    if(!connected) {
        /* Wake a sender that would otherwise wait for an endpoint callback. */
        furi_semaphore_release(transport->tx_semaphore);
    }
    if(notify_usb) usb_transport_notify(transport, usb_event);
    if(notify_port) usb_transport_notify(transport, port_event);
}

static int32_t usb_transport_worker(void* context) {
    UsbTransport* transport = context;
    uint8_t bytes[CDC_DATA_SZ];

    while(true) {
        const uint32_t flags = furi_thread_flags_wait(
            UsbTransportWorkerFlagStop | UsbTransportWorkerFlagRx | UsbTransportWorkerFlagState |
                UsbTransportWorkerFlagControlLine | UsbTransportWorkerFlagOverflow,
            FuriFlagWaitAny,
            FuriWaitForever);
        if((flags & FuriFlagError) != 0U) continue;
        if((flags & UsbTransportWorkerFlagStop) != 0U) break;

        if((flags & UsbTransportWorkerFlagOverflow) != 0U) {
            furi_stream_buffer_reset(transport->rx_stream);
            usb_transport_notify(transport, UsbTransportEventRxOverflow);
        }

        if((flags & UsbTransportWorkerFlagRx) != 0U) {
            while(true) {
                const size_t received =
                    furi_stream_buffer_receive(transport->rx_stream, bytes, sizeof(bytes), 0U);
                if(received == 0U) break;
                if(transport->receive_callback) {
                    transport->receive_callback(bytes, received, transport->callback_context);
                }
            }
        }

        if((flags & (UsbTransportWorkerFlagState | UsbTransportWorkerFlagControlLine)) != 0U) {
            usb_transport_update_state(transport);
        }
    }

    return 0;
}

UsbTransport* usb_transport_alloc(
    UsbTransportReceiveCallback receive_callback,
    UsbTransportEventCallback event_callback,
    void* callback_context) {
    UsbTransport* transport = malloc(sizeof(UsbTransport));
    if(!transport) return NULL;
    memset(transport, 0, sizeof(*transport));

    transport->receive_callback = receive_callback;
    transport->event_callback = event_callback;
    transport->callback_context = callback_context;
    transport->rx_stream = furi_stream_buffer_alloc(FIB_CDC_RX_STREAM_SIZE, 1U);
    transport->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    transport->tx_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    transport->tx_semaphore = furi_semaphore_alloc(1U, 1U);
    transport->worker = furi_thread_alloc_ex(
        "FibUsbWorker", FIB_TRANSPORT_WORKER_STACK, usb_transport_worker, transport);

    if(!transport->rx_stream || !transport->state_mutex || !transport->tx_mutex ||
       !transport->tx_semaphore || !transport->worker) {
        usb_transport_free(transport);
        return NULL;
    }

    transport->cdc_callbacks.tx_ep_callback = usb_transport_cdc_tx_complete;
    transport->cdc_callbacks.rx_ep_callback = usb_transport_cdc_rx;
    transport->cdc_callbacks.state_callback = usb_transport_cdc_state;
    transport->cdc_callbacks.ctrl_line_callback = usb_transport_cdc_control_line;
    transport->cdc_callbacks.config_callback = NULL;
    return transport;
}

static void usb_transport_reset_connection_latches(UsbTransport* transport) {
    transport->callback_usb_connected = false;
    transport->callback_control_lines = 0U;
    if(transport->state_mutex) {
        furi_mutex_acquire(transport->state_mutex, FuriWaitForever);
        transport->usb_connected = false;
        transport->port_open = false;
        furi_mutex_release(transport->state_mutex);
    }
}

bool usb_transport_start(UsbTransport* transport) {
    if(!transport || transport->started) return false;

    usb_transport_reset_connection_latches(transport);
    furi_stream_buffer_reset(transport->rx_stream);

    if(furi_hal_usb_is_locked()) {
        FURI_LOG_W(TAG, "USB mode is locked");
        return false;
    }

    transport->previous_usb_config = furi_hal_usb_get_config();
    transport->accepting_callbacks = true;
    furi_thread_start(transport->worker);

    if(transport->previous_usb_config != &usb_cdc_dual) {
        if(!furi_hal_usb_set_config(&usb_cdc_dual, NULL)) {
            transport->accepting_callbacks = false;
            usb_transport_signal_worker(transport, UsbTransportWorkerFlagStop);
            furi_thread_join(transport->worker);
            usb_transport_reset_connection_latches(transport);
            transport->previous_usb_config = NULL;
            FURI_LOG_E(TAG, "Unable to enable dual CDC");
            return false;
        }
        transport->usb_config_changed = true;
    }

    transport->started = true;
    furi_hal_cdc_set_callbacks(FIB_CDC_INTERFACE, &transport->cdc_callbacks, transport);
    return true;
}

void usb_transport_stop(UsbTransport* transport) {
    if(!transport) return;
    if(!transport->started) {
        usb_transport_reset_connection_latches(transport);
        return;
    }

    transport->accepting_callbacks = false;
    furi_hal_cdc_set_callbacks(FIB_CDC_INTERFACE, NULL, NULL);
    /* Unblock any TX wait before joining the worker that may own that wait. */
    furi_semaphore_release(transport->tx_semaphore);
    usb_transport_signal_worker(transport, UsbTransportWorkerFlagStop);
    furi_thread_join(transport->worker);

    usb_transport_reset_connection_latches(transport);
    furi_stream_buffer_reset(transport->rx_stream);

    if(transport->usb_config_changed) {
        if(!furi_hal_usb_set_config(transport->previous_usb_config, NULL)) {
            FURI_LOG_W(TAG, "Unable to restore previous USB mode");
        }
    }

    transport->started = false;
    transport->usb_config_changed = false;
    transport->previous_usb_config = NULL;
}

static bool usb_transport_ready(UsbTransport* transport) {
    bool ready = false;
    furi_mutex_acquire(transport->state_mutex, FuriWaitForever);
    ready = transport->started && transport->accepting_callbacks && transport->usb_connected &&
            transport->port_open && transport->callback_usb_connected &&
            ((transport->callback_control_lines & CdcCtrlLineDTR) != 0U);
    furi_mutex_release(transport->state_mutex);
    return ready;
}

static uint32_t usb_transport_ticks_until(uint32_t deadline_tick) {
    const int32_t remaining = (int32_t)(deadline_tick - furi_get_tick());
    return (remaining > 0) ? (uint32_t)remaining : 0U;
}

static bool usb_transport_send_packet(
    UsbTransport* transport,
    const uint8_t* data,
    uint16_t length,
    uint32_t deadline_tick) {
    uint32_t remaining_ticks = usb_transport_ticks_until(deadline_tick);
    if(remaining_ticks == 0U ||
       furi_semaphore_acquire(transport->tx_semaphore, remaining_ticks) != FuriStatusOk) {
        return false;
    }
    if(!usb_transport_ready(transport)) {
        furi_semaphore_release(transport->tx_semaphore);
        return false;
    }

    if(length != 0U) memcpy(transport->tx_packet, data, length);
    furi_hal_cdc_send(FIB_CDC_INTERFACE, transport->tx_packet, length);

    /* Wait until the USB controller no longer owns the source buffer. */
    remaining_ticks = usb_transport_ticks_until(deadline_tick);
    if(remaining_ticks == 0U ||
       furi_semaphore_acquire(transport->tx_semaphore, remaining_ticks) != FuriStatusOk) {
        return false;
    }
    furi_semaphore_release(transport->tx_semaphore);
    return usb_transport_ready(transport);
}

bool usb_transport_send_until(
    UsbTransport* transport,
    const uint8_t* data,
    size_t length,
    uint32_t deadline_tick) {
    if(!transport || (!data && length != 0U) || length == 0U) {
        return false;
    }
    const uint32_t mutex_wait = usb_transport_ticks_until(deadline_tick);
    if(mutex_wait == 0U || furi_mutex_acquire(transport->tx_mutex, mutex_wait) != FuriStatusOk) {
        return false;
    }

    bool success = true;
    size_t offset = 0U;
    uint16_t last_packet_length = 0U;
    while(offset < length) {
        const size_t remaining = length - offset;
        last_packet_length = (uint16_t)((remaining > CDC_DATA_SZ) ? CDC_DATA_SZ : remaining);
        if(!usb_transport_send_packet(
               transport, data + offset, last_packet_length, deadline_tick)) {
            success = false;
            break;
        }
        offset += last_packet_length;
    }

    /* Terminate a transfer ending on the endpoint packet boundary. */
    if(success && last_packet_length == CDC_DATA_SZ) {
        uint8_t unused = 0U;
        success = usb_transport_send_packet(transport, &unused, 0U, deadline_tick);
    }

    furi_mutex_release(transport->tx_mutex);
    return success;
}

bool usb_transport_is_usb_connected(UsbTransport* transport) {
    if(!transport) return false;
    bool connected;
    furi_mutex_acquire(transport->state_mutex, FuriWaitForever);
    connected = transport->usb_connected;
    furi_mutex_release(transport->state_mutex);
    return connected;
}

bool usb_transport_is_port_open(UsbTransport* transport) {
    if(!transport) return false;
    bool open;
    furi_mutex_acquire(transport->state_mutex, FuriWaitForever);
    open = transport->port_open;
    furi_mutex_release(transport->state_mutex);
    return open;
}

void usb_transport_free(UsbTransport* transport) {
    if(!transport) return;
    usb_transport_stop(transport);
    if(transport->worker) furi_thread_free(transport->worker);
    if(transport->tx_semaphore) furi_semaphore_free(transport->tx_semaphore);
    if(transport->tx_mutex) furi_mutex_free(transport->tx_mutex);
    if(transport->state_mutex) furi_mutex_free(transport->state_mutex);
    if(transport->rx_stream) furi_stream_buffer_free(transport->rx_stream);
    free(transport);
}
