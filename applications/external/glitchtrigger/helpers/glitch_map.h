#pragma once

#include <stdint.h>
#include <stdbool.h>

/* A fault map: the delay x width search grid with a bit set for every cell that
 * produced a hit during a sweep. Rendered as a heatmap and exportable to CSV so
 * you can see the fault window take shape. */

#define GLITCH_MAP_W     100 // max width-axis cells
#define GLITCH_MAP_H     44 // max delay-axis cells
#define GLITCH_MAP_BYTES ((GLITCH_MAP_W * GLITCH_MAP_H + 7) / 8)

typedef struct {
    uint8_t bits[GLITCH_MAP_BYTES];
    uint16_t cols; // active width cells  (0 = no data captured yet)
    uint16_t rows; // active delay cells  (1 for a 1D sweep)
    bool is_2d;
    uint32_t w_from, w_to; // width range the columns span (ns)
    uint32_t d_from, d_to; // delay range the rows span (us)
    uint32_t hits; // distinct hit cells set
} GlitchFaultMap;

/* Start a fresh map over the given grid dimensions and axis ranges. */
void glitch_map_reset(
    GlitchFaultMap* m,
    uint16_t cols,
    uint16_t rows,
    bool is_2d,
    uint32_t w_from,
    uint32_t w_to,
    uint32_t d_from,
    uint32_t d_to);

void glitch_map_set(GlitchFaultMap* m, uint16_t col, uint16_t row);
bool glitch_map_get(const GlitchFaultMap* m, uint16_t col, uint16_t row);

/* Value at a cell, in the axis' natural units (ns for width, us for delay). */
uint32_t glitch_map_col_width(const GlitchFaultMap* m, uint16_t col);
uint32_t glitch_map_row_delay(const GlitchFaultMap* m, uint16_t row);

/* Write the grid to /ext/apps_data/glitch_trigger/faultmap.csv. */
bool glitch_map_export(const GlitchFaultMap* m);
