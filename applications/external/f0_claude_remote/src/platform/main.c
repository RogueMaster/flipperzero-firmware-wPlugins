#include <furi.h>

#include "app.h"

int32_t main_entry(void* argument) {
    UNUSED(argument);

    App* app = app_alloc();
    app_run(app);
    app_free(app);

    return 0;
}
