#pragma once

#include <gui/canvas.h>

#define DND_SPLASH_IMAGE_WIDTH    128U
#define DND_SPLASH_IMAGE_HEIGHT   64U
#define DND_SPLASH_IMAGE_BYTES    1024U
#define DND_SPLASH_IMAGE_COUNT    15U
#define DND_SPLASH_IMAGE_HUB_ROOT "/ext/apps_assets/dndolphins/loading"
#define DND_SPLASH_IMAGE_FAL_ROOT "/ext/apps_assets/dnd_loading/splashes"

typedef struct DndSplashImage DndSplashImage;

/* Acquire/release only from the serialized DND app thread. All overlapping
 * FAP/FAL instances share one immutable bitmap; callbacks only borrow it. */
DndSplashImage* dnd_splash_image_acquire(const char* asset_root);
void dnd_splash_image_draw(Canvas* canvas, const DndSplashImage* image);
void dnd_splash_image_release(DndSplashImage* image);
