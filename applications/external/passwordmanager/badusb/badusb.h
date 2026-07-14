#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>
#include <usb_hid.h>

void initialize_hid(void);
void release_all_keys(void);
void type_string(const char* str);
void press_key(uint8_t key);
