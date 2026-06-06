#include "include/app/tutu_app.h"
#include <furi.h>

int32_t tutu_app_entry(void* p) {
    UNUSED(p);
    return tutu_app_run();
}
