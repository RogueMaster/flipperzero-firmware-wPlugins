#include "game/Defines.h"
#include "game/Map.h"
#include "game/Game.h"
#include "game/FixedMath.h"
#include "game/Draw.h"
#include "game/Platform.h"
#include "game/Enemy.h"

uint8_t Map::level[Map::width * Map::height / 2];

bool Map::IsBlocked(uint8_t x, uint8_t y) {
    return GetCellSafe(x, y) >= CellType::FirstCollidableCell;
}

bool Map::IsSolid(uint8_t x, uint8_t y) {
    return GetCellSafe(x, y) >= CellType::FirstSolidCell;
}

CellType Map::GetCell(uint8_t x, uint8_t y) {
    int index = y * Map::width + x;
    uint8_t cellData = level[index / 2];

    if(index & 1) {
        return (CellType)(cellData >> 4);
    } else {
        return (CellType)(cellData & 0xf);
    }
}

CellType Map::GetCellSafe(uint8_t x, uint8_t y) {
    if(x >= Map::width || y >= Map::height) return CellType::BrickWall;

    int index = y * Map::width + x;
    uint8_t cellData = level[index / 2];

    if(index & 1) {
        return (CellType)(cellData >> 4);
    } else {
        return (CellType)(cellData & 0xf);
    }
}

void Map::SetCell(uint8_t x, uint8_t y, CellType type) {
    if(x >= Map::width || y >= Map::height) {
        return;
    }

    int index = (y * Map::width + x) / 2;
    uint8_t cellType = (uint8_t)type;

    if(x & 1) {
        level[index] = (level[index] & 0xf) | (cellType << 4);
    } else {
        level[index] = (level[index] & 0xf0) | (cellType & 0xf);
    }
}

void Map::DebugDraw() {
    for(int y = 0; y < Map::height; y++) {
        for(int x = 0; x < Map::width; x++) {
            Platform::PutPixel(x, y, GetCell(x, y) == CellType::BrickWall ? 1 : 0);

            if(x == Renderer::camera.cellX && y == Renderer::camera.cellY &&
               (Game::globalTickFrame & 8) != 0) {
                Platform::PutPixel(x, y, 1);
            }
        }
    }

    if((Game::globalTickFrame & 2) != 0) {
        for(uint8_t n = 0; n < EnemyManager::maxEnemies; n++) {
            Enemy& enemy = EnemyManager::enemies[n];

            if(enemy.IsValid()) {
                Platform::PutPixel(enemy.x / CELL_SIZE, enemy.y / CELL_SIZE, 1);
            }
        }
    }
}

bool Map::IsClearLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    int cellX1 = x1 / CELL_SIZE;
    int cellX2 = x2 / CELL_SIZE;
    int cellY1 = y1 / CELL_SIZE;
    int cellY2 = y2 / CELL_SIZE;

    int xdist = ABS(cellX2 - cellX1);

    int partial, delta;
    int deltafrac;
    int xfrac, yfrac;
    int xstep, ystep;
    int32_t ltemp;
    int x, y;

    if(xdist > 0) {
        if(cellX2 > cellX1) {
            partial = (CELL_SIZE * (cellX1 + 1) - x1);
            xstep = 1;
        } else {
            partial = (x1 - CELL_SIZE * (cellX1));
            xstep = -1;
        }

        deltafrac = ABS(x2 - x1);
        delta = y2 - y1;
        ltemp = ((int32_t)delta * CELL_SIZE) / deltafrac;
        if(ltemp > 0x7fffl)
            ystep = 0x7fff;
        else if(ltemp < -0x7fffl)
            ystep = -0x7fff;
        else
            ystep = ltemp;
        yfrac = y1 + (((int32_t)ystep * partial) / CELL_SIZE);

        x = cellX1 + xstep;
        cellX2 += xstep;
        do {
            y = (yfrac) / CELL_SIZE;
            yfrac += ystep;

            if(IsSolid(x, y)) return false;

            x += xstep;

            //
            // see if the door is open enough
            //
            /*value &= ~0x80;
            intercept = yfrac-ystep/2;

            if (intercept>doorposition[value])
                return false;*/

        } while(x != cellX2);
    }

    int ydist = ABS(cellY2 - cellY1);

    if(ydist > 0) {
        if(cellY2 > cellY1) {
            partial = (CELL_SIZE * (cellY1 + 1) - y1);
            ystep = 1;
        } else {
            partial = (y1 - CELL_SIZE * (cellY1));
            ystep = -1;
        }

        deltafrac = ABS(y2 - y1);
        delta = x2 - x1;
        ltemp = ((int32_t)delta * CELL_SIZE) / deltafrac;
        if(ltemp > 0x7fffl)
            xstep = 0x7fff;
        else if(ltemp < -0x7fffl)
            xstep = -0x7fff;
        else
            xstep = ltemp;
        xfrac = x1 + (((int32_t)xstep * partial) / CELL_SIZE);

        y = cellY1 + ystep;
        cellY2 += ystep;
        do {
            x = (xfrac) / CELL_SIZE;
            xfrac += xstep;

            if(IsSolid(x, y)) return false;
            y += ystep;

            //
            // see if the door is open enough
            //
            /*value &= ~0x80;
            intercept = xfrac-xstep/2;

            if (intercept>doorposition[value])
                return false;*/
        } while(y != cellY2);
    }

    return true;
}

void Map::DrawMinimap() {
    constexpr uint8_t minimapWidth = 24;
    constexpr uint8_t minimapHeight = 18;
    constexpr uint8_t minimapX = 0; //DISPLAY_WIDTH / 2 - minimapWidth / 2;
    constexpr uint8_t minimapY = 0; //DISPLAY_HEIGHT - minimapHeight;
    uint8_t playerCellX = Game::player.x / CELL_SIZE;
    uint8_t playerCellY = Game::player.y / CELL_SIZE;
    uint8_t startCellX = playerCellX - minimapWidth / 2;
    uint8_t startCellY = playerCellY - minimapHeight / 2;

    uint8_t outX = minimapX;
    uint8_t cellX = startCellX;

    for(uint8_t x = 0; x < minimapWidth; x++) {
        uint8_t outY = minimapY;
        uint8_t cellY = startCellY;

        for(uint8_t y = 0; y < minimapHeight; y++) {
            if(cellX == playerCellX && cellY == playerCellY) {
                Platform::PutPixel(
                    outX, outY, (Game::globalTickFrame & 3) ? COLOUR_BLACK : COLOUR_WHITE);
            } else {
                Platform::PutPixel(
                    outX,
                    outY,
                    cellX < width && cellY < height && IsSolid(cellX, cellY) ? COLOUR_BLACK :
                                                                               COLOUR_WHITE);
            }
            outY++;
            cellY++;
        }

        outX++;
        cellX++;
    }
}
