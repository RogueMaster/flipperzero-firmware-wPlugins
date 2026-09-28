/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdint.h>

/* Kept outside LTO so benchmark callers must materialize their results.
 * Reading one initialized byte avoids timing a second full buffer traversal. */
static volatile uint8_t sink;
void tc_benchmark_consume(const void* value)
{
  sink ^= *(const uint8_t*)value;
}
