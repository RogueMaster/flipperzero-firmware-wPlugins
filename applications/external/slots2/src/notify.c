#include <furi_hal.h>

#include "include/notify.h"

void notify(notification_t notification) {
    switch(notification) {
    case NOTIFICATION_WIN:
        furi_hal_light_set(LightGreen, 0xFF);
        furi_hal_vibro_on(true);
        furi_delay_ms(100);
        furi_hal_vibro_on(false);
        furi_hal_light_set(LightGreen, 0x00);
        break;

    case NOTIFICATION_NO_BALANCE:
        furi_hal_light_set(LightRed, 0xFF);
        for(int i = 0; i < 2; i++) {
            furi_hal_vibro_on(true);
            furi_delay_ms(50);
            furi_hal_vibro_on(false);
            furi_delay_ms(50);
        }
        furi_hal_light_set(LightRed, 0x00);
        break;

    case NOTIFICATION_CHEAT_ENABLED:
        furi_hal_light_set(LightBlue, 0xFF);
        furi_hal_vibro_on(true);
        furi_delay_ms(250);
        furi_hal_vibro_on(false);
        furi_hal_light_set(LightBlue, 0x00);
        break;
    }
}
