#ifndef MAP_H_
#define MAP_H_

#include "game/Engine.h"
#include "game/Defines.h"

#ifdef STANDARD_FILE_STREAMING
#include <stdio.h>
#endif
#ifdef PETIT_FATFS_FILE_STREAMING
#include <petit_fatfs.h>
#endif

#define MAP_OUT_OF_BOUNDS 0xff

//#define WRAP_TILE_BUFFER_SIZE(x) ((x) & 0x1f)
#define WRAP_TILE_BUFFER_SIZE(x) ((x) % MAP_BUFFER_SIZE)

enum MapRead_Orientation {
    MapRead_Horizontal,
    MapRead_Vertical
};

enum DoorType {
    DoorType_None,
    DoorType_StandardHorizontal,
    DoorType_StandardVertical,
    DoorType_Locked1Horizontal,
    DoorType_Locked1Vertical,
    DoorType_Locked2Horizontal,
    DoorType_Locked2Vertical,
    DoorType_ExitHorizontal,
    DoorType_ExitVertical,
    DoorType_SecretPushWall
};

enum DoorState {
    DoorState_Idle = 0,
    DoorState_Opening,
    DoorState_Closing,
    DoorState_PushNorth,
    DoorState_PushEast,
    DoorState_PushSouth,
    DoorState_PushWest,
    DoorState_Pushed,
    DoorState_FirstPushWallState = DoorState_PushNorth
};

enum Direction {
    Direction_None = -1,
    Direction_North,
    Direction_East,
    Direction_South,
    Direction_West,
};

#define DOOR_MAX_OPEN 63

class Door {
public:
    void update();

    uint8_t type;
    int8_t x, z;
    uint8_t open;
    uint8_t state;
    uint8_t texture;
};

class Item {
public:
    uint8_t type;
    int8_t x, z;
};

class Map {
public:
    void initStreaming();
    void init();
    bool isValid(int8_t cellX, int8_t cellZ);
    bool isBlocked(int8_t cellX, int8_t cellZ);
    bool isSolid(int8_t cellX, int8_t cellZ);
    bool isDoor(int8_t cellX, int8_t cellZ);
    uint8_t getTextureId(int8_t cellX, int8_t cellZ);
    uint8_t getTile(int8_t cellX, int8_t cellZ);
    uint8_t getTileFast(int8_t cellX, int8_t cellZ) {
        cellX = WRAP_TILE_BUFFER_SIZE(cellX);
        cellZ = WRAP_TILE_BUFFER_SIZE(cellZ);
        return m_mapBuffer[cellZ * MAP_BUFFER_SIZE + cellX];
    }

    Door* getDoor(int8_t cellX, int8_t cellZ);

    int8_t bufferX;
    int8_t bufferZ;
    Door doors[MAX_DOORS];
    Item items[MAX_ACTIVE_ITEMS];

    int8_t currentLevel;

    uint8_t treasureCount;
    uint8_t secretCount;
    uint8_t enemyCount;

    uint8_t m_mapBuffer[MAP_BUFFER_SIZE * MAP_BUFFER_SIZE];

    void updateBufferPosition(int8_t newX, int8_t newZ);

    void update();
    void openDoorsAt(int8_t x, int8_t z, int8_t direction, bool isPlayer = true);
    bool dropItem(uint8_t type, int8_t x, int8_t z);
    void markItemCollectedAt(int8_t x, int8_t z);

    bool isItemCollected(uint8_t spawnId) {
        uint8_t index = spawnId / 8;
        uint8_t mask = 1 << (spawnId - (index * 8));
        return (m_itemState[index] & mask) != 0;
    }
    void markItemCollected(uint8_t spawnId) {
        uint8_t index = spawnId / 8;
        uint8_t mask = 1 << (spawnId - (index * 8));
        m_itemState[index] |= mask;
    }
    bool isActorKilled(uint8_t spawnId) {
        uint8_t index = spawnId / 8;
        uint8_t mask = 1 << (spawnId - (index * 8));
        return (m_actorState[index] & mask) != 0;
    }
    void markActorKilled(uint8_t spawnId) {
        uint8_t index = spawnId / 8;
        uint8_t mask = 1 << (spawnId - (index * 8));
        m_actorState[index] |= mask;
    }

    bool isClearLine(int16_t x1, int16_t z1, int16_t x2, int16_t z2);
    uint8_t getDoorTexture(uint8_t tile);
    void updateEntireBuffer();

private:
    void streamData(uint8_t* buffer, uint8_t orientation, int8_t x, int8_t z, int8_t length);
    void updateHorizontalSlice(int8_t offsetZ);
    void updateVerticalSlice(int8_t offsetX);
    void updateDoors();
    uint8_t streamIn(uint8_t tile, uint8_t metadata, int8_t x, int8_t z);
    Door* streamInDoor(uint8_t type, uint8_t metadata, int8_t x, int8_t z);

    uint8_t m_itemState[256 / 8];
    uint8_t m_actorState[256 / 8];

#ifdef STANDARD_FILE_STREAMING
    FILE* m_mapStream;
#endif
#ifdef PETIT_FATFS_FILE_STREAMING
    FATFS m_fileSystem;
#endif
    bool m_mapLoaded;
};

#endif
