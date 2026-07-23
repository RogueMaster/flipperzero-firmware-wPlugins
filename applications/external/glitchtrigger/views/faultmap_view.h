#pragma once

#include <gui/view.h>
#include "../helpers/glitch_map.h"

/* Renders a GlitchFaultMap as a heatmap: width on X, delay on Y, a dot per hit
 * cell. A movable cursor reads out the width/delay under it; OK exports the grid
 * to CSV. */

typedef struct FaultmapView FaultmapView;

typedef enum {
    FaultmapViewEventExport, // OK
} FaultmapViewEvent;

typedef void (*FaultmapViewCallback)(FaultmapViewEvent event, void* ctx);

FaultmapView* faultmap_view_alloc(void);
void faultmap_view_free(FaultmapView* v);
View* faultmap_view_get_view(FaultmapView* v);

void faultmap_view_set_callback(FaultmapView* v, FaultmapViewCallback cb, void* ctx);
void faultmap_view_set_map(FaultmapView* v, const GlitchFaultMap* map);
