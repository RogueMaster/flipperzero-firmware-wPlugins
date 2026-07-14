
#include <furi.h>

#include "ui/ui.h"

#define LOG_TAG "gs1_rfid_parser_main"

int32_t gs1_rfid_parser_app(void* p) {
    FURI_LOG_T(LOG_TAG, __func__);
    UNUSED(p);

    FURI_LOG_D(LOG_TAG, "Tick Rate %ld", furi_kernel_get_tick_frequency());
    furi_hal_power_enable_otg();
    furi_delay_ms(100);

    UI* ui = ui_alloc();
    ui_start(ui);
    ui_free(ui);

    furi_hal_power_disable_otg();

    return 0;
}
