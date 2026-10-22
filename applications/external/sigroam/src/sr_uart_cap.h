#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ One join plus the following upload. No furi.
 *
 * Caller holds the app mutex for arm, disarm, mark, note, and pull.
 * sr_uart_cap_armed() is safe from the worker before it takes that mutex.
 * This buffer does not decide Saved.
 */

void sr_uart_cap_arm(bool reset);
void sr_uart_cap_disarm(void);
bool sr_uart_cap_armed(void);
void sr_uart_cap_mark(const char* label);
void sr_uart_cap_note(const char* line, size_t len);

/* Copies out up to cap bytes. *reset is true once after arm(true), including when *out_n is 0. */
bool sr_uart_cap_pull(char* dst, size_t cap, size_t* out_n, bool* reset);
