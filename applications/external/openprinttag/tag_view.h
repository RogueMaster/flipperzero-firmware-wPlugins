#pragma once

#include <stdint.h>
#include <gui/view.h>

/**
 * Screen that shows what was read from an OpenPrintTag, in three pages (left/right switches):
 *   Summary         material, brand, type, remaining weight with a bar, nozzle and bed range
 *   Details         scrolling list of temperatures, weights, lengths, density, ...
 *   Identification  scrolling list of brand, material, type, GTIN, UUID, dates, tag UID
 *
 * Fill it with openprinttag_tag_view_set_data() (declared in openprinttag_i.h, which has the data
 * types) before showing the view.
 */

typedef struct TagView TagView;

TagView* tag_view_alloc(void);

void tag_view_free(TagView* tag_view);

View* tag_view_get_view(TagView* tag_view);
