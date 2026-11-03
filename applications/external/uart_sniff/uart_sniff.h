/**
 * uart_sniff.h — Shared types and app state for UART Sniff FAP.
 */
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "uart_sniff_worker.h"

/* -----------------------------------------------------------------------
 * View IDs
 * ----------------------------------------------------------------------- */
typedef enum {
    UartSniffViewMenu,
    UartSniffViewSniff,
    UartSniffViewSettings,
    UartSniffViewCount,
} UartSniffView;

/* -----------------------------------------------------------------------
 * Baud rate options
 * ----------------------------------------------------------------------- */
typedef enum {
    UartSniffBaud9600 = 0,
    UartSniffBaud19200,
    UartSniffBaud38400,
    UartSniffBaud57600,
    UartSniffBaud115200,
    UartSniffBaud230400,
    UartSniffBaudCount,
} UartSniffBaud;

/* -----------------------------------------------------------------------
 * Display mode
 * ----------------------------------------------------------------------- */
typedef enum {
    UartSniffShowHex = 0,
    UartSniffShowAscii,
    UartSniffShowBoth,
    UartSniffShowCount,
} UartSniffShow;

/* -----------------------------------------------------------------------
 * Main menu items
 * ----------------------------------------------------------------------- */
typedef enum {
    UartSniffMenuStart = 0,
    UartSniffMenuSettings,
    UartSniffMenuClear,
} UartSniffMenuItem;

/* -----------------------------------------------------------------------
 * Custom events dispatched via view_dispatcher_send_custom_event()
 * ----------------------------------------------------------------------- */
typedef enum {
    UartSniffEventRefresh = 0,
} UartSniffEvent;

/* -----------------------------------------------------------------------
 * Application state — heap-allocated to keep off the FAP stack.
 * ----------------------------------------------------------------------- */
typedef struct {
    /* GUI */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Submenu* menu;
    TextBox* text_box;
    VariableItemList* settings_list;

    /* Worker (NULL when not sniffing) */
    UartSniffWorker* worker;

    /* Refresh timer */
    FuriTimer* refresh_timer;

    /* Display text buffers — heap-allocated, double-buffered.  The TextBox
     * stores the raw pointer and is drawn on the GUI service thread, so the
     * buffer being (re)formatted must never be the one the TextBox currently
     * points at.  Each refresh formats into display_buf[idx^1] then hands that
     * to the TextBox, leaving the live buffer untouched. */
    char* display_buf[2];
    uint8_t display_idx;
    size_t display_buf_size;

    /* Capture buffer for reading from ring — heap-allocated */
    uint8_t* capture_buf;

    /* Settings */
    UartSniffBaud baud_idx;
    FuriHalSerialId serial_id; /* USART or LPUART */
    UartSniffShow show_mode;

    /* Which view is currently shown (for gating the refresh). */
    UartSniffView current_view;

    /* State — volatile: GUI thread writes, timer daemon reads */
    volatile bool sniffing;
} UartSniffApp;

/* -----------------------------------------------------------------------
 * Constants
 * ----------------------------------------------------------------------- */

/* Lines kept in the rolling display (each line = 8 bytes of data) */
#define UART_SNIFF_DISPLAY_LINES 32u

/* Bytes per display line */
#define UART_SNIFF_BYTES_PER_LINE 8u

/* Total displayed bytes = 32 * 8 = 256 */
#define UART_SNIFF_DISPLAY_BYTES (UART_SNIFF_DISPLAY_LINES * UART_SNIFF_BYTES_PER_LINE)

/* Upper bound on chars per display line.  Worst case is "Both" mode:
 *   "XXXX: XX XX XX XX XX XX XX XX  ABCDEFGH\n"
 *   addr(4) + ": "(2) + 8*"XX "(24) + " "(1) + 8 ascii(8) + "\n"(1) = 40
 * The 48 here keeps ~8 chars of headroom so a format tweak cannot silently
 * overflow the per-line budget (the formatter is also bounds-checked). */
#define UART_SNIFF_LINE_MAX_CHARS 48u

/* Total display buffer: lines * chars-per-line + NUL */
#define UART_SNIFF_DISPLAY_BUF_SIZE (UART_SNIFF_DISPLAY_LINES * UART_SNIFF_LINE_MAX_CHARS + 1u)
