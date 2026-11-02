/*
 * drop_dect_svc.c — headless background-service entry point.
 *
 * This is the "drop-in for custom firmware" variant: no GUI, no view dispatcher.
 * It loads the model once at boot and then runs the shared monitor loop forever,
 * sounding the siren whenever a drop is detected — even with no app open.
 *
 * Because stock firmware unloads FAPs on exit, a true always-on service must be
 * compiled into a firmware image (apptype=FlipperAppType.SERVICE). See README.md
 * for the manifest block and integration steps. The detection logic itself is
 * identical to the FAP — both call monitor_run().
 */
#include <furi.h>
#include <storage/storage.h>

#include <stdlib.h>

#include "model.h"
#include "monitor.h"

#define TAG "drop_dect_svc"

/* Where the service expects its weights. When integrating into firmware, deploy
 * model.json here (apps_assets path for the service appid). */
#define SVC_MODEL_PATH "/ext/apps_assets/drop_detect_svc/model.json"

#define MODEL_LOAD_ATTEMPTS 10
#define MODEL_LOAD_RETRY_MS 1000u
#define DROP_THRESHOLD      0.5f
#define IMU_RETRY_MS        2000u /* re-attempt monitor/IMU bring-up if the module isn't present yet */
#define SVC_IDLE_MS         60000u /* wake interval while parked with nothing to run */

/*
 * A FlipperAppType.SERVICE entry point MUST NEVER RETURN. The kernel panics with
 * "Service threads MUST NOT return" (locking the Flipper) the instant a service
 * thread falls off its entry function. So every "give up" path below parks here
 * forever instead of returning — the system stays up, the siren just never arms.
 */
__attribute__((__noreturn__)) static void drop_dect_svc_idle_forever(void) {
    while(true)
        furi_delay_ms(SVC_IDLE_MS);
}

int32_t drop_dect_svc(void* p) {
    UNUSED(p);

    DropModel* model = malloc(sizeof(DropModel));
    if(!model) {
        FURI_LOG_E(TAG, "out of memory; service idle");
        drop_dect_svc_idle_forever(); /* must not return */
    }

    /* SD may mount slightly after services start; retry a few times rather than
     * giving up if the model isn't readable yet. */
    bool loaded = false;
    for(int attempt = 0; attempt < MODEL_LOAD_ATTEMPTS && !loaded; attempt++) {
        loaded = model_load_json(model, SVC_MODEL_PATH);
        if(!loaded) {
            FURI_LOG_W(TAG, "model load attempt %d/%d failed", attempt + 1, MODEL_LOAD_ATTEMPTS);
            furi_delay_ms(MODEL_LOAD_RETRY_MS);
        }
    }

    if(!loaded) {
        FURI_LOG_E(TAG, "no model at %s; service idle (siren won't arm)", SVC_MODEL_PATH);
        free(model);
        drop_dect_svc_idle_forever(); /* must not return */
    }

    MonitorCfg cfg = {0};
    cfg.model = model;
    cfg.running = true; /* headless service: run indefinitely */
    cfg.thresh = DROP_THRESHOLD;
    cfg.on_verdict = NULL; /* monitor.c already logs the verdict */
    cfg.ud = NULL;
    cfg.notif = NULL; /* no notification record held; speaker-only alarm */

    /* monitor_run() loops forever while running, but returns early if the IMU
     * can't be opened (e.g. the module isn't attached). Retry instead of
     * returning, so the siren arms as soon as the hardware shows up. This loop
     * never breaks: the service owns `model` for the device's lifetime. */
    while(true) {
        FURI_LOG_I(TAG, "drop service running");
        monitor_run(&cfg);
        FURI_LOG_W(
            TAG,
            "monitor exited (IMU unavailable?); retrying in %lu ms",
            (unsigned long)IMU_RETRY_MS);
        furi_delay_ms(IMU_RETRY_MS);
    }
}
