#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration pour éviter l'inclusion croisée
typedef struct App App;

// API Quick Log (implémentée dans quick_log.c)
void app_start_quick_log(App* app);
void app_stop_quick_log(App* app);
void quick_log_refresh(App* app);

// Exécute un save différé si `save_deferred` est posé. À appeler depuis le tick
// du ViewDispatcher uniquement (pas depuis un input handler — sinon on perd
// l'avantage d'afficher "Saving..." avant le blocage I/O).
void quick_log_tick_deferred(App* app);

#ifdef __cplusplus
}
#endif
