#include "caixianlin_types.h"

const char* mode_names[] = {NULL, "Shock", "Vibrate", "Beep"};

void app_init(CaixianlinRemoteApp* app) {
    app->station_id = 0;
    app->channel = 0;
    app->mode = 2;
    app->strength = 5;
    app->running = true;
}
