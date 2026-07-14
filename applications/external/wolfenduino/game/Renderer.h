#ifndef RENDERER_H_
#define RENDERER_H_

#include <stdint.h>
#include "game/Engine.h"
#include "game/Defines.h"
#include "game/FixedMath.h"
#include "game/SpriteFrame.h"

#define NULL_QUEUE_ITEM       0xff
#define RENDER_QUEUE_CAPACITY 16

enum {
    UI_Key1,
    UI_Key2,
    UI_BJFace,
    UI_BJFace_Dead,
    UI_BJFace_Baby,
    UI_BJFace_Easy,
    UI_BJFace_Medium,
    UI_BJFace_Hard,
    UI_Gun,
};

struct RenderQueueItem {
    SpriteFrame* frame;
    uint24_t spriteAddress;
    uint8_t x, w;
    uint8_t next;
};

class Renderer {
public:
    void init();
    void drawFrame();
    void queueSprite(SpriteFrame* frame, uint24_t spriteAddress, int16_t x, int16_t z);

    void drawGlyph(char glyph, uint8_t x, uint8_t y, uint8_t colour = 0);
    void drawString(const char* str, uint8_t x, uint8_t y, uint8_t colour = 0);
    void drawInt(int8_t val, uint8_t x, uint8_t y, uint8_t colour = 0);
    void drawLong(int32_t val, uint8_t x, uint8_t y, uint8_t colour = 0);
    void drawBox(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t colour);
    void drawSprite2D(uint8_t spriteId, int16_t x, int16_t y);
    void drawLevelLoadScreen();
    void drawBackground(uint24_t address);
    void fadeScreen(int8_t amount);

#ifdef DEFER_RENDER
    void drawDeferredFrame();
#endif

    int8_t damageIndicator;

    inline bool isFrustrumClipped(int16_t x, int16_t z) {
        if((view.clipCos * (x - view.cellX) - view.clipSin * (z - view.cellZ)) < -FIXED_ONE)
            return true;
        if((view.clipSin * (x - view.cellX) + view.clipCos * (z - view.cellZ)) < -FIXED_ONE)
            return true;

        return false;
    }

    struct {
        int16_t x, z;
        int16_t cellX, cellZ;
        int16_t rotCos, rotSin;
        int16_t clipCos, clipSin;
    } view;

private:
    void initWBuffer();
    void drawFloorAndCeiling();
    void drawCell(int8_t cellX, int8_t cellZ);
    /*inline*/ void drawStrip(int16_t x, int16_t w, int8_t u, uint8_t textureId);
    void drawWall(
        int16_t _x1,
        int16_t _z1,
        int16_t _x2,
        int16_t _z2,
        uint8_t textureId = 0,
        int8_t _u1 = 0,
        int8_t _u2 = TEXTURE_SIZE - 1);
    void drawFrustumCells();
    void drawBufferedCells();
    void drawDoors();

    void drawQueuedSprite(uint8_t id);
    void drawWeapon();
    void drawDamage();
    void drawHUD();

    /*	int16_t xpos, zpos;
	int16_t cos_dir;
	int16_t sin_dir;
	int8_t xcell, zcell;
	int8_t numColumns;*/
    uint8_t wbuffer[DISPLAYWIDTH];

#ifdef DEFER_RENDER
    uint8_t texbuffer[DISPLAYWIDTH];
    uint8_t ubuffer[DISPLAYWIDTH];
#endif

    uint8_t renderQueueHead;
    RenderQueueItem renderQueue[RENDER_QUEUE_CAPACITY];
    int numBufferSlicesFilled;
};

class BitPairReader {
public:
    BitPairReader(uint8_t* ptr, uint16_t offset = 0) {
        uint16_t byteOffset = offset >> 2;
        m_readOffset = (offset - (byteOffset << 2)) << 1;
        m_ptr = ptr + byteOffset;
        m_lastRead = pgm_read_byte(m_ptr);
    }

    uint8_t read() {
        uint8_t result = (m_lastRead & (3 << m_readOffset)) >> m_readOffset;
        m_readOffset += 2;
        if(m_readOffset == 8) {
            m_ptr++;
            m_lastRead = pgm_read_byte(m_ptr);
            m_readOffset = 0;
        }

        return result;
    }

private:
    uint8_t* m_ptr;
    uint8_t m_lastRead;
    uint8_t m_readOffset;
};

#endif
