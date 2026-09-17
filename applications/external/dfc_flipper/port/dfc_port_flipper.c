/* Flipper implementation of the engine's platform interface. */
#include "dfc_port.h"

#include <furi.h>
#include <furi_hal.h>
#include <gui/view_dispatcher.h>
#include <string.h>

#include "../dfc_i.h"

void dfc_random_fill(uint8_t* buf, size_t len) {
    furi_hal_random_fill_buf(buf, len);
}

void* dfc_platform_alloc(size_t size, DfcAllocTag tag) {
    DFC_UNUSED(tag);
    void* block = malloc(size);
    if(block) memset(block, 0, size);
    return block;
}

void dfc_platform_free(void* ptr) {
    free(ptr);
}

void dfc_assert_fail(const char* file, int line) {
    FURI_LOG_E("DFC", "Core assertion failed at %s:%d", file, line);
    furi_crash("DFC core assertion failed");
}

void dfc_port_notify(void* context, DfcEvent event) {
    Dfc* dfc = context;
    if(!dfc) return;
    switch(event) {
    case DfcEventApplicationSelected:
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventAppSelected);
        break;
    case DfcEventAuthenticated:
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventAuthenticated);
        break;
    case DfcEventFileRequested:
        view_dispatcher_send_custom_event(dfc->view_dispatcher, DfcCustomEventFileRequested);
        break;
    }
}
