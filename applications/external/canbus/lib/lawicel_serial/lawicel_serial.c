#include "lawicel_serial.h"

bool lawicel_open_port(void) {
    bool state = false;
    if(furi_hal_usb_get_config() != &usb_cdc_dual)
        state = furi_hal_usb_set_config(&usb_cdc_dual, NULL);
    return state;
}

bool lawicel_close_port(void) {
    bool state = false;
    if(furi_hal_usb_get_config() == &usb_cdc_dual)
        state = furi_hal_usb_set_config(&usb_cdc_single, NULL);
    return state;
}
