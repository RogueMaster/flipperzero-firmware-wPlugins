#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include "usb.h"
bool combo_hid_is_connected(void);
uint8_t combo_hid_get_led_state(void);
void combo_hid_set_state_callback(HidStateCallback cb, void* ctx);
bool combo_hid_kb_press(uint16_t button);
bool combo_hid_kb_release(uint16_t button);
bool combo_hid_kb_release_all(void);
bool combo_hid_mouse_move(int8_t dx, int8_t dy);
bool combo_hid_mouse_press(uint8_t button);
bool combo_hid_mouse_release(uint8_t button);
bool combo_hid_mouse_release_all(void);
bool combo_hid_mouse_scroll(int8_t delta);
bool combo_hid_consumer_key_press(uint16_t button);
bool combo_hid_consumer_key_release(uint16_t button);
bool combo_hid_consumer_key_release_all(void);

void hid_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
void hid_deinit(usbd_device* dev);
void hid_on_wakeup(usbd_device* dev);
void hid_on_suspend(usbd_device* dev);
usbd_respond hid_ep_config(usbd_device* dev, uint8_t cfg);
usbd_respond hid_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);
void* combo_usb_config_descriptor(void);
