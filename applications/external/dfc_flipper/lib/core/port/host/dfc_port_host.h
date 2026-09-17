/* Test control over the host platform implementation. */
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define DFC_HOST_RANDOM_BUFFER_MAX 64

// Queues the octets dfc_random_fill returns, so a session replays exactly.
void dfc_host_set_random_buffer(const uint8_t* data, size_t len);
void dfc_host_clear_random_buffer(void);
// Octets still queued. Zero after a session consumed everything it was given.
size_t dfc_host_random_remaining(void);
