#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Live serial console: a scrolling ring buffer of text lines streamed back from
 * the Marauder board, with a branded header (op title + link status) and a
 * footer hint bar. New lines are pushed from the UART worker thread; the owning
 * scene calls console_view_tick() ~10x/s to repaint and animate.
 */

typedef struct ConsoleView ConsoleView;
typedef void (*ConsoleViewCallback)(void* context);

ConsoleView* console_view_alloc(void);
void console_view_free(ConsoleView* v);
View* console_view_get_view(ConsoleView* v);

// OK button (used by the scene to open the raw-command sender)
void console_view_set_ok_callback(ConsoleView* v, ConsoleViewCallback cb, void* context);

void console_view_clear(ConsoleView* v);
void console_view_push_line(ConsoleView* v, const char* line); // worker thread
void console_view_set_header(ConsoleView* v, const char* title);
void console_view_set_channel(ConsoleView* v, const char* chan); // e.g. "13/14"
void console_view_set_live(ConsoleView* v, bool live);
void console_view_set_autoscroll(ConsoleView* v, bool autoscroll);
void console_view_tick(ConsoleView* v); // repaint + advance animation
