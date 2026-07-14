#include "game/MapGenerator.h"
#include "game/Map.h"
#include "game/FixedMath.h"
#include "game/Enemy.h"
#include "game/Game.h"

uint8_t MapGenerator::GetDistanceToCellType(uint8_t x, uint8_t y, CellType cellType) {
    uint8_t ringWidth = 3;

    for(uint8_t offset = 1; offset < Map::width; offset++) {
        for(uint8_t i = 0; i < ringWidth; i++) {
            if(Map::GetCellSafe(x - offset + i, y - offset) == cellType) {
                return offset;
            }
            if(Map::GetCellSafe(x - offset + i, y + offset) == cellType) {
                return offset;
            }
            if(Map::GetCellSafe(x - offset, y - offset + i) == cellType) {
                return offset;
            }
            if(Map::GetCellSafe(x + offset, y - offset + i) == cellType) {
                return offset;
            }
        }

        ringWidth += 2;
    }

    return 0xff;
}

uint8_t MapGenerator::CountNeighbours(uint8_t x, uint8_t y) {
    uint8_t result = 0;

    if(Map::GetCellSafe(x + 1, y) == CellType::Empty) result++;
    if(Map::GetCellSafe(x, y + 1) == CellType::Empty) result++;
    if(Map::GetCellSafe(x - 1, y) == CellType::Empty) result++;
    if(Map::GetCellSafe(x, y - 1) == CellType::Empty) result++;
    if(Map::GetCellSafe(x + 1, y + 1) == CellType::Empty) result++;
    if(Map::GetCellSafe(x - 1, y + 1) == CellType::Empty) result++;
    if(Map::GetCellSafe(x - 1, y - 1) == CellType::Empty) result++;
    if(Map::GetCellSafe(x + 1, y - 1) == CellType::Empty) result++;

    return result;
}

MapGenerator::NeighbourInfo MapGenerator::GetCellNeighbourInfo(uint8_t x, uint8_t y) {
    NeighbourInfo result;

    result.count = 0;
    result.mask = 0;

    if(Map::IsSolid(x, y - 1)) {
        result.hasNorth = true;
        result.count++;
    }
    if(Map::IsSolid(x + 1, y)) {
        result.hasEast = true;
        result.count++;
    }
    if(Map::IsSolid(x, y + 1)) {
        result.hasSouth = true;
        result.count++;
    }
    if(Map::IsSolid(x - 1, y)) {
        result.hasWest = true;
        result.count++;
    }

    return result;
}

uint8_t MapGenerator::CountImmediateNeighbours(uint8_t x, uint8_t y) {
    uint8_t result = 0;

    if(Map::GetCellSafe(x + 1, y) == CellType::Empty) result++;
    if(Map::GetCellSafe(x, y + 1) == CellType::Empty) result++;
    if(Map::GetCellSafe(x - 1, y) == CellType::Empty) result++;
    if(Map::GetCellSafe(x, y - 1) == CellType::Empty) result++;

    return result;
}

MapGenerator::NeighbourInfo
    MapGenerator::GetRoomNeighbourMask(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    NeighbourInfo result;
    result.mask = 0;
    result.count = 0;

    result.canDemolishNorth = y > 1;
    result.canDemolishWest = x > 1;
    result.canDemolishEast = x + w + 1 < Map::width - 1;
    result.canDemolishSouth = y + h + 1 < Map::height - 1;

    // Don't demolish walls if the neighbouring room has the same wall length
    if(Map::GetCell(x - 1, y - 2) != CellType::Empty &&
       Map::GetCell(x + w, y - 2) != CellType::Empty) {
        result.canDemolishNorth = false;
    }
    if(Map::GetCell(x - 2, y - 1) != CellType::Empty &&
       Map::GetCell(x - 2, y + h) != CellType::Empty) {
        result.canDemolishWest = false;
    }
    if(Map::GetCell(x + w + 1, y - 1) != CellType::Empty &&
       Map::GetCell(x + w, y + h + 1) != CellType::Empty) {
        result.canDemolishEast = false;
    }
    if(Map::GetCell(x - 1, y + h + 1) != CellType::Empty &&
       Map::GetCell(x + w, y + h + 1) != CellType::Empty) {
        result.canDemolishSouth = false;
    }

    // Don't demolish wall if this will leave an unattached wall
    if(Map::GetCell(x - 1, y - 2) == CellType::Empty &&
       Map::GetCell(x - 2, y - 1) == CellType::Empty) {
        result.canDemolishNorth = false;
        result.canDemolishWest = false;
    }
    if(Map::GetCell(x + w, y - 2) == CellType::Empty &&
       Map::GetCell(x + w + 1, y - 1) == CellType::Empty) {
        result.canDemolishNorth = false;
        result.canDemolishEast = false;
    }
    if(Map::GetCell(x + w, y + h + 1) == CellType::Empty &&
       Map::GetCell(x + w + 1, y + h) == CellType::Empty) {
        result.canDemolishSouth = false;
        result.canDemolishEast = false;
    }
    if(Map::GetCell(x - 1, y + h + 1) == CellType::Empty &&
       Map::GetCell(x - 2, y + h) == CellType::Empty) {
        result.canDemolishSouth = false;
        result.canDemolishWest = false;
    }

    bool hasNorthWall = Map::GetCell(x, y - 1) != CellType::Empty &&
                        Map::GetCell(x + w - 1, y - 1) != CellType::Empty;
    bool hasEastWall = Map::GetCell(x + w, y) != CellType::Empty &&
                       Map::GetCell(x + w, y + h - 1) != CellType::Empty;
    bool hasSouthWall = Map::GetCell(x, y + h) != CellType::Empty &&
                        Map::GetCell(x + w - 1, y + h) != CellType::Empty;
    bool hasWestWall = Map::GetCell(x - 1, y) != CellType::Empty &&
                       Map::GetCell(x - 1, y + h - 1) != CellType::Empty;

    if(!hasNorthWall) {
        result.canDemolishNorth = false;
        result.canDemolishEast = false;
        result.canDemolishWest = false;
    }
    if(!hasEastWall) {
        result.canDemolishNorth = false;
        result.canDemolishEast = false;
        result.canDemolishSouth = false;
    }
    if(!hasSouthWall) {
        result.canDemolishEast = false;
        result.canDemolishSouth = false;
        result.canDemolishWest = false;
    }
    if(!hasWestWall) {
        result.canDemolishNorth = false;
        result.canDemolishSouth = false;
        result.canDemolishWest = false;
    }

    for(int i = x; i < x + w; i++) {
        if(Map::GetCell(i, y - 1) == CellType::Empty) {
            result.hasNorth = true;
            result.count++;
        }
        if(Map::GetCell(i, y + h) == CellType::Empty) {
            result.hasSouth = true;
            result.count++;
        }

        // Don't demolish wall if there is an intersecting wall attached
        if(y > 1 && Map::GetCell(i, y - 2) != CellType::Empty) {
            result.canDemolishNorth = false;
        }
        if(y + h + 1 < Map::height - 1 && Map::GetCell(i, y + h + 1) != CellType::Empty) {
            result.canDemolishSouth = false;
        }
    }
    for(int j = y; j < y + h; j++) {
        if(Map::GetCell(x - 1, j) == CellType::Empty) {
            result.hasWest = true;
            result.count++;
        }
        if(Map::GetCell(x + w, j) == CellType::Empty) {
            result.hasEast = true;
            result.count++;
        }

        // Don't demolish wall if there is an intersecting wall attached
        if(x > 1 && Map::GetCell(x - 2, j) != CellType::Empty) {
            result.canDemolishWest = false;
        }
        if(x + w + 1 < Map::width - 1 && Map::GetCell(x + w + 1, j) != CellType::Empty) {
            result.canDemolishEast = false;
        }
    }

    return result;
}

void MapGenerator::SplitMap(
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    uint8_t doorX,
    uint8_t doorY) {
    constexpr int minRoomSize = 3;
    constexpr int maxRoomSize = 8;
    //constexpr int maxFloorSpace = 80;
    constexpr int demolishWallChance = 20;

    if(doorX != 0 && doorY != 0) {
        Map::SetCell(doorX, doorY, CellType::Empty);
    }

    bool splitVertical = false;
    bool splitHorizontal = false;

    if(w > maxRoomSize || h > maxRoomSize) //w * h > maxFloorSpace)
    {
        if(w < h) {
            splitVertical = true;
        } else {
            splitHorizontal = true;
        }
    }

    if(splitVertical) {
        uint8_t splitSize;
        uint8_t splitAttempts = 255;
        do {
            splitSize = (Random() % (h - 2 * minRoomSize)) + minRoomSize;
            splitAttempts--;
        } while(splitAttempts > 0 && (Map::GetCell(x - 1, y + splitSize) == CellType::Empty ||
                                      Map::GetCell(x + w, y + splitSize) == CellType::Empty ||
                                      Map::GetCell(x - 1, y + splitSize - 1) == CellType::Empty ||
                                      Map::GetCell(x + w, y + splitSize - 1) == CellType::Empty ||
                                      Map::GetCell(x - 1, y + splitSize + 1) == CellType::Empty ||
                                      Map::GetCell(x + w, y + splitSize + 1) == CellType::Empty));

        if(splitAttempts > 0) {
            uint8_t splitDoorX = x + (Random() % (w - 2)) + 1;
            uint8_t splitDoorY = y + splitSize;

            for(uint8_t i = x; i < x + w; i++) {
                Map::SetCell(i, y + splitSize, CellType::BrickWall);
            }

            SplitMap(x, y + splitSize + 1, w, h - splitSize - 1, splitDoorX, splitDoorY);
            SplitMap(x, y, w, splitSize, splitDoorX, splitDoorY);
            return;
        }
    } else if(splitHorizontal) {
        uint8_t splitSize;
        uint8_t splitAttempts = 255;
        do {
            splitSize = (Random() % (w - 2 * minRoomSize)) + minRoomSize;
            splitAttempts--;
        } while(splitAttempts > 0 && (Map::GetCell(x + splitSize, y - 1) == CellType::Empty ||
                                      Map::GetCell(x + splitSize, y + h) == CellType::Empty ||
                                      Map::GetCell(x + splitSize - 1, y - 1) == CellType::Empty ||
                                      Map::GetCell(x + splitSize - 1, y + h) == CellType::Empty ||
                                      Map::GetCell(x + splitSize + 1, y - 1) == CellType::Empty ||
                                      Map::GetCell(x + splitSize + 1, y + h) == CellType::Empty));

        if(splitAttempts > 0) {
            uint8_t splitDoorX = x + splitSize;
            uint8_t splitDoorY = y + (Random() % (h - 2)) + 1;

            for(uint8_t j = y; j < y + h; j++) {
                Map::SetCell(x + splitSize, j, CellType::BrickWall);
            }

            SplitMap(x + splitSize + 1, y, w - splitSize - 1, h, splitDoorX, splitDoorY);
            SplitMap(x, y, splitSize, h, splitDoorX, splitDoorY);
            return;
        }
    }

    {
        NeighbourInfo neighbours = GetRoomNeighbourMask(x, y, w, h);

        if(neighbours.canDemolishNorth && (Random() % 100) < demolishWallChance) {
            for(int i = 0; i < w; i++) {
                Map::SetCell(x + i, y - 1, CellType::Empty);
            }
        } else if(neighbours.canDemolishWest && (Random() % 100) < demolishWallChance) {
            for(int j = 0; j < h; j++) {
                Map::SetCell(x - 1, y + j, CellType::Empty);
            }
        } else if(neighbours.canDemolishSouth && (Random() % 100) < demolishWallChance) {
            for(int i = 0; i < w; i++) {
                Map::SetCell(x + i, y + h, CellType::Empty);
            }
        } else if(neighbours.canDemolishEast && (Random() % 100) < demolishWallChance) {
            for(int j = 0; j < h; j++) {
                Map::SetCell(x + w, y + j, CellType::Empty);
            }
        }

        // Add decorations
        {
            // Add four cornering columns
            if(w == h && w >= 7 && h >= 7) {
                Map::SetCell(x + 1, y + 1, CellType::BrickWall);
                Map::SetCell(x + w - 2, y + 1, CellType::BrickWall);
                Map::SetCell(x + w - 2, y + h - 2, CellType::BrickWall);
                Map::SetCell(x + 1, y + h - 2, CellType::BrickWall);
            }
        }
    }
}

void MapGenerator::Generate() {
    uint8_t playerStartX = 1;
    uint8_t playerStartY = 1;

    for(int y = 0; y < Map::height; y++) {
        for(int x = 0; x < Map::width; x++) {
            bool isEdge = x == 0 || y == 0 || x == Map::width - 1 || y == Map::height - 1;
            Map::SetCell(x, y, isEdge ? CellType::BrickWall : CellType::Empty);
        }
    }

    SplitMap(1, 1, Map::width - 2, Map::height - 2, 0, 0);

    // Find any big open spaces
    {
        bool hasOpenSpaces = true;

        while(hasOpenSpaces) {
            hasOpenSpaces = false;

            uint8_t x = 0, y = 0, space = 0;

            for(uint8_t i = 1; i < Map::width - 1; i++) {
                for(uint8_t j = 0; j < Map::height - 1; j++) {
                    bool foundWall = false;

                    for(uint8_t k = 0; k < Map::height && !foundWall; k++) {
                        for(uint8_t u = 0; u < k && !foundWall; u++) {
                            for(uint8_t v = 0; v < k && !foundWall; v++) {
                                if(Map::GetCellSafe(i + u, j + v) != CellType::Empty) {
                                    foundWall = true;
                                }
                            }
                        }

                        if(!foundWall && k > space) {
                            space = k;
                            x = i;
                            y = j;
                        }
                    }
                }
            }

            if(space > 6) {
                hasOpenSpaces = true;

                // Stick a donut in the middle
                for(uint8_t n = 2; n < space - 2; n++) {
                    Map::SetCell(x + n, y + 2, CellType::BrickWall);
                    Map::SetCell(x + 2, y + n, CellType::BrickWall);
                    Map::SetCell(x + n, y + space - 3, CellType::BrickWall);
                    Map::SetCell(x + space - 3, y + n, CellType::BrickWall);
                }
            }
        }
    }

    // Add torches
    {
        uint8_t attempts = 255;
        uint8_t toSpawn = 64;
        uint8_t minSpacing = 3;

        while(attempts > 0 && toSpawn > 0) {
            uint8_t x = Random() % Map::width;
            uint8_t y = Random() % Map::height;

            if(Map::GetCellSafe(x, y) == CellType::Empty) {
                NeighbourInfo info = GetCellNeighbourInfo(x, y);

                if(info.count == 1 && GetDistanceToCellType(x, y, CellType::Torch) > minSpacing) {
                    Map::SetCell(x, y, CellType::Torch);
                    toSpawn--;
                    attempts = 255;
                }
            }

            attempts--;
        }
    }

    // Add monsters
    {
        uint8_t attempts = 255;
        uint8_t monstersToSpawn = EnemyManager::maxEnemies;
        CellType monsterType = CellType::Monster;
        uint8_t minSpacing = 3;

        while(attempts > 0 && monstersToSpawn > 0) {
            uint8_t x = Random() % Map::width;
            uint8_t y = Random() % Map::height;

            if(Map::GetCellSafe(x, y) == CellType::Empty &&
               Map::IsClearLine(
                   x * CELL_SIZE + CELL_SIZE / 2,
                   y * CELL_SIZE + CELL_SIZE / 2,
                   playerStartX * CELL_SIZE + CELL_SIZE / 2,
                   playerStartY * CELL_SIZE + CELL_SIZE / 2) == false) {
                NeighbourInfo info = GetCellNeighbourInfo(x, y);
                if(info.count == 0 && GetDistanceToCellType(x, y, monsterType) > minSpacing) {
                    Map::SetCell(x, y, monsterType);
                    monstersToSpawn--;
                    attempts = 255;
                }
            }

            attempts--;
        }
    }

    // Add blocking decorations
    {
        uint8_t attempts = 255;
        uint8_t toSpawn = 255;
        CellType cellType = CellType::Urn;
        uint8_t minSpacing = 3;

        while(attempts > 0 && toSpawn > 0) {
            uint8_t x = Random() % Map::width;
            uint8_t y = Random() % Map::height;

            if(Map::GetCellSafe(x, y) == CellType::Empty) {
                NeighbourInfo info = GetCellNeighbourInfo(x, y);

                if(info.IsCorner() && GetDistanceToCellType(x, y, cellType) > minSpacing) {
                    Map::SetCell(x, y, cellType);
                    toSpawn--;
                    attempts = 255;
                }
            }

            attempts--;
        }
    }

    // Add entrance and exit
    Map::SetCell(1, 1, CellType::Entrance);
    Map::SetCell(Map::width - 3, Map::height - 3, CellType::Exit);

    // Add sign
    if(false) {
        uint16_t attempts = 65535;
        constexpr uint8_t closeness = 5;

        while(attempts > 0) {
            uint8_t x = Random() % closeness;
            uint8_t y = Random() % closeness;

            if(Map::GetCellSafe(x, y) == CellType::Empty &&
               Map::GetCellSafe(x - 1, y) == CellType::Empty &&
               Map::GetCellSafe(x, y - 1) == CellType::Empty &&
               Map::GetCellSafe(x + 1, y) == CellType::Empty &&
               Map::GetCellSafe(x, y + 1) == CellType::Empty &&
               Map::GetCellSafe(x - 1, y - 1) == CellType::Empty &&
               Map::GetCellSafe(x + 1, y - 1) == CellType::Empty &&
               Map::GetCellSafe(x - 1, y + 1) == CellType::Empty &&
               Map::GetCellSafe(x + 1, y + 1) == CellType::Empty &&
               Map::IsClearLine(
                   x * CELL_SIZE + CELL_SIZE / 2,
                   y * CELL_SIZE + CELL_SIZE / 2,
                   playerStartX * CELL_SIZE + CELL_SIZE / 2,
                   playerStartY * CELL_SIZE + CELL_SIZE / 2)) {
                Map::SetCell(x, y, CellType::Sign);
                break;
            }

            attempts--;
        }
    } else if(Game::floor == 1) {
        Map::SetCell(2, 2, CellType::Sign);
    }

    // Add treasure / items
    {
        uint16_t attempts = 65535;
        uint8_t toSpawn = 8;
        CellType cellType = CellType::Chest;
        uint8_t minSpacing = 3;
        uint8_t minExitSpacing = 6;

        while(attempts > 0 && toSpawn > 0) {
            uint8_t x = Random() % Map::width;
            uint8_t y = Random() % Map::height;

            switch(Random() % 5) {
            case 0:
                cellType = CellType::Potion;
                break;
            case 1:
                cellType = CellType::Coins;
                break;
            case 2:
                cellType = CellType::Chest;
                break;
            case 3:
                cellType = CellType::Crown;
                break;
            case 4:
                cellType = CellType::Scroll;
                break;
            }

            if(Map::GetCellSafe(x, y) == CellType::Empty) {
                NeighbourInfo info = GetCellNeighbourInfo(x, y);

                if(info.count == 1 && GetDistanceToCellType(x, y, cellType) > minSpacing &&
                   GetDistanceToCellType(x, y, CellType::Entrance) > minExitSpacing &&
                   GetDistanceToCellType(x, y, CellType::Exit) > minExitSpacing) {
                    Map::SetCell(x, y, cellType);
                    toSpawn--;
                    attempts = 255;
                }
            }

            attempts--;
        }
    }
}
