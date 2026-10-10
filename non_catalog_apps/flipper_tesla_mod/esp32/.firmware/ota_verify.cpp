/*
 * ota_verify.cpp — deferred OTA image confirmation (see ota_verify.h).
 */

#include "ota_verify.h"
#include "config.h"
#include <Arduino.h>
#include <esp_ota_ops.h>

// Arduino-ESP32 2.0.x (esp32-hal-misc.c) calls this weak hook from
// initArduino(); returning true leaves a PENDING_VERIFY image unconfirmed so
// the bootloader can still roll it back.
extern "C" bool verifyRollbackLater() { return true; }

static bool     g_pending  = false;
static uint32_t g_start_ms = 0;

void ota_verify_begin() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t st;
    if (running == nullptr || esp_ota_get_state_partition(running, &st) != ESP_OK) return;
    if (st == ESP_OTA_IMG_PENDING_VERIFY) {
        g_pending  = true;
        g_start_ms = millis();
        Serial.printf("[OTA] New image on %s - confirming after %lu ms of runtime\n",
                      running->label, (unsigned long)OTA_SELF_VERIFY_MS);
    } else if (st == ESP_OTA_IMG_VALID) {
        Serial.println("[OTA] Running verified firmware");
    }
}

void ota_verify_confirm(const char *why) {
    if (!g_pending) return;
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        g_pending = false;
        Serial.printf("[OTA] Image confirmed (%s)\n", why);
    } else {
        Serial.printf("[OTA] WARNING: could not confirm image (%s)\n", why);
    }
}

void ota_verify_tick(uint32_t now_ms) {
    if (g_pending && (uint32_t)(now_ms - g_start_ms) >= OTA_SELF_VERIFY_MS)
        ota_verify_confirm("runtime healthy");
}
