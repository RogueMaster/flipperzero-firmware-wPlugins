#ifndef NFC_HID_HEADERS
#define NFC_HID_HEADERS

#include "app.h"
#include "callbacks.h"
#include "keyboard.h"
#include "tools.h"

NfcHidApp* nfc_hid_alloc();
void nfc_hid_free(NfcHidApp* app);
int32_t nfc_hid_app(void* p);

#endif
