#pragma once

#include <gui/view.h>
#include <input/input.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Live text console: a scrolling ring buffer of lines with a branded header
 * (title + live status) and a configurable footer. Used for the ESP32 serial
 * console (lines from the UART worker) and the NRF24 sniffer (lines from the
 * NRF24 worker). The owning scene calls console_view_tick() ~10x/s to repaint.
 */

typedef struct ConsoleView ConsoleView;
typedef void (*ConsoleViewCallback)(void* context);
typedef void (*ConsoleViewKeyCallback)(void* context, InputKey key);

ConsoleView* console_view_alloc(void);
void console_view_free(ConsoleView* v);
View* console_view_get_view(ConsoleView* v);

// OK button (e.g. open the raw-command sender, or clear the sniffer log)
void console_view_set_ok_callback(ConsoleView* v, ConsoleViewCallback cb, void* context);
// Left / Right forwarded here when set (e.g. change the sniffer channel)
void console_view_set_key_callback(ConsoleView* v, ConsoleViewKeyCallback cb, void* context);

void console_view_clear(ConsoleView* v);
void console_view_push_line(ConsoleView* v, const char* line); // worker thread
void console_view_set_header(ConsoleView* v, const char* title);
void console_view_set_channel(ConsoleView* v, const char* chan); // right footer "UART <chan>"
void console_view_set_footer_left(ConsoleView* v, const char* text); // default "OK:cmd"
void console_view_set_footer_right(ConsoleView* v, const char* text); // overrides UART/chan
void console_view_set_empty(ConsoleView* v, const char* l1, const char* l2);
void console_view_set_live(ConsoleView* v, bool live);
void console_view_set_autoscroll(ConsoleView* v, bool autoscroll);
void console_view_tick(ConsoleView* v); // repaint + advance animation
