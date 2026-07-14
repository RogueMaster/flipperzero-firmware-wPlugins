#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>

#include "usb_eth.h"
#include "usb_descriptors.h"

#define TAG "USB_ETH"

/* Saved USB interface to restore on deinit */
static FuriHalUsbInterface* prev_usb_interface = NULL;
static bool usb_eth_initialized = false;

/* External functions from usb_descriptors.c */
extern void usb_eth_set_mac(const uint8_t mac[6]);
extern bool usb_eth_is_connected_internal(void);
extern bool usb_eth_send_frame_internal(const uint8_t* frame, uint16_t len);
extern int16_t usb_eth_receive_frame_internal(uint8_t* frame, uint16_t max_len);
extern uint8_t* usb_rx_frame;

bool usb_eth_init(void) {
    if(usb_eth_initialized) {
        FURI_LOG_W(TAG, "Already initialized");
        return true;
    }

    FURI_LOG_I(TAG, "Initializing USB CDC-ECM...");

    /* Allocate RX buffer once (persists for app lifetime) */
    if(!usb_rx_frame) {
        usb_rx_frame = malloc(1520);
        if(!usb_rx_frame) return false;
    }

    /* Save current USB config so we can restore it later */
    prev_usb_interface = furi_hal_usb_get_config();

    /* Unlock USB if it was locked (e.g. by CLI) */
    furi_hal_usb_unlock();

    /* Switch USB to our CDC-ECM interface */
    if(!furi_hal_usb_set_config(&usb_eth_ecm_interface, NULL)) {
        FURI_LOG_E(TAG, "Failed to set USB config");
        return false;
    }

    usb_eth_initialized = true;
    FURI_LOG_I(TAG, "USB CDC-ECM initialized");

    /* Give the host some time to enumerate */
    furi_delay_ms(200);

    return true;
}

void usb_eth_deinit(void) {
    if(!usb_eth_initialized) return;

    FURI_LOG_I(TAG, "Deinitializing USB CDC-ECM...");

    usb_eth_initialized = false;

    /* Restore previous USB interface.
     * furi_hal_usb_set_config handles disconnect/reconnect internally. */
    if(prev_usb_interface) {
        furi_hal_usb_set_config(prev_usb_interface, NULL);
        prev_usb_interface = NULL;
    }

    FURI_LOG_I(TAG, "USB CDC-ECM deinitialized, previous USB restored");
}

bool usb_eth_send_frame(const uint8_t* frame, uint16_t len) {
    if(!usb_eth_initialized) return false;
    return usb_eth_send_frame_internal(frame, len);
}

int16_t usb_eth_receive_frame(uint8_t* frame, uint16_t max_len) {
    if(!usb_eth_initialized) return 0;
    return usb_eth_receive_frame_internal(frame, max_len);
}

bool usb_eth_is_connected(void) {
    if(!usb_eth_initialized) return false;
    return usb_eth_is_connected_internal();
}
