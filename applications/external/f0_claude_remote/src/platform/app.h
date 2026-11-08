#pragma once

typedef struct App App;

App* app_alloc(void);

void app_free(App* app);

void app_run(App* app);
