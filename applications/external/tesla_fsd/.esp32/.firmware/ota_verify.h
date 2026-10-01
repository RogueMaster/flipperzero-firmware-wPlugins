#pragma once
/*
 * ota_verify.h — confirm a web-OTA'd image only after it has run.
 *
 * The Arduino core marks a new OTA image valid in initArduino(), before
 * setup(), unless verifyRollbackLater() returns true. That made the
 * bootloader's rollback (CONFIG_APP_ROLLBACK_ENABLE=y in Arduino-ESP32 2.0.x)
 * dead: an image that crashed in setup() or the first seconds of loop()
 * boot-looped instead of falling back to the previous firmware.
 *
 * ota_verify.cpp overrides verifyRollbackLater(); the image stays
 * PENDING_VERIFY until it has run OTA_SELF_VERIFY_MS, and any reset before
 * that (panic, watchdog, brownout, power cut) boots the previous image.
 */

#include <stdint.h>

/** setup(): note whether this boot is an unconfirmed OTA image. */
void ota_verify_begin();

/** loop(): confirm the image once it has run OTA_SELF_VERIFY_MS. */
void ota_verify_tick(uint32_t now_ms);

/** Confirm now. Call before every deliberate restart or deep sleep (a reset
 *  while pending would roll back a working image) and before starting the next
 *  web OTA (esp_ota_begin refuses while the running image is unconfirmed). */
void ota_verify_confirm(const char *why);
