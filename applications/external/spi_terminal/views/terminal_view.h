#pragma once

#include <gui/view.h>
#include <gui/scene_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

/** TextBox anonymous structure */
typedef struct TerminalView TerminalView;

typedef enum {
    TerminalDisplayModeAuto,
    TerminalDisplayModeText,
    TerminalDisplayModeHex,
    TerminalDisplayModeBinary,

    TerminalDisplayModeMax,
} TerminalDisplayMode;

TerminalView* terminal_view_alloc();
View* terminal_view_get_view(TerminalView* terminal);
void terminal_view_free(TerminalView* terminal);
void terminal_view_reset(TerminalView* terminal);
void terminal_view_set_display_mode(TerminalView* terminal, TerminalDisplayMode mode);
void terminal_view_append_data_from_stream(TerminalView* terminal, FuriStreamBuffer* buffer);
void terminal_view_debug_print_buffer(TerminalView* view);

#ifdef __cplusplus
}
#endif
