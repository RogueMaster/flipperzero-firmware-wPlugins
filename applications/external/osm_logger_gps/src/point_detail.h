#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct App App;

// Affiche les détails d'un point à partir de sa ligne JSONL brute.
void app_show_point_detail(App* app, const char* raw_jsonl);
void app_stop_point_detail(App* app);

#ifdef __cplusplus
}
#endif
