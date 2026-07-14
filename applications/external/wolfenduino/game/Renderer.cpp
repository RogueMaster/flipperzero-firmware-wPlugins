#include "game/Engine.h"
#include "game/Renderer.h"
#include "game/FixedMath.h"
#include "game/TileTypes.h"

#include "game/Generated/fxdata.h"
//#include "game/Generated/Data_Walls.h"
#include "game/Generated/Data_Pistol.h"
#include "game/Generated/Data_Knife.h"
#include "game/Generated/Data_Machinegun.h"
#include "game/Generated/Data_Chaingun.h"
#include "game/Generated/Data_Decorations.h"
#include "game/Generated/Data_BlockingDecorations.h"
#include "game/Generated/Data_Items.h"
#include "game/Generated/Data_Font.h"
#include "game/Generated/Data_UI.h"

#include <stdio.h>

void Renderer::init() {
}

void Renderer::drawDamage() {
    if(damageIndicator > 0) {
        damageIndicator--;
        for(int x = 0; x < DISPLAYWIDTH; x++) {
            setPixel(x, 0);
            setPixel(x, DISPLAYHEIGHT - 1);
        }
        for(int y = 0; y < DISPLAYHEIGHT; y++) {
            setPixel(0, y);
            setPixel(DISPLAYWIDTH - 1, y);
        }
    } else if(damageIndicator < 0) {
        damageIndicator++;
        for(int x = 0; x < DISPLAYWIDTH; x++) {
            clearPixel(x, 0);
            clearPixel(x, DISPLAYHEIGHT - 1);
        }
        for(int y = 0; y < DISPLAYHEIGHT; y++) {
            clearPixel(0, y);
            clearPixel(DISPLAYWIDTH - 1, y);
        }
    }
}

void Renderer::drawSprite2D(uint8_t id, int16_t x, int16_t y) {
    uint8_t* buffer = engine.streamBuffer;
    const SpriteFrame* frame = &Data_uiSprite_frames[id];
    uint24_t spriteAddress = Data_uiSprite;

    spriteAddress += pgm_read_word(&frame->offset);

    uint8_t frameWidth = pgm_read_byte(&frame->width);
    uint8_t frameHeight = pgm_read_byte(&frame->height);
    uint8_t xOffset = pgm_read_byte(&frame->xOffset);

    x += xOffset;

    for(int8_t i = 0; i < frameWidth; i++) {
        diskRead(spriteAddress, buffer, frameHeight);
        uint8_t v = 0;

        for(int8_t j = frameHeight - 1; j >= 0; j--) {
            uint8_t pixel = buffer[v++];
            switch(pixel) {
            case 1:
                arduboy.drawPixel(i + x, y + j, 1);
                break;
            case 2:
                arduboy.drawPixel(i + x, y + j, 0);
                break;
            }
        }

        spriteAddress += frameHeight;
    }
}

void Renderer::drawWeapon() {
    SpriteFrame* frame;
    uint24_t address;

    switch(engine.player.weapon.type) {
    case WeaponType_Knife:
        frame = (SpriteFrame*)&Data_knifeSprite_frames[engine.player.weapon.frame];
        address = Data_knifeSprite;
        break;
    case WeaponType_Pistol:
        frame = (SpriteFrame*)&Data_pistolSprite_frames[engine.player.weapon.frame];
        address = Data_pistolSprite;
        break;
    case WeaponType_MachineGun:
        frame = (SpriteFrame*)&Data_machinegunSprite_frames[engine.player.weapon.frame];
        address = Data_machinegunSprite;
        break;
    case WeaponType_ChainGun:
        frame = (SpriteFrame*)&Data_chaingunSprite_frames[engine.player.weapon.frame];
        address = Data_chaingunSprite;
        break;
    default:
        return;
    }

    uint8_t* buffer = engine.streamBuffer;
    address += pgm_read_word(&frame->offset);

    uint8_t frameWidth = pgm_read_byte(&frame->width);
    uint8_t frameHeight = pgm_read_byte(&frame->height);
    uint8_t x = HALF_DISPLAYWIDTH - 32 + pgm_read_byte(&frame->xOffset);

    for(int8_t i = 0; i < frameWidth; i++) {
        diskRead(address, buffer, frameHeight);
        uint8_t v = 0;

        for(int8_t j = frameHeight - 1; j >= 0; j--) {
            uint8_t pixel = buffer[v++];
            switch(pixel) {
            case 1:
                arduboy.drawPixel(i + x, DISPLAYHEIGHT - frameHeight + j, 1);
                break;
            case 2:
                arduboy.drawPixel(i + x, DISPLAYHEIGHT - frameHeight + j, 0);
                break;
            }
        }

        address += frameHeight;
    }
}

void Renderer::drawFrame() {
    renderQueueHead = NULL_QUEUE_ITEM;
    numBufferSlicesFilled = 0;
    for(int8_t n = 0; n < RENDER_QUEUE_CAPACITY; n++) {
        renderQueue[n].spriteAddress = 0;
    }

    view.x = engine.player.x;
    view.z = engine.player.z;
    view.rotCos = FixedMath::Cos(-engine.player.direction);
    view.rotSin = FixedMath::Sin(-engine.player.direction);
    view.clipCos = FixedMath::Cos(-engine.player.direction + DEGREES_90 / 2);
    view.clipSin = FixedMath::Sin(-engine.player.direction + DEGREES_90 / 2);

    view.cellX = WORLD_TO_CELL(engine.player.x);
    view.cellZ = WORLD_TO_CELL(engine.player.z);
    initWBuffer();

#if !defined(DEFER_RENDER)
    drawFloorAndCeiling();
#endif

    drawBufferedCells();
    drawDoors();

    for(int8_t n = 0; n < MAX_ACTIVE_ACTORS; n++) {
        if(engine.actors[n].type != ActorType_Empty && !engine.actors[n].flags.frozen) {
            engine.actors[n].draw();
        }
    }

    for(int8_t n = 0; n < MAX_ACTIVE_ITEMS; n++) {
        if(engine.map.items[n].type != 0) {
            int16_t x = CELL_TO_WORLD(engine.map.items[n].x) + CELL_SIZE / 2,
                    z = CELL_TO_WORLD(engine.map.items[n].z) + CELL_SIZE / 2;
            queueSprite(
                (SpriteFrame*)&Data_itemSprites_frames[(engine.map.items[n].type - Tile_FirstItem)],
                Data_itemSprites,
                x,
                z);
        }
    }

#if 0
	int fill1 = 0;
	int fill2 = 0;
	for(int i = engine.map.bufferX; i < engine.map.bufferX + MAP_BUFFER_SIZE; i++)
	{
		for(int j = engine.map.bufferZ; j < engine.map.bufferZ + MAP_BUFFER_SIZE; j++)
		{
			uint8_t tile = engine.map.getTile(i, j);
			uint8_t colour = 1;

			if(!((view.clipCos * (i - view.cellX) - view.clipSin * (j - view.cellZ)) <= 0))
				fill1 ++;
			if(!isFrustrumClipped(i, j))
				fill2 ++;
			if(tile >= Tile_FirstWall && tile <= Tile_LastWall)
			{
				colour = 0;
				if((view.clipCos * (i - view.cellX) - view.clipSin * (j - view.cellZ)) <= 0)
					colour = 1;
				colour = isFrustrumClipped(i, j) ? 1 : 0;
			}
			drawPixel(i - engine.map.bufferX, j - engine.map.bufferZ, colour);
		}
	}
	WARNING("Old: %d\tNew: %d\t Diff=%f\n", fill1, fill2, (float)fill2 / (float)fill1);
	drawPixel(view.cellX - engine.map.bufferX, view.cellZ - engine.map.bufferZ, 0);

#endif
#if !defined(DEFER_RENDER)
    for(uint8_t item = renderQueueHead; item != NULL_QUEUE_ITEM; item = renderQueue[item].next) {
        drawQueuedSprite(item);
    }

    if(engine.player.hp > 0) {
        drawWeapon();
    }
    drawDamage();

    drawHUD();
#endif
}

void Renderer::drawBox(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t colour) {
    while(h > 0) {
        for(int i = 0; i < w; i++) {
            arduboy.drawPixel(x + i, y, colour);
        }
        y++;
        h--;
    }
}

void Renderer::drawHUD() {
    constexpr int statusBarHeight = 12;
    drawBox(0, DISPLAYHEIGHT - statusBarHeight, 48, statusBarHeight, 0);
    drawBox(DISPLAYWIDTH - 48, DISPLAYHEIGHT - statusBarHeight, 48, statusBarHeight, 0);

    drawBox(DISPLAYWIDTH - 48 + 1, DISPLAYHEIGHT - statusBarHeight + 1, 7, statusBarHeight - 1, 1);
    if(engine.player.inventory.hasKey1 ||
       (engine.player.blinkKeyTimer && (engine.frameCount & 0x4))) {
        drawSprite2D(UI_Key1, DISPLAYWIDTH - 48 + 1, DISPLAYHEIGHT - statusBarHeight + 1);
    }

    // No need for key 2 in shareware
    //drawBox(DISPLAYWIDTH - 48 + 9, DISPLAYHEIGHT - statusBarHeight + 1, 7, statusBarHeight - 1, 1);
    //if (engine.player.inventory.hasKey2)
    //{
    //	drawSprite2D(UI_Key2, DISPLAYWIDTH - 48 + 9, DISPLAYHEIGHT - statusBarHeight + 1);
    //}

    drawBox(26, DISPLAYHEIGHT - statusBarHeight + 1, 1, statusBarHeight - 1, 1);

    int textLine = DISPLAYHEIGHT - FONT_HEIGHT * 2 - 1;
    int numberLine = DISPLAYHEIGHT - FONT_HEIGHT;

    drawString(PSTR("HEALTH"), 1, textLine, 1);
    drawGlyph('%' - FIRST_FONT_GLYPH, 18, numberLine, 1);
    drawInt(engine.player.hp, 14, numberLine, 1);
    drawString(PSTR("AMMO"), 30, textLine, 1);
    drawInt(engine.player.weapon.ammo, 38, numberLine, 1);

    drawString(PSTR("SCORE"), DISPLAYWIDTH - 30, textLine, 1);
    drawLong(engine.player.score, DISPLAYWIDTH - FONT_WIDTH - 1, numberLine, 1);
    //drawInt(99, 36, DISPLAYHEIGHT - FONT_HEIGHT * 2 - 1, 1);
}

#ifdef DEFER_RENDER
void Renderer::drawDeferredFrame() {
    for(int x = 0; x < DISPLAYWIDTH; x++) {
        int16_t w = wbuffer[x];
        int16_t halfW = w >> 1;
        for(int y = 0; y < HALF_DISPLAYHEIGHT - halfW - 1; y++) {
            clearPixel(x, y);
            clearPixel(x, DISPLAYHEIGHT - y - 1);
        }
        drawStrip(x, wbuffer[x], ubuffer[x], texbuffer[x]);
    }
    /*for(uint8_t item = renderQueueHead; item != NULL_QUEUE_ITEM; item = renderQueue[item].next)
	{
		drawQueuedSprite(item);
	}

	drawWeapon();*/
}
#endif

void Renderer::drawBufferedCells() {
    int8_t xd, zd;
    int8_t x1, z1, x2, z2;

    if(view.rotCos > 0) {
        x1 = engine.map.bufferX;
        x2 = x1 + MAP_BUFFER_SIZE;
        xd = 1;
    } else {
        x2 = engine.map.bufferX - 1;
        x1 = x2 + MAP_BUFFER_SIZE;
        xd = -1;
    }
    if(view.rotSin < 0) {
        z1 = engine.map.bufferZ;
        z2 = z1 + MAP_BUFFER_SIZE;
        zd = 1;
    } else {
        z2 = engine.map.bufferZ - 1;
        z1 = z2 + MAP_BUFFER_SIZE;
        zd = -1;
    }

    if(mabs(view.rotCos) < mabs(view.rotSin)) {
        for(int8_t z = z1; z != z2; z += zd) {
            for(int8_t x = x1; x != x2; x += xd) {
                drawCell(x, z);
            }
        }
    } else {
        for(int8_t x = x1; x != x2; x += xd) {
            for(int8_t z = z1; z != z2; z += zd) {
                drawCell(x, z);
            }
        }
    }
}

void Renderer::initWBuffer() {
    for(uint16_t i = 0; i < DISPLAYWIDTH; i++)
        wbuffer[i] = 0;
}

#if defined(PLATFORM_GAMEBUINO)
extern uint8_t _displayBuffer[];
void Renderer::drawFloorAndCeiling() {
    memset(_displayBuffer, 0x00, 3 * 84);
    for(int y = 3, ofs = 3 * 84; y < 6; y++) {
        for(int8_t x = 0; x < 84; x += 2) {
            _displayBuffer[ofs++] = 0x55;
            _displayBuffer[ofs++] = 0x00;
        }
    }
}
#else
void Renderer::drawFloorAndCeiling() {
#ifdef _WIN32
    for(int x = 0; x < DISPLAYWIDTH; x++) {
        for(int y = 0; y < DISPLAYHEIGHT; y++) {
#if defined(EMULATE_UZEBOX)
            if(y < HALF_DISPLAYHEIGHT || ((x & y) & 1) == 0) {
                drawPixel(x, y, 3);
            } else {
                drawPixel(x, y, 2);
            }
#elif 1
            if(y < HALF_DISPLAYHEIGHT) {
                if((x & 1)) {
                    if(((x >> 1) & 1) == (y & 1)) {
                        setPixel(x, y);
                    } else {
                        clearPixel(x, y);
                    }
                } else {
                    setPixel(x, y);
                }
            } else {
                if(!(x & 1)) {
                    if(((x >> 1) & 1) == (y & 1)) {
                        clearPixel(x, y);
                    } else {
                        setPixel(x, y);
                    }
                } else {
                    clearPixel(x, y);
                }
                //clearPixel(x, y);
            }
#else
            if(y < HALF_DISPLAYHEIGHT || ((x ^ y) & 1) == 1) {
                setPixel(x, y);
            } else {
                clearPixel(x, y);
            }
#endif
        }
    }
#elif 1
    uint8_t* ptr = GetScreenBuffer();

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
#endif
}
#endif

void Renderer::drawCell(int8_t cellX, int8_t cellZ) {
    if(numBufferSlicesFilled >= DISPLAYWIDTH) {
        return;
    }

    // clip cells out of frustum view
    if(isFrustrumClipped(cellX, cellZ)) return;

    uint8_t tile = engine.map.getTileFast(cellX, cellZ);
    if(tile == 0) return;

    int16_t worldX = CELL_TO_WORLD(cellX);
    int16_t worldZ = CELL_TO_WORLD(cellZ);

    if(tile >= Tile_FirstDecoration && tile <= Tile_LastDecoration) {
        queueSprite(
            (SpriteFrame*)&Data_decorations_frames[tile - Tile_FirstDecoration],
            Data_decorations,
            worldX + CELL_SIZE / 2,
            worldZ + CELL_SIZE / 2);
        return;
    }
    if(tile >= Tile_FirstBlockingDecoration && tile <= Tile_LastBlockingDecoration) {
        queueSprite(
            (SpriteFrame*)&Data_blockingDecorations_frames[tile - Tile_FirstBlockingDecoration],
            Data_blockingDecorations,
            worldX + CELL_SIZE / 2,
            worldZ + CELL_SIZE / 2);
        return;
    }
    if(tile >= Tile_FirstItem && tile <= Tile_LastItem) {
        queueSprite(
            (SpriteFrame*)&Data_itemSprites_frames[(tile - Tile_FirstItem)],
            Data_itemSprites,
            worldX + CELL_SIZE / 2,
            worldZ + CELL_SIZE / 2);
    }

    if(tile >= Tile_FirstDoor && tile <= Tile_LastDoor) {
        // Draw inactive doors that are not yet streamed in
        Door* door = engine.map.getDoor(cellX, cellZ);

        if(!door) {
            uint8_t textureId = engine.map.getDoorTexture(tile);

            if((tile & 0x1) == 0) {
                worldX += CELL_SIZE / 2;
                if(view.x < worldX) {
                    drawWall(
                        worldX,
                        worldZ + CELL_SIZE,
                        worldX,
                        worldZ,
                        textureId,
                        0,
                        (TEXTURE_SIZE - 1));
                } else {
                    drawWall(
                        worldX,
                        worldZ,
                        worldX,
                        worldZ + CELL_SIZE,
                        textureId,
                        (TEXTURE_SIZE - 1),
                        0);
                }
            } else {
                worldZ += CELL_SIZE / 2;
                if(view.z > worldZ) {
                    drawWall(
                        worldX + CELL_SIZE,
                        worldZ,
                        worldX,
                        worldZ,
                        textureId,
                        0,
                        (TEXTURE_SIZE - 1));
                } else {
                    drawWall(
                        worldX,
                        worldZ,
                        worldX + CELL_SIZE,
                        worldZ,
                        textureId,
                        (TEXTURE_SIZE - 1),
                        0);
                }
            }
        }

        return;
    }

    if(tile >= Tile_FirstWall && tile <= Tile_LastWall) {
        uint8_t textureId = tile - Tile_FirstWall; //engine.map.getTextureId(cellX, cellZ);

        if(view.z < worldZ) {
            if(view.x > worldX) {
                // north west quadrant
                if(view.z < worldZ) {
                    if(engine.map.isDoor(cellX, cellZ - 1)) {
                        drawWall(
                            worldX,
                            worldZ,
                            worldX + CELL_SIZE,
                            worldZ,
                            DOOR_FRAME_TEXTURE); // south wall
                    } else if(!engine.map.isSolid(cellX, cellZ - 1)) {
                        drawWall(
                            worldX, worldZ, worldX + CELL_SIZE, worldZ, textureId); // south wall
                    }
                }
                if(view.x > worldX + CELL_SIZE) {
                    if(engine.map.isDoor(cellX + 1, cellZ)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ,
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            DOOR_FRAME_TEXTURE); // east wall
                    } else if(!engine.map.isSolid(cellX + 1, cellZ)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ,
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            textureId); // east wall
                    }
                }
            } else {
                // north east quadrant
                if(view.z < worldZ) {
                    if(engine.map.isDoor(cellX, cellZ - 1)) {
                        drawWall(
                            worldX,
                            worldZ,
                            worldX + CELL_SIZE,
                            worldZ,
                            DOOR_FRAME_TEXTURE); // south wall
                    } else if(!engine.map.isSolid(cellX, cellZ - 1)) {
                        drawWall(
                            worldX, worldZ, worldX + CELL_SIZE, worldZ, textureId); // south wall
                    }
                }
                if(view.x < worldX) {
                    if(engine.map.isDoor(cellX - 1, cellZ)) {
                        drawWall(
                            worldX,
                            worldZ + CELL_SIZE,
                            worldX,
                            worldZ,
                            DOOR_FRAME_TEXTURE); // west wall
                    } else if(!engine.map.isSolid(cellX - 1, cellZ)) {
                        drawWall(
                            worldX, worldZ + CELL_SIZE, worldX, worldZ, textureId); // west wall
                    }
                }
            }
        } else {
            if(view.x > worldX) {
                // south west quadrant
                if(view.z > worldZ + CELL_SIZE) {
                    if(engine.map.isDoor(cellX, cellZ + 1)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            worldX,
                            worldZ + CELL_SIZE,
                            DOOR_FRAME_TEXTURE); // north wall
                    } else if(!engine.map.isSolid(cellX, cellZ + 1)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            worldX,
                            worldZ + CELL_SIZE,
                            textureId); // north wall
                    }
                }
                if(view.x > worldX + CELL_SIZE) {
                    if(engine.map.isDoor(cellX + 1, cellZ)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ,
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            DOOR_FRAME_TEXTURE); // east wall
                    } else if(!engine.map.isSolid(cellX + 1, cellZ)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ,
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            textureId); // east wall
                    }
                }
            } else {
                // south east quadrant
                if(view.z > worldZ + CELL_SIZE) {
                    if(engine.map.isDoor(cellX, cellZ + 1)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            worldX,
                            worldZ + CELL_SIZE,
                            DOOR_FRAME_TEXTURE); // north wall
                    } else if(!engine.map.isSolid(cellX, cellZ + 1)) {
                        drawWall(
                            worldX + CELL_SIZE,
                            worldZ + CELL_SIZE,
                            worldX,
                            worldZ + CELL_SIZE,
                            textureId); // north wall
                    }
                }
                if(view.x < worldX) {
                    if(engine.map.isDoor(cellX - 1, cellZ)) {
                        drawWall(
                            worldX,
                            worldZ + CELL_SIZE,
                            worldX,
                            worldZ,
                            DOOR_FRAME_TEXTURE); // west wall
                    } else if(!engine.map.isSolid(cellX - 1, cellZ)) {
                        drawWall(
                            worldX, worldZ + CELL_SIZE, worldX, worldZ, textureId); // west wall
                    }
                }
            }
        }
    }
}

#define FORCE_WALL_STRIP_EDGES 1

bool renderingVerticalWall = false;

/*inline*/ void Renderer::drawStrip(int16_t x, int16_t w, int8_t, uint8_t) {
    int halfW = w >> 1;
    int y1 = (HALF_DISPLAYHEIGHT)-halfW;
    int y2 = (HALF_DISPLAYHEIGHT) + halfW;
    int verror = halfW;
    uint8_t* buffer = engine.streamBuffer;
    uint8_t texData;
    uint8_t v = 0;

    texData = 2;

    if(y2 > DISPLAYHEIGHT) {
        y2 = DISPLAYHEIGHT;
    }
    if(y1 < 0) {
        int iterations = -y1;
        verror -= (TEXTURE_SIZE - 1) * iterations;
        v += (-verror / w) + 1;
        verror += v * w;
        y1 = 0;
        texData = buffer[v++];
    }

#if FORCE_WALL_STRIP_EDGES
    for(int y = y1; y < y2; y++)
#else
    for(int y = y1; y <= y2; y++)
#endif
    {
        if(y >= 0 && y < DISPLAYHEIGHT) {
            if(renderingVerticalWall) {
                switch(texData) {
                case 1:
                    if((x | y) & 1) {
                        clearPixel(x, y);
                    } else {
                        setPixel(x, y);
                    }
                    break;
                case 2:
                    setPixel(x, y);
                    break;
                case 0:
                    if((x ^ y) & 1) {
                        clearPixel(x, y);
                    } else {
                        setPixel(x, y);
                    }
                    break;
                case 3:
                    if((x & y) & 1) {
                        clearPixel(x, y);
                    } else {
                        setPixel(x, y);
                    }
                    break;
                }
            } else {
                switch(texData) {
                case 1:
                    clearPixel(x, y);
                    break;
                case 2:
                    setPixel(x, y);
                    break;
                case 3:
                    if((x ^ y) & 1) {
                        clearPixel(x, y);
                    } else {
                        setPixel(x, y);
                    }
                    break;
                case 0:
                    if((x & y) & 1) {
                        setPixel(x, y);
                    } else {
                        clearPixel(x, y);
                    }
                    break;
                }
            }
        }

        //verror -= 15;
        verror -= TEXTURE_SIZE - 1;

        while(verror < 0) {
            //texData = textureReader.read();
            texData = buffer[v++];
            verror += w;
        }
    }

#if FORCE_WALL_STRIP_EDGES
    if(y2 < DISPLAYHEIGHT - 1) setPixel(x, y2);
#endif
}

#ifdef PERSPECTIVE_CORRECT_TEXTURE_MAPPING
// draws one side of a cell
void Renderer::drawWall(
    int16_t _x1,
    int16_t _z1,
    int16_t _x2,
    int16_t _z2,
    uint8_t textureId,
    int8_t _u1,
    int8_t _u2) {
    // find position of wall edges relative to eye

    int16_t z2 = (int16_t)(FIXED_TO_INT(view.rotCos * (int32_t)(_x1 - view.x)) -
                           FIXED_TO_INT(view.rotSin * (int32_t)(_z1 - view.z)));
    int16_t x2 = (int16_t)(FIXED_TO_INT(view.rotSin * (int32_t)(_x1 - view.x)) +
                           FIXED_TO_INT(view.rotCos * (int32_t)(_z1 - view.z)));
    int16_t z1 = (int16_t)(FIXED_TO_INT(view.rotCos * (int32_t)(_x2 - view.x)) -
                           FIXED_TO_INT(view.rotSin * (int32_t)(_z2 - view.z)));
    int16_t x1 = (int16_t)(FIXED_TO_INT(view.rotSin * (int32_t)(_x2 - view.x)) +
                           FIXED_TO_INT(view.rotCos * (int32_t)(_z2 - view.z)));

    // clip to the front pane
    if((z1 < CLIP_PLANE) && (z2 < CLIP_PLANE)) return;
    if(z1 < CLIP_PLANE) {
        x1 += (CLIP_PLANE - z1) * (x2 - x1) / (z2 - z1);
        z1 = CLIP_PLANE;
    } else if(z2 < CLIP_PLANE) {
        x2 += (CLIP_PLANE - z2) * (x1 - x2) / (z1 - z2);
        z2 = CLIP_PLANE;
    }

    // apply perspective projection
    int16_t vx1 = (int16_t)(x1 * NEAR_PLANE * CAMERA_SCALE / z1);
    int16_t vx2 = (int16_t)(x2 * NEAR_PLANE * CAMERA_SCALE / z2);

    // transform the end points into screen space
    int16_t sx1 = (int16_t)((DISPLAYWIDTH / 2) + vx1);
    int16_t sx2 = (int16_t)((DISPLAYWIDTH / 2) + vx2) - 1;

    // clamp to the visible portion of the screen
    int16_t firstx = max(sx1, 0);
    int16_t lastx = min(sx2, DISPLAYWIDTH - 1);
    if(lastx < firstx) return;

    int16_t w1 = (int16_t)((CELL_SIZE * NEAR_PLANE * CAMERA_SCALE) / z1);
    int16_t w2 = (int16_t)((CELL_SIZE * NEAR_PLANE * CAMERA_SCALE) / z2);
    int16_t dx = sx2 - sx1;
    int16_t werror = dx >> 1;
    int16_t uerror = werror;
    int16_t w = w1;
    int16_t du, ustep;
    int16_t dw, wstep;

    int16_t u1 = _u1 * w1;
    int16_t u2 = _u2 * w2;
    int16_t u = _u1;
    int16_t z = z1;
    int8_t dz, zstep;
    int16_t zerror = werror;
    if(z1 < z2) {
        dz = z2 - z1;
        zstep = 1;
    } else {
        dz = z1 - z2;
        zstep = -1;
    }

    if(w1 < w2) {
        dw = w2 - w1;
        wstep = 1;
    } else {
        dw = w1 - w2;
        wstep = -1;
    }

    if(u1 < u2) {
        du = u2 - u1;
        ustep = 1;
    } else {
        du = u1 - u2;
        ustep = -1;
    }

    //	for (int x=firstx; x<=lastx; x++)
    for(int x = sx1; x <= sx2; x++) {
        if(x >= 0 && x < DISPLAYWIDTH && w > wbuffer[x]) {
            if(w <= 255) {
                wbuffer[x] = (uint8_t)w;
            } else {
                wbuffer[x] = 255;
            }

#ifdef DEFER_RENDER
            ubuffer[x] = u;
            texbuffer[x] = textureId;
#else
            int16_t testOutU = (u * z) / (CELL_SIZE * NEAR_PLANE * CAMERA_SCALE);
            float interpX = (float)(x - sx1) / (float)(sx2 - sx1);
            float u1OverZ = (float)_u1 / (float)z1;
            float u2OverZ = (float)_u2 / (float)z2;
            float interpUOverZ = (interpX * u2OverZ) + (1.0f - interpX) * u1OverZ;
            float interpZ = (interpX * z2) + (1.0f - interpX) * z1;
            float interpU = interpUOverZ * interpZ;
            uint8_t outU = (uint8_t)interpU;
            outU = (uint8_t)testOutU;
            outU = clamp(outU, 0, 15);
            drawStrip(x, w, outU, textureId);
#endif
        }

        werror -= dw;
        uerror -= du;
        zerror -= dz;

        if(dx > 0) {
            while(werror < 0) {
                w += wstep;
                werror += dx;
            }
            while(uerror < 0) {
                u += ustep;
                uerror += dx;
            }
            while(zerror < 0) {
                z += zstep;
                zerror += dx;
            }
        }
    }
}
#else
// draws one side of a cell
void Renderer::drawWall(
    int16_t _x1,
    int16_t _z1,
    int16_t _x2,
    int16_t _z2,
    uint8_t textureId,
    int8_t _u1,
    int8_t _u2) {
    renderingVerticalWall = _x1 == _x2;
    // find position of wall edges relative to eye

    int16_t z2 = (int16_t)(FIXED_TO_INT(view.rotCos * (int32_t)(_x1 - view.x)) -
                           FIXED_TO_INT(view.rotSin * (int32_t)(_z1 - view.z)));
    int16_t x2 = (int16_t)(FIXED_TO_INT(view.rotSin * (int32_t)(_x1 - view.x)) +
                           FIXED_TO_INT(view.rotCos * (int32_t)(_z1 - view.z)));
    int16_t z1 = (int16_t)(FIXED_TO_INT(view.rotCos * (int32_t)(_x2 - view.x)) -
                           FIXED_TO_INT(view.rotSin * (int32_t)(_z2 - view.z)));
    int16_t x1 = (int16_t)(FIXED_TO_INT(view.rotSin * (int32_t)(_x2 - view.x)) +
                           FIXED_TO_INT(view.rotCos * (int32_t)(_z2 - view.z)));

    // clip to the front pane
    if((z1 < CLIP_PLANE) && (z2 < CLIP_PLANE)) return;
    if(z1 < CLIP_PLANE) {
        x1 += (CLIP_PLANE - z1) * (x2 - x1) / (z2 - z1);
        z1 = CLIP_PLANE;
    } else if(z2 < CLIP_PLANE) {
        x2 += (CLIP_PLANE - z2) * (x1 - x2) / (z1 - z2);
        z2 = CLIP_PLANE;
    }

    // apply perspective projection
    int16_t vx1 = (int16_t)((int32_t)x1 * NEAR_PLANE * CAMERA_SCALE / z1);
    int16_t vx2 = (int16_t)((int32_t)x2 * NEAR_PLANE * CAMERA_SCALE / z2);

    // transform the end points into screen space
    int16_t sx1 = (int16_t)((DISPLAYWIDTH / 2) + vx1);
    int16_t sx2 = (int16_t)((DISPLAYWIDTH / 2) + vx2) - 1;

    // clamp to the visible portion of the screen
    int16_t firstx = max(sx1, 0);
    int16_t lastx = min(sx2, DISPLAYWIDTH - 1);
    if(lastx < firstx) return;

    int16_t w1 = (int16_t)((CELL_SIZE * NEAR_PLANE * CAMERA_SCALE) / z1);
    int16_t w2 = (int16_t)((CELL_SIZE * NEAR_PLANE * CAMERA_SCALE) / z2);
    int16_t dx = sx2 - sx1;
    int16_t werror = dx >> 1;
    int16_t uerror = werror;
    int16_t w = w1;
    int8_t u = _u1;
    int8_t du, ustep;
    int16_t dw, wstep;

    if(w1 < w2) {
        dw = w2 - w1;
        wstep = 1;
    } else {
        dw = w1 - w2;
        wstep = -1;
    }

    if(_u1 < _u2) {
        du = _u2 - _u1;
        ustep = 1;
    } else {
        du = _u1 - _u2;
        ustep = -1;
    }

    uint8_t lastU = 0xff;

    //	for (int x=firstx; x<=lastx; x++)
    for(int x = sx1; x <= sx2; x++) {
        if(x >= 0 && x < DISPLAYWIDTH && w > wbuffer[x]) {
            if(wbuffer[x] == 0) {
                numBufferSlicesFilled++;
            }
            if(w <= 255) {
                wbuffer[x] = (uint8_t)w;
            } else {
                wbuffer[x] = 255;
            }

#ifdef DEFER_RENDER
            ubuffer[x] = u;
            texbuffer[x] = textureId;
#else
            if(u != lastU) {
                diskRead(
                    Data_wallTextures + u * TEXTURE_STRIDE +
                        textureId * (TEXTURE_STRIDE * TEXTURE_SIZE),
                    engine.streamBuffer,
                    TEXTURE_SIZE);
                lastU = u;
            }

            drawStrip(x, w, u, textureId);
#endif
        }

        werror -= dw;
        uerror -= du;

        if(dx > 0) {
            while(werror < 0) {
                w += wstep;
                werror += dx;
            }
            while(uerror < 0) {
                u += ustep;
                uerror += dx;
            }
        }
    }
}
#endif

void Renderer::drawDoors() {
    for(int n = 0; n < MAX_DOORS; n++) {
        Door& door = engine.map.doors[n];
        uint8_t textureId = door.texture;

        if(!engine.map.isValid(door.x, door.z) || door.type == DoorType_None) {
            continue;
        }

        if(door.type == DoorType_SecretPushWall) {
            int16_t doorX = CELL_TO_WORLD(door.x);
            int16_t doorZ = CELL_TO_WORLD(door.z);

            switch(door.state) {
            case DoorState_PushNorth:
                doorZ -= door.open;
                break;
            case DoorState_PushEast:
                doorX += door.open;
                break;
            case DoorState_PushSouth:
                doorZ += door.open;
                break;
            case DoorState_PushWest:
                doorX -= door.open;
                break;
            }

            if(view.x < doorX) {
                drawWall(
                    doorX, doorZ + CELL_SIZE, doorX, doorZ, door.texture, 0, (TEXTURE_SIZE - 1));
            } else if(view.x > doorX) {
                drawWall(
                    doorX + CELL_SIZE,
                    doorZ,
                    doorX + CELL_SIZE,
                    doorZ + CELL_SIZE,
                    door.texture,
                    0,
                    (TEXTURE_SIZE - 1));
            }
            if(view.z > doorZ + CELL_SIZE) {
                drawWall(
                    doorX + CELL_SIZE,
                    doorZ + CELL_SIZE,
                    doorX,
                    doorZ + CELL_SIZE,
                    door.texture,
                    0,
                    (TEXTURE_SIZE - 1));
            } else if(view.z < doorZ) {
                drawWall(
                    doorX, doorZ, doorX + CELL_SIZE, doorZ, door.texture, 0, (TEXTURE_SIZE - 1));
            }
        } else {
            int offset = door.open * 2;
            if(offset >= TEXTURE_SIZE) {
                continue;
            }

            int16_t worldX = CELL_TO_WORLD(door.x);
            int16_t worldZ = CELL_TO_WORLD(door.z);

            if((door.type & 0x1) == 0) {
                worldX += CELL_SIZE / 2;
                if(view.x < worldX) {
                    drawWall(
                        worldX,
                        worldZ + CELL_SIZE,
                        worldX,
                        worldZ + offset,
                        textureId,
                        0,
                        (TEXTURE_SIZE - 1) - offset);
                } else {
                    drawWall(
                        worldX,
                        worldZ + offset,
                        worldX,
                        worldZ + CELL_SIZE,
                        textureId,
                        (TEXTURE_SIZE - 1) - offset,
                        0);
                }
            } else {
                worldZ += CELL_SIZE / 2;
                if(view.z > worldZ) {
                    drawWall(
                        worldX + CELL_SIZE,
                        worldZ,
                        worldX + offset,
                        worldZ,
                        textureId,
                        0,
                        (TEXTURE_SIZE - 1) - offset);
                } else {
                    drawWall(
                        worldX + offset,
                        worldZ,
                        worldX + CELL_SIZE,
                        worldZ,
                        textureId,
                        (TEXTURE_SIZE - 1) - offset,
                        0);
                }
            }
        }
    }
}

void Renderer::queueSprite(SpriteFrame* frame, uint24_t spriteAddress, int16_t _x, int16_t _z) {
#if 1
    int cellX = WORLD_TO_CELL(_x);
    int cellZ = WORLD_TO_CELL(_z);

    if(isFrustrumClipped(cellX, cellZ)) return;

    int16_t zt = (int16_t)(FIXED_TO_INT(view.rotCos * (int32_t)(_x - view.x)) -
                           FIXED_TO_INT(view.rotSin * (int32_t)(_z - view.z)));
    int16_t xt = (int16_t)(FIXED_TO_INT(view.rotSin * (int32_t)(_x - view.x)) +
                           FIXED_TO_INT(view.rotCos * (int32_t)(_z - view.z)));

    // clip to the front plane
    if(zt < SPRITE_CLIP_PLANE) return;

    // apply perspective projection
    int16_t vx = (int16_t)(xt * NEAR_PLANE * CAMERA_SCALE / zt);

    if(vx <= -DISPLAYWIDTH || vx >= DISPLAYWIDTH) return;

    int16_t w = (int16_t)((CELL_SIZE * NEAR_PLANE * CAMERA_SCALE) / zt);
    int16_t x = vx + HALF_DISPLAYWIDTH;

    if(w > 255) w = 255;

    // Check if this will be off screen (x)
    uint8_t frameWidth = pgm_read_byte(&frame->width);
    int16_t dx = (w * frameWidth) / (CELL_SIZE);
    int16_t sx1 = x - (w >> 1) + (w * pgm_read_byte(&frame->xOffset)) / (CELL_SIZE);
    int16_t sx2 = sx1 + dx;
    if(sx1 >= DISPLAYWIDTH || sx2 < 0) {
        return;
    }

    uint8_t newItem = NULL_QUEUE_ITEM;
    for(int n = 0; n < RENDER_QUEUE_CAPACITY; n++) {
        if(renderQueue[n].spriteAddress == 0) {
            newItem = n;
            break;
        }
    }

    if(newItem == NULL_QUEUE_ITEM) {
        if(w > renderQueue[renderQueueHead].w) {
            newItem = renderQueueHead;
            renderQueueHead = renderQueue[renderQueueHead].next;
        } else {
            //WARNING("Out of queue space!\n");
            return;
        }
    }

    renderQueue[newItem].x = x;
    renderQueue[newItem].w = w;
    renderQueue[newItem].frame = frame;
    renderQueue[newItem].spriteAddress = spriteAddress;

    if(renderQueueHead == NULL_QUEUE_ITEM) {
        renderQueueHead = newItem;
        renderQueue[newItem].next = NULL_QUEUE_ITEM;
        return;
    } else {
        if(w < renderQueue[renderQueueHead].w) {
            renderQueue[newItem].next = renderQueueHead;
            renderQueueHead = newItem;
        } else {
            for(uint8_t item = renderQueueHead; item != NULL_QUEUE_ITEM;
                item = renderQueue[item].next) {
                if(renderQueue[item].next == NULL_QUEUE_ITEM) {
                    renderQueue[item].next = newItem;
                    renderQueue[newItem].next = NULL_QUEUE_ITEM;
                    break;
                } else if(w < renderQueue[renderQueue[item].next].w) {
                    renderQueue[newItem].next = renderQueue[item].next;
                    renderQueue[item].next = newItem;
                    break;
                }
            }
        }
    }
#endif
}

void Renderer::drawQueuedSprite(uint8_t id) {
    uint8_t frameWidth = pgm_read_byte(&renderQueue[id].frame->width);
    uint8_t frameHeight = pgm_read_byte(&renderQueue[id].frame->height);
    int16_t halfW = renderQueue[id].w >> 1;
    int y2 = (HALF_DISPLAYHEIGHT) + halfW;
    int y1 = y2 - (renderQueue[id].w * frameHeight) / (CELL_SIZE);

    int16_t w = renderQueue[id].w;

    int16_t dx = (w * frameWidth) / (CELL_SIZE);

    int16_t sx1 = renderQueue[id].x - halfW +
                  (w * pgm_read_byte(&renderQueue[id].frame->xOffset)) / (CELL_SIZE);
    int16_t sx2 = sx1 + dx;
    int16_t uerror = dx;
    int8_t u = 0;
    int8_t du = frameWidth, ustep = 1;
    int8_t v;
    bool outline = false;
    uint8_t* buffer = engine.streamBuffer;
    uint8_t* leftBuffer = engine.streamBuffer + 32;
    uint8_t outlineColour = 0;
    int8_t firstV = 0;
    int firstVerror = halfW;

    if(y2 >= DISPLAYHEIGHT) {
        int iterations = y2 - (DISPLAYHEIGHT - 1);
        firstVerror -= (TEXTURE_SIZE - 1) * iterations;
        firstV += (-firstVerror / w) + 1;
        firstVerror += firstV * w;
        y2 = DISPLAYHEIGHT - 1;
    }

    for(int x = 0; x < frameHeight; x++) {
        leftBuffer[x] = 0;
    }

    for(int x = sx1; x <= sx2; x++) {
        if(x >= 0 && x < DISPLAYWIDTH && w > wbuffer[x]) {
            int verror = firstVerror;

            diskRead(
                renderQueue[id].spriteAddress + pgm_read_word(&renderQueue[id].frame->offset) +
                    frameHeight * u,
                buffer,
                frameHeight);

            v = firstV;

            uint8_t texData = buffer[v];
            uint8_t texDataLeft = leftBuffer[v];

            outline = texData != 0;

            for(int y = y2; y >= y1 && y >= 0 && v < frameHeight; y--) {
                if(y < DISPLAYHEIGHT) {
                    if((!texDataLeft && texData) || (texDataLeft && !texData)) {
                        outline = true;
                    }
                    if(texData && v == frameHeight - 1) {
                        outline = true;
                    }

                    if(outline) {
                        arduboy.drawPixel(x, y, outlineColour);
                        outline = false;
                    } else {
                        switch(texData) {
                        case 0:
                            break;
                        case 1:
                            clearPixel(x, y);
                            break;
                        case 2:
                            setPixel(x, y);
                            break;
                        case 3:
#if defined(EMULATE_UZEBOX)
                            drawPixel(x, y, 2);
#else
                            if((x ^ y) & 1) {
                                clearPixel(x, y);
                            } else {
                                setPixel(x, y);
                            }
#endif
                            break;
                        }
                    }
                }
                verror -= 31;

                uint8_t lastTexDataValue = texData;

                if(verror < 0) {
                    leftBuffer[v] = buffer[v];

                    while(verror < 0) {
                        verror += w;
                        v++;
                    }

                    texData = buffer[v];
                    texDataLeft = leftBuffer[v];
                }

                if(lastTexDataValue && !texData) outline = true;
                if(!lastTexDataValue && texData) outline = true;
            }
        }

        uerror -= du;

        if(dx > 0) {
            while(u < frameWidth - 1 && uerror < 0) {
                u += ustep;
                uerror += dx;
            }
        }
    }
}

void Renderer::drawGlyph(char glyph, uint8_t x, uint8_t y, uint8_t colour) {
    uint8_t* ptr = (uint8_t*)(Data_font + glyph * FONT_GLYPH_BYTE_SIZE);
    uint8_t readMask = 1;
    uint8_t read = pgm_read_byte(ptr++);

    for(int i = 0; i < FONT_WIDTH; i++) {
        for(int j = 0; j < FONT_HEIGHT; j++) {
            uint8_t col = (read & readMask) ? 0 : 1;
            arduboy.drawPixel(x + i, y + j, colour ^ col);
            readMask <<= 1;
            if(readMask == 0) {
                readMask = 1;
                read = pgm_read_byte(ptr++);
            }
        }
        //		clearPixel(x + i, y);
        //	clearPixel(x + i, y + FONT_HEIGHT + 1);
    }
    //for(int j = 0; j < FONT_HEIGHT; j++)
    //{
    //	clearPixel(x + FONT_WIDTH, y + j);
    //}
}

void Renderer::drawString(const char* str, uint8_t x, uint8_t y, uint8_t colour) {
    char* ptr = (char*)str;
    char current = 0;
    uint8_t startX = x;

    do {
        current = pgm_read_byte(ptr);
        ptr++;

        if(current >= FIRST_FONT_GLYPH && current <= LAST_FONT_GLYPH) {
            drawGlyph(current - FIRST_FONT_GLYPH, x, y, colour);
        }

        x += FONT_WIDTH + 1;

        if(current == '\n') {
            x = startX;
            y += FONT_HEIGHT + 1;
        }
    } while(current);
}

void Renderer::drawInt(int8_t val, uint8_t x, uint8_t y, uint8_t colour) {
    unsigned char c, i;

    for(i = 0; i < 3; i++) {
        c = val % 10;
        if(val > 0 || i == 0) {
            drawGlyph(c + '0' - FIRST_FONT_GLYPH, x, y, colour);
        } else {
            drawGlyph(' ' - FIRST_FONT_GLYPH, x, y, colour);
        }
        x -= FONT_WIDTH + 1;
        val = val / 10;
    }
}

void Renderer::drawLong(int32_t val, uint8_t x, uint8_t y, uint8_t colour) {
    unsigned char c, i;

    for(i = 0;; i++) {
        c = val % 10;
        if(val > 0 || i == 0) {
            drawGlyph(c + '0' - FIRST_FONT_GLYPH, x, y, colour);
        } else {
            drawGlyph(' ' - FIRST_FONT_GLYPH, x, y, colour);
        }
        x -= FONT_WIDTH + 1;
        val = val / 10;

        if(!val) break;
    }
}

void Renderer::drawLevelLoadScreen() {
    clearDisplay(1);
    drawString(PSTR("GET PSYCHED!"), 44, 5);
    drawString(PSTR("FLOOR:"), 51, 53);
    drawInt(engine.map.currentLevel + 1, 80, 53);
    drawSprite2D(UI_BJFace, 36, 16);
    drawGlyph('X' - FIRST_FONT_GLYPH, 70, 32, 0);
    drawInt(engine.player.lives, 82, 32, 0);
}

void Renderer::drawBackground(uint24_t address) {
#ifdef _WIN32
    uint8_t background[1024];
    diskRead(address, background, 1024);
    int index = 0;
    for(int y = 0; y < 64; y += 8) {
        for(int x = 0; x < 128; x++) {
            for(int j = 0; j < 8; j++) {
                if(background[index] & (1 << j)) {
                    drawPixel(x, y + j, 1);
                } else {
                    drawPixel(x, y + j, 0);
                }
            }
            index++;
        }
    }
#else
    diskRead(address, GetScreenBuffer(), 1024);
#endif
}

void Renderer::fadeScreen(int8_t amount) {
    uint8_t fadeMask1 = 0xff, fadeMask2 = 0xff;

    switch(amount) {
    case 0:
        break;
    case 1:
        fadeMask1 = 0x55;
        fadeMask2 = 0xff;
        break;
    case 2:
        fadeMask1 = 0x55;
        fadeMask2 = 0xaa;
        break;
    case 3:
        fadeMask1 = 0x00;
        fadeMask2 = 0xaa;
        break;
    case 4:
        fadeMask1 = 0x00;
        fadeMask2 = 0x00;
        break;
    }

#ifdef _WIN32
    for(int y = 0; y < 64; y += 8) {
        for(int x = 0; x < 128; x += 2) {
            for(int j = 0; j < 8; j++) {
                if(!(fadeMask1 & (1 << j))) {
                    drawPixel(x, y + j, 0);
                }
                if(!(fadeMask2 & (1 << j))) {
                    drawPixel(x + 1, y + j, 0);
                }
            }
        }
    }
#else
    uint8_t* ptr = GetScreenBuffer();
    int count = 512;

    while(count--) {
        *ptr++ &= fadeMask1;
        *ptr++ &= fadeMask2;
    }
#endif
}
