
#include <furi.h>
#include <ui/ui.h>

#define LOG_TAG "nfc_sniffer_application"

int32_t nfc_sniffer_app(void* p) {
    FURI_LOG_T(LOG_TAG, __func__);
    UNUSED(p);

    UI* ui = ui_alloc();
    ui_start(ui);
    ui_free(ui);

    return 0;
}
