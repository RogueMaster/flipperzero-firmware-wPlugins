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

// v6.2: XOR 画法 (画"永远与背景相反色"的像素 — 准星/标记专用)
static inline void fb_xor(int x, int y) {
    if((unsigned)x >= SCREEN_W || (unsigned)y >= SCREEN_H) return;
    uint16_t idx = ((uint16_t)y << 4) + ((uint16_t)x >> 3);
    uint8_t bit = 1u << (x & 7);
    g.fb[idx] ^= bit;
}

// v6.2: 画一个实心像素 + 反色边框 (高亮小方块, 用于粒子)
static inline void fb_highlight_px(int x, int y) {
    fb_set(x, y, 1);
    // 周围做一个反色小十字, 突出该像素 (即使在亮/暗背景上都可见)
    fb_xor(x + 1, y);
    fb_xor(x - 1, y);
    fb_xor(x, y + 1);
    fb_xor(x, y - 1);
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
           c == WALL_VINE || c == WALL_WATER || c == WALL_GRASS ||
           c == WALL_WOOD || c == WALL_TREE || c == CELL_DOOR ||
           // v6.3 MC 新方块
           c == WALL_SAND || c == WALL_DIRT || c == WALL_LOG;
}

static inline int wall_tex_id(uint8_t c) {
    switch(c) {
        case WALL_BRICK: return 0;
        case WALL_STONE: return 1;
        case WALL_METAL: return 2;
        case WALL_VINE:  return 3;
        case WALL_WATER: return 4;   // 动态流动水
        case WALL_GRASS: return 5;   // 草+土侧面
        case WALL_WOOD:  return 6;   // 木板
        case WALL_TREE:  return 7;   // 树叶
        case CELL_DOOR:  return 2;   // 门用金属纹理
        // v6.3 MC 新方块材质
        case WALL_SAND:  return 8;   // 沙子
        case WALL_DIRT:  return 9;   // 土
        case WALL_LOG:   return 10;  // 原木
        default:         return 0;
    }
}

extern uint8_t texture_sample(int tex_id, int tx, int ty);

// v6.2: 全局高亮 — 提亮墙体 (shade 档位 -1, 阴影阈值放宽).
// shade 档位: 0=实心/最亮, 4=极远. 现在同样距离比之前亮一档.
// v6.9: cfg_fog=false 时关闭距离雾 (所有墙近似最亮档)
static inline int shade_from(float perp, int side) {
    int s;
    if(!g.cfg_fog) {
        // 雾效关闭: 所有距离视作近距离, 仅保留侧面区分
        s = (perp < 4.5f) ? 0 : 1;
    } else {
        if(perp < 2.5f) s = 0;
        else if(perp < 4.5f) s = 1;
        else if(perp < 7.5f) s = 2;
        else if(perp < 11.0f) s = 3;
        else s = 4;
    }
    if(side == 1 && s < 4) s++;
    return s;
}

// 4x4 Bayer 有序抖动表: 比"棋盘+稀疏点"过渡更平滑, 远处墙体不会突然消失
static const uint8_t BAYER4[16] = {
     0, 8, 2,10,
    12, 4,14, 6,
     3,11, 1, 9,
    15, 7,13, 5,
};
static inline uint8_t bayer_at(int x, int y) {
    return BAYER4[((y & 3) << 2) | (x & 3)];
}

static inline void apply_shade_px(int x, int y, int shade) {
    // 阈值越小越稀疏; 用 4 档密度做平滑距离渐变
    // v6.9: cfg_brightness 0..4 → 乘数 0.6/0.8/1.0/1.25/1.5, 调节阈值(阈值越高→像素越多越亮)
    int thr;
    static const float bright_mul[] = {0.60f, 0.80f, 1.00f, 1.25f, 1.50f};
    float bm = bright_mul[(g.cfg_brightness < 5) ? g.cfg_brightness : 2];
    // v6.2: 阈值整体上移, 所有档位都更亮
    switch(shade) {
        case 0: fb_set(x, y, 1); return;   // 最近: 实心
        case 1: thr = (int)(14.0f * bm); break;
        case 2: thr = (int)(10.0f * bm); break;
        case 3: thr = (int)(6.0f  * bm); break;
        default: thr = (int)(3.0f * bm); break;
    }
    if(thr < 1) thr = 1;
    if(thr > 15) thr = 15;
    if(bayer_at(x, y) < thr) fb_set(x, y, 1);
}

// 地板/天花板抖动 pattern: 沿距地平线距离做平滑密度渐变 (Bayer)
// v6.9: cfg_floor_tex=false 时简化地板 (纯抖动无渐变, 更省运算)
static inline uint8_t floor_px(int x, int y) {
    if(!g.cfg_floor_tex) {
        // 简化版: 纯棋盘/稀疏点, 无距离依赖 (更像"平坦地面")
        return (bayer_at(x, y) < 5) ? 1 : 0;
    }
    int d = y - (SCREEN_H >> 1);   // 离地平线距离 (下方为正), 越大越近->越密
    if(d <= 0) return 0;
    int thr = 1 + (d >> 1);        // 近地平线稀疏, 脚下密集
    if(thr > 10) thr = 10;
    return (bayer_at(x, y) < thr) ? 1 : 0;
}
// v6.4: MC 天空亮度 — 随日升月落变化.
//   太阳角度 0..255 循环: 0=日出(左下), 64=正午(顶), 128=日落(右下), 192=午夜.
//   亮度 = sin(太阳角度) 的正值部分.
// v6.9: 周期由 cfg_mc_day_len 控制: 0=1024,1=512,2=256,3=128 tick
static int mc_sky_brightness(void) {
    if(g.mode != MODE_MC) return -1;  // 非 MC 模式用原逻辑
    // v6.9: cfg_mc_day_len → 周期掩码 (1023/511/255/127)
    static const uint16_t day_mask[] = { 1023u, 511u, 255u, 127u };
    uint16_t mask = day_mask[(g.cfg_mc_day_len < 4) ? g.cfg_mc_day_len : 0];
    int period = (int)mask + 1;
    int phase = ((g.tick & mask) * 256) / period;   // 0..255
    // 用查表近似 sin: 正午最亮(15), 日出日落中等(8), 夜晚最暗(2)
    if(phase < 128) {
        // 白天半周期: 0→128 对应日出→正午→日落
        int h = (phase < 64) ? phase : (127 - phase);  // 0→64→0 三角波
        return 4 + (h * 11) / 64;   // 4..15
    } else {
        // 夜晚半周期: 128→256
        int h = (phase < 192) ? (phase - 128) : (255 - phase);
        return 1 + (h * 3) / 64;    // 1..4
    }
}

// v6.4: 天花板. MC 模式画"天空" (亮度随日升月落), 普通模式保持暗
// v6.9: cfg_sky_ceil=false 时完全不画天空/天花板 (上半屏纯黑)
static inline uint8_t ceil_px(int x, int y) {
    if(!g.cfg_sky_ceil) return 0;   // 天空/天花板关闭
    int d = (SCREEN_H >> 1) - y;   // 离地平线距离 (上方为正)
    if(d <= 0) return 0;
    int thr;
    if(g.mode == MODE_MC) {
        int bright = mc_sky_brightness();
        // 地平线附近暗, 顶部亮 (天空渐变)
        thr = bright - (d >> 3);
        if(thr < 1) thr = 1;
        if(thr > 15) thr = 15;
    } else {
        thr = d >> 2;              // 普通天花板整体偏暗
        if(thr > 4) thr = 4;
    }
    return (bayer_at(x, y) < thr) ? 1 : 0;
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

// ---- Raycasting 主渲染(v6.9: 可变 DDA 列密度 32/48/64) ----
void engine_render(void) {
    fb_clear();

    Player* p = &g.player;
    const float posX = p->x, posY = p->y;
    const float dirX = p->dir_x, dirY = p->dir_y;
    const float planeX = p->plane_x, planeY = p->plane_y;

    // v6.9: cfg_density 0..2 → 列数 32/48/64
    static const int dens_cols[] = { 32, 48, 64 };
    int RENDER_COLS_DYN = dens_cols[(g.cfg_density < 3) ? g.cfg_density : 2];

    for(int ci = 0; ci < RENDER_COLS_DYN; ci++) {
        // 本列覆盖的屏幕像素范围 [px_start, px_end] (inclusive)
        int px_start = (ci * SCREEN_W) / RENDER_COLS_DYN;
        int px_end   = ((ci + 1) * SCREEN_W) / RENDER_COLS_DYN - 1;
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
            // 没打到墙: 仅画地板/天花板, 填充整列像素范围
            for(int y = 0; y < SCREEN_H; y++) {
                uint8_t on = (y < SCREEN_H/2) ? ceil_px(px_ctr, y) : floor_px(px_ctr, y);
                for(int px = px_start; px <= px_end; px++)
                    fb_set(px, y, on);
            }
            continue;
        }

        float perp;
        if(side == 0) perp = sideX - deltaX;
        else perp = sideY - deltaY;
        if(perp < 0.01f) perp = 0.01f;

        int lineH = (int)((float)SCREEN_H / perp);
        if(lineH < 1) lineH = 1;
        // v6.7-beta: 跳跃 — 根据离地 jump_z 像素高度, 按距离将墙面+地平线整体下移
        int jumpShift = 0;
        if(g.jump_z != 0.0f) {
            jumpShift = (int)(g.jump_z * (float)SCREEN_H / perp);
        }
        int drawStart = -lineH / 2 + SCREEN_H / 2 + jumpShift;
        int drawEnd   =  lineH / 2 + SCREEN_H / 2 + jumpShift;
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

        // 出口在这条光线经过: 出口列上方闪烁提示
        if(exit_on_ray) {
            float dxm = mapX - posX, dym = mapY - posY;
            if(dxm*dxm + dym*dym < 64.0f && (g.tick & 7) < 3) {
                int yy = drawStart - 1;
                if(yy >= 0) fb_set(px_start, yy, 1);
            }
        }

        // 画天花板 (填充像素范围)
        for(int y = 0; y < drawStart; y++) {
            uint8_t on = ceil_px(px_ctr, y);
            for(int px = px_start; px <= px_end; px++)
                fb_set(px, y, on);
        }
        // 画墙 (填充像素范围)
        int constHalf = -lineH / 2 + SCREEN_H / 2 + jumpShift;
        for(int y = drawStart; y <= drawEnd; y++) {
            int texY = ((y - constHalf) * 8) / lineH;
            if(texY < 0) texY = 0; else if(texY > 7) texY = 7;
            if(texture_sample(texId, texX, texY)) {
                for(int px = px_start; px <= px_end; px++)
                    apply_shade_px(px, y, shade);
            }
        }
        // 画地板 (填充像素范围)
        for(int y = drawEnd + 1; y < SCREEN_H; y++) {
            uint8_t on = floor_px(px_ctr, y);
            for(int px = px_start; px <= px_end; px++)
                fb_set(px, y, on);
        }
    }

    // 出口指示箭头: 用与精灵相同的逆行列式投影, 把出口格投到屏幕空间,
    // 得到出口在屏幕上的真实水平位置 -> 箭头精准指向该位置.
    // 旧实现用 delta*40 的线性近似, 视场边缘误差极大, 此处修正.
    if(g.exit_found) {
        float spx = (float)g.exit_x + 0.5f - g.player.x;
        float spy = (float)g.exit_y + 0.5f - g.player.y;
        float dist = sqrtf(spx*spx + spy*spy);
        float invDet = 1.0f / (g.player.plane_x * g.player.dir_y -
                               g.player.dir_x * g.player.plane_y);
        float transX = invDet * ( g.player.dir_y * spx - g.player.dir_x * spy);
        float transY = invDet * (-g.player.plane_y * spx + g.player.plane_x * spy);

        // 靠近出口时, 在屏幕中上方画脉冲提示 (不管方向)
        if(dist < 4.0f && (g.tick & 7) < 5) {
            int bx = SCREEN_W/2, by = 16;
            for(int i = -7; i <= 7; i++) fb_set(bx + i, by - 5, 1);
            fb_set(bx - 6, by - 4, 1); fb_set(bx + 6, by - 4, 1);
            fb_set(bx - 5, by - 3, 1); fb_set(bx + 5, by - 3, 1);
            fb_set(bx - 3, by - 2, 1); fb_set(bx + 3, by - 2, 1);
            fb_set(bx, by + 1, 1);
        }

        // 主箭头闪烁
        if((g.tick & 15) < 10) {
            int cy = 14;
            int cx = SCREEN_W / 2;
            if(transY > 0.05f) {
                // 出口在前方: 计算真实屏幕 X 并钳制到可见带
                int ax = (int)((SCREEN_W / 2.0f) * (1.0f + transX / transY));
                bool offscreen = (ax < 4 || ax > SCREEN_W - 5);
                if(ax < 6)        ax = 6;
                if(ax > SCREEN_W-7) ax = SCREEN_W-7;
                // 画向上的 ^ 箭头(表示出口在该水平方向的前方)
                int as = 6;
                for(int i = 0; i <= as; i++) {
                    fb_set(ax - i, cy - as + i, 1);
                    fb_set(ax + i, cy - as + i, 1);
                }
                fb_set(ax, cy - as - 1, 1); // 顶点
                // 箭杆
                for(int i = -2; i <= 2; i++) fb_set(ax, cy + i, 1);
                // 出口大致正前方(投影接近屏幕中心) -> 大闪烁框 "EXIT AHEAD"
                if(!offscreen && abs(ax - cx) < 9) {
                    int bw = (dist < 5.0f) ? 56 : 40;
                    for(int i = -bw/2; i <= bw/2; i++) {
                        fb_set(cx + i, 2, 1);
                        fb_set(cx + i, 10, 1);
                    }
                    for(int y = 2; y <= 10; y++) {
                        fb_set(cx - bw/2, y, 1);
                        fb_set(cx + bw/2, y, 1);
                    }
                }
            } else {
                // 出口在身后: 左右两端各画一个朝外的小箭头, 提示转身
                for(int i = 0; i < 5; i++) {
                    fb_set(2 + i, cy - i, 1);      // 左上角 <-
                    fb_set(2 + i, cy + i, 1);
                    fb_set(SCREEN_W-3 - i, cy - i, 1); // 右上角 ->
                    fb_set(SCREEN_W-3 - i, cy + i, 1);
                }
                fb_set(2, cy, 1); fb_set(SCREEN_W-3, cy, 1);
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
                // v6.7-beta: 加上跳跃位移 — 相机升高, 地板上的精灵整体下移
                int jumpShiftSprite = 0;
                if(g.jump_z != 0.0f) {
                    jumpShiftSprite = (int)(g.jump_z * (float)SCREEN_H / transY);
                }
                int cy_base = SCREEN_H / 2 + (int)((float)SCREEN_H * 0.12f / transY) + jumpShiftSprite;
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

    // v6.1: 渲染敌人精灵 (3D 投影 + 外形 + 受伤反白 + 移动呼吸)
    {
        Player* pp = &g.player;
        const float invDet = 1.0f / (pp->plane_x * pp->dir_y - pp->dir_x * pp->plane_y);
        for(int i = 0; i < g.actor_count; i++) {
            Actor* a = &g.actors[i];
            if(!a->active || a->hp == 0) continue;
            float spx = a->x - pp->x;
            float spy = a->y - pp->y;
            float transX = invDet * ( pp->dir_y * spx - pp->dir_x * spy);
            float transY = invDet * (-pp->plane_y * spx + pp->plane_x * spy);
            if(transY <= 0.2f) continue;   // 背后或太近不画
            int screenX = (int)((SCREEN_W / 2.0f) * (1.0f + transX / transY));
            if(screenX < -8 || screenX > SCREEN_W + 8) continue;
            // 投影高度 (敌人约 0.8 格高)
            int sh = (int)((float)SCREEN_H * 0.8f / transY);
            if(sh < 3) sh = 3;
            if(sh > 36) sh = 36;
            int sw = sh / 2;             // 宽度约高度一半
            // 垂直锚点: 站在地板上 (脚部在 drawEnd 附近)
            // v6.7-beta: 跳跃位移 — 敌人站地板上, 随相机升高而下移
            int jumpShiftActor = 0;
            if(g.jump_z != 0.0f) {
                jumpShiftActor = (int)(g.jump_z * (float)SCREEN_H / transY);
            }
            int feet_y = SCREEN_H / 2 + (int)((float)SCREEN_H * 0.4f / transY) + jumpShiftActor;
            if(feet_y > SCREEN_H - 1) feet_y = SCREEN_H - 1;
            int top_y = feet_y - sh;
            if(top_y < 0) top_y = 0;
            // 受伤反白: hurt_flash>0 时画空心 (反相), 否则实心
            bool hurt = (a->hurt_flash > 0);
            // 移动呼吸: 每 8 帧上下抖 1 像素
            int bob = ((g.tick + i * 3) & 8) ? 1 : 0;
            top_y += bob; feet_y += bob;
            // 敌人外形: 头(小圆) + 身(矩形) + 眼(亮点)
            // 身体: 实心矩形 (留头顶 1/4 给头)
            int body_h = sh - sh / 4;
            int body_top = top_y + sh / 4;
            for(int yy = 0; yy < body_h; yy++) {
                int py = body_top + yy;
                if(py < 0 || py >= SCREEN_H) continue;
                for(int xx = -sw; xx <= sw; xx++) {
                    int px = screenX + xx;
                    if(px < 0 || px >= SCREEN_W) continue;
                    // 边框始终亮, 内部按 hurt 决定
                    bool edge = (xx == -sw || xx == sw || yy == 0 || yy == body_h - 1);
                    if(edge) fb_set(px, py, 1);
                    else if(!hurt) fb_set(px, py, 1);   // 实心敌人
                    // hurt 时内部留空 (反白效果)
                }
            }
            // 头: 小方块
            int head_h = sh / 4;
            if(head_h < 2) head_h = 2;
            for(int yy = 0; yy < head_h; yy++) {
                int py = top_y + yy;
                if(py < 0 || py >= SCREEN_H) continue;
                int hw = sw / 2 + 1;
                for(int xx = -hw; xx <= hw; xx++) {
                    int px = screenX + xx;
                    if(px < 0 || px >= SCREEN_W) continue;
                    fb_set(px, py, 1);
                }
            }
            // 眼睛: 两个亮点 (闪烁, 模拟"瞄准玩家")
            if(sh >= 8 && (g.tick & 3) < 3) {
                int ey_y = top_y + head_h / 2 + 1;
                int ex_off = (sw > 2) ? 1 : 0;
                if(ex_off) {
                    fb_set(screenX - ex_off, ey_y, hurt ? 0 : 1);
                    fb_set(screenX + ex_off, ey_y, hurt ? 0 : 1);
                }
            }
            // 敌人射击预警: fire_cd 接近 0 时画一个红点(此处黑白=亮点)在枪口位置
            if(a->fire_cd > 0 && a->fire_cd < 8 && sh >= 6) {
                int mx = screenX;
                int my = body_top + body_h / 2;
                fb_set(mx, my, 1);
                fb_set(mx + 1, my, 1);
            }
        }
    }

    // v6.1: 渲染子弹 (沿屏幕水平线的发光点, 远近按距离缩放)
    {
        Player* pp = &g.player;
        const float invDet = 1.0f / (pp->plane_x * pp->dir_y - pp->dir_x * pp->plane_y);
        for(int i = 0; i < MAX_BULLETS; i++) {
            Bullet* b = &g.bullets[i];
            if(!b->active) continue;
            float spx = b->x - pp->x;
            float spy = b->y - pp->y;
            float transX = invDet * ( pp->dir_y * spx - pp->dir_x * spy);
            float transY = invDet * (-pp->plane_y * spx + pp->plane_x * spy);
            if(transY <= 0.1f) continue;
            int sx = (int)((SCREEN_W / 2.0f) * (1.0f + transX / transY));
            if(sx < -2 || sx > SCREEN_W + 1) continue;
            // 子弹在屏幕中线高度 (略低于视线)
            // v6.7-beta: 子弹在世界空间中 (高度接近视线), 跳跃时跟着略微下移
            int jumpShiftBullet = 0;
            if(g.jump_z != 0.0f) {
                jumpShiftBullet = (int)(g.jump_z * (float)SCREEN_H / transY);
            }
            int sy = SCREEN_H / 2 + (int)(4.0f / transY) + jumpShiftBullet;
            if(sy < 0) sy = 0;
            if(sy >= SCREEN_H) sy = SCREEN_H - 1;
            // 大小随距离: 近处画 3 点, 远处 1 点, 闪烁
            bool flick = ((g.tick + i) & 1) == 0;
            if(transY < 2.0f && flick) {
                fb_set(sx - 1, sy, 1);
                fb_set(sx + 1, sy, 1);
            }
            fb_set(sx, sy, 1);
            // 玩家子弹拖尾 (向上偏移的渐淡点)
            if(b->owner == 0 && transY < 3.0f) {
                fb_set(sx, sy - 1, (g.tick & 1));
            }
        }
    }

    // v6.1: 枪口闪光 (屏幕中央亮斑, 模拟开火瞬间)
    if(g.muzzle_flash > 0) {
        int cx = SCREEN_W / 2, cy = SCREEN_H / 2 + 4;
        // 十字闪光 + 中心实心
        fb_set(cx, cy, 1);
        for(int r = 1; r <= 3; r++) {
            fb_set(cx + r, cy, 1); fb_set(cx - r, cy, 1);
            fb_set(cx, cy + r, 1); fb_set(cx, cy - r, 1);
        }
        // 对角线 (稀疏)
        fb_set(cx + 2, cy + 2, 1); fb_set(cx - 2, cy - 2, 1);
        fb_set(cx - 2, cy + 2, 1); fb_set(cx + 2, cy - 2, 1);
    }

    // v6.1: 受伤闪白 (整屏反相几帧)
    if(g.hurt_flash > 0 && (g.tick & 1)) {
        for(int i = 0; i < FB_BYTES; i++) g.fb[i] ^= 0xFF;
    }

    draw_minimap();
    if(g.mode != MODE_MC) draw_compass();   // MC 沙盒无出口, 不画罗盘

    // v6.4: MC 日升月落 — 太阳/月亮沿弧线从左到右划过天空
    if(g.mode == MODE_MC) {
        // 太阳周期 1024 tick: 0-512 白天(太阳), 512-1024 夜晚(月亮)
        int phase = g.tick & 1023;
        bool is_day = (phase < 512);
        // 弧线: x 从 4→124, y 按 sin 弧线从地平线升到顶再落下
        float t = (is_day ? phase : (phase - 512)) / 511.0f;   // 0..1
        int arc_x = 4 + (int)(t * (SCREEN_W - 12));
        // y: 0=地平线(28), 0.5=最高(4), 1=地平线(28)
        int arc_y = (int)(28.0f - sinf(t * 3.14159f) * 24.0f);
        if(arc_y < 2) arc_y = 2;
        if(arc_y > 28) arc_y = 28;

        if(is_day) {
            // 太阳: 5x5 实心 + 光芒
            for(int yy = 0; yy < 5; yy++)
                for(int xx = 0; xx < 5; xx++)
                    fb_set(arc_x + xx, arc_y + yy, 1);
            // 光晕 (正午最强)
            int glow = (int)(sinf(t * 3.14159f) * 4.0f);
            for(int i = 1; i <= glow; i++) {
                fb_set(arc_x + 2, arc_y - i, 1);
                fb_set(arc_x + 2, arc_y + 4 + i, 1);
                fb_set(arc_x - i, arc_y + 2, 1);
                fb_set(arc_x + 4 + i, arc_y + 2, 1);
            }
        } else {
            // 月亮: 3x3 空心方框 (弯月感)
            for(int i = 0; i < 3; i++) {
                fb_set(arc_x + i, arc_y, 1);
                fb_set(arc_x + i, arc_y + 2, 1);
            }
            fb_set(arc_x, arc_y + 1, 1);
            fb_set(arc_x + 2, arc_y + 1, 1);
            // 星星: 夜晚随机点缀 3 颗 (基于 tick 闪烁)
            for(int s = 0; s < 3; s++) {
                int sx = (s * 37 + 7) & 127;
                int sy = (s * 19 + 3) % 20;
                if((g.tick + s * 31) & 64) fb_set(sx, sy, 1);
            }
        }
    }

    // v6.2: 渲染粒子 (3D 投影: 和子弹/敌人相同逆行列式)
    {
        Player* pp = &g.player;
        const float invDet = 1.0f / (pp->plane_x * pp->dir_y - pp->dir_x * pp->plane_y);
        for(int i = 0; i < MAX_PARTICLES; i++) {
            Particle* p = &g.particles[i];
            if(!p->active) continue;
            float spx = p->x - pp->x;
            float spy = p->y - pp->y;
            float transX = invDet * ( pp->dir_y * spx - pp->dir_x * spy);
            float transY = invDet * (-pp->plane_y * spx + pp->plane_x * spy);
            if(transY <= 0.1f) continue;
            int sx = (int)((SCREEN_W / 2.0f) * (1.0f + transX / transY));
            if(sx < -4 || sx > SCREEN_W + 3) continue;
            // 粒子高度: 子弹尾迹在中线附近, 行走尘土在地板附近
            // v6.7-beta: 跳跃位移 — 世界空间粒子都下移
            int jumpShiftP = 0;
            if(g.jump_z != 0.0f) {
                jumpShiftP = (int)(g.jump_z * (float)SCREEN_H / transY);
            }
            int sy;
            if(p->type == 3) sy = SCREEN_H / 2 + (int)(SCREEN_H * 0.38f / transY) + jumpShiftP;
            else             sy = SCREEN_H / 2 + (int)(2.0f / transY) + jumpShiftP;
            if(sy < 0) sy = 0;
            if(sy >= SCREEN_H) sy = SCREEN_H - 1;
            // 不同类型大小样式
            if(p->type == 1 && p->size >= 2) {
                // 命中爆炸: 3x3 高亮
                for(int yy = -1; yy <= 1; yy++)
                    for(int xx = -1; xx <= 1; xx++)
                        fb_highlight_px(sx + xx, sy + yy);
            } else {
                // 其他类型: 单点高亮 (十字+反色边框)
                fb_highlight_px(sx, sy);
            }
        }
    }

    // v6.2: 准星 (反差色十字, 恒可见).
    // 所有非菜单模式都画, 保证用户要求的"屏幕一直有反差十字准心".
    if(g.mode != MODE_MENU && g.mode != MODE_PAUSED &&
       g.mode != MODE_INVENTORY && g.mode != MODE_LEVEL_SELECT &&
       g.mode != MODE_STORY) {
        int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
        // 后坐力: 射击时准星下移
        if(g.shoot_kick > 0) cy += (int)g.shoot_kick;
        // 准星样式: 中心空心框 + 四向短尖 (共 13 px)
        // 都用 XOR 画法, 保证黑底白点/白底黑点永远相反
        // 中央空框 (2x2 空)
        fb_xor(cx - 1, cy - 2); fb_xor(cx, cy - 2); fb_xor(cx + 1, cy - 2);
        fb_xor(cx - 2, cy - 1);                                 fb_xor(cx + 2, cy - 1);
        fb_xor(cx - 2, cy);                                     fb_xor(cx + 2, cy);
        fb_xor(cx - 2, cy + 1);                                 fb_xor(cx + 2, cy + 1);
        fb_xor(cx - 1, cy + 2); fb_xor(cx, cy + 2); fb_xor(cx + 1, cy + 2);
        // 中心十字点
        fb_xor(cx, cy);
        // 四向尖刺 (尖端朝外)
        fb_xor(cx, cy - 4); fb_xor(cx, cy - 5);
        fb_xor(cx, cy + 4); fb_xor(cx, cy + 5);
        fb_xor(cx - 4, cy); fb_xor(cx - 5, cy);
        fb_xor(cx + 4, cy); fb_xor(cx + 5, cy);
    }
}
