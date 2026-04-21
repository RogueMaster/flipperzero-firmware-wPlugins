#pragma once
#include <Arduino.h>
#include "fsd_handler.h"

/**
 * can_dump.h — SD-card CAN bus logger (Lilygo T-CAN485)
 *
 * Logs all received CAN frames to SD card in candump ASCII format:
 *   (elapsed_s.elapsed_us) can0 ID#DATA
 *
 * Directory layout:
 *   /dumps/00001/candump_0.dump
 *   /dumps/00001/candump_900.dump   (file rotation at 1 M entries)
 *   /dumps/00002/candump_0.dump     (next dump session)
 *
 * Auto-stops after 15 minutes. File is flushed and closed on stop.
 *
 * All functions are safe no-ops on non-Lilygo builds.
 */

void   can_dump_init();
bool   can_dump_start();
void   can_dump_stop();
void   can_dump_record(const CanFrame &frame);
void   can_dump_tick(uint32_t now_ms);
bool   can_dump_active();
String sd_format_card();

// Write a timestamped human-readable entry to debug.log in the active session
// directory. No-op if no dump is active. Printf-style format.
void   can_dump_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
