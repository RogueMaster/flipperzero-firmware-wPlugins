#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct App App;

void app_start_about(App* app);
void app_stop_about(App* app);

#ifdef __cplusplus
}
#endif
