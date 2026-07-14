#include <stdint.h>
#include "game/Draw.h"
#include "game/Defines.h"
#include "game/Game.h"
#include "game/Particle.h"
#include "game/FixedMath.h"
#include "game/Map.h"
#include "game/Projectile.h"
#include "game/Platform.h"
#include "game/Enemy.h"
#include "game/Font.h"

#include "game/LUT.h"
#include "game/Generated/spritedata_inc.h"

#if WITH_VECTOR_TEXTURES
#include "game/Textures.h"
#endif

#if WITH_SPRITE_OUTLINES
#define DrawScaledInner DrawScaledOutline
#else
#define DrawScaledInner DrawScaledNoOutline
#endif

Camera Renderer::camera;
uint8_t Renderer::wBuffer[DISPLAY_WIDTH];
int8_t Renderer::horizonBuffer[DISPLAY_WIDTH];
uint8_t Renderer::globalRenderFrame = 0;
uint8_t Renderer::numBufferSlicesFilled = 0;
QueuedDrawable Renderer::queuedDrawables[MAX_QUEUED_DRAWABLES];
uint8_t Renderer::numQueuedDrawables = 0;

const uint8_t scaleDrawWriteMasks[] PROGMEM =
    {(1), (1 << 1), (1 << 2), (1 << 3), (1 << 4), (1 << 5), (1 << 6), (1 << 7)};

const uint16_t scaleDrawReadMasks[] PROGMEM = {
    (1),
    (1 << 1),
    (1 << 2),
    (1 << 3),
    (1 << 4),
    (1 << 5),
    (1 << 6),
    (1 << 7),
    (1 << 8),
    (1 << 9),
    (1 << 10),
    (1 << 11),
    (1 << 12),
    (1 << 13),
    (1 << 14),
    (1 << 15)};

#if WITH_VECTOR_TEXTURES
void Renderer::DrawWallLine(
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint8_t clipLeft,
    uint8_t clipRight,
    uint8_t col) {
    if(x1 > x2) return;

    if(y1 < 0) {
        if(y2 < 0) return;

        if(y2 != y1) x1 += (0 - y1) * (x2 - x1) / (y2 - y1);
        y1 = 0;
    }
    if(y2 > DISPLAY_HEIGHT - 1) {
        if(y1 > DISPLAY_HEIGHT - 1) return;

        if(y2 != y1) x2 += (((DISPLAY_HEIGHT - 1) - y2) * (x1 - x2)) / (y1 - y2);
        y2 = DISPLAY_HEIGHT - 1;
    }

    if(x1 < clipLeft) {
        if(x2 != x1) {
            y1 += ((clipLeft - x1) * (y2 - y1)) / (x2 - x1);
        }
        x1 = clipLeft;
    }

    int16_t dx = x2 - x1;
    int16_t yerror = dx / 2;
    int16_t y = y1;
    int16_t dy;
    int8_t ystep;

    if(y1 < y2) {
        dy = y2 - y1;
        ystep = 1;
    } else {
        dy = y1 - y2;
        ystep = -1;
    }

    for(int x = x1; x <= x2 && x <= clipRight; x++) {
        int8_t horizon = horizonBuffer[x] - HORIZON;

        Platform::PutPixel(x, horizon + y, col);

        yerror -= dy;

        while(yerror < 0) {
            y += ystep;

            //if(y < 0 || y >= DISPLAY_HEIGHT)
            //	return;

            yerror += dx;

            if(yerror < 0) {
                Platform::PutPixel(x, horizon + y, col);
            }

            if(x == x2 && y == y2) break;
        }
    }
}
#endif
#if WITH_IMAGE_TEXTURES
void Renderer::DrawWallSegment(
    const uint16_t* texture,
    int16_t x1,
    int16_t w1,
    int16_t x2,
    int16_t w2,
    uint8_t u1clip,
    uint8_t u2clip,
    bool edgeLeft,
    bool edgeRight,
    bool shadeEdge)
#elif WITH_VECTOR_TEXTURES
void Renderer::DrawWallSegment(
    const uint8_t* texture,
    int16_t x1,
    int16_t w1,
    int16_t x2,
    int16_t w2,
    uint8_t u1clip,
    uint8_t u2clip,
    bool edgeLeft,
    bool edgeRight,
    bool shadeEdge)
#else
void Renderer::DrawWallSegment(
    int16_t x1,
    int16_t w1,
    int16_t x2,
    int16_t w2,
    bool edgeLeft,
    bool edgeRight,
    bool shadeEdge)
#endif
{
    if(x1 < 0) {
#if WITH_TEXTURES
        u1clip += ((int32_t)(0 - x1) * (int32_t)(u2clip - u1clip)) / (x2 - x1);
#endif
        w1 += ((int32_t)(0 - x1) * (int32_t)(w2 - w1)) / (x2 - x1);
        x1 = 0;
        edgeLeft = false;
    }

    int16_t dx = x2 - x1;
    if(dx <= 0) return;

    int16_t werror = dx / 2;
    int16_t w = w1;
    int16_t dw;
    int8_t wstep;

#if WITH_IMAGE_TEXTURES
    uint8_t du = (uint8_t)(u2clip - u1clip);
    int16_t uerror = werror;
    uint8_t u = u1clip;
#endif

    if(w1 < w2) {
        dw = w2 - w1;
        wstep = 1;
    } else {
        dw = w1 - w2;
        wstep = -1;
    }

    constexpr uint8_t edgeColour = COLOUR_BLACK;

    uint8_t segmentClipLeft = (uint8_t)x1;
    uint8_t segmentClipRight = (x2 < DISPLAY_WIDTH) ? (uint8_t)x2 : (uint8_t)(DISPLAY_WIDTH - 1);

    for(int x = x1; x < DISPLAY_WIDTH; x++) {
        // NOTE: x is >= x1 and x1 is clamped to >=0 above, so x>=0 always here.
        bool drawSlice = (wBuffer[x] < w);
        bool shadeSlice = shadeEdge && ((x & 1) == 0);

        // Use wider type for safe math
        int16_t horizon = (int16_t)horizonBuffer[x];

        if(drawSlice) {
            uint8_t sliceMask = 0xff;

            if((edgeLeft && x == x1) || (edgeRight && x == x2)) {
                sliceMask = 0x00;
            } else if(shadeSlice) {
                sliceMask = 0x55;
            }

#if WITH_IMAGE_TEXTURES
            {
                // Clip vertical extents (already safe here)
                uint8_t y1s = (w > horizon) ? 0 : (uint8_t)(horizon - w);
                uint8_t y2s = (horizon + w > DISPLAY_HEIGHT) ? DISPLAY_HEIGHT :
                                                               (uint8_t)(horizon + w);

                DrawVLine(x, y1s, y2s, sliceMask);

                uint16_t textureData = pgm_read_word(&texture[u % 16]);
                const uint16_t wallSize = (uint16_t)(w * 2);
                uint16_t wallPos = (uint16_t)(y1s - (horizon - w));

                for(uint8_t y = y1s; y < y2s; y++) {
                    uint8_t v = (uint8_t)((16u * wallPos) / wallSize);
                    uint16_t mask = pgm_read_word(&scaleDrawReadMasks[v]);

                    if((textureData & mask) == 0) {
                        Platform::PutPixel((uint8_t)x, y, 0);
                    }
                    wallPos++;
                }
            }
#else
            // -------- FIX: hard clip Y to screen to prevent OOB writes --------
            // Original code used horizon±extent without clipping; with extent=64 this goes outside 0..63.
            int16_t extent = (w > 64) ? 64 : w;
            int16_t yTop = horizon - extent;
            int16_t yBot = horizon + extent;

            if(yTop < 0) yTop = 0;
            if(yBot > (DISPLAY_HEIGHT - 1)) yBot = (DISPLAY_HEIGHT - 1);

            if(yTop <= yBot) {
                Platform::DrawVLine((uint8_t)x, (int16_t)yTop, (int16_t)yBot, sliceMask);
                Platform::PutPixel((uint8_t)x, (uint8_t)yTop, edgeColour);
                Platform::PutPixel((uint8_t)x, (uint8_t)yBot, edgeColour);
            }
#endif

            if(wBuffer[x] == 0) {
                numBufferSlicesFilled++;
            }

            wBuffer[x] = (w > 255) ? 255 : (uint8_t)w;
        } else {
            if(x == segmentClipLeft) {
                segmentClipLeft++;
            } else if(x < segmentClipRight) {
                segmentClipRight = (uint8_t)x;
                break;
            }
        }

        if(x == x2) break;

        werror -= dw;
        while(werror < 0) {
            w += wstep;
            werror += dx;

            // These pixels can also go OOB. Clip them too.
            if(drawSlice && werror < 0 && w <= DISPLAY_HEIGHT / 2) {
                int16_t yA = horizon + w - 1;
                int16_t yB = horizon - w;

                if((uint16_t)yA < DISPLAY_HEIGHT)
                    Platform::PutPixel((uint8_t)x, (uint8_t)yA, edgeColour);
                if((uint16_t)yB < DISPLAY_HEIGHT)
                    Platform::PutPixel((uint8_t)x, (uint8_t)yB, edgeColour);
            }
        }

#if WITH_IMAGE_TEXTURES
        uerror -= du;
        while(uerror < 0) {
            u++;
            uerror += dx;
        }
#endif
    }

    if(segmentClipLeft == segmentClipRight) return;

#if WITH_VECTOR_TEXTURES
    if(w1 < MIN_TEXTURE_DISTANCE || w2 < MIN_TEXTURE_DISTANCE || !texture) return;
    if(u1clip == u2clip) return;

    const uint8_t* texPtr = texture;
    uint8_t numLines = pgm_read_byte(texPtr++);
    while(numLines) {
        numLines--;

        uint8_t u1 = pgm_read_byte(texPtr++);
        uint8_t v1 = pgm_read_byte(texPtr++);
        uint8_t u2 = pgm_read_byte(texPtr++);
        uint8_t v2 = pgm_read_byte(texPtr++);

        if(u2 < u1clip || u1 > u2clip) continue;

        if(u1 < u1clip) {
            if(u2 != u1)
                v1 = (uint8_t)(v1 +
                               (uint8_t)((u1clip - u1) * (int16_t)(v2 - v1) / (int16_t)(u2 - u1)));
            u1 = u1clip;
        }
        if(u2 > u2clip) {
            if(u2 != u1)
                v2 = (uint8_t)(v2 +
                               (uint8_t)((u2clip - u2) * (int16_t)(v1 - v2) / (int16_t)(u1 - u2)));
            u2 = u2clip;
        }

        // Normalize u to 0..128 within the clipped interval
        u1 = (uint8_t)((128u * (uint16_t)(u1 - u1clip)) / (uint16_t)(u2clip - u1clip));
        u2 = (uint8_t)((128u * (uint16_t)(u2 - u1clip)) / (uint16_t)(u2clip - u1clip));

        int16_t outU1 = (int16_t)((((int32_t)u1 * dx) >> 7) + x1);
        int16_t outU2 = (int16_t)((((int32_t)u2 * dx) >> 7) + x1);

        int16_t interpw1 = (int16_t)(((int16_t)u1 * (w2 - w1)) >> 7) + w1;
        int16_t interpw2 = (int16_t)(((int16_t)u2 * (w2 - w1)) >> 7) + w1;

        int16_t outV1 = (int16_t)((interpw1 * (int16_t)v1) >> 6);
        int16_t outV2 = (int16_t)((interpw2 * (int16_t)v2) >> 6);

        DrawWallLine(
            outU1,
            (int16_t)(HORIZON - interpw1 + outV1),
            outU2,
            (int16_t)(HORIZON - interpw2 + outV2),
            segmentClipLeft,
            segmentClipRight,
            edgeColour);
    }
#endif
}

bool Renderer::isFrustrumClipped(int16_t x, int16_t y) {
    if((camera.clipCos * (x - camera.cellX) - camera.clipSin * (y - camera.cellY)) < -512)
        return true;
    if((camera.clipSin * (x - camera.cellX) + camera.clipCos * (y - camera.cellY)) < -512)
        return true;

    return false;
}

void Renderer::TransformToViewSpace(int16_t x, int16_t y, int16_t& outX, int16_t& outY) {
    int32_t relX = x - camera.x;
    int32_t relY = y - camera.y;
    outY = (int16_t)((camera.rotCos * relX) >> 8) - (int16_t)((camera.rotSin * relY) >> 8);
    outX = (int16_t)((camera.rotSin * relX) >> 8) + (int16_t)((camera.rotCos * relY) >> 8);
}

void Renderer::TransformToScreenSpace(int16_t viewX, int16_t viewZ, int16_t& outX, int16_t& outW) {
    // apply perspective projection
    outX = (int16_t)((int32_t)viewX * NEAR_PLANE * CAMERA_SCALE / viewZ);
    outW = (int16_t)((CELL_SIZE / 2 * NEAR_PLANE * CAMERA_SCALE) / viewZ);

    // transform into screen space
    outX = (int16_t)((DISPLAY_WIDTH / 2) + outX);
}

#if WITH_IMAGE_TEXTURES
void Renderer::DrawWall(
    const uint16_t* texture,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool edgeLeft,
    bool edgeRight,
    bool shadeEdge)
#elif WITH_VECTOR_TEXTURES
void Renderer::DrawWall(
    const uint8_t* texture,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool edgeLeft,
    bool edgeRight,
    bool shadeEdge)
#else
void Renderer::DrawWall(
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool edgeLeft,
    bool edgeRight,
    bool shadeEdge)
#endif
{
    int16_t viewX1, viewZ1, viewX2, viewZ2;
#if WITH_VECTOR_TEXTURES
    uint8_t u1 = 0, u2 = 128;
#elif WITH_IMAGE_TEXTURES
    uint8_t u1 = 0, u2 = 16;
#endif

    TransformToViewSpace(x1, y1, viewX1, viewZ1);
    TransformToViewSpace(x2, y2, viewX2, viewZ2);

    // Frustum cull
    if(viewX2 < 0 && -2 * viewZ2 > viewX2) return;
    if(viewX1 > 0 && 2 * viewZ1 < viewX1) return;

    // clip to the front pane
    if((viewZ1 < CLIP_PLANE) && (viewZ2 < CLIP_PLANE)) return;

    if(viewZ1 < CLIP_PLANE) {
#if WITH_TEXTURES
        u1 += (CLIP_PLANE - viewZ1) * (u2 - u1) / (viewZ2 - viewZ1);
#endif
        viewX1 += (CLIP_PLANE - viewZ1) * (viewX2 - viewX1) / (viewZ2 - viewZ1);
        viewZ1 = CLIP_PLANE;
        edgeLeft = false;
    } else if(viewZ2 < CLIP_PLANE) {
#if WITH_TEXTURES
        u2 += (CLIP_PLANE - viewZ2) * (u1 - u2) / (viewZ1 - viewZ2);
#endif
        viewX2 += (CLIP_PLANE - viewZ2) * (viewX1 - viewX2) / (viewZ1 - viewZ2);
        viewZ2 = CLIP_PLANE;
        edgeRight = false;
    }

    // apply perspective projection
    int16_t vx1 = (int16_t)((int32_t)viewX1 * NEAR_PLANE * CAMERA_SCALE / viewZ1);
    int16_t vx2 = (int16_t)((int32_t)viewX2 * NEAR_PLANE * CAMERA_SCALE / viewZ2);

    // transform the end points into screen space
    int16_t sx1 = (int16_t)((DISPLAY_WIDTH / 2) + vx1);
    int16_t sx2 = (int16_t)((DISPLAY_WIDTH / 2) + vx2) - 1;

    if(sx1 >= sx2 || sx2 <= 0 || sx1 >= DISPLAY_WIDTH) return;

    int16_t w1 = (int16_t)((CELL_SIZE / 2 * NEAR_PLANE * CAMERA_SCALE) / viewZ1);
    int16_t w2 = (int16_t)((CELL_SIZE / 2 * NEAR_PLANE * CAMERA_SCALE) / viewZ2);

#if WITH_TEXTURES
    DrawWallSegment(texture, sx1, w1, sx2, w2, u1, u2, edgeLeft, edgeRight, shadeEdge);
#else
    DrawWallSegment(sx1, w1, sx2, w2, edgeLeft, edgeRight, shadeEdge);
#endif
}

void swap(int16_t& a, int16_t& b) {
    int16_t temp = a;
    a = b;
    b = temp;
}

void Renderer::DrawFloorLineInner(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    uint8_t color = COLOUR_BLACK;

    bool steep = ABS(y1 - y0) > ABS(x1 - x0);
    if(steep) {
        swap(x0, y0);
        swap(x1, y1);
    }

    if(x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = ABS(y1 - y0);

    int16_t err = dx / 2;
    int8_t ystep;

    if(y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }

    for(; x0 <= x1; x0++) {
        if(steep) {
            if(y0 >= 0 && y0 < DISPLAY_WIDTH && x0 >= 0 && x0 < DISPLAY_HEIGHT &&
               x0 > GetHorizon(y0) + wBuffer[y0] && x0 > GetHorizon(y0) + 8) {
                Platform::PutPixel((uint8_t)y0, (uint8_t)x0 + horizonBuffer[y0] - HORIZON, color);
            }
        } else {
            if(x0 >= 0 && x0 < DISPLAY_WIDTH && y0 >= 0 && y0 < DISPLAY_HEIGHT &&
               y0 > GetHorizon(x0) + wBuffer[x0] && y0 > GetHorizon(x0) + 8) {
                Platform::PutPixel((uint8_t)x0, (uint8_t)y0 + horizonBuffer[x0] - HORIZON, color);
            }
        }

        err -= dy;
        if(err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

void Renderer::DrawFloorLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    int16_t viewX1, viewZ1, viewX2, viewZ2;

    TransformToViewSpace(x1, y1, viewX1, viewZ1);
    TransformToViewSpace(x2, y2, viewX2, viewZ2);

    //if(viewX1 > viewX2)
    //{
    //	swap(viewX1, viewX2);
    //	swap(viewZ1, viewZ2);
    //}

    // Frustum cull
    //	if (viewX2 < 0 && -2 * viewZ2 > viewX2)
    //		return;
    //	if (viewX1 > 0 && 2 * viewZ1 < viewX1)
    //		return;

    // clip to the front pane
    if((viewZ1 < CLIP_PLANE) && (viewZ2 < CLIP_PLANE)) return;

    if(viewZ1 < CLIP_PLANE) {
        viewX1 += (CLIP_PLANE - viewZ1) * (viewX2 - viewX1) / (viewZ2 - viewZ1);
        viewZ1 = CLIP_PLANE;
    } else if(viewZ2 < CLIP_PLANE) {
        viewX2 += (CLIP_PLANE - viewZ2) * (viewX1 - viewX2) / (viewZ1 - viewZ2);
        viewZ2 = CLIP_PLANE;
    }

    // apply perspective projection
    int16_t vx1 = (int16_t)((int32_t)viewX1 * NEAR_PLANE * CAMERA_SCALE / viewZ1);
    int16_t vx2 = (int16_t)((int32_t)viewX2 * NEAR_PLANE * CAMERA_SCALE / viewZ2);

    // transform the end points into screen space
    int16_t sx1 = (int16_t)((DISPLAY_WIDTH / 2) + vx1);
    int16_t sx2 = (int16_t)((DISPLAY_WIDTH / 2) + vx2) - 1;

    //if (sx2 <= 0 || sx1 >= DISPLAY_WIDTH)
    //	return;

    int16_t w1 = (int16_t)((CELL_SIZE / 2 * NEAR_PLANE * CAMERA_SCALE) / viewZ1);
    int16_t w2 = (int16_t)((CELL_SIZE / 2 * NEAR_PLANE * CAMERA_SCALE) / viewZ2);

    DrawFloorLineInner(sx1, HORIZON + w1, sx2, HORIZON + w2);
}

void Renderer::DrawFloorLines() {
    constexpr int size = 10;
    int16_t baseX = (Game::player.x - CELL_SIZE * size / 2) & 0xff00;
    int16_t baseY = (Game::player.y - CELL_SIZE * size / 2) & 0xff00;

    for(int n = 0; n < 10; n++) {
        DrawFloorLine(
            baseX,
            baseY + n * CELL_SIZE,
            baseX + CELL_SIZE * 10 - n * CELL_SIZE,
            baseY + CELL_SIZE * 10);
        DrawFloorLine(baseX, baseY + n * CELL_SIZE, baseX + n * CELL_SIZE, baseY);
    }

    for(int n = 1; n < 10; n++) {
        DrawFloorLine(
            baseX + n * CELL_SIZE,
            baseY,
            baseX + CELL_SIZE * 10,
            baseY + CELL_SIZE * 10 - n * CELL_SIZE);
        DrawFloorLine(
            baseX + n * CELL_SIZE,
            baseY + 10 * CELL_SIZE,
            baseX + 10 * CELL_SIZE,
            baseY + n * CELL_SIZE);
    }
}

void Renderer::DrawCell(uint8_t x, uint8_t y) {
    CellType cellType = Map::GetCellSafe(x, y);

    if(isFrustrumClipped(x, y)) {
        return;
    }

    switch(cellType) {
    case CellType::Torch: {
        const uint16_t* torchSpriteData = Game::globalTickFrame & 4 ? torchSpriteData1 :
                                                                      torchSpriteData2;
        constexpr uint8_t torchScale = 75;

        if(Map::IsSolid(x - 1, y)) {
            DrawObject(
                torchSpriteData,
                x * CELL_SIZE + CELL_SIZE / 7,
                y * CELL_SIZE + CELL_SIZE / 2,
                torchScale,
                AnchorType::Center);
        } else if(Map::IsSolid(x + 1, y)) {
            DrawObject(
                torchSpriteData,
                x * CELL_SIZE + 6 * CELL_SIZE / 7,
                y * CELL_SIZE + CELL_SIZE / 2,
                torchScale,
                AnchorType::Center);
        } else if(Map::IsSolid(x, y - 1)) {
            DrawObject(
                torchSpriteData,
                x * CELL_SIZE + CELL_SIZE / 2,
                y * CELL_SIZE + CELL_SIZE / 7,
                torchScale,
                AnchorType::Center);
        } else if(Map::IsSolid(x, y + 1)) {
            DrawObject(
                torchSpriteData,
                x * CELL_SIZE + CELL_SIZE / 2,
                y * CELL_SIZE + 6 * CELL_SIZE / 7,
                torchScale,
                AnchorType::Center);
        }
    }
        return;
    case CellType::Entrance:
        DrawObject(
            entranceSpriteData,
            x * CELL_SIZE + CELL_SIZE / 2,
            y * CELL_SIZE + CELL_SIZE / 2,
            96,
            AnchorType::Ceiling);
        return;
    case CellType::Exit:
        DrawObject(
            exitSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 96);
        return;
    case CellType::Urn:
        DrawObject(
            urnSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 80);
        return;
    case CellType::Potion:
        DrawObject(
            potionSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 64);
        return;
    case CellType::Scroll:
        DrawObject(
            scrollSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 64);
        return;
    case CellType::Coins:
        DrawObject(
            coinsSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 64);
        return;
    case CellType::Crown:
        DrawObject(
            crownSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 64);
        return;
    case CellType::Sign:
        DrawObject(
            signSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 80);
        return;
    case CellType::Chest:
        DrawObject(
            chestSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 75);
        return;
    case CellType::ChestOpened:
        DrawObject(
            chestOpenSpriteData, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2, 75);
        return;
    default:
        break;
    }

    if(numBufferSlicesFilled >= DISPLAY_WIDTH) {
        return;
    }

    if(!Map::IsSolid(x, y)) {
        return;
    }

    int16_t x1 = x * CELL_SIZE;
    int16_t y1 = y * CELL_SIZE;
    int16_t x2 = x1 + CELL_SIZE;
    int16_t y2 = y1 + CELL_SIZE;

    bool blockedLeft = Map::IsSolid(x - 1, y);
    bool blockedRight = Map::IsSolid(x + 1, y);
    bool blockedUp = Map::IsSolid(x, y - 1);
    bool blockedDown = Map::IsSolid(x, y + 1);

#if WITH_IMAGE_TEXTURES
    const uint16_t* texture = wallTextureData + (16 * (cellType - 1));
#elif WITH_VECTOR_TEXTURES
    const uint8_t* texture =
        vectorTexture0; // (const uint8_t*) pgm_read_ptr(&textures[(uint8_t)cellType - (uint8_t)CellType::FirstSolidCell]);
#endif

    if(!blockedLeft && camera.x < x1) {
#if WITH_TEXTURES
        DrawWall(
            texture,
            x1,
            y1,
            x1,
            y2,
            !blockedUp && camera.y > y1,
            !blockedDown && camera.y < y2,
            true);
#else
        DrawWall(x1, y1, x1, y2, !blockedUp && camera.y > y1, !blockedDown && camera.y < y2, true);
#endif
    }

    if(!blockedDown && camera.y > y2) {
#if WITH_TEXTURES
        DrawWall(
            texture,
            x1,
            y2,
            x2,
            y2,
            !blockedLeft && camera.x > x1,
            !blockedRight && camera.x < x2,
            false);
#else
        DrawWall(
            x1, y2, x2, y2, !blockedLeft && camera.x > x1, !blockedRight && camera.x < x2, false);
#endif
    }

    if(!blockedRight && camera.x > x2) {
#if WITH_TEXTURES
        DrawWall(
            texture,
            x2,
            y2,
            x2,
            y1,
            !blockedDown && camera.y < y2,
            !blockedUp && camera.y > y1,
            true);
#else
        DrawWall(x2, y2, x2, y1, !blockedDown && camera.y < y2, !blockedUp && camera.y > y1, true);
#endif
    }

    if(!blockedUp && camera.y < y1) {
#if WITH_TEXTURES
        DrawWall(
            texture,
            x2,
            y1,
            x1,
            y1,
            !blockedRight && camera.x < x2,
            !blockedLeft && camera.x > x1,
            false);
#else
        DrawWall(
            x2, y1, x1, y1, !blockedRight && camera.x < x2, !blockedLeft && camera.x > x1, false);
#endif
    }
}

void Renderer::DrawCells() {
    constexpr int8_t MAP_BUFFER_WIDTH = 16;
    constexpr int8_t MAP_BUFFER_HEIGHT = 16;

    int16_t cosAngle = FixedCos(camera.angle);
    int16_t sinAngle = FixedSin(camera.angle);

    int8_t bufferX = (int8_t)((camera.x + cosAngle * 7) >> 8) - MAP_BUFFER_WIDTH / 2;
    int8_t bufferY = (int8_t)((camera.y + sinAngle * 7) >> 8) - MAP_BUFFER_WIDTH / 2;
    ;

    if(bufferX < 0) bufferX = 0;
    if(bufferY < 0) bufferY = 0;
    if(bufferX > Map::width - MAP_BUFFER_WIDTH) bufferX = Map::width - MAP_BUFFER_WIDTH;
    if(bufferY > Map::height - MAP_BUFFER_HEIGHT) bufferY = Map::height - MAP_BUFFER_HEIGHT;

    // This should make cells draw front to back

    int8_t xd, yd;
    int8_t x1, y1, x2, y2;

    if(camera.rotCos > 0) {
        x1 = bufferX;
        x2 = x1 + MAP_BUFFER_WIDTH;
        xd = 1;
    } else {
        x2 = bufferX - 1;
        x1 = x2 + MAP_BUFFER_WIDTH;
        xd = -1;
    }
    if(camera.rotSin < 0) {
        y1 = bufferY;
        y2 = y1 + MAP_BUFFER_HEIGHT;
        yd = 1;
    } else {
        y2 = bufferY - 1;
        y1 = y2 + MAP_BUFFER_HEIGHT;
        yd = -1;
    }

    if(ABS(camera.rotCos) < ABS(camera.rotSin)) {
        for(int8_t y = y1; y != y2; y += yd) {
            for(int8_t x = x1; x != x2; x += xd) {
                DrawCell(x, y);
            }
        }
    } else {
        for(int8_t x = x1; x != x2; x += xd) {
            for(int8_t y = y1; y != y2; y += yd) {
                DrawCell(x, y);
            }
        }
    }
}

void DrawScaledOutline(
    const uint16_t* data,
    int8_t x,
    int8_t y,
    uint8_t halfSize,
    uint8_t inverseCameraDistance,
    uint8_t shiftAmount,
    bool invert) {
    uint8_t size = 2 * halfSize;
    const uint8_t* lut =
        scaleLUT + (((halfSize - 1) >> shiftAmount) * ((halfSize - 1) >> shiftAmount));

    uint8_t i0 = x < 0 ? -x : 0;
    uint8_t i1 = x + size > DISPLAY_WIDTH ? DISPLAY_WIDTH - x : size;
    uint8_t j0 = y < 0 ? -y : 0;
    uint8_t j1 = y + size > DISPLAY_HEIGHT ? DISPLAY_HEIGHT - y : size;

    int8_t outX = x >= 0 ? x : 0;
    uint16_t invertMask = invert ? 0xffff : 0;

    uint16_t leftTransparencyAndColourColumn = 0;
    uint16_t middleTransparencyColumn = 0;
    uint16_t rightTransparencyColumn = 0;
    uint16_t middleColourColumn = 0;
    uint16_t rightColourColumn = 0;
    bool wasVisible = false;

    for(uint8_t i = i0; i < i1; i++) {
        const bool isVisible = Renderer::wBuffer[outX] < inverseCameraDistance;

        if(isVisible) {
            uint16_t leftRightOutlineColumn = 0;

            if(i >= i1 - 2) {
                if(wasVisible) {
                    leftTransparencyAndColourColumn = middleColourColumn &
                                                      middleTransparencyColumn;
                    middleColourColumn = rightColourColumn;
                    middleTransparencyColumn = rightTransparencyColumn;
                    rightTransparencyColumn = 0;
                    rightColourColumn = 0;
                    leftRightOutlineColumn = leftTransparencyAndColourColumn;
                } else {
                    break;
                }
            } else {
                const uint8_t u = pgm_read_byte(&lut[i >> shiftAmount]);

                if(wasVisible) {
                    leftTransparencyAndColourColumn = middleColourColumn &
                                                      middleTransparencyColumn;
                    middleColourColumn = rightColourColumn;
                    middleTransparencyColumn = rightTransparencyColumn;
                    rightTransparencyColumn = pgm_read_word(&data[u * 2]);
                    rightColourColumn = pgm_read_word(&data[u * 2 + 1]) ^ invertMask;
                    leftRightOutlineColumn = leftTransparencyAndColourColumn |
                                             (rightColourColumn & rightTransparencyColumn);
                } else {
                    leftTransparencyAndColourColumn = 0;
                    rightTransparencyColumn = pgm_read_word(&data[u * 2]);
                    rightColourColumn = pgm_read_word(&data[u * 2 + 1]) ^ invertMask;
                    middleColourColumn = rightColourColumn;
                    middleTransparencyColumn = rightTransparencyColumn;
                    leftRightOutlineColumn = (rightColourColumn & rightTransparencyColumn);
                }
            }

            int8_t outY = y >= 0 ? y : 0;
            uint8_t bufferPos = (outY & 7);
            uint8_t* screenBuffer = Platform::GetScreenBuffer() + outX + ((outY & 0x38) << 4);
            uint8_t localBuffer = *screenBuffer;
            uint8_t writeMask = pgm_read_byte(&scaleDrawWriteMasks[bufferPos]);

            bool upIsOpaqueAndWhite = false;
            bool middleIsOpaque = false;
            bool downIsOpaque = false;
            bool middleIsWhite = false;
            bool downIsWhite = false;
            bool leftOrRightIsOutline = false;
            bool downLeftOrRightIsOutline = false;

            for(uint8_t j = j0; j < j1; j++) {
                upIsOpaqueAndWhite = middleIsOpaque && middleIsWhite;
                middleIsOpaque = downIsOpaque;
                middleIsWhite = downIsWhite;
                leftOrRightIsOutline = downLeftOrRightIsOutline;

                if(j >= j1 - 2) {
                    downIsOpaque = false;
                    downIsWhite = false;
                    downLeftOrRightIsOutline = false;
                } else {
                    uint8_t v = pgm_read_byte(&lut[j >> shiftAmount]);
                    uint16_t mask = pgm_read_word(&scaleDrawReadMasks[v]);
                    downLeftOrRightIsOutline = (leftRightOutlineColumn & mask) != 0;
                    downIsOpaque = (middleTransparencyColumn & mask) != 0;
                    downIsWhite = (middleColourColumn & mask) != 0;
                }

                if(middleIsOpaque && j < j1) {
                    if(middleIsWhite) {
                        localBuffer |= writeMask;
                    } else {
                        localBuffer &= ~writeMask;
                    }
                } else if(
                    leftOrRightIsOutline || (upIsOpaqueAndWhite) ||
                    (downIsOpaque && downIsWhite)) {
                    localBuffer &= ~writeMask;
                }

                outY++;
                bufferPos++;
                writeMask <<= 1;

                if(bufferPos == 8) {
                    bufferPos = 0;
                    writeMask = 1;

                    *screenBuffer = localBuffer;
                    if(outY < DISPLAY_HEIGHT) {
                        screenBuffer += 128;
                    }
                    localBuffer = *screenBuffer;
                }
            }

            *screenBuffer = localBuffer;
        }

        outX++;
        wasVisible = isVisible;
    }
}

template <int scaleMultiplier>
inline void DrawScaledNoOutline(
    const uint16_t* data,
    int8_t x,
    int8_t y,
    uint8_t halfSize,
    uint8_t inverseCameraDistance) {
    uint8_t size = 2 * halfSize;
    const uint8_t* lut = scaleLUT + ((halfSize / scaleMultiplier) * (halfSize / scaleMultiplier));

    uint8_t i0 = x < 0 ? -x : 0;
    uint8_t i1 = x + size > DISPLAY_WIDTH ? DISPLAY_WIDTH - x : size;
    uint8_t j0 = y < 0 ? -y : 0;
    uint8_t j1 = y + size > DISPLAY_HEIGHT ? DISPLAY_HEIGHT - y : size;

    int8_t outX = x >= 0 ? x : 0;

    for(uint8_t i = i0; i < i1; i++) {
        const bool isVisible = Renderer::wBuffer[outX] < inverseCameraDistance;

        if(isVisible) {
            const uint8_t u = pgm_read_byte(&lut[i / scaleMultiplier]);
            int8_t outY = y >= 0 ? y : 0;
            uint8_t bufferPos = (outY & 7);
            uint8_t* screenBuffer = Platform::GetScreenBuffer() + outX + ((outY & 0x38) << 4);
            uint8_t localBuffer = *screenBuffer;
            uint8_t writeMask = pgm_read_byte(&scaleDrawWriteMasks[bufferPos]);
            uint16_t transparencyColumn = pgm_read_word(&data[u * 2]);
            uint16_t colourColumn = pgm_read_word(&data[u * 2 + 1]);

            for(uint8_t j = j0; j < j1; j += scaleMultiplier) {
                uint8_t v = pgm_read_byte(&lut[j / scaleMultiplier]);
                uint16_t mask = pgm_read_word(&scaleDrawReadMasks[v]);

                for(uint8_t k = 0; k < scaleMultiplier; k++) {
                    bool isOpaque = (transparencyColumn & mask) != 0;

                    if(isOpaque) {
                        bool isWhite = (colourColumn & mask) != 0;

                        if(isWhite) {
                            localBuffer |= writeMask;
                        } else {
                            localBuffer &= ~writeMask;
                        }
                    }

                    outY++;
                    bufferPos++;
                    writeMask <<= 1;

                    if(bufferPos == 8) {
                        bufferPos = 0;
                        writeMask = 1;

                        *screenBuffer = localBuffer;
                        if(outY < DISPLAY_HEIGHT) {
                            screenBuffer += 128;
                        }
                        localBuffer = *screenBuffer;
                    }
                }
            }

            *screenBuffer = localBuffer;
        }

        outX++;
    }
}

void Renderer::DrawScaled(
    const uint16_t* data,
    int8_t x,
    int8_t y,
    uint8_t halfSize,
    uint8_t inverseCameraDistance,
    bool invert,
    uint8_t color) {
    if(halfSize > MAX_SPRITE_SIZE * 2) {
        return;
    } else if(halfSize > MAX_SPRITE_SIZE) {
        DrawScaledInner(data, x, y, halfSize, inverseCameraDistance, 2, invert);
    } else if(halfSize * 2 > MAX_SPRITE_SIZE) {
        DrawScaledInner(data, x, y, halfSize, inverseCameraDistance, 1, invert);
    } else if(halfSize > 2) {
        DrawScaledInner(data, x, y, halfSize, inverseCameraDistance, 0, invert);
    } else if(halfSize == 2) {
        if(Renderer::wBuffer[x] < inverseCameraDistance) {
            Platform::PutPixel(x, y, color);
            Platform::PutPixel(x, y + 1, color);
        }
        if(Renderer::wBuffer[x + 1] < inverseCameraDistance) {
            Platform::PutPixel(x + 1, y, color);
            Platform::PutPixel(x + 1, y + 1, color);
        }
    } else {
        if(Renderer::wBuffer[x] < inverseCameraDistance) {
            Platform::PutPixel(x, y, color);
        }
    }
}

QueuedDrawable* Renderer::CreateQueuedDrawable(uint8_t inverseCameraDistance) {
    uint8_t insertionPoint = MAX_QUEUED_DRAWABLES;

    for(uint8_t n = 0; n < numQueuedDrawables; n++) {
        if(inverseCameraDistance < queuedDrawables[n].inverseCameraDistance) {
            if(numQueuedDrawables < MAX_QUEUED_DRAWABLES) {
                insertionPoint = n;
                numQueuedDrawables++;

                for(uint8_t i = numQueuedDrawables - 1; i > n; i--) {
                    queuedDrawables[i] = queuedDrawables[i - 1];
                }
            } else {
                if(n == 0) {
                    // List is full and this is smaller than the first element so just cull
                    return nullptr;
                }

                // Drop the smallest element to make a space
                for(uint8_t i = 0; i < n - 1; i++) {
                    queuedDrawables[i] = queuedDrawables[i + 1];
                }

                insertionPoint = n - 1;
            }

            break;
        }
    }

    if(insertionPoint == MAX_QUEUED_DRAWABLES) {
        if(numQueuedDrawables < MAX_QUEUED_DRAWABLES) {
            insertionPoint = numQueuedDrawables;
            numQueuedDrawables++;
        } else if(
            inverseCameraDistance >
            queuedDrawables[numQueuedDrawables - 1].inverseCameraDistance) {
            // Drop the smallest element to make a space
            for(uint8_t i = 0; i < numQueuedDrawables - 1; i++) {
                queuedDrawables[i] = queuedDrawables[i + 1];
            }
            insertionPoint = numQueuedDrawables - 1;
        } else {
            return nullptr;
        }
    }

    return &queuedDrawables[insertionPoint];
}

void Renderer::QueueSprite(
    const uint16_t* data,
    int8_t x,
    int8_t y,
    uint8_t halfSize,
    uint8_t inverseCameraDistance,
    bool invert) {
    if(x < -halfSize * 2) return;
    //if(x >= DISPLAY_WIDTH)
    //	return;
    //if(halfSize <= 2)
    //	return;

    QueuedDrawable* drawable = CreateQueuedDrawable(inverseCameraDistance);

    if(drawable != nullptr) {
        drawable->type = DrawableType::Sprite;
        drawable->spriteData = data;
        drawable->x = x;
        drawable->y = y;
        drawable->halfSize = halfSize;
        drawable->inverseCameraDistance = inverseCameraDistance;
        drawable->invert = invert;
    }
}

void Renderer::RenderQueuedDrawables() {
    for(uint8_t n = 0; n < numQueuedDrawables; n++) {
        QueuedDrawable& drawable = queuedDrawables[n];

        if(drawable.type == DrawableType::Sprite) {
            DrawScaled(
                drawable.spriteData,
                drawable.x,
                drawable.y,
                drawable.halfSize,
                drawable.inverseCameraDistance,
                drawable.invert);
        } else {
            drawable.particleSystem->Draw(drawable.x, drawable.inverseCameraDistance);
        }
    }
}

int8_t Renderer::GetHorizon(int16_t x) {
    if(x < 0) x = 0;
    if(x >= DISPLAY_WIDTH) x = DISPLAY_WIDTH - 1;
    return horizonBuffer[x];
}

bool Renderer::TransformAndCull(
    int16_t worldX,
    int16_t worldY,
    int16_t& outScreenX,
    int16_t& outScreenW) {
    int16_t relX, relZ;
    TransformToViewSpace(worldX, worldY, relX, relZ);

    // Frustum cull
    if(relZ < CLIP_PLANE) return false;

    if(relX < 0 && -2 * relZ > relX) return false;
    if(relX > 0 && 2 * relZ < relX) return false;

    TransformToScreenSpace(relX, relZ, outScreenX, outScreenW);

    return true;
}

void Renderer::DrawObject(
    const uint16_t* spriteData,
    int16_t x,
    int16_t y,
    uint8_t scale,
    AnchorType anchor,
    bool invert) {
    int16_t screenX, screenW;

    if(TransformAndCull(x, y, screenX, screenW)) {
        // Bit of a hack: nudge sorting closer to the camera
        uint8_t inverseCameraDistance = (uint8_t)(screenW + 1);
        int16_t spriteSize = (screenW * scale) / 128;
        int8_t outY = GetHorizon(screenX);

        switch(anchor) {
        case AnchorType::Floor:
            outY += screenW - 2 * spriteSize;
            break;
        case AnchorType::Center:
            outY -= spriteSize;
            break;
        case AnchorType::BelowCenter:
            break;
        case AnchorType::Ceiling:
            outY -= screenW;
            break;
        }

        QueueSprite(
            spriteData,
            screenX - spriteSize,
            outY,
            (uint8_t)spriteSize,
            inverseCameraDistance,
            invert);
    }
}

void Renderer::DrawWeapon() {
    int x = DISPLAY_WIDTH / 2 + 22 + camera.tilt / 4;
    int y = DISPLAY_HEIGHT - 21 - camera.bob;
    uint8_t reloadTime = Game::player.reloadTime;

    if(reloadTime > 0) {
        Platform::DrawSprite(x - reloadTime / 3 - 1, y - reloadTime / 3 - 1, handSpriteData2, 0);
        //DrawSprite(x - reloadTime / 3 - 1, y - reloadTime / 3 - 1, handSpriteData2, handSpriteData2_mask, 0, 0);
    } else {
        Platform::DrawSprite(x + 2, y + 2, handSpriteData1, 0);
        //DrawSprite(x + 2, y + 2, handSpriteData1, handSpriteData1_mask, 0, 0);
    }
}

void Renderer::DrawBackground() {
    uint8_t* ptr = Platform::GetScreenBuffer();
    uint8_t counter = 128;

    while(counter--) {
        *ptr++ = 0x55;
        *ptr++ = 0x00;
        *ptr++ = 0xaa;
        *ptr++ = 0x00;
    }

    counter = 128;
    while(counter--) {
        *ptr++ = 0x55;
        *ptr++ = 0xff;
        *ptr++ = 0xaa;
        *ptr++ = 0xff;
    }
}

void Renderer::DrawBar(uint8_t* screenPtr, const uint8_t* iconData, uint8_t amount, uint8_t max) {
    constexpr uint8_t iconWidth = 8;
    constexpr uint8_t barWidth = 32;
    constexpr uint8_t unfilledBar = 0xfe;
    constexpr uint8_t filledBar = 0xc6;

    uint8_t fillAmount = (amount * barWidth) / max;
    uint8_t x = 0;

    while(x < iconWidth) {
        screenPtr[x] = pgm_read_byte(&iconData[x]);
        x++;
    }

    while(fillAmount--) {
        screenPtr[x++] = filledBar;
    }

    while(x < barWidth + iconWidth) {
        screenPtr[x++] = unfilledBar;
    }

    screenPtr[x++] = unfilledBar;
    screenPtr[x] = 0;
}

void Renderer::DrawDamageIndicator() {
    uint8_t* upper = Platform::GetScreenBuffer();
    uint8_t* lower = upper + DISPLAY_WIDTH * 7;

    for(int x = 1; x < DISPLAY_WIDTH - 1; x++) {
        upper[x] &= 0xfe;
        lower[x] &= 0x7f;
    }

    uint8_t* ptr = Platform::GetScreenBuffer();
    for(int y = 0; y < DISPLAY_HEIGHT / 8; y++) {
        *ptr = 0;
        ptr += (DISPLAY_WIDTH - 1);
        *ptr = 0;
        ptr++;
    }
}

void Renderer::DrawHUD() {
    //constexpr uint8_t barWidth = 40;
    uint8_t* screenBuffer = Platform::GetScreenBuffer();
    //uint8_t* screenPtr = screenBuffer + DISPLAY_WIDTH * 6;

    DrawBar(
        screenBuffer + DISPLAY_WIDTH * 7, heartSpriteData, Game::player.hp, Game::player.maxHP);
    DrawBar(
        screenBuffer + DISPLAY_WIDTH * 6, manaSpriteData, Game::player.mana, Game::player.maxMana);

    if(Game::player.damageTime > 0) DrawDamageIndicator();

    if(Game::displayMessage) Font::PrintString(Game::displayMessage, 0, 0);
}

void Renderer::Render() {
    globalRenderFrame++;

    DrawBackground();

    numBufferSlicesFilled = 0;
    numQueuedDrawables = 0;

    for(uint8_t n = 0; n < DISPLAY_WIDTH; n++) {
        wBuffer[n] = 0;
        horizonBuffer[n] = HORIZON + (((DISPLAY_WIDTH / 2 - n) * camera.tilt) >> 8) + camera.bob;
    }

    camera.cellX = camera.x / CELL_SIZE;
    camera.cellY = camera.y / CELL_SIZE;

    camera.rotCos = FixedCos(-camera.angle);
    camera.rotSin = FixedSin(-camera.angle);
    camera.clipCos = FixedCos(-camera.angle + CLIP_ANGLE);
    camera.clipSin = FixedSin(-camera.angle + CLIP_ANGLE);

    DrawCells();
    //DrawFloorLines();

    EnemyManager::Draw();
    ProjectileManager::Draw();
    ParticleSystemManager::Draw();

    RenderQueuedDrawables();

    DrawWeapon();

    DrawHUD();

    //Map::DrawMinimap();
}
