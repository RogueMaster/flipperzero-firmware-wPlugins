
#include <furi.h>
#include <expansion/expansion.h>

#include "ui/ui.h"

#define LOG_TAG "tile_scanner_main"

int32_t tile_scanner_app(void* p) {
    FURI_LOG_T(LOG_TAG, __func__);
    UNUSED(p);
    
    // Disable expansion protocol to avoid UART interference
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    
    bool is_5v_enabled_by_app = false;
    
    // enable 5v pin if not enabled
    if(!furi_hal_power_is_otg_enabled()) {
        furi_hal_power_enable_otg();
        is_5v_enabled_by_app = true;
        furi_delay_ms(100);
    }
    
    UI* ui = ui_alloc();
    ui_start(ui);
    ui_free(ui);
    
    // disable 5v pin if enabled by app
    if(is_5v_enabled_by_app) {
        furi_hal_power_disable_otg();
    }
    
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    
    return 0;
}
