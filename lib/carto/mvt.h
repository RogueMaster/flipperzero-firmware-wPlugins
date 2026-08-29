#ifndef CARTO_MVT_H
#define CARTO_MVT_H

#include "carto/framebuffer.h"
#include "carto/style.h"
#include "carto/raster.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void carto_mvt_render_category(carto_framebuffer *fb, const carto_style *style,
                              const uint8_t *tile, size_t len,
                              carto_layer_kind category,
                              float ox, float oy, float tile_px,
                              int zoom,
                              carto_ipt *scratch, int scratch_cap);

#ifdef __cplusplus
}
#endif

#endif
