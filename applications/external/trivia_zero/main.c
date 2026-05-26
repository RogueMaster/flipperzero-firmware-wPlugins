#include "include/app/trivia_zero_app.h"
#include <furi.h>

int32_t trivia_zero_app(void *p) {
    UNUSED(p);
    return trivia_zero_app_run();
}
