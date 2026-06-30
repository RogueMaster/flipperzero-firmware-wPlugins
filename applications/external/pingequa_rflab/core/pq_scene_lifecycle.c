/**
 * @file pq_scene_lifecycle.c
 * @brief Scene 生命周期 + app shutdown 实现.
 */
#include "pq_scene_lifecycle.h"

#include "pq_chip_arbiter.h"

#include <furi.h>

#define TAG "PqScene"

void pq_scene_enter(const char* scene_name) {
    FURI_LOG_I(TAG, "enter %s", scene_name);
}

void pq_scene_exit(const char* scene_name) {
    FURI_LOG_I(TAG, "exit %s", scene_name);
}

void pq_app_shutdown(void) {
    FURI_LOG_I(TAG, "app_shutdown");
    pq_chip_arbiter_force_release();
    pq_chip_arbiter_deinit();
    /* 规范 §5.7:不主动关 OTG. */
}
