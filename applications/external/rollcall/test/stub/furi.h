/* Host stub. Just enough of furi.h for the pure logic files to compile off
 * the Flipper - no threads, no HAL, no GUI. */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_assert(expr)  ((void)0)
#define furi_check(expr)   ((void)0)
#define UNUSED(x)          ((void)(x))

#define FURI_LOG_D(tag, ...) ((void)0)
#define FURI_LOG_I(tag, ...) ((void)0)
#define FURI_LOG_W(tag, ...) ((void)0)
#define FURI_LOG_E(tag, ...) ((void)0)
