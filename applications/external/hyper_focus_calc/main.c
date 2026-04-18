#include "include/app/hyperfocus_app.h"
#include <furi.h>

int32_t hyperfocus_calc_app(void *p) {
    UNUSED(p);
    return hyperfocus_app_run();
}
