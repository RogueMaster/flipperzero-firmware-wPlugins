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

/** When a stream is active with exactly one ?ids= filter id, report it so the
 *  caller can install a hardware acceptance filter for full-rate single-ID
 *  capture. Returns false (and leaves *id_out untouched) otherwise. */
bool     http_can_stream_single_filter(uint32_t *id_out);

/** When a stream is active with a ?bus=can0/can1 filter, report the selected
 *  bus so the caller can scope hardware filtering to that CAN controller. */
bool     http_can_stream_bus_filter(CanBusId *bus_out);
uint32_t http_can_stream_frames_sent();
uint32_t http_can_stream_frames_dropped();
uint32_t http_can_stream_frames_filtered();
uint16_t http_can_stream_buffered_frames();

/** Feed the module the running SUM of every installed controller's
 *  rxMissedCount() (frames the CAN controller silently dropped on a full RX
 *  queue). Call once per main-loop iteration; the stream snapshots this at
 *  capture start so it can report a per-capture delta without holding driver
 *  pointers. */
void     http_can_stream_note_rx_missed(uint32_t total_rx_missed);

/** Controller-level frames missed since the current capture began (delta of the
 *  fed-in total against the value snapshotted at stream start). Returns 0 when a
 *  counter reset would otherwise underflow the unsigned subtraction. */
uint32_t http_can_stream_rx_missed(void);
