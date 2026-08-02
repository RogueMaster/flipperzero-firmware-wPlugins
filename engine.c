#include "maze3d.h"
#include <math.h>

GameState g;

// ---- Framebuffer ----
static inline void fb_set(int x, int y, uint8_t on) {
    if((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return;
    uint16_t idx = ((uint16_t)y << 4) + ((uint16_t)x >> 3); // y*16 + x/8
    uint8_t bit = 1u << (x & 7);
    if(on) g.fb[idx] |= bit;
    else g.fb[idx] &= ~bit;
}

static inline void fb_hline(int x1, int x2, int y, uint8_t on) {
    if((unsigned)y >= SCREEN_H) return;
    if(x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if(x1 < 0) x1 = 0;
    if(x2 >= SCREEN_W) x2 = SCREEN_W - 1;
    for(int x = x1; x <= x2; x++) fb_set(x, y, on);
}

static inline void fb_clear(void) {
    for(int i = 0; i < FB_BYTES; i++) g.fb[i] = 0;
}

// 绘制 XBM 位图(预生成的中文字符)
void fb_blit_xbm(int x0, int y0, int w, int h, int bpr, const uint8_t* bits, uint8_t on) {
    for(int y = 0; y < h; y++) {
        if(y0 + y < 0 || y0 + y >= SCREEN_H) continue;
        for(int x = 0; x < w; x++) {
            uint8_t byte = bits[y * bpr + (x >> 3)];
            if(byte & (1 << (x & 7))) fb_set(x0 + x, y0 + y, on);
        }
    }
}

// ---- 地图访问 ----
static inline uint8_t map_at(int x, int y) {
    if((unsigned)x >= (unsigned)g.map_w || (unsigned)y >= (unsigned)g.map_h) return WALL_BRICK;
    return g.map[(uint16_t)y * MAP_MAX + x];
}

static inline bool is_wall(uint8_t c) {
    return c == WALL_BRICK || c == WALL_STONE || c == WALL_METAL ||
           c == WALL_VINE || c == CELL_DOOR;
}

static inline int wall_tex_id(uint8_t c) {
    switch(c) {
        case WALL_BRICK: return 0;
        case WALL_STONE: return 1;
        case WALL_METAL: return 2;
        case WALL_VINE:  return 3;
        case CELL_DOOR:  return 2;
        default:         return 0;
    }
}

extern uint8_t texture_sample(int tex_id, int tx, int ty);

// 距离/明暗处理: 0=最近(亮),3=最远(极稀疏)
// 避免 sqrtf,用 perp距离(已经是无透视失真的垂直距离)
static inline int shade_from(float perp, int side) {
    int s;
    if(perp < 2.0f) s = 0;
    else if(perp < 3.5f) s = 1;
    else if(perp < 6.0f) s = 2;
    else s = 3;
    if(side == 1 && s < 3) s++; // 东西墙暗一档
    return s;
}

static inline void apply_shade_px(int x, int y, int shade) {
    switch(shade) {
        case 0: fb_set(x, y, 1); break;
        case 1: if(((x >> 1) ^ (y >> 1)) & 1) fb_set(x, y, 1); break;
        case 2: if(((x + y * 3) & 7) == 0) fb_set(x, y, 1); break;
        case 3: if(((x * 5 + y * 7) & 15) == 0) fb_set(x, y, 1); break;
    }
}

// 地板/天花板抖动 pattern
static inline uint8_t floor_px(int x, int y) {
    int d = y - (SCREEN_H >> 1);
    if(d <= 0) return 0;
    if(d < 6) return (((x >> 1) ^ y) & 3) == 0 ? 1 : 0;
    if(d < 14) return ((x ^ y) & 1) ? 1 : 0;
    return (((x >> 2) ^ (y >> 1)) & 1) ? 1 : 0;
}
static inline uint8_t ceil_px(int x, int y) {
    int d = (SCREEN_H >> 1) - y;
    if(d <= 0) return 0;
    if(d < 5) return ((x + (y << 1)) & 3) == 0 ? 1 : 0;
    return 0;
}

// 绘制罗盘:屏幕右下,箭头指向出口相对玩家的角度
static void draw_compass(void) {
    int cx = SCREEN_W - 11;
    int cy = SCREEN_H - 11;
    int r = 5;
    if(!g.exit_found) {
        // 画一个 X 表示没出口(游客模式)
        for(int i = -3; i <= 3; i++) {
            fb_set(cx + i, cy + i, 1);
            fb_set(cx + i, cy - i, 1);
        }
        return;
    }
    // 圆圈
    for(int a = 0; a < 16; a++) {
        float ang = (float)a * 0.3927f; // pi/8
        int xx = cx + (int)(cosf(ang) * r);
        int yy = cy + (int)(sinf(ang) * r);
        fb_set(xx, yy, 1);
    }
    // 计算相对角度(出口相对玩家的方向 减去 玩家朝向)
    float dx = (float)g.exit_x + 0.5f - g.player.x;
    float dy = (float)g.exit_y + 0.5f - g.player.y;
    // 目标绝对角
    float tgt = atan2f(dy, dx);
    // 玩家朝向角
    float cur = atan2f(g.player.dir_y, g.player.dir_x);
    float delta = tgt - cur;
    // 画箭头从圆心指向 delta 方向
    int ex = cx + (int)(cosf(delta) * 3);
    int ey = cy + (int)(sinf(delta) * 3);
    // 直线(画简单的)
    fb_set(cx, cy, 1);
    fb_set(ex, ey, 1);
    int mx = (cx + ex) >> 1, my = (cy + ey) >> 1;
    fb_set(mx, my, 1);
    // 箭头头部点
    fb_set(ex, ey, 1);
    if((g.tick & 31) < 16) {
        // 闪烁外圈提示近
        float dist_sq = dx*dx + dy*dy;
        if(dist_sq < 36.0f) {
            for(int a = 0; a < 16; a++) {
                float ang = (float)a * 0.3927f;
                int xx = cx + (int)(cosf(ang) * (r+1));
                int yy = cy + (int)(sinf(ang) * (r+1));
                fb_set(xx, yy, 1);
            }
        }
    }
}

// 小地图:屏幕右下(罗盘上面)
static void draw_minimap(void) {
    int mw = g.map_w, mh = g.map_h;
    if(mw > 16) mw = 16;
    if(mh > 12) mh = 12;
    int ox = SCREEN_W - mw - 1;
    int oy = SCREEN_H - mh - 14;
    // 玩家中心平移
    int px = (int)g.player.x;
    int py = (int)g.player.y;
    int sx = px - mw/2;
    int sy = py - mh/2;

    // 边框
    for(int x = ox-1; x <= ox+mw; x++) {
        fb_set(x, oy-1, 1);
        fb_set(x, oy+mh, 1);
    }
    for(int y = oy-1; y <= oy+mh; y++) {
        fb_set(ox-1, y, 1);
        fb_set(ox+mw, y, 1);
    }

    // 地图内容
    for(int y = 0; y < mh; y++) {
        for(int x = 0; x < mw; x++) {
            uint8_t c = map_at(sx + x, sy + y);
            if(is_wall(c)) fb_set(ox + x, oy + y, 1);
            else if(c == CELL_EXIT) {
                // 出口闪烁
                if((g.tick & 7) < 4) fb_set(ox + x, oy + y, 1);
            } else if(c == CELL_KEY || c == CELL_TORCH) {
                if((g.tick & 3) == 0) fb_set(ox + x, oy + y, 1);
            } else if(c == CELL_POTION || c == CELL_AMULET) {
                if((g.tick & 7) < 2) fb_set(ox + x, oy + y, 1);
            } else if(c == CELL_DOOR) {
                if((g.tick & 15) < 8) fb_set(ox + x, oy + y, 1);
            }
        }
    }
    // 玩家点
    int lp = (SCREEN_W - mw - 1) + (px - sx);
    int mp = (SCREEN_H - mh - 14) + (py - sy);
    fb_set(lp, mp, 1);
    // 朝向短线
    int dx = (int)(g.player.dir_x * 1.5f);
    int dy = (int)(g.player.dir_y * 1.5f);
    fb_set(lp + dx, mp + dy, 1);
}

// ---- Raycasting 主渲染(半列: RENDER_COLS = 64, 每列输出 2 像素宽) ----
void engine_render(void) {
    fb_clear();

    Player* p = &g.player;
    const float posX = p->x, posY = p->y;
    const float dirX = p->dir_x, dirY = p->dir_y;
    const float planeX = p->plane_x, planeY = p->plane_y;

    for(int cx = 0; cx < RENDER_COLS; cx++) {
        float cameraX = 2.0f * (float)(cx * 2 + 1) / (float)SCREEN_W - 1.0f;
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
        if(rayX < 0) { stepX = -1; sideX = (posX - mapX) * deltaX; }
        else         { stepX = 1;  sideX = (mapX + 1.0f - posX) * deltaX; }
        if(rayY < 0) { stepY = -1; sideY = (posY - mapY) * deltaY; }
        else         { stepY = 1;  sideY = (mapY + 1.0f - posY) * deltaY; }

        int side = 0;
        uint8_t hit = 0;
        uint8_t exit_on_ray = 0; // 这条光线途中是否经过出口
        for(int i = 0; i < MAP_MAX + 4 && !hit; i++) {
            if(sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
            else              { sideY += deltaY; mapY += stepY; side = 1; }
            uint8_t c = map_at(mapX, mapY);
            if(is_wall(c)) { hit = c; break; }
            if(c == CELL_EXIT) exit_on_ray = 1;
        }
        if(!hit) {
            // 没打到墙: 仅画地板/天花板
            for(int y = 0; y < SCREEN_H; y++) {
                uint8_t on = (y < SCREEN_H/2) ? ceil_px(cx*2, y) : floor_px(cx*2, y);
                fb_set(cx * 2, y, on);
                fb_set(cx * 2 + 1, y, on);
            }
            continue;
        }

        float perp;
        if(side == 0) perp = sideX - deltaX;
        else perp = sideY - deltaY;
        if(perp < 0.01f) perp = 0.01f;

        int lineH = (int)((float)SCREEN_H / perp);
        if(lineH < 1) lineH = 1;
        int drawStart = -lineH / 2 + SCREEN_H / 2;
        int drawEnd = lineH / 2 + SCREEN_H / 2;
        if(drawStart < 0) drawStart = 0;
        if(drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;

        // 命中坐标
        float wallX;
        if(side == 0) wallX = posY + perp * rayY;
        else wallX = posX + perp * rayX;
        wallX -= (float)((int)wallX);
        int texX = (int)(wallX * 8.0f);
        if(side == 0 && rayX > 0) texX = 7 - texX;
        if(side == 1 && rayY < 0) texX = 7 - texX;

        int texId = wall_tex_id(hit);
        int shade = shade_from(perp, side);

        // 出口在这条光线经过: 在该列最亮像素上叠加闪烁标记,或者让出口处的地板高亮
        // 这里做法: 如果靠近出口(格子距离小),每8帧让列的底部画亮
        if(exit_on_ray) {
            float dxm = mapX - posX, dym = mapY - posY;
            if(dxm*dxm + dym*dym < 64.0f && (g.tick & 7) < 3) {
                // 出口列边缘加亮: 画一列闪烁的竖线在墙上方
                int yy = drawStart - 1;
                if(yy >= 0) fb_set(cx * 2, yy, 1);
            }
        }

        // 画天花板
        for(int y = 0; y < drawStart; y++) {
            uint8_t on = ceil_px(cx*2, y);
            fb_set(cx * 2, y, on);
            fb_set(cx * 2 + 1, y, on);
        }
        // 画墙
        int constHalf = -lineH / 2 + SCREEN_H / 2;
        for(int y = drawStart; y <= drawEnd; y++) {
            int texY = ((y - constHalf) * 8) / lineH;
            if(texY < 0) texY = 0; else if(texY > 7) texY = 7;
            if(texture_sample(texId, texX, texY)) {
                apply_shade_px(cx * 2, y, shade);
                apply_shade_px(cx * 2 + 1, y, shade);
            }
        }
        // 画地板
        for(int y = drawEnd + 1; y < SCREEN_H; y++) {
            uint8_t on = floor_px(cx*2, y);
            fb_set(cx * 2, y, on);
            fb_set(cx * 2 + 1, y, on);
        }
    }

    // 出口高亮: 如果玩家靠近出口格,出口正上方画闪烁框
    if(g.exit_found) {
        float dx = (float)g.exit_x + 0.5f - g.player.x;
        float dy = (float)g.exit_y + 0.5f - g.player.y;
        float dist_sq = dx*dx + dy*dy;
        if(dist_sq < 16.0f) {
            // 在屏幕中上方画闪烁大箭头
            int cx = SCREEN_W/2, cy = 16;
            if((g.tick & 15) < 8) {
                for(int i = -6; i <= 6; i++) {
                    fb_set(cx + i, cy - 4, 1);
                }
                fb_set(cx - 5, cy - 3, 1); fb_set(cx + 5, cy - 3, 1);
                fb_set(cx - 4, cy - 2, 1); fb_set(cx + 4, cy - 2, 1);
                fb_set(cx, cy + 2, 1);
            }
        }
    }

    // HUD 覆盖层: 小地图 + 罗盘 + 屏幕中央出口箭头 + 地面道具
    // 1) 屏幕中央的巨大"指向出口"闪烁箭头线(不管距离多少都显示)
    if(g.exit_found && ((g.tick & 15) < 8)) {
        float dx = (float)g.exit_x + 0.5f - g.player.x;
        float dy = (float)g.exit_y + 0.5f - g.player.y;
        float tgt = atan2f(dy, dx);
        float cur = atan2f(g.player.dir_y, g.player.dir_x);
        float delta = tgt - cur;
        // 归一化到 [-PI, PI]
        while(delta >  3.14159f) delta -= 6.28318f;
        while(delta < -3.14159f) delta += 6.28318f;
        // 左右箭头显示区: 屏幕上半 (y=8..24)
        int cy = 14;
        int cx = SCREEN_W / 2;
        // 箭头离中心的偏移(按 delta 比例),delta=0 指向正前方=正中央
        int off_x = (int)(delta * 40.0f); // ~±128像素偏移(±PI)
        if(off_x >  58) off_x =  58;
        if(off_x < -58) off_x = -58;
        int ax = cx + off_x;
        // 画巨大下箭头 (↓) 表示出口在这个水平方向上
        int as = 8;
        // 竖线
        for(int i = -as; i <= as; i++) {
            fb_set(ax, cy + i, 1);
        }
        // 三角箭头头
        for(int i = 0; i < 6; i++) {
            fb_set(ax - i - 1, cy + as - i, 1);
            fb_set(ax + i + 1, cy + as - i, 1);
        }
        // 如果 delta 接近 0 (出口正对前方), 在屏幕上半画大闪烁外框提示"出口在前"
        if(fabsf(delta) < 0.35f) {
            float dist = sqrtf(dx*dx + dy*dy);
            int bw = (dist < 5.0f) ? 60 : 40;
            for(int i = -bw/2; i <= bw/2; i++) {
                fb_set(cx + i, 2, 1);
                fb_set(cx + i, 10, 1);
            }
            for(int y = 2; y <= 10; y++) {
                fb_set(cx - bw/2, y, 1);
                fb_set(cx + bw/2, y, 1);
            }
        }
    }

    // 2) 绘制屏幕空间中的地面道具 (在画墙之后、绘制2D覆盖物之前做精灵投影)
    //    做法: 遍历玩家附近 5x5 格,非空道具格做简单投影 (z-buffer 简单近似)
    {
        int px = (int)g.player.x;
        int py = (int)g.player.y;
        Player* pp = &g.player;
        const float invDet = 1.0f / (pp->plane_x * pp->dir_y - pp->dir_x * pp->plane_y);
        for(int y = -3; y <= 3; y++) {
            for(int x = -3; x <= 3; x++) {
                int cx1 = px + x, cy1 = py + y;
                uint8_t cell = maze_get(cx1, cy1);
                if(cell != CELL_KEY && cell != CELL_TORCH && cell != CELL_POTION &&
                   cell != CELL_AMULET && cell != CELL_TRAP && cell != CELL_EXIT)
                    continue;
                // 精灵世界坐标
                float spx = (float)cx1 + 0.5f - pp->x;
                float spy = (float)cy1 + 0.5f - pp->y;
                float transX = invDet * ( pp->dir_y * spx - pp->dir_x * spy);
                float transY = invDet * (-pp->plane_y * spx + pp->plane_x * spy);
                if(transY <= 0.1f) continue; // 背后或太近
                int screenX = (int)((SCREEN_W / 2.0f) * (1.0f + transX / transY));
                // 投影尺寸
                int ss = (int)(SCREEN_H / transY);
                if(ss < 2) continue;
                if(ss > 40) ss = 40;
                // 道具中心位于地板线附近 (下半屏, drawEnd 附近)
                int cy_base = SCREEN_H / 2 + (int)((float)SCREEN_H * 0.12f / transY);
                if(cy_base > SCREEN_H - 1) cy_base = SCREEN_H - 1;
                // 根据道具类型选简单标志形状 (2x2 或 十字 闪烁)
                bool show = true;
                if(cell == CELL_TRAP) show = ((g.tick & 7) < 3); // 陷阱稀有闪烁(半隐藏)
                else                  show = ((g.tick & 3) < 2);
                if(!show) continue;
                int mark = ss;
                if(mark < 3) mark = 3;
                if(mark > 10) mark = 10;
                // 画方块 (道具)
                if(cell == CELL_KEY) {
                    // 钥匙: 画空心方块
                    for(int i = -mark/2; i <= mark/2; i++) {
                        fb_set(screenX + i, cy_base - mark/2, 1);
                        fb_set(screenX + i, cy_base + mark/2, 1);
                        fb_set(screenX - mark/2, cy_base + i, 1);
                        fb_set(screenX + mark/2, cy_base + i, 1);
                    }
                } else if(cell == CELL_TORCH) {
                    // 火把: 十字 (4 点)+ 中心
                    fb_set(screenX, cy_base, 1);
                    fb_set(screenX - 1, cy_base - 1, 1);
                    fb_set(screenX + 1, cy_base - 1, 1);
                    fb_set(screenX, cy_base - 2, 1);
                    if(mark > 5) {
                        fb_set(screenX - 2, cy_base - 3, 1);
                        fb_set(screenX + 2, cy_base - 3, 1);
                    }
                } else if(cell == CELL_POTION) {
                    // 药水: 实心圆(近似方块)
                    for(int yy = -mark/3; yy <= mark/3; yy++)
                        for(int xx = -mark/3; xx <= mark/3; xx++)
                            fb_set(screenX + xx, cy_base + yy, 1);
                } else if(cell == CELL_AMULET) {
                    // 护符: 菱形
                    for(int i = 0; i < mark/2 + 1; i++) {
                        fb_set(screenX + i, cy_base - mark/2 + i, 1);
                        fb_set(screenX - i, cy_base - mark/2 + i, 1);
                        fb_set(screenX + i, cy_base + mark/2 - i, 1);
                        fb_set(screenX - i, cy_base + mark/2 - i, 1);
                    }
                } else if(cell == CELL_TRAP) {
                    // 陷阱: X 形
                    for(int i = -mark/2; i <= mark/2; i++) {
                        fb_set(screenX + i, cy_base + i, 1);
                        fb_set(screenX + i, cy_base - i, 1);
                    }
                } else if(cell == CELL_EXIT) {
                    // 出口: 大闪烁框 (在 3D 墙上已经有高亮,但此处投影更醒目)
                    for(int i = -mark; i <= mark; i++) {
                        fb_set(screenX + i, cy_base - mark, 1);
                        fb_set(screenX + i, cy_base + mark, 1);
                        fb_set(screenX - mark, cy_base + i, 1);
                        fb_set(screenX + mark, cy_base + i, 1);
                    }
                }
            }
        }
    }

    draw_minimap();
    draw_compass();
}
