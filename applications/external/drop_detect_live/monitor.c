#include "monitor.h"

#include <furi.h>

#include "alarm.h"
#include "imu.h"

#define TAG "drop_monitor"

#define SAMPLE_PERIOD_MS    10u /* ~100 Hz, matches the trainer's capture cadence */
#define INFER_EVERY_SAMPLES 25 /* re-run the CNN every ~0.25 s (sliding window) */

/* Ring buffer of the most recent DROP_WINDOW scaled IMU samples. Static (not on
 * the thread stack) — 250 * 6 * 4 = 6000 bytes — and there is only one monitor. */
static float s_ring[DROP_WINDOW][6];

int32_t monitor_run(void* context) {
    MonitorCfg* cfg = context;
    if(!cfg || !cfg->model) {
        FURI_LOG_E(TAG, "no model");
        return -1;
    }

    if(!imu_open()) {
        FURI_LOG_E(TAG, "IMU open failed");
        return -1;
    }

    int head = 0; /* index of the oldest buffered sample */
    int count = 0; /* number of valid samples in the ring (<= DROP_WINDOW) */
    int since_infer = 0;

    FURI_LOG_I(TAG, "monitoring");

    while(cfg->running) {
        float sample[6];
        if(!imu_read_scaled(sample)) {
            furi_delay_ms(SAMPLE_PERIOD_MS);
            continue;
        }

        /* Push newest sample; once full, advance head so the window slides. */
        int widx = (head + count) % DROP_WINDOW;
        if(count < DROP_WINDOW) {
            count++;
        } else {
            head = (head + 1) % DROP_WINDOW;
        }
        for(int i = 0; i < 6; i++)
            s_ring[widx][i] = sample[i];

        since_infer++;
        if(count >= DROP_WINDOW && since_infer >= INFER_EVERY_SAMPLES) {
            since_infer = 0;

            float probs[DROP_NUM_CLASSES];
            int cls = model_infer(cfg->model, s_ring, head, probs);

            if(cfg->on_verdict) cfg->on_verdict(cls, probs, cfg->ud);

            if(cls == DROP_CLASS_DROP && probs[DROP_CLASS_DROP] >= cfg->thresh) {
                FURI_LOG_I(TAG, "DROP p=%d%%", (int)(probs[DROP_CLASS_DROP] * 100.0f));
                alarm_play_siren(cfg->notif); /* blocks ~1 s */

                /* Clear the 2.5 s interval so we don't re-trigger on the same
                 * drop, then refill a fresh window before the next inference. */
                head = 0;
                count = 0;
                since_infer = 0;
            }
        }

        furi_delay_ms(SAMPLE_PERIOD_MS);
    }

    imu_close();
    FURI_LOG_I(TAG, "monitor stopped");
    return 0;
}
