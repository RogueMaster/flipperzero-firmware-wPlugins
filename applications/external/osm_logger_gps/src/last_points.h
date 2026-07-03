#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct App App;

void app_start_last_points(App* app);
void app_stop_last_points(App* app);

#ifdef __cplusplus
}
#endif
