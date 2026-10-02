#include "maze3d.h"
#include <gui/canvas.h>
#include <math.h>

GameState g;

static inline uint8_t map_at(int x, int y) {
    if((unsigned)x >= (unsigned)g.map_w || (unsigned)y >= (unsigned)g.map_h) return WALL_BRICK;
    return g.map[(uint16_t)y * MAP_MAX + x];
}

// Compute per-column wall geometry into g.cols. Runs on the app thread.
void engine_compute(void) {
    Player* p = &g.player;
    const float posX = p->x, posY = p->y;
    const float dirX = p->dir_x, dirY = p->dir_y;
    const float planeX = p->plane_x, planeY = p->plane_y;

    for(int ci = 0; ci < RENDER_COLS; ci++) {
        ColData* col = &g.cols[ci];
        col->hit = false;

        // Camera X for the center of this column strip.
        float cx = (ci + 0.5f) / (float)RENDER_COLS * 2.0f - 1.0f;
        float rayX = dirX + planeX * cx;
        float rayY = dirY + planeY * cx;

        int mapX = (int)posX;
        int mapY = (int)posY;
        if(mapX < 0) mapX = 0;
        if(mapY < 0) mapY = 0;
        if(mapX >= g.map_w) mapX = g.map_w - 1;
        if(mapY >= g.map_h) mapY = g.map_h - 1;

        float deltaX = (fabsf(rayX) < 1e-6f) ? 1e30f : fabsf(1.0f / rayX);
        float deltaY = (fabsf(rayY) < 1e-6f) ? 1e30f : fabsf(1.0f / rayY);

        int stepX, stepY;
        float sideX, sideY;
        if(rayX < 0) {
            stepX = -1;
            sideX = (posX - mapX) * deltaX;
        } else {
            stepX = 1;
            sideX = (mapX + 1.0f - posX) * deltaX;
        }
        if(rayY < 0) {
            stepY = -1;
            sideY = (posY - mapY) * deltaY;
        } else {
            stepY = 1;
            sideY = (mapY + 1.0f - posY) * deltaY;
        }

        int side = 0;
        bool hit = false;
        for(int i = 0; i < MAP_MAX + 4 && !hit; i++) {
            if(sideX < sideY) {
                sideX += deltaX;
                mapX += stepX;
                side = 0;
            } else {
                sideY += deltaY;
                mapY += stepY;
                side = 1;
            }
            if(map_at(mapX, mapY) == WALL_BRICK) {
                hit = true;
                break;
            }
        }

        if(!hit) {
            // Open sky: full ceiling + floor split.
            col->hit = false;
            col->wall_top = SCREEN_H / 2;
            col->wall_bot = SCREEN_H / 2;
            col->shade = 4;
            continue;
        }

        float perp;
        if(side == 0)
            perp = sideX - deltaX;
        else
            perp = sideY - deltaY;
        if(perp < 0.01f) perp = 0.01f;

        int lineH = (int)((float)SCREEN_H / perp);
        if(lineH < 1) lineH = 1;
        int drawStart = -lineH / 2 + SCREEN_H / 2;
        int drawEnd = lineH / 2 + SCREEN_H / 2;
        if(drawStart < 0) drawStart = 0;
        if(drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

        // Distance shading 0..4.
        int s;
        if(perp < 1.5f)
            s = 0;
        else if(perp < 3.0f)
            s = 1;
        else if(perp < 5.0f)
            s = 2;
        else if(perp < 8.0f)
            s = 3;
        else
            s = 4;
        if(side == 1) s++;
        if(s > 4) s = 4;

        col->hit = true;
        col->wall_top = (uint8_t)drawStart;
        col->wall_bot = (uint8_t)drawEnd;
        col->shade = (uint8_t)s;
    }
}

// Draw ceiling, walls, floor directly with canvas primitives. Runs on GUI thread.
//
// Strategy to stay well under the GUI watchdog budget (~80 canvas calls/frame):
//   * canvas is cleared to white by the caller
//   * for each column we draw exactly two solid black boxes: ceiling (top)
//     and floor (bottom). The gap between them is the white wall.
//   * no per-line stripes, no per-pixel floor dots.
// This keeps walls readable (white bars on black) while using ~64 box calls.
void engine_draw(Canvas* canvas) {
    const int col_w = SCREEN_W / RENDER_COLS; // 4px per column

    for(int ci = 0; ci < RENDER_COLS; ci++) {
        ColData* col = &g.cols[ci];
        int x = ci * col_w;
        int w = col_w;

        int top = col->wall_top;
        int bot = col->wall_bot;

        // Ceiling: black from y=0 down to the wall top.
        if(top > 0) {
            canvas_draw_box(canvas, x, 0, w, top);
        }

        // Floor: black from just below the wall to the bottom of the screen.
        int floor_y = bot + 1;
        if(floor_y < SCREEN_H) {
            canvas_draw_box(canvas, x, floor_y, w, SCREEN_H - floor_y);
        }

        // Far walls get a single black hairline across their middle so distance
        // is still readable without per-line dithering.
        if(col->shade >= 3) {
            int mid = (top + bot) / 2;
            canvas_draw_box(canvas, x, mid, w, 1);
        }
    }

    // Exit direction arrow (simple on-screen indicator).
    if(g.exit_found && (g.tick & 15) < 10) {
        float spx = (float)g.exit_x + 0.5f - g.player.x;
        float spy = (float)g.exit_y + 0.5f - g.player.y;
        float invDet =
            1.0f / (g.player.plane_x * g.player.dir_y - g.player.dir_x * g.player.plane_y);
        float transY = invDet * (-g.player.plane_y * spx + g.player.plane_x * spy);

        int cy = 14;
        if(transY > 0.05f) {
            float transX = invDet * (g.player.dir_y * spx - g.player.dir_x * spy);
            int ax = (int)((SCREEN_W / 2.0f) * (1.0f + transX / transY));
            if(ax < 6) ax = 6;
            if(ax > SCREEN_W - 7) ax = SCREEN_W - 7;
            for(int i = 0; i <= 5; i++) {
                canvas_draw_dot(canvas, ax - i, cy - 5 + i);
                canvas_draw_dot(canvas, ax + i, cy - 5 + i);
            }
            canvas_draw_dot(canvas, ax, cy - 6);
            for(int i = -2; i <= 2; i++)
                canvas_draw_dot(canvas, ax, cy + i);
        } else {
            for(int i = 0; i < 4; i++) {
                canvas_draw_dot(canvas, 2 + i, cy - i);
                canvas_draw_dot(canvas, 2 + i, cy + i);
                canvas_draw_dot(canvas, SCREEN_W - 3 - i, cy - i);
                canvas_draw_dot(canvas, SCREEN_W - 3 - i, cy + i);
            }
        }
    }
}
