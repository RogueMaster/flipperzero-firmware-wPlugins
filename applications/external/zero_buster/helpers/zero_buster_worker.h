#pragma once

#include "zero_buster.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Safely release USB CDC/HID and re-enable system CLI VCP */
void zero_buster_release_usb(ZeroBusterApp* app);

/* Main background worker thread for injection and CDC exfiltration */
int32_t zero_buster_worker_thread(void* context);

#ifdef __cplusplus
}
#endif
