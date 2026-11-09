#include "transport_usb.h"

#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

struct TransportUsb {
    FuriMessageQueue* link_queue;
    FuriHalUsbInterface* previous;
};

/* The firmware's own IDs, furi_hal_usb_hid.c:16-17: a config replaces them, and nobody assigned
 * this project any. Static: a switch keeps the pointer for a restart (furi_hal_usb.c:374). */
static FuriHalUsbHidConfig transport_usb_config = {
    .vid = 0x046D,
    .pid = 0xC529,
    .product = "Claude Remote",
};

/* Runs in the USB interrupt (furi_hal_usb.c:320), where a queue put takes timeout 0. */
static void transport_usb_state_callback(bool connected, void* context) {
    FuriMessageQueue* link_queue = context;
    furi_message_queue_put(link_queue, &connected, 0);
}

TransportUsb* transport_usb_alloc(FuriMessageQueue* link_queue) {
    TransportUsb* usb = malloc(sizeof(TransportUsb));
    usb->link_queue = link_queue;
    usb->previous = furi_hal_usb_get_config();

    /* The stock order, hid.c:177-187. An open RPC session locks the switch (rpc_cli.c:49). */
    furi_hal_usb_unlock();
    furi_check(furi_hal_usb_set_config(&usb_hid, &transport_usb_config));
    furi_hal_hid_set_state_callback(transport_usb_state_callback, link_queue);

    return usb;
}

void transport_usb_free(TransportUsb* usb) {
    furi_hal_hid_set_state_callback(NULL, NULL);
    furi_hal_usb_set_config(usb->previous, NULL);
    free(usb);
}

void transport_usb_press(TransportUsb* usb, uint16_t key) {
    UNUSED(usb);
    furi_hal_hid_kb_press(key);
}

void transport_usb_release(TransportUsb* usb, uint16_t key) {
    UNUSED(usb);
    furi_hal_hid_kb_release(key);
}

/* ModeStopped ignored the report that came, and none follows until the cable moves. */
void transport_usb_offer(TransportUsb* usb) {
    bool connected = furi_hal_hid_is_connected();
    furi_message_queue_put(usb->link_queue, &connected, 0);
}
