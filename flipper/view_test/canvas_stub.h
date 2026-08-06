#pragma once

#include <gui/canvas.h>

typedef enum {
    TpmsItemText,
    TpmsItemShape,
} TpmsItemKind;

typedef struct {
    TpmsItemKind kind;
    int32_t x, y; /**< top left corner of the occupied rectangle */
    int32_t w, h;
    char text[64];
} TpmsDrawItem;

/** How many primitives were drawn since the last canvas_clear(). */
size_t tpms_canvas_items(void);
const TpmsDrawItem* tpms_canvas_item(size_t index);

/** ASCII picture of the screen: the rows that have something on them. */
void tpms_canvas_print(const char* title);

/** Layout checks. Returns the number of problems found. */
int tpms_canvas_check(const char* title);
