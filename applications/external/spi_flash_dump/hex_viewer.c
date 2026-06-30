/* hex_viewer.c — Custom View that displays a scrollable hex+ASCII dump.
 * Shows 4 bytes per row on the 128x64 display, loads the first 4 KB of a file. */

#include "hex_viewer.h"
#include "spi_flash_dump.h"

#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Layout constants                                                  */
/* ------------------------------------------------------------------ */
#define BYTES_PER_ROW  4
#define CHAR_W         6 /* monospace glyph width  */
#define CHAR_H         10 /* line height            */
#define SCREEN_W       128
#define SCREEN_H       64
#define ROWS_ON_SCREEN (SCREEN_H / CHAR_H) /* 6 visible rows */

/* ------------------------------------------------------------------ */
/*  Model held inside the View                                        */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t data[HEX_PREVIEW_SIZE];
    uint32_t data_len;
    uint32_t scroll_offset; /* in rows */
} HexViewerModel;

struct HexViewer {
    View* view;
};

/* ------------------------------------------------------------------ */
/*  Draw callback                                                     */
/* ------------------------------------------------------------------ */

static void hex_viewer_draw_cb(Canvas* canvas, void* model_ptr) {
    HexViewerModel* m = model_ptr;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontKeyboard); /* small mono font */

    uint32_t total_rows = (m->data_len + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
    if(total_rows == 0) {
        canvas_draw_str(canvas, 2, 12, "No data loaded");
        return;
    }

    for(uint32_t screen_row = 0; screen_row < ROWS_ON_SCREEN; screen_row++) {
        uint32_t data_row = m->scroll_offset + screen_row;
        if(data_row >= total_rows) break;

        uint32_t addr = data_row * BYTES_PER_ROW;
        int y = (int)(screen_row * CHAR_H) + CHAR_H; /* baseline */

        /* Address column: 4 hex digits */
        char line[64];
        snprintf(line, sizeof(line), "%04lX:", (unsigned long)addr);
        canvas_draw_str(canvas, 0, y, line);

        /* Hex bytes */
        int hex_x = 5 * CHAR_W + 1; /* after "XXXX:" */
        for(uint32_t col = 0; col < BYTES_PER_ROW; col++) {
            uint32_t offset = addr + col;
            if(offset < m->data_len) {
                snprintf(line, sizeof(line), "%02X", m->data[offset]);
            } else {
                snprintf(line, sizeof(line), "  ");
            }
            canvas_draw_str(canvas, hex_x + (int)col * 3 * CHAR_W, y, line);
        }

        /* ASCII column */
        int ascii_x = SCREEN_W - BYTES_PER_ROW * CHAR_W - 1;
        char ascii[BYTES_PER_ROW + 1];
        for(uint32_t col = 0; col < BYTES_PER_ROW; col++) {
            uint32_t offset = addr + col;
            if(offset < m->data_len) {
                uint8_t ch = m->data[offset];
                ascii[col] = (ch >= 0x20 && ch <= 0x7E) ? (char)ch : '.';
            } else {
                ascii[col] = ' ';
            }
        }
        ascii[BYTES_PER_ROW] = '\0';
        canvas_draw_str(canvas, ascii_x, y, ascii);
    }

    /* Scrollbar indicator */
    if(total_rows > ROWS_ON_SCREEN) {
        uint32_t bar_h = SCREEN_H * ROWS_ON_SCREEN / total_rows;
        if(bar_h < 4) bar_h = 4;
        uint32_t bar_y = (SCREEN_H - bar_h) * m->scroll_offset / (total_rows - ROWS_ON_SCREEN);
        canvas_draw_box(canvas, SCREEN_W - 2, (int)bar_y, 2, (int)bar_h);
    }
}

/* ------------------------------------------------------------------ */
/*  Input callback                                                    */
/* ------------------------------------------------------------------ */

static bool hex_viewer_input_cb(InputEvent* event, void* ctx) {
    View* view = ctx;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    if(event->key == InputKeyUp) {
        with_view_model(
            view,
            HexViewerModel * m,
            {
                if(m->scroll_offset > 0) m->scroll_offset--;
            },
            true);
        return true;
    }

    if(event->key == InputKeyDown) {
        with_view_model(
            view,
            HexViewerModel * m,
            {
                uint32_t total_rows = (m->data_len + BYTES_PER_ROW - 1) / BYTES_PER_ROW;
                if(total_rows > ROWS_ON_SCREEN && m->scroll_offset < total_rows - ROWS_ON_SCREEN) {
                    m->scroll_offset++;
                }
            },
            true);
        return true;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/*  Alloc / free                                                      */
/* ------------------------------------------------------------------ */

HexViewer* hex_viewer_alloc(void) {
    HexViewer* hv = malloc(sizeof(HexViewer));
    furi_assert(hv);
    hv->view = view_alloc();
    view_allocate_model(hv->view, ViewModelTypeLocking, sizeof(HexViewerModel));
    view_set_draw_callback(hv->view, hex_viewer_draw_cb);
    view_set_input_callback(hv->view, hex_viewer_input_cb);
    view_set_context(hv->view, hv->view);

    /* Zero-init model */
    with_view_model(
        hv->view,
        HexViewerModel * m,
        {
            memset(m->data, 0, sizeof(m->data));
            m->data_len = 0;
            m->scroll_offset = 0;
        },
        false);

    return hv;
}

void hex_viewer_free(HexViewer* hv) {
    if(!hv) return;
    view_free(hv->view);
    free(hv);
}

View* hex_viewer_get_view(HexViewer* hv) {
    return hv->view;
}

/* ------------------------------------------------------------------ */
/*  Load file data                                                    */
/* ------------------------------------------------------------------ */

uint32_t hex_viewer_load_file(HexViewer* hv, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    uint32_t loaded = 0;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        with_view_model(
            hv->view,
            HexViewerModel * m,
            {
                uint16_t rd = storage_file_read(file, m->data, (uint16_t)HEX_PREVIEW_SIZE);
                m->data_len = rd;
                m->scroll_offset = 0;
                loaded = rd;
            },
            true);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}
