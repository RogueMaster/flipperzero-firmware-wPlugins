#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BadUsbApp BadUsbApp;

void bad_usb_nfc_pairing_start(BadUsbApp* app);
void bad_usb_nfc_pairing_stop(BadUsbApp* app);

#ifdef __cplusplus
}
#endif
