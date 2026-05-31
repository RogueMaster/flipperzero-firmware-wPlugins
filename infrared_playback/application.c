
#include <furi.h>
#include <ui/ui.h>

#define LOG_TAG "infrared_playback"

int32_t infrared_playback_app(void* p) {
    FURI_LOG_T(LOG_TAG, __func__);
    UNUSED(p);
    
    UI* ui = ui_alloc();
    ui_start(ui);
    ui_free(ui);
    
    return 0;
}
