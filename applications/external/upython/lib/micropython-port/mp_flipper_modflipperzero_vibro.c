#include <furi_hal.h>

#include <mp_flipper_modflipperzero.h>

inline void mp_flipper_vibro(bool state) {
    furi_hal_vibro_on(state);
}
