#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct App App;

void app_start_status(App* app);
void app_stop_status(App* app);
void status_refresh(App* app);

#ifdef __cplusplus
}
#endif
