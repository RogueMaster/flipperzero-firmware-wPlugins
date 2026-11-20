#pragma once

#include "zero_buster.h"

#ifdef __cplusplus
extern "C" {
#endif

void zero_buster_release_usb(ZeroBusterApp* app);

int32_t zero_buster_worker_thread(void* context);

#ifdef __cplusplus
}
#endif
