#pragma once

#include "../utils/Arduboy2Ext.h"
#include "GamePlay.h"
#include "Prince.h"
#include "Enemy.h"
#include "../utils/Constants.h"
#include "../utils/Stack.h"
#include "Item.h"

#define TILE_NONE               -1
#define TILE_FLOOR_NONE         1
#define TILE_FLOOR_NONE_PATTERN 18
#define TILE_WINDOW_LH          56
#define TILE_WINDOW_RH          57
#define TILE_RUG_1              59
#define TILE_RUG_2              60
#define TILE_RUG_FRONT_TRACK    78

#define TILE_FLOOR_NONE_LH_WALL_1 23
#define TILE_FLOOR_NONE_LH_WALL_2 46
#define TILE_FLOOR_NONE_LH_WALL_3 17
#define TILE_FLOOR_NONE_LH_WALL_4 45
#define TILE_FLOOR_NONE_LH_WALL_5 75
#define TILE_FLOOR_NONE_LH_WALL_6 81
#define TILE_FLOOR_NONE_LH_WALL_7 62

#define TILE_FLOOR_BASIC              3
#define TILE_FLOOR_BASIC_TORCH        124
#define TILE_FLOOR_LH_END_1           2
#define TILE_FLOOR_LH_END_2           76
#define TILE_FLOOR_PATTERN_1          4
#define TILE_FLOOR_PATTERN_2          5
#define TILE_FLOOR_LH_END_PATTERN_1   28
#define TILE_FLOOR_LH_END_PATTERN_2   19
#define TILE_FLOOR_RH_END             25
#define TILE_FLOOR_RH_END_GATE_1      44
#define TILE_FLOOR_RH_END_GATE_2      74
#define TILE_FLOOR_RH_END_GATE_RUG    69
#define TILE_FLOOR_GATE_REAR_TRACK_1  40 // Missing top pixels
#define TILE_FLOOR_GATE_REAR_TRACK_2  68 // Full height
#define TILE_FLOOR_GATE_FRONT_TRACK_1 41
#define TILE_FLOOR_GATE_FRONT_TRACK_2 42
#define TILE_FLOOR_GATE_FRONT_TRACK_3 43
#define TILE_FLOOR_GATE_FRONT_TRACK_4 58 // Used on rug
#define TILE_FLOOR_GATE_FRONT_TRACK_5 67 // Used below rug

#define TILE_FLOOR_RH_END_1        24
#define TILE_FLOOR_RH_END_2        11
#define TILE_FLOOR_RH_END_3        29
#define TILE_FLOOR_RH_END_4        20
#define TILE_FLOOR_RH_END_5        37
#define TILE_FLOOR_RH_PILLAR_END_1 71
#define TILE_FLOOR_RH_PILLAR_END_2 79

#define TILE_FLOOR_LH_WALL_1 9
#define TILE_FLOOR_LH_WALL_2 35
#define TILE_FLOOR_LH_WALL_3 39
#define TILE_FLOOR_LH_RUG_1  61

#define TILE_FG_WALL_1 6
#define TILE_FG_WALL_2 7
#define TILE_FG_WALL_3 8
#define TILE_FG_WALL_4 12
#define TILE_FG_WALL_5 13
#define TILE_FG_WALL_6 14
#define TILE_FG_WALL_7 47
#define TILE_FG_WALL_8 48

#define TILE_COLUMN_LH_WALL 26
#define TILE_COLUMN_1       10
#define TILE_COLUMN_2       38
#define TILE_COLUMN_3       22
#define TILE_COLUMN_4       15
#define TILE_COLUMN_5       27
#define TILE_PILLAR_1       50
#define TILE_PILLAR_2       70

#define TILE_COLUMN_REAR_1 30
#define TILE_COLUMN_REAR_2 16
#define TILE_PILLAR_REAR_1 51

#define TILE_UPPER_COLUMN_FOREGROUND 52
#define TILE_UPPER_COLUMN_BACKGROUND 53

#define TILE_COLLAPSING_FLOOR -2

struct Level {
private:
    uint8_t width = 60;
    uint8_t height = 0;
    uint8_t xLoc = 60;
    uint8_t yLoc = 0;
    uint8_t yOffset = 0; // Ofset when rendering.
    Direction yOffsetDir = Direction::None; // Ofset movement

    int8_t bg[5][22]; // 6 + 10 + 6
    int8_t fg[5][22];
    Flash flash;
    Sign sign;
    Item items[Constants::Items_Count];

public:
    uint8_t getWidth() {
        return this->width;
    }
    uint8_t getHeight() {
        return this->height;
    }
    uint8_t getXLocation() {
        return this->xLoc;
    }
    uint8_t getYLocation() {
        return this->yLoc;
    }
    uint8_t getYOffset() {
        return this->yOffset;
    }
    Flash& getFlash() {
        return this->flash;
    }
    Sign& getSign() {
        return this->sign;
    }
    Item& getItem(uint8_t idx) {
        return this->items[idx];
    }
    Direction getYDirection() {
        return this->yOffsetDir;
    }

    void setWidth(uint8_t val) {
        this->width = val;
    }
    void setHeight(uint8_t val) {
        this->height = val;
    }
    void setXLocation(uint8_t val) {
        this->xLoc = val;
    }
    void setYLocation(uint8_t val) {
        this->yLoc = val;
    }
    void setYOffset(uint8_t val) {
        this->yOffset = val;
    }
    void setYOffsetDir(Direction val) {
        this->yOffsetDir = val;
    }

public:
#include "Level_InitLevel.h"
#include "Level_Utils.h"
#include "Level_CanRunningJump.h"
#include "Level_CanStandingJump.h"

#ifndef LEVEL_DATA_FROM_FX

    void init(
        GamePlay& gamePlay,
        Prince& prince,
        uint8_t width,
        uint8_t height,
        uint8_t xLoc,
        uint8_t yLoc) {
        //this->level = gamePlay.level;
        this->width = width;
        this->height = height;

        this->xLoc = xLoc;
        this->yLoc = yLoc;

        this->loadMap(gamePlay);
        this->loadItems(gamePlay.level, prince);

        if(prince.getY() > 56) {
            this->yOffset = prince.getY() - 56;

        } else {
            this->yOffset = 0;
        }
    }

#endif

#ifndef SAVE_MEMORY_ENEMY
    void init_PositionChars(GamePlay& gamePlay, Prince& prince, Enemy& enemy, bool clearSword) {
        enemy.init();
#else
    void init_PositionChars(GamePlay& gamePlay, Prince& prince, bool clearSword) {

#endif
#ifdef LEVEL_DATA_FROM_FX

        FX::seekData(FX::readIndexedUInt24(Levels::level_Data, gamePlay.level));

        {
            uint8_t data[8];

            FX::readBytes((uint8_t*)&data, sizeof(data));
            prince.init(
                data[0],
                data[1],
                static_cast<Direction>(data[2]),
                static_cast<uint16_t>(data[3]),
                clearSword);
            this->width = data[4];
            this->height = data[5];
            this->xLoc = data[6];
            this->yLoc = data[7];
            FX::readEnd();

            this->loadMap(gamePlay);
            this->loadItems(gamePlay.level, prince);

            uint8_t yOffset = 0;
            int16_t y = prince.getY() - 56;

            if(y > 0) {
                yOffset = y;
            }
            this->yOffset = yOffset;
        }

        FX::seekData(FX::readIndexedUInt24(Levels::level_Data, gamePlay.level) + 8);

        {
            EnemyType enemyType = static_cast<EnemyType>(FX::readPendingUInt8());

            while(enemyType != EnemyType::None) {
                uint8_t data[9];
                FX::readBytes((uint8_t*)data, sizeof(data));

#ifndef SAVE_MEMORY_ENEMY
                enemy.init(
                    enemyType,
                    (data[0] * Constants::TileWidth) + data[2],
                    data[0],
                    data[2],
                    data[3],
                    data[4],
                    data[5],
                    (data[1] * Constants::TileHeight) + data[6],
                    Direction::None,
                    Stance::Upright,
                    data[7],
                    static_cast<Status>(data[8]));
#endif

                enemyType = static_cast<EnemyType>(FX::readPendingUInt8());
            }
        }

        FX::readEnd();

#else
        /*
                // Level 1

                if (gamePlay.level == 1) {

                    // #ifndef SAVE_MEMORY_ENEMY
                    //     enemy.init(EnemyType::Guard, 104 - 12 + (70 * Constants::TileWidth), 25+31 + (3 * Constants::TileHeight), Direction::Left, Stance::Upright, 3, Status::Active);          // Sword fight from Left
                    //     enemy.init(EnemyType::Guard, 80 + (40 * Constants::TileWidth), 25 + (0 * Constants::TileHeight), Direction::Left, Stance::Upright, 3, Status::Active);          // Sword fight from Left
                    // #endif

                    // Normal starting pos
                    // prince.init(38-28+12+4, 56, Direction::Right, Stance::Crouch_3_End, clearSword);          
                    // this->init(gamePlay, prince, 90, 9, 60, 0);  

                    // Error falling
                    // prince.init(38+12+8, 56 + 31, Direction::Right, Stance::Crouch_3_End, clearSword);        
                    // this->init(gamePlay, prince, 90, 9, 50, 0);  

                }
*/
#endif
    }

    void incYOffset(int8_t inc) {
        this->yOffset = this->yOffset + inc;
    }

#ifdef SAVE_MEMORY_SOUND
    LevelUpdate update(Arduboy2Base& arduboy, Prince& prince, GamePlay& gamePlay){
#else
    LevelUpdate
        update(Arduboy2Base& arduboy, Prince& prince, GamePlay& gamePlay, ArduboyTonesFX& sound) {
#endif

        LevelUpdate levelUpdate = LevelUpdate::NoAction;

    // Update level offset ..

    switch(this->yOffsetDir) {
    case Direction::Down:

        if(this->yOffset < 31) {
            this->yOffset++;

            if(this->yOffset == 31) {
                this->yOffsetDir = Direction::None;
            }
        }

        break;

    case Direction::Up:

        if(this->yOffset > 0) {
            this->yOffset--;

            if(this->yOffset == 0) {
                this->yOffsetDir = Direction::None;
            }
        }

        break;

    default:
        break;
    }

    for(uint8_t i = 0; i < Constants::Items_Count; i++) {
        Item& item = this->getItem(i);

        //if (item.itemType != ItemType::None) {

        switch(item.itemType) {
        case ItemType::Blade:

            item.data.blade.position++;

            if(item.data.blade.position == 55) {
                item.data.blade.position = -5;
            }

            break;

        case ItemType::ExitDoor_SelfOpen:
        case ItemType::ExitDoor_ButtonOpen:

            if(arduboy.isFrameCount(2)) {
                if(item.data.exitDoor.direction == Direction::Up &&
                   item.data.exitDoor.position < 11) {
                    item.data.exitDoor.position++;
                }
            }

            break;

        case ItemType::Gate:
        case ItemType::Gate_StayOpen:
        case ItemType::Gate_StayClosed:

            if(arduboy.isFrameCount(4)) {
                // Open Gate ..

                switch(item.data.gate.movement) {
                case GateMovement::GoingUp:

                    if(item.data.gate.closingDelay + 9 > item.data.gate.closingDelayMax) {
                        if(item.data.gate.position < 9) {
                            if(item.data.gate.position == 0) {
#ifndef SAVE_MEMORY_SOUND
                                sound.tonesFromFX(Sounds::GateGoingUp);
#endif
                            }

                            item.data.gate.position++;

                            if(item.data.gate.position == 9) {
                                switch(item.itemType) {
                                case ItemType::Gate:

                                    item.data.gate.movement = GateMovement::WaitingToFall;
                                    break;

                                case ItemType::Gate_StayOpen:

                                    item.data.gate.movement = GateMovement::StayOpen;
                                    break;

                                default:
                                    break;
                                }
                            }
                        }
                    }

                    break;

                case GateMovement::WaitingToFall:

                    if(item.data.gate.closingDelay > 0 && item.data.gate.closingDelay <= 9 &&
                       item.itemType != ItemType::Gate_StayOpen) {
                        if(item.data.gate.position == 9) {
#ifndef SAVE_MEMORY_SOUND
                            sound.tonesFromFX(Sounds::GateGoingDown);
#endif
                        }

                        item.data.gate.position = item.data.gate.position - 3;
                        item.data.gate.movement = GateMovement::GoingDown;
                    }

                    break;

                case GateMovement::GoingDown:

                    if(item.data.gate.closingDelay > 0 && item.data.gate.closingDelay <= 9 &&
                       item.itemType != ItemType::Gate_StayOpen) {
                        if(item.data.gate.position >= 3) {
                            item.data.gate.position = item.data.gate.position - 3;

                        } else if(item.data.gate.position < 3) {
                            item.data.gate.position = 0;

                            switch(item.itemType) {
                            case ItemType::Gate:
                                item.data.gate.movement = GateMovement::None;
                                break;

                            case ItemType::Gate_StayClosed:
                                item.data.gate.movement = GateMovement::StayClosed;
                                break;

                            default:
                                break;
                            }
                        }

                        break;
                    }

                default:
                    break;
                }

                // Update gate counter ..

                if(item.data.gate.closingDelay > 0) {
                    item.data.gate.closingDelay--;
                }
            }
            break;

        case ItemType::CollapsingFloor:

            if(arduboy.isFrameCount(4) && item.data.collapsingFloor.frame > 0) {
                item.data.collapsingFloor.frame--;
            }

            if(item.data.collapsingFloor.timeToFall > 1) {
                item.data.collapsingFloor.timeToFall--;
            }

            if(item.data.collapsingFloor.timeToFall == 1) {
                item.data.collapsingFloor.distanceFallen =
                    item.data.collapsingFloor.distanceFallen + 2;

                if(item.data.collapsingFloor.distanceFallen >=
                   item.data.collapsingFloor.distToFall) {
                    if(item.data.collapsingFloor.distToFall == 254) {
                        item.itemType = ItemType::None;

                    } else {
                        item.data.location.y = item.data.location.y +
                                               ((item.data.collapsingFloor.distToFall / 31) + 1);
                        item.itemType = ItemType::CollpasedFloor;

                        // Did the tile fall on the prince?

                        Point newPos = prince.getPosition();
                        int8_t tileXIdx = this->coordToTileIndexX(newPos.x);
                        int8_t tileYIdx = this->coordToTileIndexY(newPos.y);

                        if(tileXIdx == item.data.location.x && tileYIdx == item.data.location.y) {
                            levelUpdate = LevelUpdate::FloorCollapsedOnPrince;
                        }

                        // Remvoe any buttons the tile may have landed on (level 11 only)..

                        if(gamePlay.level == 11) {
                            for(Item& button : items) {
                                if((button.itemType == ItemType::FloorButton1 ||
                                    button.itemType == ItemType::FloorButton2 ||
                                    button.itemType == ItemType::FloorButton_NoEdgeTile) &&
                                   button.data.location.x == item.data.location.x &&
                                   button.data.location.y == item.data.location.y) {
                                    button.itemType = ItemType::None;
                                }
                            }
                        }

                        // Open the gate on level 6 ..

                        if(gamePlay.level == 6) {
                            for(Item& button : items) {
                                if((button.itemType == ItemType::FloorButton1 ||
                                    button.itemType == ItemType::FloorButton_NoEdgeTile) &&
                                   button.data.location.x == item.data.location.x &&
                                   abs(button.data.location.y - item.data.location.y) <= 2) {
                                    this->openGate(3, 0, 0);
                                }
                            }
                        }
                    }
                }
            }

            break;

        case ItemType::Potion_Small:
        case ItemType::Potion_Large:
        case ItemType::Potion_Poison:
        case ItemType::Potion_Float:

            if(arduboy.isFrameCount(6)) {
                switch(item.itemType) {
                case ItemType::Potion_Small:
                case ItemType::Potion_Large:

                    item.data.potion.frame = !item.data.potion.frame;
                    break;

                case ItemType::Potion_Poison:

                    item.data.potion.frame++;
                    if(item.data.potion.frame == 6) item.data.potion.frame = 0;
                    break;

                case ItemType::Potion_Float:

                    item.data.potion.frame++;
                    if(item.data.potion.frame == 7) item.data.potion.frame = 0;
                    break;

                default:
                    break;
                }
            }

            break;

        case ItemType::FloorButton1:
        case ItemType::FloorButton2:
        case ItemType::FloorButton_NoEdgeTile:
        case ItemType::FloorButton3_UpOnly:
        case ItemType::FloorButton3_DownOnly:
        case ItemType::FloorButton4:

            if(arduboy.isFrameCount(4)) {
                if(item.data.floorButton.timeToFall > 1) {
                    item.data.floorButton.timeToFall--;
                }

                if(item.data.floorButton.timeToFall == 1) {
                    item.data.floorButton.frame = 0;
                    item.data.floorButton.timeToFall = 0;
                }
            }
            break;

        case ItemType::Spikes:

            if(arduboy.isFrameCount(4)) {
                switch(item.data.spikes.closingDelay) {
                case Constants::SpikeClosingDelay - 2 ... Constants::SpikeClosingDelay:
                    item.data.spikes.closingDelay--;
                    if(item.data.spikes.position < 3) {
                        item.data.spikes.position++;
                    }
                    break;

                case 1 ... 3:
                    item.data.spikes.closingDelay--;
                    if(item.data.spikes.position > 0) {
                        item.data.spikes.position--;
                    }
                    break;

                case 0:
                    break;

                default:
                    item.data.spikes.closingDelay--;
                    break;
                }
            }

            break;

        default:
            break;
        }

        //}
    }

    if(flash.frame > 0) {
        if(arduboy.isFrameCount(2)) {
            flash.frame--;
        }
    }

    if(arduboy.isFrameCount(4)) {
        if(sign.counter > 1) {
            sign.counter--;
        }
    }

    return levelUpdate;

}

void clearSign() {
    sign.counter = 0;
}

int8_t coordToTileIndexX(int16_t x) {
    return (x / 12);
}

int8_t coordToTileIndexY(int16_t y) {
    return (y / 31);
}

int8_t distToEdgeOfTile(Direction direction, int16_t x) {
    int8_t tile = coordToTileIndexX(x);

    switch(direction) {
    case Direction::Left:
        return x - (tile * 12);

    case Direction::Right:
        return ((tile + 1) * 12) - x;

    default:
        return 255;
    }
}

int8_t getTile(Layer layer, int8_t x, int8_t y, int8_t returnCollapsingTile) {
    if(x <= -6 || x > 15) return TILE_NONE;

    switch(layer) {
    case Layer::Foreground:

#if defined(DEBUG) && defined(DEBUG_GET_TILE)
        DEBUG_PRINT(F("getTile(FG, "));
        DEBUG_PRINT(x);
        DEBUG_PRINT(F(","));
        DEBUG_PRINT(y);
        DEBUG_PRINT(F(") = "));
        DEBUG_PRINTLN(fg[y + 1][x + 3]);
#endif

        return fg[y + 1][x + 6];

    case Layer::Background: {
        int8_t tile = bg[y + 1][x + 6];

#if defined(DEBUG) && defined(DEBUG_GET_TILE)
        DEBUG_PRINT(F("getTile(BG, "));
        DEBUG_PRINT(x);
        DEBUG_PRINT(F(","));
        DEBUG_PRINT(y);
        DEBUG_PRINT(F(") = "));
        DEBUG_PRINTLN(tile);
#endif

        // Substitute tiles if needed ..

        if(returnCollapsingTile != TILE_NONE &&
           this->isCollapsingOrAppearingFloor(this->xLoc + x, this->yLoc + y)) {
            return returnCollapsingTile;
        }

        return tile;

    }

    break;
    }

    return TILE_NONE;
}

uint8_t getItem(ItemType itemType, int8_t x, int8_t y) {
    return getItem(itemType, itemType, x, y);
}

// Locate the itemType at x, y.  Return the index of the specified itemType

uint8_t getItem(ItemType itemType_Start, ItemType itemType_End, int8_t x, int8_t y) {
    for(uint8_t i = 0; i < Constants::Items_Count; i++) {
        Item& item = this->items[i];

        if(item.itemType >= itemType_Start && item.itemType <= itemType_End) {
            if(item.itemType != ItemType::None && item.data.location.x == x &&
               item.data.location.y == y) {
                return i;
            }
        }
    }

    return Constants::NoItemFound;
}

// Get the nth Gate (these are 1based)

Item& getItemByIndex(ItemType itemType_One, ItemType itemType_Two, uint8_t idx) {
    uint8_t count = 0;

    for(uint8_t i = 0; i < Constants::Items_Count; i++) {
        Item& item = this->items[i];

        if(item.itemType >= itemType_One &&
           item.itemType <= (itemType_Two == ItemType::None ? itemType_One : itemType_Two)) {
            count++;

            if(count == idx) {
                return this->items[i];
            }
        }
    }

    return this->items[0];
}

bool isCollapsingOrAppearingFloor(int8_t x, int8_t y) {
    for(Item& item : this->items) {
        if(((item.itemType == ItemType::CollapsingFloor &&
             item.data.collapsingFloor.timeToFall == 0) ||
            item.itemType == ItemType::AppearingFloor) &&
           item.data.location.x == x && item.data.location.y == y) {
            return true;
        }
    }

    return false;
}

WallTileResults isWallTile_ByCoords(
    int8_t x = Constants::CoordNone,
    int8_t y = Constants::CoordNone,
    Direction direction = Direction::Left,
    bool addOffsets = true,
    uint8_t gateHeight = 7) {
    int8_t fgTile = this->getTile(Layer::Foreground, x, y, TILE_FLOOR_BASIC);

    return isWallTile(fgTile, x, y, direction, addOffsets, gateHeight);
}

WallTileResults isWallTile(
    int8_t fgTile,
    int8_t x = Constants::CoordNone,
    int8_t y = Constants::CoordNone,
    Direction direction = Direction::Left,
    bool addOffsets = true,
    uint8_t gateHeight = 7) {
    switch(fgTile) {
    case TILE_FG_WALL_1:
    case TILE_FG_WALL_2:
    case TILE_FG_WALL_3:
    case TILE_FG_WALL_4:
    case TILE_FG_WALL_5:
    case TILE_FG_WALL_6:
    case TILE_FG_WALL_7:
    case TILE_FG_WALL_8:
    case TILE_FLOOR_GATE_FRONT_TRACK_4:
    case TILE_RUG_FRONT_TRACK:

        return WallTileResults::SolidWall;

    case TILE_RUG_1:
    case TILE_RUG_2:

        if(direction == Direction::Right) {
            return WallTileResults::SolidWall;
        }

        return WallTileResults::None;

    case TILE_FLOOR_GATE_FRONT_TRACK_1:
    case TILE_FLOOR_GATE_FRONT_TRACK_2:
    case TILE_FLOOR_GATE_FRONT_TRACK_3:
    case TILE_FLOOR_GATE_FRONT_TRACK_5:

        return testForGate(x, y, direction, addOffsets, gateHeight);

    default:

        if(direction == Direction::Right) {
            return testForGate(x, y, direction, addOffsets, gateHeight);
        }

        return WallTileResults::None;
    }
}

WallTileResults testForGate(
    int8_t x = Constants::CoordNone,
    int8_t y = Constants::CoordNone,
    Direction direction = Direction::Left,
    bool addOffsets = true,
    uint8_t gateHeight = 7) {
    int8_t offset = 0;

    if(direction == Direction::Left) {
        offset = (addOffsets ? 1 : 0);

    } else {
        offset = (addOffsets ? 0 : 1);
    }

    if(x != Constants::CoordNone && y != Constants::CoordNone) {
        uint8_t idx = this->getItem(
            ItemType::Gate,
            ItemType::Gate_StayOpen,
            x + this->getXLocation() + offset,
            y + this->getYLocation());

        if(idx == Constants::NoItemFound) {
            idx = this->getItem(
                ItemType::Gate_StayClosed,
                x + this->getXLocation() + offset,
                y + this->getYLocation());
        }

        if(idx != Constants::NoItemFound) {
            Item& item = this->getItem(idx);

            switch(item.data.gate.movement) {
            case GateMovement::None:
            case GateMovement::GoingUp:
            case GateMovement::WaitingToFall:
                return (
                    item.data.gate.position >= gateHeight ? WallTileResults::None :
                                                            WallTileResults::GateClosed);

            default:
                return WallTileResults::GateClosed;
            }
        }
    }

    return WallTileResults::None;
}

bool isGroundTile_ByCoords(int8_t x, int8_t y) {
    int8_t bgTile = this->getTile(Layer::Background, x, y, TILE_FLOOR_BASIC);

    return isGroundTile(bgTile);
}

bool isGroundTile(int8_t bgTile) {
#if defined(DEBUG) && defined(DEBUG_ISGROUNDTILE)
    DEBUG_PRINT("isGroundTile: ");
    DEBUG_PRINT(bgTile);
    DEBUG_PRINT(" ");
#endif

    switch(bgTile) {
    case TILE_FLOOR_BASIC:
    case TILE_FLOOR_BASIC_TORCH:
    case TILE_FLOOR_LH_END_1:
    case TILE_FLOOR_LH_END_2:
    case TILE_FLOOR_PATTERN_1:
    case TILE_FLOOR_PATTERN_2:
    case TILE_FLOOR_LH_END_PATTERN_1:
    case TILE_FLOOR_LH_END_PATTERN_2:
    case TILE_FLOOR_RH_END:
    case TILE_FLOOR_LH_WALL_1:
    case TILE_FLOOR_LH_WALL_2:
    case TILE_FLOOR_LH_WALL_3:
    case TILE_FLOOR_LH_RUG_1:
    case TILE_COLUMN_LH_WALL:
    case TILE_COLUMN_1:
    case TILE_COLUMN_2:
    case TILE_COLUMN_3:
    case TILE_COLUMN_4:
    case TILE_COLUMN_5:
    case TILE_PILLAR_1:
    case TILE_PILLAR_2:
    case TILE_COLUMN_REAR_1:
    case TILE_COLUMN_REAR_2:
    case TILE_PILLAR_REAR_1:
    case TILE_FLOOR_GATE_REAR_TRACK_1:
    case TILE_FLOOR_GATE_REAR_TRACK_2:
    case TILE_FLOOR_GATE_FRONT_TRACK_1:

#if defined(DEBUG) && defined(DEBUG_ISGROUNDTILE)
        DEBUG_PRINTLN("- BG True");
#endif

        return true;

    default:
        break;
    }

#if defined(DEBUG) && defined(DEBUG_ISGROUNDTILE)
    DEBUG_PRINTLN("- Default False");
#endif

    return false;
}

bool isEdgeTile(int8_t bgTile, int8_t fgTile) {
    (void)fgTile;

    switch(bgTile) {
    case TILE_FLOOR_RH_END_1:
    case TILE_FLOOR_RH_END_2:
    case TILE_FLOOR_RH_END_3:
    case TILE_FLOOR_RH_END_4:
    case TILE_FLOOR_RH_END_5:
    case TILE_FLOOR_RH_PILLAR_END_1:
    case TILE_FLOOR_RH_PILLAR_END_2:
        return true;
    }

    return false;
}

CanFallResult
    canFall(int8_t bgTile, int8_t x = Constants::CoordNone, int8_t y = Constants::CoordNone) {
    switch(bgTile) {
    case TILE_NONE:
    case TILE_WINDOW_LH:
    case TILE_WINDOW_RH:
    case TILE_RUG_1:
    case TILE_RUG_2:
    case TILE_FLOOR_NONE:
    case TILE_FLOOR_NONE_PATTERN:
    case TILE_FLOOR_NONE_LH_WALL_1:
    case TILE_FLOOR_NONE_LH_WALL_2:
    case TILE_FLOOR_NONE_LH_WALL_3:
    case TILE_FLOOR_NONE_LH_WALL_4:
    case TILE_FLOOR_NONE_LH_WALL_5:
    case TILE_FLOOR_NONE_LH_WALL_6:
    case TILE_FLOOR_NONE_LH_WALL_7:
    case TILE_FLOOR_RH_END_1:
    case TILE_FLOOR_RH_END_2:
    case TILE_FLOOR_RH_END_3:
    case TILE_FLOOR_RH_END_4:
    case TILE_FLOOR_RH_END_5:
    case TILE_FLOOR_RH_PILLAR_END_1:
    case TILE_FLOOR_RH_PILLAR_END_2:
    case TILE_FLOOR_RH_END_GATE_1:
    case TILE_FLOOR_RH_END_GATE_2:
    case TILE_FLOOR_RH_END_GATE_RUG:

        if(x != Constants::CoordNone && y != Constants::CoordNone) {
            uint8_t idx = this->getItem(
                ItemType::AppearingFloor,
                ItemType::CollapsingFloor,
                x + this->getXLocation(),
                y + this->getYLocation());

            if(idx != Constants::NoItemFound) {
                Item& item = this->getItem(idx);

                if(item.data.collapsingFloor.distanceFallen == 0) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
                    DEBUG_PRINTLN(" false (on collapsing floor");
#endif

                    return CanFallResult::CannotFall;
                }
            }
        }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
        DEBUG_PRINTLN(" true");
#endif

        return CanFallResult::CanFall;

    case TILE_UPPER_COLUMN_BACKGROUND:
    case TILE_UPPER_COLUMN_FOREGROUND:

        return CanFallResult::CanFall;

    default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
        DEBUG_PRINTLN(" false");
#endif
        return CanFallResult::CannotFall;
    }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
    DEBUG_PRINTLN(" false");
#endif

    return CanFallResult::CannotFall;
}

CanFallResult canFall(Prince& prince, bool testForHoldon) {
    CanFallResult canFall = CanFallResult::CannotFall;
    Point newPos = prince.getPosition();

    ImageDetails imageDetails;
    prince.getImageDetails(imageDetails);

    int8_t tileXIdx = this->coordToTileIndexX(newPos.x + imageDetails.toe) - this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(newPos.y) - this->getYLocation();

    int8_t bgTile1 = this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
    DEBUG_PRINT(F("canFall(toe) stance:"));
    DEBUG_PRINT(prince.getStance());
    DEBUG_PRINT(F(", r:"));
    DEBUG_PRINT(imageDetails.reach);
    DEBUG_PRINT(F(", ft "));
    DEBUG_PRINT(imageDetails.toe);
    DEBUG_PRINT(F(", fh "));
    DEBUG_PRINT(imageDetails.heel);
    DEBUG_PRINT(F(", tileXIdx:"));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", tileYIdx:"));
    DEBUG_PRINT(tileYIdx);
    DEBUG_PRINT(F(", bg "));
    DEBUG_PRINT(bgTile1);
    DEBUG_PRINT(" = ");
#endif

    if(imageDetails.toe != Constants::InAir) {
        canFall = this->canFall(bgTile1, tileXIdx, tileYIdx);

        // If the price cannot fall then return this now.  Ortherwise check the heel position ..

        if(canFall == CanFallResult::CannotFall) {
            return CanFallResult::CannotFall;

        } else {
            int8_t tileXIdx =
                this->coordToTileIndexX(newPos.x + imageDetails.heel) - this->getXLocation();
            int8_t tileYIdx = this->coordToTileIndexY(newPos.y) - this->getYLocation();

            int8_t bgTile1 =
                this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
            DEBUG_PRINT(F("canFall(heel) stance:"));
            DEBUG_PRINT(prince.getStance());
            DEBUG_PRINT(F(", r:"));
            DEBUG_PRINT(imageDetails.reach);
            DEBUG_PRINT(F(", ft "));
            DEBUG_PRINT(imageDetails.toe);
            DEBUG_PRINT(F(", fh "));
            DEBUG_PRINT(imageDetails.heel);
            DEBUG_PRINT(F(", tileXIdx:"));
            DEBUG_PRINT(tileXIdx);
            DEBUG_PRINT(F(", tileYIdx:"));
            DEBUG_PRINT(tileYIdx);
            DEBUG_PRINT(F(", bg "));
            DEBUG_PRINT(bgTile1);
            DEBUG_PRINT(" = ");
#endif

            canFall = this->canFall(bgTile1, tileXIdx, tileYIdx);

            if(canFall == CanFallResult::CannotFall) {
                return CanFallResult::CannotFall;
            }

            // If we can fall, check to see if we can hold on to a ledge below ..

            if(testForHoldon) {
                int8_t distToEdgeOfCurrentTile =
                    distToEdgeOfTile(prince.getDirection(), prince.getPosition().x);

                if(distToEdgeOfCurrentTile == 2) {
                    int8_t tileXIdx = this->coordToTileIndexX(newPos.x + imageDetails.toe) -
                                      this->getXLocation() +
                                      (prince.getDirection() == Direction::Right ? 1 : 0);
                    int8_t tileYIdx = this->coordToTileIndexY(newPos.y) - this->getYLocation();

                    int8_t bgTile1 =
                        this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
                    int8_t fgTile1 =
                        this->getTile(Layer::Foreground, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
                    DEBUG_PRINT(F("Can Fall to Ledge?  tileXIdx:"));
                    DEBUG_PRINT(tileXIdx);
                    DEBUG_PRINT(F(", tileYIdx:"));
                    DEBUG_PRINT(tileYIdx);
                    DEBUG_PRINT(F(", bg "));
                    DEBUG_PRINT(bgTile1);
                    DEBUG_PRINT(F(", fg "));
                    DEBUG_PRINTLN(fgTile1);
#endif

                    switch(prince.getDirection()) {
                    case Direction::Left:

                        switch(bgTile1) {
                        case TILE_FLOOR_RH_END_1:
                        case TILE_FLOOR_RH_END_2:
                        case TILE_FLOOR_RH_END_3:
                        case TILE_FLOOR_RH_END_4:
                        case TILE_FLOOR_RH_END_5:
                        case TILE_FLOOR_RH_END_GATE_1:
                        case TILE_FLOOR_RH_END_GATE_2:
                        case TILE_FLOOR_RH_END_GATE_RUG:
                        case TILE_FLOOR_RH_PILLAR_END_1:
                        case TILE_FLOOR_RH_PILLAR_END_2:
                            return CanFallResult::CanFallToHangingPosition;

                        default:
                            return CanFallResult::CanFall;
                        }

                    case Direction::Right:

                        switch(bgTile1) {
                        case TILE_FLOOR_LH_END_1:
                        case TILE_FLOOR_LH_END_2:
                        case TILE_COLUMN_3:
                        case TILE_FLOOR_LH_END_PATTERN_1:
                        case TILE_FLOOR_LH_END_PATTERN_2:
                            return CanFallResult::CanFallToHangingPosition;

                        default:
                            return CanFallResult::CanFall;
                        }

                    default:
                        break;
                    }
                }

                return canFall;

            } else {
                return canFall;
            }
        }

    } else {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALL)
        DEBUG_PRINTLN(" footToe=InAir ");
#endif
    }

    return CanFallResult::CannotFall;
}

CanFallResult canFallSomeMore(Prince& prince, int8_t xOffset = 0) {
    CanFallResult canFall = CanFallResult::CannotFall;
    Point newPos = prince.getPosition();
    newPos.x = newPos.x + prince.getDirectionOffset(xOffset);

    int8_t tileXIdx = this->coordToTileIndexX(newPos.x) - this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(newPos.y) - this->getYLocation();
    int8_t bgTile1 = this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);

    canFall = this->canFall(bgTile1, tileXIdx, tileYIdx);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANFALLSOMEMORE)
    DEBUG_PRINT(F("canFallSomeMore() stance:"));
    DEBUG_PRINT(prince.getStance());
    DEBUG_PRINT(F(", bg "));
    DEBUG_PRINT(bgTile1);
    DEBUG_PRINT(" = ");
    if(canFall) {
        DEBUG_PRINTLN("true");
    } else {
        DEBUG_PRINTLN("false");
    }
#endif

    return canFall;
}

// Prince is hanging from upper level.  Test to see if he can fall down one level, down one level and 'back, down twwo levels, three .. etc.

CanClimbDownPart2Result canClimbDown_Part2(Prince& prince, int8_t xOffset = 0) {
    Point newPos = prince.getPosition();
    newPos.x = newPos.x + prince.getDirectionOffset(xOffset);

    int8_t tileXIdx = this->coordToTileIndexX(newPos.x) - this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(newPos.y) - this->getYLocation();

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN_PART2)
    DEBUG_PRINT(F("canClimbDown_Part2() "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(","));
    DEBUG_PRINTLN(tileYIdx);
#endif

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN_PART2)
    printCoordToIndex(newPos, tileXIdx, tileYIdx);
#endif

    int8_t bgTile = this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
    int8_t fgTile = this->getTile(Layer::Foreground, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN_PART2)
    DEBUG_PRINT(F("canClimbDown_Part2() Test can land 1 level below ("));
    DEBUG_PRINT(tileXIdx + (prince.getDirection() == Direction::Left ? 1 : 0));
    DEBUG_PRINT(F(","));
    DEBUG_PRINT(tileYIdx);
    DEBUG_PRINT(F(") bg "));
    DEBUG_PRINT(bgTile);
    DEBUG_PRINT(F(", fg "));
    DEBUG_PRINT(fgTile);
    DEBUG_PRINT(F(", isGroundTile: "));
    DEBUG_PRINT(this->isGroundTile(bgTile));
#endif

    bool isGroundTile = this->isGroundTile(bgTile);

    if(isGroundTile) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN_PART2)
        DEBUG_PRINTLN(" return Level_1");
#endif

        return CanClimbDownPart2Result::Level_1;
    }

    bgTile = this->getTile(
        Layer::Background,
        tileXIdx + (prince.getDirection() == Direction::Left ? 0 : 1),
        tileYIdx,
        TILE_FLOOR_BASIC);
    fgTile = this->getTile(
        Layer::Foreground,
        tileXIdx + (prince.getDirection() == Direction::Left ? 0 : 1),
        tileYIdx,
        TILE_FLOOR_BASIC);

    isGroundTile = this->isGroundTile(bgTile);
    bool isEdgeTile = this->isEdgeTile(bgTile, fgTile);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN_PART2)
    DEBUG_PRINTLN(" skip");
    DEBUG_PRINT(F("canClimbDown_Part2() Test can land 1 level below and under ("));
    DEBUG_PRINT(tileXIdx + (prince.getDirection() == Direction::Left ? 0 : 1));
    DEBUG_PRINT(F(","));
    DEBUG_PRINT(tileYIdx);
    DEBUG_PRINT(F(") bg "));
    DEBUG_PRINT(bgTile);
    DEBUG_PRINT(F(", fg "));
    DEBUG_PRINT(fgTile);
    DEBUG_PRINT(F(", isGroundTile: "));
    DEBUG_PRINT(isGroundTile);
    DEBUG_PRINT(F(", isEdgeTile: "));
    DEBUG_PRINT(isEdgeTile);
#endif

    if(isGroundTile || isEdgeTile) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN_PART2)
        DEBUG_PRINTLN(" return Level_1_Under");
#endif

        return CanClimbDownPart2Result::Level_1_Under;
    }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN_PART2)
    DEBUG_PRINTLN(" return Falling");
#endif

    return CanClimbDownPart2Result::Falling;
}

uint8_t
    canReachItem(Prince& prince, ItemType itemTypeStart, ItemType itemTypeEnd = ItemType::None) {
    if(itemTypeEnd == ItemType::None) itemTypeEnd = itemTypeStart;

    int8_t tileXIdx = this->coordToTileIndexX(prince.getPosition().x);
    int8_t tileYIdx = this->coordToTileIndexY(prince.getPosition().y);

    for(uint8_t i = 0; i < Constants::Items_Count; i++) {
        Item& item = this->items[i];

        if((item.itemType >= itemTypeStart && item.itemType <= itemTypeEnd) &&
           item.data.location.x == tileXIdx && item.data.location.y == tileYIdx) {
            return i;
        }
    }

    return Constants::NoItemFound;
}

bool canMoveForward(BaseEntity& entity, Action action, Direction direction, uint8_t xOffset) {
    int8_t offset = (direction == Direction::Left ? -1 : 1);
    int8_t tileXIdx = this->coordToTileIndexX(entity.getPosition().x + (xOffset * offset)) -
                      this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(entity.getPosition().y) - this->getYLocation();

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
    DEBUG_PRINTLN(F("------------------------------"));
    printAction(action);
    DEBUG_PRINT(F(" direction "));
    DEBUG_PRINT((uint8_t)direction);
    DEBUG_PRINT(F(" tileXIdx "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", tileYIdx "));
    DEBUG_PRINT(tileYIdx);
    DEBUG_PRINT(F(" xOffset "));
    DEBUG_PRINT(xOffset);
    DEBUG_PRINT(F(", offset "));
    DEBUG_PRINT(offset);
    DEBUG_PRINT(F(", coordToTileIndexX + offset "));
    DEBUG_PRINT(entity.getPosition().x + (xOffset * offset));
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", coordToTileIndexY "));
    DEBUG_PRINT(entity.getPosition().y);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINTLN(tileYIdx);
#endif

    switch(direction) {
    case Direction::Left: {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
        int8_t bgTile2 =
            this->getTile(Layer::Background, tileXIdx + (1 * offset), tileYIdx, TILE_FLOOR_BASIC);
#endif

        int8_t fgTile2 =
            this->getTile(Layer::Foreground, tileXIdx + (1 * offset), tileYIdx, TILE_FLOOR_BASIC);
        int8_t distToEdgeOfCurrentTile =
            distToEdgeOfTile(direction, entity.getPosition().x + (xOffset * offset));

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
        int8_t bgTile1 = this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
        int8_t fgTile1 = this->getTile(Layer::Foreground, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
        DEBUG_PRINT(F(" left dist "));
        DEBUG_PRINT(distToEdgeOfCurrentTile);
        DEBUG_PRINT(F(", bg "));
        DEBUG_PRINT(bgTile2);
        DEBUG_PRINT(F(" "));
        DEBUG_PRINT(bgTile1);
        DEBUG_PRINT(F(", fg "));
        DEBUG_PRINT(fgTile2);
        DEBUG_PRINT(F(" "));
        DEBUG_PRINT(fgTile1);
        DEBUG_PRINTLN("");
#endif

        switch(action) {
        case Action::SwordStep:
        case Action::SwordStep2:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
            DEBUG_PRINT(F(" Ret 1: "));
            DEBUG_PRINTLN(
                entity.getPosition().x + (xOffset * offset) - static_cast<uint8_t>(action) >=
                (entity.getX_Tile() * Constants::TileWidth) + entity.getX_LeftExtent());
#endif

            return (
                entity.getPosition().x + (xOffset * offset) - static_cast<uint8_t>(action) >=
                (entity.getX_Tile() * Constants::TileWidth) + entity.getX_LeftExtent());

        case Action::SmallStep:
        case Action::CrouchHop:

            switch(distToEdgeOfCurrentTile) {
            case 0 ... 5: {
                WallTileResults wallTile =
                    this->isWallTile(fgTile2, tileXIdx, tileYIdx, Direction::Left, false);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                printTileInfo(bgTile2, fgTile2);
                DEBUG_PRINT(F(" Ret 2: "));
                DEBUG_PRINTLN(wallTile == WallTileResults::None);
#endif

                return (wallTile == WallTileResults::None);
            }
                return false;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                DEBUG_PRINT(F(" Ret 3: "));
                DEBUG_PRINTLN(true);
#endif

                return true;
            }

            break;

        case Action::Step:
        case Action::RunStart:
        case Action::RunningTurn:

            switch(distToEdgeOfCurrentTile) {
            case 0 ... 9: {
                WallTileResults wallTile =
                    this->isWallTile(fgTile2, tileXIdx, tileYIdx, Direction::Left, false);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                printTileInfo(bgTile2, fgTile2);
                DEBUG_PRINT(F(" Ret 4: "));
                DEBUG_PRINTLN(wallTile == WallTileResults::None);
#endif

                return (wallTile == WallTileResults::None);
            }
                return false;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                DEBUG_PRINT(F(" Ret 5: "));
                DEBUG_PRINTLN(true);
#endif

                return true;
            }

            break;

        case Action::RunRepeat: {
            WallTileResults wallTile =
                this->isWallTile(fgTile2, tileXIdx, tileYIdx, Direction::Left, false);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
            printTileInfo(bgTile2, fgTile2);
            DEBUG_PRINT(F(" Ret 4: "));
            DEBUG_PRINTLN(wallTile == WallTileResults::None);
#endif

            return wallTile == WallTileResults::None;
        }

            return false;

        default:
            return false;
        }

    }

    break;

    case Direction::Right: {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
        int8_t bgTile2 =
            this->getTile(Layer::Background, tileXIdx + offset, tileYIdx, TILE_FLOOR_BASIC);
#endif

        int8_t fgTile2 =
            this->getTile(Layer::Foreground, tileXIdx + offset, tileYIdx, TILE_FLOOR_BASIC);
        int8_t distToEdgeOfCurrentTile = distToEdgeOfTile(direction, entity.getPosition().x);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
        int8_t bgTile1 = this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
        int8_t fgTile1 = this->getTile(Layer::Foreground, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
        DEBUG_PRINT(F(" right dist "));
        DEBUG_PRINT(distToEdgeOfCurrentTile);
        DEBUG_PRINT(F(", bg "));
        DEBUG_PRINT(bgTile1);
        DEBUG_PRINT(F(" "));
        DEBUG_PRINT(bgTile2);
        DEBUG_PRINT(F(", fg "));
        DEBUG_PRINT(fgTile1);
        DEBUG_PRINT(F(" "));
        DEBUG_PRINT(fgTile2);
        DEBUG_PRINTLN("");
#endif

        switch(action) {
        case Action::SwordStep:
        case Action::SwordStep2:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
            DEBUG_PRINT(F(" Ret 1: "));
            DEBUG_PRINTLN(
                entity.getPosition().x + (xOffset * offset) + static_cast<uint8_t>(action) <=
                (entity.getX_Tile() * Constants::TileWidth) + entity.getX_RightExtent());
#endif

            return (
                entity.getPosition().x + (xOffset * offset) + static_cast<uint8_t>(action) <=
                (entity.getX_Tile() * Constants::TileWidth) + entity.getX_RightExtent());

        case Action::SmallStep:
        case Action::CrouchHop:

            switch(distToEdgeOfCurrentTile) {
            case 0 ... 5: {
                WallTileResults wallTile =
                    this->isWallTile(fgTile2, tileXIdx, tileYIdx, Direction::Right, false);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                printTileInfo(bgTile2, fgTile2);
                DEBUG_PRINT(F(" Ret 2: "));
                DEBUG_PRINTLN(wallTile == WallTileResults::None);
#endif

                return (wallTile == WallTileResults::None);
            }
                return false;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                DEBUG_PRINT(F(" Ret 3: "));
                DEBUG_PRINTLN(true);
#endif

                return true;
            }

        case Action::Step:
        case Action::RunStart:
        case Action::RunningTurn:

            switch(distToEdgeOfCurrentTile) {
            case 0 ... 9: {
                WallTileResults wallTile =
                    this->isWallTile(fgTile2, tileXIdx, tileYIdx, Direction::Right, false);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                printTileInfo(bgTile2, fgTile2);
                DEBUG_PRINT(F(" Ret 4: "));
                DEBUG_PRINTLN(wallTile == WallTileResults::None);
#endif
                return (wallTile == WallTileResults::None);
            }
                return false;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
                DEBUG_PRINT(F(" Ret 5: "));
                DEBUG_PRINTLN(true);
#endif

                return true;
            }

            return false;

        case Action::RunRepeat: {
            WallTileResults wallTile =
                this->isWallTile(fgTile2, tileXIdx, tileYIdx, Direction::Right, false);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANMOVEFORWARD)
            DEBUG_PRINT(F(" Ret 6: "));
            DEBUG_PRINTLN(wallTile == WallTileResults::None);
            printTileInfo(bgTile2, fgTile2);
#endif

            return wallTile == WallTileResults::None;
        }

            return false;

        default:
            return false;
        }

    }

    break;

    default:
        break;
    }

    return true;
}

CanJumpUpResult canJumpUp(Prince& prince) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
    DEBUG_PRINTLN(F("-----------------------------------------------------"));
#endif

    CanJumpUpResult resultLeft = this->canJumpUp_Test(prince, prince.getDirection());

    switch(resultLeft) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)

    case CanJumpUpResult::Jump:
    case CanJumpUpResult::StepThenJump:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
        DEBUG_PRINT(F("L1 Test success, Return "));
        DEBUG_PRINTLN(static_cast<uint8_t>(resultLeft));
#endif

        return resultLeft;

    case CanJumpUpResult::JumpThenFall_CollapseFloor:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
        DEBUG_PRINTLN(F("L2 Test success, Return JumpThenFall_CollapseFloor"));
#endif

        return CanJumpUpResult::JumpThenFall_CollapseFloor;

    case CanJumpUpResult::JumpThenFall_CollapseFloorAbove:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
        DEBUG_PRINTLN(F("L3 Test success, Return JumpThenFall_CollapseFloorAbove"));
#endif

        return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;

    case CanJumpUpResult::StepThenJumpThenFall_CollapseFloor:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
        DEBUG_PRINTLN(F("L4 Test success, Return StepThenJumpThenFall_CollapseFloor"));
#endif

        return CanJumpUpResult::StepThenJumpThenFall_CollapseFloor;

#else

        case CanJumpUpResult::Jump:
        case CanJumpUpResult::StepThenJump:
        case CanJumpUpResult::JumpThenFall_CollapseFloor:
        case CanJumpUpResult::JumpThenFall_CollapseFloorAbove:
        case CanJumpUpResult::StepThenJumpThenFall_CollapseFloor:
            return resultLeft;

#endif

    default: {
        CanJumpUpResult resultRight = this->canJumpUp_Test(prince, prince.getOppositeDirection());

        switch(resultRight) {
        case CanJumpUpResult::Jump:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
            DEBUG_PRINT(F("R1 Test success, Return TurnThenJump"));
#endif

            return CanJumpUpResult::TurnThenJump;

        default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
            DEBUG_PRINT(F("R2 Test failed, Return "));
            DEBUG_PRINTLN(static_cast<uint8_t>(resultLeft));
#endif

            CanJumpUpResult result = this->canJumpUp_Test_Dist10(prince, prince.getDirection());

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
            DEBUG_PRINT(F("R3 canJumpUp_Test_Dist10 Return "));
            DEBUG_PRINTLN(static_cast<uint8_t>(result));
#endif

            return result;
        }

    }

    break;
    }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
    DEBUG_PRINT(F("Return None"));
#endif

    return CanJumpUpResult::None;
}

CanJumpUpResult canJumpUp_Test(Prince& prince, Direction direction) {
    int8_t inc = (direction == Direction::Left ? -1 : 1);
    int8_t tileXIdx = this->coordToTileIndexX(prince.getPosition().x) - this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation();

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
    DEBUG_PRINT(F("canJumpUp_Test: direction "));
    DEBUG_PRINT((direction == Direction::Left ? "Left" : "Right"));
    DEBUG_PRINT(F(", canJumpUp_Test: coordToTileIndexX "));
    DEBUG_PRINT(prince.getPosition().x);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", coordToTileIndexY "));
    DEBUG_PRINT(prince.getPosition().y);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINTLN(tileYIdx);
#endif

    int8_t bgTile1 = this->getTile(Layer::Background, tileXIdx, tileYIdx - 1, TILE_FLOOR_BASIC);
    int8_t bgTile2 =
        this->getTile(Layer::Background, tileXIdx + inc, tileYIdx - 1, TILE_COLLAPSING_FLOOR);
    int8_t midTile =
        this->getTile(Layer::Background, tileXIdx, tileYIdx - 1, TILE_COLLAPSING_FLOOR);
    int8_t distToEdge = distToEdgeOfTile(direction, prince.getPosition().x);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
    DEBUG_PRINT(F("dist "));
    DEBUG_PRINT(distToEdge);
    DEBUG_PRINT(F(", bg "));
    DEBUG_PRINT(bgTile1);
    DEBUG_PRINT(F(" "));
    DEBUG_PRINT(bgTile2);
    DEBUG_PRINT(F(", mid "));
    DEBUG_PRINT(midTile);
    DEBUG_PRINT(F(" ("));
    DEBUG_PRINT(tileXIdx - inc);
    DEBUG_PRINT(F(","));
    DEBUG_PRINT(tileYIdx - 1);
    DEBUG_PRINT(F(")"));
    DEBUG_PRINTLN("");
#endif

    switch(direction) {
    case Direction::Left:

        switch(distToEdge) {
        case 7 ... 12:

            switch(midTile) {
            case TILE_COLLAPSING_FLOOR:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                DEBUG_PRINTLN(F("L1 JumpThenFall_CollapseFloorAbove"));
#endif

                return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;

            case TILE_FLOOR_NONE:
            case TILE_FLOOR_NONE_PATTERN:
#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                DEBUG_PRINTLN(F("L1.1 JumpThenFall"));
#endif

                return CanJumpUpResult::JumpThenFall;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                DEBUG_PRINTLN(F("L2  JumpThenFall_HideHands"));
#endif

                return CanJumpUpResult::JumpThenFall_HideHands;
            }

            break;

        case 3 ... 6:

            switch(bgTile1) {
            case TILE_FLOOR_RH_END:
            case TILE_FLOOR_RH_END_1:
            case TILE_FLOOR_RH_END_2:
            case TILE_FLOOR_RH_END_3:
            case TILE_FLOOR_RH_END_4:
            case TILE_FLOOR_RH_END_5:
            case TILE_FLOOR_RH_END_GATE_1:
            case TILE_FLOOR_RH_END_GATE_2:
            case TILE_FLOOR_RH_END_GATE_RUG:
            // case TILE_COLUMN_LH_WALL:        Found this bug in level 12.
            case TILE_FLOOR_RH_PILLAR_END_1:
            case TILE_FLOOR_RH_PILLAR_END_2:

                switch(midTile) {
                case TILE_COLLAPSING_FLOOR:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                    DEBUG_PRINTLN(F("L3  JumpThenFall_CollapseFloorAbove"));
#endif

                    return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;

                default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                    DEBUG_PRINTLN(F("L4  StepThenJump"));
#endif

                    return CanJumpUpResult::StepThenJump;
                }

                break;

            default:

                switch(bgTile2) {
                case TILE_COLLAPSING_FLOOR:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                    DEBUG_PRINTLN(F("L5  StepThenJumpThenFall_CollapseFloor"));
#endif

                    return CanJumpUpResult::StepThenJumpThenFall_CollapseFloor;

                default:

                    switch(midTile) {
                    case TILE_COLLAPSING_FLOOR:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                        DEBUG_PRINTLN(F("L6  JumpThenFall_CollapseFloorAbove"));
#endif

                        return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;

                    case TILE_FLOOR_NONE:
                    case TILE_FLOOR_NONE_PATTERN:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                        DEBUG_PRINTLN(F("L61  JumpThenFall"));
#endif

                        return CanJumpUpResult::JumpThenFall;

                    default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                        DEBUG_PRINTLN(F("L7  JumpThenFall_HideHands"));
#endif

                        return CanJumpUpResult::JumpThenFall_HideHands;
                    }

                    break;
                }

                break;
            }

            break;

        case 0 ... 2:

            switch(bgTile1) {
            case TILE_FLOOR_RH_END:
            case TILE_FLOOR_RH_END_1:
            case TILE_FLOOR_RH_END_2:
            case TILE_FLOOR_RH_END_3:
            case TILE_FLOOR_RH_END_4:
            case TILE_FLOOR_RH_END_5:
            case TILE_FLOOR_RH_END_GATE_1:
            case TILE_FLOOR_RH_END_GATE_2:
            case TILE_FLOOR_RH_END_GATE_RUG:
            case TILE_FLOOR_RH_PILLAR_END_1:
            case TILE_FLOOR_RH_PILLAR_END_2:

                // case TILE_COLUMN_LH_WALL:                << SJH removed 29/12

                if(midTile == TILE_COLLAPSING_FLOOR) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                    DEBUG_PRINTLN(F("L8  JumpThenFall_CollapseFloorAbove"));
#endif

                    return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;
                } else {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                    DEBUG_PRINTLN(F("L9  Jump"));
#endif

                    return CanJumpUpResult::Jump;
                }

                break;

            default:

                switch(bgTile2) {
                case TILE_COLLAPSING_FLOOR:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                    DEBUG_PRINTLN(F("L10  JumpThenFall_CollapseFloor"));
#endif

                    return CanJumpUpResult::JumpThenFall_CollapseFloor;

                default:

                    switch(midTile) {
                    case TILE_COLLAPSING_FLOOR:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                        DEBUG_PRINTLN(F("L11  JumpThenFall_CollapseFloorAbove"));
#endif

                        return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;

                    case TILE_FLOOR_NONE:
                    case TILE_FLOOR_NONE_PATTERN:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                        DEBUG_PRINTLN(F("L61  JumpThenFall"));
#endif

                        return CanJumpUpResult::JumpThenFall;

                    default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
                        DEBUG_PRINTLN(F("L12  JumpThenFall_HideHands"));
#endif

                        return CanJumpUpResult::JumpThenFall_HideHands;
                    }
                }
            }

            break;
        }

        break;

    case Direction::Right:

        switch(distToEdge) {
        case 7 ... 12:

            if(midTile == TILE_COLLAPSING_FLOOR) {
                return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;
            } else {
                return CanJumpUpResult::JumpThenFall;
            }

            break;

        case 3 ... 6:

            switch(bgTile2) {
            case TILE_FLOOR_LH_END_1:
            case TILE_FLOOR_LH_END_2:
            case TILE_FLOOR_LH_END_PATTERN_1:
            case TILE_FLOOR_LH_END_PATTERN_2:
            case TILE_COLUMN_3:
            case TILE_PILLAR_2:

                if(midTile == TILE_COLLAPSING_FLOOR) {
                    return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;
                } else {
                    return CanJumpUpResult::StepThenJump;
                }

            case TILE_COLLAPSING_FLOOR:

                return CanJumpUpResult::StepThenJumpThenFall_CollapseFloor;

            default:

                if(midTile == TILE_COLLAPSING_FLOOR) {
                    return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;
                } else {
                    return CanJumpUpResult::JumpThenFall;
                }
            }

            break;

        case 0 ... 2:

            switch(bgTile2) {
            case TILE_FLOOR_LH_END_1:
            case TILE_FLOOR_LH_END_2:
            case TILE_FLOOR_LH_END_PATTERN_1:
            case TILE_FLOOR_LH_END_PATTERN_2:
            case TILE_COLUMN_3:
            case TILE_PILLAR_2:

                if(midTile == TILE_COLLAPSING_FLOOR) {
                    return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;
                } else {
                    return CanJumpUpResult::Jump;
                }

                break;

            case TILE_COLLAPSING_FLOOR:
                return CanJumpUpResult::JumpThenFall_CollapseFloor;

            default:

                if(midTile == TILE_COLLAPSING_FLOOR) {
                    return CanJumpUpResult::JumpThenFall_CollapseFloorAbove;
                } else {
                    return CanJumpUpResult::JumpThenFall;
                }
            }

            break;
        }

        break;

    default:
        break;
    }

    return CanJumpUpResult::JumpThenFall;
}

CanJumpUpResult canJumpUp_Test_Dist10(Prince& prince, Direction direction) {
    int8_t tileXIdx = this->coordToTileIndexX(prince.getPosition().x) - this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation();

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
    DEBUG_PRINT(F("canJumpUp_Test_Dist10: direction "));
    DEBUG_PRINT((direction == Direction::Left ? "Left" : "Right"));
    DEBUG_PRINT(F(", coordToTileIndexX "));
    DEBUG_PRINT(prince.getPosition().x);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", coordToTileIndexY "));
    DEBUG_PRINT(prince.getPosition().y);
    DEBUG_PRINT(" = ");
    DEBUG_PRINTLN(tileYIdx);
#endif

    int8_t bgTile1 =
        this->getTile(Layer::Background, tileXIdx + 1, tileYIdx - 1, TILE_FLOOR_BASIC);
    int8_t bgTile2 = this->getTile(Layer::Background, tileXIdx, tileYIdx - 1, TILE_FLOOR_BASIC);
    int8_t bgTile3 =
        this->getTile(Layer::Background, tileXIdx - 1, tileYIdx - 1, TILE_FLOOR_BASIC);
    int8_t distToEdge = distToEdgeOfTile(direction, prince.getPosition().x);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP)
    DEBUG_PRINT(F("dist "));
    DEBUG_PRINT(distToEdge);
    DEBUG_PRINT(F(", bg "));
    DEBUG_PRINT(bgTile1);
    DEBUG_PRINT(F(" "));
    DEBUG_PRINT(bgTile2);
    DEBUG_PRINT(F(" "));
    DEBUG_PRINT(bgTile3);
    DEBUG_PRINTLN("");
#endif

    switch(direction) {
    case Direction::Left:

        switch(distToEdge) {
        case 10:

            switch(bgTile1) {
            case TILE_FLOOR_RH_END:
            case TILE_FLOOR_RH_END_1:
            case TILE_FLOOR_RH_END_2:
            case TILE_FLOOR_RH_END_3:
            case TILE_FLOOR_RH_END_4:
            case TILE_FLOOR_RH_END_5:
            case TILE_FLOOR_RH_END_GATE_1:
            case TILE_FLOOR_RH_END_GATE_2:
            case TILE_FLOOR_RH_END_GATE_RUG:
            case TILE_COLUMN_LH_WALL:
            case TILE_FLOOR_RH_PILLAR_END_1:
            case TILE_FLOOR_RH_PILLAR_END_2:

                if(this->isGroundTile(bgTile1)) {
                    return CanJumpUpResult::JumpThenFall_HideHands;
                }

                return CanJumpUpResult::JumpDist10;

            case TILE_FLOOR_NONE:
            case TILE_FLOOR_NONE_PATTERN:
                return CanJumpUpResult::JumpThenFall;

            default:
                return CanJumpUpResult::JumpThenFall_HideHands;
            }

            break;

        default:

            switch(bgTile2) {
            case TILE_FLOOR_NONE:
            case TILE_FLOOR_NONE_PATTERN:
                return CanJumpUpResult::JumpThenFall;

            default:
                return CanJumpUpResult::JumpThenFall_HideHands;
            }

            break;
        }

        break;

    case Direction::Right:

        switch(distToEdge) {
        case 10:

            switch(bgTile2) {
            case TILE_FLOOR_LH_END_1:
            case TILE_FLOOR_LH_END_2:
            case TILE_FLOOR_LH_END_PATTERN_1:
            case TILE_FLOOR_LH_END_PATTERN_2:
            case TILE_COLUMN_3:
            case TILE_PILLAR_2:

                if(this->isGroundTile(bgTile3)) {
                    return CanJumpUpResult::JumpThenFall_HideHands;
                }
                return CanJumpUpResult::JumpDist10;

            case TILE_FLOOR_NONE:
            case TILE_FLOOR_NONE_PATTERN:
                return CanJumpUpResult::JumpThenFall;

            default:
                return CanJumpUpResult::JumpThenFall_HideHands;
            }

            break;

        default:

            switch(bgTile2) {
            case TILE_FLOOR_NONE:
            case TILE_FLOOR_NONE_PATTERN:
                return CanJumpUpResult::JumpThenFall;

            default:
                return CanJumpUpResult::JumpThenFall_HideHands;
            }

            break;
        }

        break;

    default:
        break;
    }

    return CanJumpUpResult::JumpThenFall;
}

bool canJumpUp_Part2(Prince& prince) {
    int8_t tileXIdx = 0;
    int8_t tileYIdx = 0;

    switch(prince.getDirection()) {
    case Direction::Left: {
        tileXIdx = this->coordToTileIndexX(prince.getPosition().x) - this->getXLocation();
        tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation() - 1;

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP_PART2)
        printCoordToIndex(prince.getPosition(), tileXIdx, tileYIdx);
#endif

    }

    break;

    case Direction::Right: {
        tileXIdx = this->coordToTileIndexX(prince.getPosition().x) - this->getXLocation() + 1;
        tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation() - 1;

#if defined(DEBUG) && defined(DEBUG_ACTION_CANJUMPUP_PART2)
        printCoordToIndex(prince.getPosition(), tileXIdx, tileYIdx);
#endif

    }

    break;

    default:
        break;
    }

    // Look for closed gate in the same cell as possible ledge ..

    uint8_t gatePosition = 255;

    for(uint8_t i = 0; i < Constants::Items_Count; i++) {
        Item& item = this->items[i];

        if(item.itemType == ItemType::Gate || item.itemType == ItemType::Gate_StayClosed) {
            if(item.data.location.x == tileXIdx + this->xLoc &&
               item.data.location.y == tileYIdx + this->yLoc) {
                gatePosition = item.data.gate.position;
                break;
            }
        }
    }

    // Was a gate found?

    return (gatePosition == 9 || gatePosition == 255);
}

CanClimbDownResult canClimbDown(Prince& prince) {
    // switch (prince.getDirection()) {

    //     case Direction::Left:
    //         {
    CanClimbDownResult resultRight = canClimbDown_Test(prince, prince.getOppositeDirection());

    switch(resultRight) {
    case CanClimbDownResult::ClimbDown:
    case CanClimbDownResult::StepThenClimbDown:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
        DEBUG_PRINT(F("L1 Return: "));
        DEBUG_PRINTLN(static_cast<uint8_t>(resultRight));
#endif

        return resultRight;

    default: {
        CanClimbDownResult resultLeft = canClimbDown_Test(prince, prince.getDirection());

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
        DEBUG_PRINT(F("canClimbDown: check right, result "));
        DEBUG_PRINTLN(static_cast<uint8_t>(resultLeft));
#endif

        switch(resultLeft) {
        case CanClimbDownResult::ClimbDown:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
            DEBUG_PRINTLN(F("L2 Return: TurnThenClimbDown"));
#endif

            return CanClimbDownResult::TurnThenClimbDown;

        case CanClimbDownResult::StepThenClimbDown:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
            DEBUG_PRINTLN(F("L3 Return: StepThenTurnThenClimbDown"));
#endif

            return CanClimbDownResult::StepThenTurnThenClimbDown;

        default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
            DEBUG_PRINT(F("L4 Return: "));
            DEBUG_PRINTLN(static_cast<uint8_t>(resultRight));
#endif

            return resultRight;
        }
    }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
        DEBUG_PRINTLN(F("L5 Return: None"));
#endif

        return CanClimbDownResult::None;
        break;
    }

    // }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    DEBUG_PRINTLN(F("L6 Return: None"));
#endif

    return CanClimbDownResult::None;

    //     case Direction::Right:
    //         {
    //             CanClimbDownResult resultLeft = canClimbDown_Test(prince, Direction::Left);

    //             #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //             DEBUG_PRINT(F("canClimbDown: facing right, initial result "));
    //             DEBUG_PRINTLN(static_cast<uint8_t>(resultLeft));
    //             #endif

    //             switch (resultLeft) {

    //                 case CanClimbDownResult::ClimbDown:
    //                 case CanClimbDownResult::StepThenClimbDown:

    //                     #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //                     DEBUG_PRINT(F("R1 Return: "));
    //                     DEBUG_PRINTLN(static_cast<uint8_t>(resultLeft));
    //                     #endif

    //                     return resultLeft;
    //                     break;

    //                 default:
    //                     {
    //                         CanClimbDownResult resultRight = canClimbDown_Test(prince, Direction::Right);

    //                         #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //                         DEBUG_PRINT(F("canClimbDown: check left, result "));
    //                         DEBUG_PRINTLN(static_cast<uint8_t>(resultLeft));
    //                         #endif

    //                         switch (resultRight) {

    //                             case CanClimbDownResult::ClimbDown:

    //                                 #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //                                 DEBUG_PRINTLN(F("R2 Return: TurnThenClimbDown"));
    //                                 #endif

    //                                 return CanClimbDownResult::TurnThenClimbDown;
    //                                 break;

    //                             case CanClimbDownResult::StepThenClimbDown:

    //                                 #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //                                 DEBUG_PRINTLN(F("R3 Return: StepThenTurnThenClimbDown"));
    //                                 #endif

    //                                 return CanClimbDownResult::StepThenTurnThenClimbDown;
    //                                 break;

    //                             default:

    //                                 #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //                                 DEBUG_PRINT(F("R4 Return: "));
    //                                 DEBUG_PRINTLN(static_cast<uint8_t>(resultLeft));
    //                                 #endif

    //                                 return resultLeft;
    //                                 break;

    //                         }

    //                     }

    //                     #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //                     DEBUG_PRINTLN(F("R5 Return: None"));
    //                     #endif

    //                     return CanClimbDownResult::None;
    //                     break;

    //             }

    //         }

    //         #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //         DEBUG_PRINTLN(F("R^ Return: None"));
    //         #endif

    //         return CanClimbDownResult::None;
    //         break;

    //     default:

    //         #if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    //         DEBUG_PRINTLN(F("Return: None"));
    //         #endif

    //         return CanClimbDownResult::None;
    //         break;

    // }

    return CanClimbDownResult::None;
}

CanClimbDownResult canClimbDown_Test(Prince& prince, Direction direction) {
    int8_t tileXIdx = this->coordToTileIndexX(prince.getPosition().x) - this->getXLocation() +
                      (direction == Direction::Right ? 1 : 0);
    int8_t tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation();

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    printCoordToIndex(prince.getPosition(), tileXIdx, tileYIdx);
#endif

    int8_t bgTile1 = this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
    int8_t bgTile2 = this->getTile(Layer::Background, tileXIdx - 1, tileYIdx, TILE_FLOOR_BASIC);
    int8_t distToEdge = distToEdgeOfTile(direction, prince.getPosition().x);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANCLIMBDOWN)
    if(direction == Direction::Left) {
        DEBUG_PRINT(F("Left "));
    } else {
        DEBUG_PRINT(F("Right "));
    }
    DEBUG_PRINT(F("dist "));
    DEBUG_PRINT(distToEdge);
    DEBUG_PRINT(F(", bg1 "));
    DEBUG_PRINT(bgTile1);
    DEBUG_PRINT(F(", bg2 "));
    DEBUG_PRINT(bgTile2);
    DEBUG_PRINTLN("");
#endif

    switch(direction) {
    case Direction::Left:

        switch(distToEdge) {
        case 7 ... 12:
            return CanClimbDownResult::None;

        case 3 ... 6:

            switch(bgTile1) {
            case TILE_FLOOR_LH_END_1:
            case TILE_FLOOR_LH_END_2:
            case TILE_FLOOR_LH_END_PATTERN_1:
            case TILE_FLOOR_LH_END_PATTERN_2:
            case TILE_COLUMN_3:
            case TILE_PILLAR_2:

                if(bgTile2 != TILE_FLOOR_BASIC) {
                    return CanClimbDownResult::StepThenClimbDown;
                }

                return CanClimbDownResult::None;

            default:
                return CanClimbDownResult::None;
            }

            break;

        case 0 ... 2:

            switch(bgTile1) {
            case TILE_FLOOR_LH_END_1:
            case TILE_FLOOR_LH_END_2:
            case TILE_FLOOR_LH_END_PATTERN_1:
            case TILE_FLOOR_LH_END_PATTERN_2:
            case TILE_COLUMN_3:
            case TILE_PILLAR_2:

                if(bgTile2 != TILE_FLOOR_BASIC) {
                    return CanClimbDownResult::ClimbDown;
                }

                return CanClimbDownResult::None;

            default:
                return CanClimbDownResult::None;
            }

            break;
        }

        return CanClimbDownResult::None;

    case Direction::Right:

        switch(distToEdge) {
        case 7 ... 12:
            return CanClimbDownResult::None;

        case 3 ... 6:

            switch(bgTile1) {
            // case TILE_FLOOR_RH_END:
            case TILE_FLOOR_RH_END_1:
            case TILE_FLOOR_RH_END_2:
            case TILE_FLOOR_RH_END_3:
            case TILE_FLOOR_RH_END_4:
            case TILE_FLOOR_RH_END_5:
            // case TILE_FLOOR_RH_END_GATE_1:
            // case TILE_FLOOR_RH_END_GATE_2:
            // case TILE_FLOOR_RH_END_GATE_RUG:
            case TILE_FLOOR_RH_PILLAR_END_1:
            case TILE_FLOOR_RH_PILLAR_END_2:
                return CanClimbDownResult::StepThenClimbDown;

            // case TILE_FLOOR_RH_END:
            case TILE_FLOOR_RH_END_GATE_1:
            case TILE_FLOOR_RH_END_GATE_2:
            case TILE_FLOOR_RH_END_GATE_RUG:

                if(isWallTile_ByCoords(tileXIdx, tileYIdx, Direction::Right) !=
                   WallTileResults::None) {
                    return CanClimbDownResult::None;
                }

                return CanClimbDownResult::StepThenClimbDown;

            default:
                return CanClimbDownResult::None;
            }

            break;

        case 0 ... 2:

            switch(bgTile1) {
            // case TILE_FLOOR_RH_END:
            case TILE_FLOOR_RH_END_1:
            case TILE_FLOOR_RH_END_2:
            case TILE_FLOOR_RH_END_3:
            case TILE_FLOOR_RH_END_4:
            case TILE_FLOOR_RH_END_5:
            // case TILE_FLOOR_RH_END_GATE_1:
            // case TILE_FLOOR_RH_END_GATE_2:
            // case TILE_FLOOR_RH_END_GATE_RUG:
            case TILE_FLOOR_RH_PILLAR_END_1:
            case TILE_FLOOR_RH_PILLAR_END_2:
                return CanClimbDownResult::ClimbDown;

            case TILE_FLOOR_RH_END_GATE_1:
            case TILE_FLOOR_RH_END_GATE_2:
            case TILE_FLOOR_RH_END_GATE_RUG:

                if(isWallTile_ByCoords(tileXIdx, tileYIdx, Direction::Right) !=
                   WallTileResults::None) {
                    return CanClimbDownResult::None;
                }

                return CanClimbDownResult::ClimbDown;

            default:
                return CanClimbDownResult::None;
            }

            break;
        }

        return CanClimbDownResult::None;

    default:

        return CanClimbDownResult::None;
    }
}

bool collideWithWall(Prince& prince) {
    ImageDetails imageDetails;
    // Direction direction = prince.getDirection();
    prince.getImageDetails(imageDetails);

#if defined(DEBUG) && defined(DEBUG_ACTION_COLLIDEWITHWALL)
    DEBUG_PRINT("collideWithWall() Stance: ");
    DEBUG_PRINT(prince.getStance());
    DEBUG_PRINT(", reach: ");
    DEBUG_PRINTLN(imageDetails.reach);
#endif

    int8_t tileXIdx = this->coordToTileIndexX(prince.getPosition(imageDetails.reach).x) -
                      this->getXLocation(); // + (direction == Direction::Right ? 1 : 0);
    int8_t tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation();

#if defined(DEBUG) && defined(DEBUG_ACTION_COLLIDEWITHWALL)
    printCoordToIndex(prince.getPosition(), tileXIdx, tileYIdx);
#endif

    int8_t fgTile = this->getTile(Layer::Foreground, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);

#if defined(DEBUG) && defined(DEBUG_ACTION_COLLIDEWITHWALL)
    int8_t bgTile = this->getTile(Layer::Background, tileXIdx, tileYIdx, TILE_FLOOR_BASIC);
    DEBUG_PRINT(F(", direction "));
    if(prince.getDirection() == Direction::Left) {
        DEBUG_PRINT(F("Left "));
    } else {
        DEBUG_PRINT(F("Right "));
    }
    DEBUG_PRINT(F(", bg "));
    DEBUG_PRINT(bgTile);
    DEBUG_PRINT(F(", fg "));
    DEBUG_PRINT(fgTile);
    DEBUG_PRINT(F(", isWall() "));
    DEBUG_PRINT((uint8_t)isWallTile(fgTile, Constants::CoordNone, Constants::CoordNone));
    DEBUG_PRINTLN("");
#endif

    return (
        isWallTile(fgTile, Constants::CoordNone, Constants::CoordNone) != WallTileResults::None);
}

void openGate(uint8_t gateIndex, uint8_t closingDelay, uint8_t closingDelayMax) {
    if(gateIndex == 0) return;

    Item& gate = this->getItemByIndex(ItemType::Gate, ItemType::Gate_StayOpen, gateIndex);

    if(closingDelay != Constants::Gate_FallingDelayNotSpecified) {
        gate.data.gate.closingDelay = closingDelay;
        gate.data.gate.closingDelayMax = closingDelayMax;

        switch(gate.data.gate.movement) {
        case GateMovement::GoingUp:
        case GateMovement::GoingDown:
        case GateMovement::WaitingToFall:

            gate.data.gate.movement = GateMovement::GoingDown;
            break;

        case GateMovement::None:

            if(gate.data.gate.position == 9) {
                gate.data.gate.movement = GateMovement::GoingDown;
            } else {
                gate.data.gate.movement = GateMovement::GoingUp;
            }
            break;

        default:
            break;
        }
    } else {
        gate.data.gate.closingDelay = gate.data.gate.defaultClosingDelay;
        gate.data.gate.closingDelayMax = gate.data.gate.defaultClosingDelay;

        switch(gate.data.gate.movement) {
        case GateMovement::GoingUp:
        case GateMovement::GoingDown:
        case GateMovement::WaitingToRaise:
        case GateMovement::None:

            gate.data.gate.movement = GateMovement::GoingUp;
            break;

        default:
            break;
        }
    }

    //gate.data.gate.movement = GateMovement::GoingUp;
}

void rippleCollapsingFloors() {
    for(uint8_t i = 0; i < Constants::Items_Count; i++) {
        Item& item = this->getItem(i);

        if(item.itemType == ItemType::CollapsingFloor && item.data.collapsingFloor.frame == 0) {
            item.data.collapsingFloor.frame = 3;
        }
    }
}
}
;
