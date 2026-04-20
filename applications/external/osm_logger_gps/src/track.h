#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct App App;

// Ouvre l'écran de tracking. Démarre un timer périodique qui écrit un trkpt
// dans track.gpx tant que la vue est active (et qu'un fix est disponible).
void app_start_track(App* app);

// Libère la vue + timer si alloués (teardown seulement).
void app_stop_track(App* app);

// Synchronise l'état (coords, points, durée) depuis app vers le modèle de la vue.
// Appelé depuis le tick ViewDispatcher pour mettre à jour la durée en live.
void track_refresh(App* app);

#ifdef __cplusplus
}
#endif
