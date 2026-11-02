/*
 * monitor.h — the shared real-time drop-detection loop.
 *
 * This is the single unit that both entry points reuse: the foreground FAP
 * (drop_dect_app.c) and the headless firmware service (drop_dect_svc.c). It owns
 * the IMU, keeps a rolling 2.5 s window, re-runs the CNN on a sliding window
 * (~every 0.25 s), and fires the siren on a drop.
 *
 * Run it as a FuriThread body: pass a (persistent) MonitorCfg* as the thread
 * context. The loop runs while cfg->running is true.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <notification/notification.h>

#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Called after every inference with the verdict + probabilities. Optional. */
typedef void (*MonitorVerdictCb)(int cls, const float probs[4], void* ud);

typedef struct {
    const DropModel* model; /* loaded weights (must outlive the thread) */
    volatile bool running; /* set false from another thread to stop the loop */
    float thresh; /* min P(drop) to trigger the siren, e.g. 0.5 */
    MonitorVerdictCb on_verdict; /* may be NULL */
    void* ud; /* passed to on_verdict */
    NotificationApp* notif; /* optional LED during siren; NULL = speaker only */
} MonitorCfg;

/* FuriThread entry point. context must be a MonitorCfg*. Returns 0 on clean exit. */
int32_t monitor_run(void* context);

#ifdef __cplusplus
}
#endif
