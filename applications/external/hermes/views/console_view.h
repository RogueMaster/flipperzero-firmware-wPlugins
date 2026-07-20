#pragma once

#include <gui/view.h>
#include "../helpers/term.h"

typedef enum {
    ConsoleEventTypeText, // user wants the keyboard
    ConsoleEventTypeCtrl, // user wants the control-key palette
    ConsoleEventTypeEnter, // user pressed OK long: send the Enter sequence
} ConsoleEventType;

typedef struct ConsoleView ConsoleView;

typedef void (*ConsoleViewCallback)(void* context, ConsoleEventType type);

ConsoleView* console_view_alloc(void);
void console_view_free(ConsoleView* cv);
View* console_view_get_view(ConsoleView* cv);

void console_view_set_callback(ConsoleView* cv, ConsoleViewCallback cb, void* context);

/** The terminal to render. Borrowed, not owned; must outlive the view's use. */
void console_view_set_term(ConsoleView* cv, Term* term);

/** Status line contents. */
void console_view_set_link(ConsoleView* cv, uint32_t baud, const char* framing, bool tx_enabled);

/** Whether a capture is running, for the REC dot in the status bar. */
void console_view_set_logging(ConsoleView* cv, bool logging);

/** Seconds left in an autoboot burst, 0 when idle. */
void console_view_set_autoboot(ConsoleView* cv, uint32_t seconds_left);

/** Link health: framing errors, and how many times a watch has matched. */
void console_view_set_health(ConsoleView* cv, uint32_t errors, uint32_t trigger_hits);

/** The armed watch pattern, or "" when disarmed. */
void console_view_set_watch(ConsoleView* cv, const char* pattern);

/** Script playback progress. */
void console_view_set_script(ConsoleView* cv, bool running, uint16_t sent, uint16_t total);

/** Nudge the view after feeding the term, so it follows new output. */
void console_view_notify_rx(ConsoleView* cv);
