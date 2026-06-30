#include "game/icon.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

IconGroupContext *g_current_icon_group = NULL;

// ---------------------------------------------------------------------
// Icon Group Entity Implementation
// ---------------------------------------------------------------------

// Render callback: iterate over the group and draw each icon.
static void icon_group_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    IconGroupContext *igctx = (IconGroupContext *)context;
    if (!igctx)
    {
        FURI_LOG_E("Game", "Icon group context is NULL in render");
        return;
    }
    for (int i = 0; i < igctx->count; i++)
    {
        IconSpec *spec = &igctx->icons[i];
        int x_pos = spec->pos.x - draw_camera_x - (spec->size.x / 2);
        int y_pos = spec->pos.y - draw_camera_y - (spec->size.y / 2);
        if (x_pos + spec->size.x < 0 || x_pos > SCREEN_WIDTH ||
            y_pos + spec->size.y < 0 || y_pos > SCREEN_HEIGHT)
        {
            continue;
        }
        canvas_draw_icon(canvas, x_pos, y_pos, spec->icon);
    }
}

// Start callback: nothing special is needed here.
static void icon_group_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    UNUSED(context);
    // The context is assumed to be pre-filled by the world code.
}

// Free callback: free the icon specs array.
static void icon_group_free(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    IconGroupContext *igctx = (IconGroupContext *)context;
    if (igctx && igctx->icons)
    {
        free(igctx->icons);
        igctx->icons = NULL;
    }
    g_current_icon_group = NULL;
}

// The entity description for the icon group.
// Note: we set context_size to sizeof(IconGroupContext).
const EntityDescription icon_desc = {
    .start = icon_group_start,
    .stop = icon_group_free,
    .update = NULL,
    .render = icon_group_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(IconGroupContext),
};
