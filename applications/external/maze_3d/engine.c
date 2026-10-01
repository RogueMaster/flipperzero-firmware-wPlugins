#include "maze3d.h"
#include <math.h>

GameState g;

// ---- Framebuffer helpers ----
static inline void fb_set(int x, int y, uint8_t on) {
    if((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return;
    uint16_t idx = ((uint16_t)y << 4) + ((uint16_t)x >> 3); // y*16 + x/8
    uint8_t bit = 1u << (x & 7);
    if(on)
        g.fb[idx] |= bit;
    else
        g.fb[idx] &= ~bit;
}

static inline void fb_clear(void) {
    for(int i = 0; i < FB_BYTES; i++)
        g.fb[i] = 0;
}

// ---- Map access ----
static inline uint8_t map_at(int x, int y) {
    if((unsigned)x >= (unsigned)g.map_w || (unsigned)y >= (unsigned)g.map_h) return WALL_BRICK;
    return g.map[(uint16_t)y * MAP_MAX + x];
}

static inline bool is_wall(uint8_t c) {
    return c == WALL_BRICK;
}

extern uint8_t texture_sample(int tex_id, int tx, int ty);

// Distance shading: farther walls are drawn with sparser patterns.
// shade level 0..4 (0 = solid/near, 4 = far).
static inline int shade_from(float perp, int side) {
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
    if(side == 1) s++; // Y-side walls are a touch darker for depth
    if(s > 4) s = 4;
    return s;
}

// Apply a shaded pixel: only set if the texture bit passes the dither for the
// current shade level. Keeps walls readable at all distances.
static inline void apply_shade_px(int x, int y, int shade) {
    if((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return;
    uint8_t on = 1;
    switch(shade) {
    case 1:
        on = ((x + y) & 1) ? 0 : 1;
        break;
    case 2:
        on = (((x >> 1) + (y >> 1)) & 1) ? 0 : 1;
        break;
    case 3:
        on = (((x >> 1) + (y >> 1)) % 3 == 0) ? 1 : 0;
        break;
    case 4:
        on = (((x >> 2) + (y >> 2)) & 1) ? 0 : 1;
        break;
    default:
        on = 1;
    }
    if(on) {
        uint16_t idx = ((uint16_t)y << 4) + ((uint16_t)x >> 3);
        uint8_t bit = 1u << (x & 7);
        g.fb[idx] |= bit;
    }
}

// Ceiling/floor pattern: simple checker/noise to give orientation cues.
static inline uint8_t ceil_px(int x, int y) {
    return (((x >> 3) + (y >> 2)) & 1) ? 1 : 0;
}
static inline uint8_t floor_px(int x, int y) {
    return (((x >> 2) + (y >> 3)) & 1) ? 0 : 1;
}

// ---- Main raycasting render ----
void engine_render(void) {
    fb_clear();

    Player* p = &g.player;
    const float posX = p->x, posY = p->y;
    const float dirX = p->dir_x, dirY = p->dir_y;
    const float planeX = p->plane_x, planeY = p->plane_y;

    // Render 64 columns; each covers 2 screen pixels.
    const int RENDER_COLS_DYN = RENDER_COLS;

    for(int ci = 0; ci < RENDER_COLS_DYN; ci++) {
        int px_start = (ci * SCREEN_W) / RENDER_COLS_DYN;
        int px_end = ((ci + 1) * SCREEN_W) / RENDER_COLS_DYN - 1;
        if(px_end < px_start) px_end = px_start;
        int px_ctr = (px_start + px_end) / 2;

        float cameraX = 2.0f * (float)(px_ctr + 0.5f) / (float)SCREEN_W - 1.0f;
        float rayX = dirX + planeX * cameraX;
        float rayY = dirY + planeY * cameraX;

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
        uint8_t hit = 0;
        bool exit_on_ray = false;
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
            uint8_t c = map_at(mapX, mapY);
            if(is_wall(c)) {
                hit = c;
                break;
            }
            if(c == CELL_EXIT) exit_on_ray = true;
        }

        if(!hit) {
            // No wall hit: draw ceiling/floor split at horizon.
            for(int y = 0; y < SCREEN_H; y++) {
                uint8_t on = (y < SCREEN_H / 2) ? ceil_px(px_ctr, y) : floor_px(px_ctr, y);
                for(int px = px_start; px <= px_end; px++)
                    fb_set(px, y, on);
            }
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

        // Wall hit coordinate for texture mapping.
        float wallX;
        if(side == 0)
            wallX = posY + perp * rayY;
        else
            wallX = posX + perp * rayX;
        wallX -= (float)((int)wallX);
        int texX = (int)(wallX * 8.0f);
        if(side == 0 && rayX > 0) texX = 7 - texX;
        if(side == 1 && rayY < 0) texX = 7 - texX;

        int shade = shade_from(perp, side);

        // Exit indicator: a blinking pixel above the exit wall column.
        if(exit_on_ray) {
            float dxm = mapX - posX, dym = mapY - posY;
            if(dxm * dxm + dym * dym < 36.0f && (g.tick & 7) < 4) {
                int yy = drawStart - 1;
                if(yy >= 0) fb_set(px_start, yy, 1);
            }
        }

        // Ceiling
        for(int y = 0; y < drawStart; y++) {
            uint8_t on = ceil_px(px_ctr, y);
            for(int px = px_start; px <= px_end; px++)
                fb_set(px, y, on);
        }
        // Wall (textured + shaded)
        int constHalf = -lineH / 2 + SCREEN_H / 2;
        for(int y = drawStart; y <= drawEnd; y++) {
            int texY = ((y - constHalf) * 8) / lineH;
            if(texY < 0)
                texY = 0;
            else if(texY > 7)
                texY = 7;
            if(texture_sample(0, texX, texY)) {
                for(int px = px_start; px <= px_end; px++)
                    apply_shade_px(px, y, shade);
            }
        }
        // Floor
        for(int y = drawEnd + 1; y < SCREEN_H; y++) {
            uint8_t on = floor_px(px_ctr, y);
            for(int px = px_start; px <= px_end; px++)
                fb_set(px, y, on);
        }
    }

    // Exit direction arrow: project the exit cell to screen space.
    if(g.exit_found) {
        float spx = (float)g.exit_x + 0.5f - g.player.x;
        float spy = (float)g.exit_y + 0.5f - g.player.y;
        float invDet =
            1.0f / (g.player.plane_x * g.player.dir_y - g.player.dir_x * g.player.plane_y);
        float transX = invDet * (g.player.dir_y * spx - g.player.dir_x * spy);
        float transY = invDet * (-g.player.plane_y * spx + g.player.plane_x * spy);

        if((g.tick & 15) < 10) {
            int cy = 14;
            if(transY > 0.05f) {
                int ax = (int)((SCREEN_W / 2.0f) * (1.0f + transX / transY));
                if(ax < 6) ax = 6;
                if(ax > SCREEN_W - 7) ax = SCREEN_W - 7;
                int as = 5;
                for(int i = 0; i <= as; i++) {
                    fb_set(ax - i, cy - as + i, 1);
                    fb_set(ax + i, cy - as + i, 1);
                }
                fb_set(ax, cy - as - 1, 1);
                for(int i = -2; i <= 2; i++)
                    fb_set(ax, cy + i, 1);
            } else {
                // Exit is behind: draw arrows on both sides.
                for(int i = 0; i < 4; i++) {
                    fb_set(2 + i, cy - i, 1);
                    fb_set(2 + i, cy + i, 1);
                    fb_set(SCREEN_W - 3 - i, cy - i, 1);
                    fb_set(SCREEN_W - 3 - i, cy + i, 1);
                }
            }
        }
    }
}
