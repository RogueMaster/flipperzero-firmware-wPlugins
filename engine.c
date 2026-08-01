#include "maze3d.h"
#include <math.h>

// 全局状态
GameState g;

// ---- Framebuffer 操作 ----
static inline void fb_set(int x, int y, uint8_t on) {
    if(x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    uint16_t idx = (y * (SCREEN_W / 8)) + (x >> 3);
    uint8_t bit = 1 << (x & 7);
    if(on) g.fb[idx] |= bit;
    else g.fb[idx] &= ~bit;
}

static inline void fb_clear(void) {
    for(int i = 0; i < FB_BYTES; i++) g.fb[i] = 0;
}

// ---- 地图访问 ----
static inline uint8_t map_at(int x, int y) {
    if(x < 0 || y < 0 || x >= g.map_w || y >= g.map_h) return WALL_BRICK;
    return g.map[y * MAP_MAX + x];
}

static inline bool is_wall(uint8_t c) {
    return c == WALL_BRICK || c == WALL_STONE || c == WALL_METAL || c == WALL_VINE || c == CELL_DOOR;
}

// 取贴图ID: 不同墙类型不同贴图
static inline int wall_tex_id(uint8_t c) {
    switch(c) {
        case WALL_BRICK: return 0;
        case WALL_STONE: return 1;
        case WALL_METAL: return 2;
        case WALL_VINE:  return 3;
        case CELL_DOOR:  return 2; // 门用金属贴图
        default:         return 0;
    }
}

// 外部声明(在 textures.c 里)
extern uint8_t texture_sample(int tex_id, int tx, int ty);

// ---- 地板/天花板抖动 pattern ----
// 根据 y 行号返回该行地板像素是否亮(棋盘+距离稀疏)
static inline uint8_t floor_pixel(int x, int y) {
    // y 越大(越靠下/近)越密
    int density = (y - SCREEN_H / 2);
    if(density <= 0) return 0;
    // 棋盘格抖动, 越远越稀疏
    if(density < 6) {
        // 远处地板: 稀疏点
        return ((x ^ y) & 3) == 0 ? 1 : 0;
    } else if(density < 16) {
        return ((x ^ y) & 1) ? 1 : 0;
    } else {
        // 近处地板: 棋盘格
        return (((x >> 1) ^ (y >> 1)) & 1) ? 1 : 0;
    }
}

static inline uint8_t ceiling_pixel(int x, int y) {
    // 天花板: 稀疏, 越靠上越稀
    int d = (SCREEN_H / 2) - y;
    if(d <= 0) return 0;
    if(d < 6) return ((x + y) & 3) == 0 ? 1 : 0;
    return 0; // 高处全黑
}

// ---- Raycasting 渲染 ----
void engine_render(void) {
    fb_clear();

    Player* p = &g.player;
    float posX = p->x, posY = p->y;
    float dirX = p->dir_x, dirY = p->dir_y;
    float planeX = p->plane_x, planeY = p->plane_y;

    for(int x = 0; x < SCREEN_W; x++) {
        // 摄像机列对应的光线方向 (-1..1)
        float cameraX = 2.0f * x / (float)SCREEN_W - 1.0f;
        float rayX = dirX + planeX * cameraX;
        float rayY = dirY + planeY * cameraX;

        int mapX = (int)posX;
        int mapY = (int)posY;

        float deltaX = (rayX == 0) ? 1e30f : fabsf(1.0f / rayX);
        float deltaY = (rayY == 0) ? 1e30f : fabsf(1.0f / rayY);

        int stepX, stepY;
        float sideX, sideY;
        if(rayX < 0) { stepX = -1; sideX = (posX - mapX) * deltaX; }
        else { stepX = 1; sideX = (mapX + 1.0f - posX) * deltaX; }
        if(rayY < 0) { stepY = -1; sideY = (posY - mapY) * deltaY; }
        else { stepY = 1; sideY = (mapY + 1.0f - posY) * deltaY; }

        int side = 0; // 0=南北墙 1=东西墙
        uint8_t hit = 0;
        int guard = 0;
        while(!hit && guard++ < 64) {
            if(sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
            else { sideY += deltaY; mapY += stepY; side = 1; }
            hit = map_at(mapX, mapY);
            if(!is_wall(hit)) {
                // 命中出口/道具格不中断, 继续找墙(但记录出口用于高亮)
                hit = 0;
            } else {
                break;
            }
        }
        if(!hit) continue;

        // 垂直距离(防除0)
        float perp;
        if(side == 0) perp = sideX - deltaX;
        else perp = sideY - deltaY;
        if(perp < 0.01f) perp = 0.01f;

        int lineH = (int)(SCREEN_H / perp);
        if(lineH < 0) lineH = 0;
        int drawStart = -lineH / 2 + SCREEN_H / 2;
        int drawEnd = lineH / 2 + SCREEN_H / 2;
        if(drawStart < 0) drawStart = 0;
        if(drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

        // 墙上的命中坐标(用于贴图采样)
        float wallX;
        if(side == 0) wallX = posY + perp * rayY;
        else wallX = posX + perp * rayX;
        wallX -= floorf(wallX);

        int texX = (int)(wallX * 8.0f);
        if(side == 0 && rayX > 0) texX = 7 - texX;
        if(side == 1 && rayY < 0) texX = 7 - texX;

        int texId = wall_tex_id(hit);

        // 距离衰减: 越远越暗(单色=像素稀疏)
        // perp<3 全亮, >10 极稀疏
        int shade;
        if(perp < 2.0f) shade = 0;       // 全亮
        else if(perp < 4.0f) shade = 1;  // 轻微稀疏
        else if(perp < 7.0f) shade = 2;  // 中等稀疏
        else shade = 3;                  // 极稀疏

        // 墙朝向明暗: 东西墙(side=1)比南北墙暗一档
        if(side == 1 && shade < 3) shade++;

        // 绘制墙列
        for(int y = drawStart; y <= drawEnd; y++) {
            int texY = ((y - (-lineH / 2 + SCREEN_H / 2)) * 8) / lineH;
            if(texY < 0) texY = 0;
            if(texY > 7) texY = 7;
            uint8_t px = texture_sample(texId, texX, texY);
            if(!px) continue; // 贴图该位为暗,跳过
            // 应用距离稀疏
            switch(shade) {
                case 0: fb_set(x, y, 1); break;
                case 1: if((x ^ y) & 1) fb_set(x, y, 1); break;
                case 2: if(((x + y) & 3) == 0) fb_set(x, y, 1); break;
                case 3: if(((x * 7 + y * 3) & 7) == 0) fb_set(x, y, 1); break;
            }
        }

        // 地板 (drawEnd+1 .. SCREEN_H-1)
        for(int y = drawEnd + 1; y < SCREEN_H; y++) {
            fb_set(x, y, floor_pixel(x, y));
        }
        // 天花板 (0 .. drawStart-1)
        for(int y = 0; y < drawStart; y++) {
            fb_set(x, y, ceiling_pixel(x, y));
        }
    }

    // 出口高亮: 在出口位置画一个十字标记(俯视投影简化版)
    // 找地图里的出口格,若在视野内则在屏幕上画一个闪烁方块
    for(int my = 0; my < g.map_h; my++) {
        for(int mx = 0; mx < g.map_w; mx++) {
            if(g.map[my * MAP_MAX + mx] == CELL_EXIT) {
                // 相对玩家方向投影(简化:仅在玩家朝向附近显示提示)
                float dx = mx + 0.5f - posX;
                float dy = my + 0.5f - posY;
                float dist = sqrtf(dx * dx + dy * dy);
                if(dist < 12.0f) {
                    // 在屏幕顶部画一个朝向指示
                    float dot = dirX * dx + dirY * dy;
                    if(dot > 0) {
                        // 出口在前方,屏幕中央顶部画提示
                        int sz = 3;
                        for(int i = -sz; i <= sz; i++)
                            for(int j = -sz; j <= sz; j++)
                                fb_set(SCREEN_W / 2 + i, 4 + j, 1);
                    }
                }
                break;
            }
        }
    }
}
