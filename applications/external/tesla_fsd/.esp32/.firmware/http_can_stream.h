#pragma once

#include <Arduino.h>
#include "fsd_handler.h"
#include "can_driver.h"

/**
 * HTTP CAN stream logger.
 *
 * Serves a single plain HTTP stream on port 82 at /stream. The dashboard reads
 * this stream with fetch(), stores the bytes in the browser, and saves them as
 * a candump-compatible text file when the user stops collection.
 */

void     http_can_stream_init();
void     http_can_stream_update();
void     http_can_stream_record(CanBusId bus, const CanFrame &frame);
void     http_can_stream_set_enabled(bool enabled);
bool     http_can_stream_active();
uint32_t http_can_stream_frames_sent();
uint32_t http_can_stream_frames_dropped();
uint32_t http_can_stream_frames_filtered();
uint16_t http_can_stream_buffered_frames();
