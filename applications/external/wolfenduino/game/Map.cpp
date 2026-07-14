#include "game/Engine.h"
#include "game/Map.h"
#include "game/TileTypes.h"
#include "game/Sounds.h"

#include <stdio.h>

#ifdef PROGMEM_MAP_STREAMING
#include "game/Data_Maps.h"
#endif
#ifdef PETIT_FATFS_FILE_STREAMING
#include <petit_fatfs.h>
#endif

const int8_t PushWallDirections[] PROGMEM = {
    0,
    -1, // North
    1,
    0, // East
    0,
    1, // South
    -1,
    0 // West
};

bool Map::isValid(int8_t x, int8_t z) {
    if(x < bufferX || z < bufferZ || x >= bufferX + MAP_BUFFER_SIZE ||
       z >= bufferZ + MAP_BUFFER_SIZE) {
        return false;
    }

    return true;
    /*  if (cellX < 0)
    return false;
  if (cellX >= MAP_SIZE)
    return false;
  if (cellZ < 0)
    return false;
  if (cellZ >= MAP_SIZE)
    return false;
  return true;*/
}

void Map::init() {
    while(!m_mapLoaded)
#ifdef FXDATA_STREAMING
    {
        m_mapLoaded = true;
    }
#endif
#ifdef STANDARD_FILE_STREAMING
    {
        fopen_s(&m_mapStream, "wolf3d.dat", "rb");
        m_mapLoaded = true;
    }
#endif
#ifdef PETIT_FATFS_FILE_STREAMING
    {
        if(pf_mount(&m_fileSystem) == FR_OK) {
            if(pf_open("WOLF3D.DAT") == FR_OK) {
                m_mapLoaded = true;
            } else
                PLATFORM_ERROR(PSTR("NO WOLF3D.DAT FOUND!"));
        } else
            PLATFORM_ERROR(PSTR("SD CARD MOUNT PLATFORM_ERROR"));
    }
#endif

    for(int n = 0; n < 256 / 8; n++) {
        m_itemState[n] = 0;
        m_actorState[n] = 0;
    }

    for(int n = 0; n < MAX_DOORS; n++) {
        doors[n].type = DoorType_None;
    }

    for(int n = 0; n < MAX_ACTIVE_ITEMS; n++) {
        items[n].type = 0;
    }

    enemyCount = 0;
    treasureCount = 0;
    secretCount = 0;
}

void Map::initStreaming() {
    m_mapLoaded = false;
    bufferX = 0;
    bufferZ = 0;
}

void Map::update() {
    updateDoors();
}

bool Map::isDoor(int8_t cellX, int8_t cellZ) {
    uint8_t tile = getTile(cellX, cellZ);
    return tile >= Tile_FirstDoor && tile <= Tile_LastDoor;
}

Door* Map::getDoor(int8_t cellX, int8_t cellZ) {
    for(int n = 0; n < MAX_DOORS; n++) {
        if(doors[n].type != DoorType_None && doors[n].x == cellX && doors[n].z == cellZ) {
            return &doors[n];
        }
    }

    return nullptr;
}

bool Map::isBlocked(int8_t cellX, int8_t cellZ) {
    uint8_t tile = getTile(cellX, cellZ);

    // Check if this is a wall
    if((tile >= Tile_FirstWall && tile <= Tile_LastWall)) return true;

    // Check if this is a blocking decoration
    if((tile >= Tile_FirstBlockingDecoration && tile <= Tile_LastBlockingDecoration)) return true;

    // Check if this is a door
    Door* door = getDoor(cellX, cellZ);
    if(door) {
        // Check if the door is closed
        return (door->type == DoorType_SecretPushWall || door->open < 16);
    }

    if((tile >= Tile_FirstDoor && tile <= Tile_LastDoor)) {
        // This door hasn't been streamed in yet
        return true;
    }

    return false;
}

bool Map::isSolid(int8_t cellX, int8_t cellZ) {
    uint8_t tile = getTile(cellX, cellZ);
    return tile >= Tile_FirstWall && tile <= Tile_LastWall && tile != MAP_OUT_OF_BOUNDS;
}

uint8_t Map::getTextureId(int8_t cellX, int8_t cellZ) {
    uint8_t tile = getTile(cellX, cellZ);
    if(tile == MAP_OUT_OF_BOUNDS) return 0;
    return tile - 1;
}

uint8_t Map::getTile(int8_t x, int8_t z) {
    if(x < bufferX || z < bufferZ || x >= bufferX + MAP_BUFFER_SIZE ||
       z >= bufferZ + MAP_BUFFER_SIZE) {
        return MAP_OUT_OF_BOUNDS;
    }

    return getTileFast(x, z);
}

void Map::streamData(uint8_t* buffer, uint8_t orientation, int8_t x, int8_t z, int8_t length) {
#ifdef FXDATA_STREAMING
    int32_t offset = orientation == MapRead_Horizontal ?
                         (z * MAP_SIZE + x) * 2 :
                         (MAP_SIZE * MAP_SIZE * 2) + (x * MAP_SIZE + z) * 2;
    offset += (int32_t)(currentLevel)*MAP_SIZE * MAP_SIZE * 4;
    diskRead(offset, buffer, length * 2);
    return;
#endif

#ifdef STANDARD_FILE_STREAMING
    if(m_mapStream) {
        int32_t offset = orientation == MapRead_Horizontal ?
                             (z * MAP_SIZE + x) * 2 :
                             (MAP_SIZE * MAP_SIZE * 2) + (x * MAP_SIZE + z) * 2;
        offset += currentLevel * MAP_SIZE * MAP_SIZE * 4;
        fseek(m_mapStream, offset, SEEK_SET);
        fread(buffer, 1, length * 2, m_mapStream);
        return;
    }
#endif
#ifdef PETIT_FATFS_FILE_STREAMING
    if(m_mapLoaded) {
        int16_t _x = x;
        int16_t _z = z;
        int32_t offset = orientation == MapRead_Horizontal ?
                             (_z * MAP_SIZE + _x) * 2 :
                             (MAP_SIZE * MAP_SIZE * 2) + (_x * MAP_SIZE + _z) * 2;
        WORD bytesRead;
        int errorCount = 0;

        do {
            if(pf_lseek(offset) == FR_OK) {
                if(pf_read(buffer, length * 2, &bytesRead) != FR_OK) {
                    bytesRead = 0;
                }
            }
            errorCount++;
            if(errorCount > 3) {
                PLATFORM_ERROR(PSTR("PLATFORM_ERROR READING SD CARD"));
            }
        } while(bytesRead < length * 2);
    }
#endif
    //printf("Streaming %s, %d, %d\n", orientation == MapRead_Horizontal ? "Horizontal" : "Vertical", x, z, length);
    // TODO: make this stream from SD card or decompress from huffman stream in progmem

#ifdef PROGMEM_MAP_STREAMING
    if(orientation == MapRead_Horizontal) {
        for(int8_t n = 0; n < length; n++) {
            buffer[n * 2] = pgm_read_byte(&mapData[z * MAP_SIZE + x + n]);
            buffer[n * 2 + 1] = 0xff;
        }
    } else {
        for(int8_t n = 0; n < length; n++) {
            buffer[n * 2] = pgm_read_byte(&mapData[(z + n) * MAP_SIZE + x]);
            buffer[n * 2 + 1] = 0xff;
        }
    }
#endif
}

uint8_t Map::streamIn(uint8_t tile, uint8_t metadata, int8_t x, int8_t z) {
    if(engine.gameState == GameState_Loading) {
        if(tile == Tile_SecretPushWall) {
            streamInDoor(DoorType_SecretPushWall, metadata - Tile_FirstWall, x, z);
            secretCount++;
        } else if(tile >= Tile_FirstTreasure && tile <= Tile_LastTreasure) {
            treasureCount++;
        } else if(tile >= Tile_FirstActor && tile <= Tile_LastActor) {
            switch(tile) {
            case Tile_Actor_Guard_Hard:
                if(engine.difficulty < Difficulty_Hard) break;
                __attribute__((fallthrough));
            case Tile_Actor_Guard_Medium:
                if(engine.difficulty < Difficulty_Medium) break;
                __attribute__((fallthrough));
            case Tile_Actor_Guard_Easy:
                enemyCount++;
                break;
            case Tile_Actor_SS_Hard:
                if(engine.difficulty < Difficulty_Hard) break;
                __attribute__((fallthrough));
            case Tile_Actor_SS_Medium:
                if(engine.difficulty < Difficulty_Medium) break;
                __attribute__((fallthrough));
            case Tile_Actor_SS_Easy:
                enemyCount++;
                break;
            case Tile_Actor_Dog_Hard:
                if(engine.difficulty < Difficulty_Hard) break;
                __attribute__((fallthrough));
            case Tile_Actor_Dog_Medium:
                if(engine.difficulty < Difficulty_Medium) break;
                __attribute__((fallthrough));
            case Tile_Actor_Dog_Easy:
                enemyCount++;
                break;
            case Tile_Actor_Boss:
                enemyCount++;
                break;
            }
        }
    }

    if(tile >= Tile_FirstItem && tile <= Tile_LastItem) {
        if(isItemCollected(metadata)) {
            return Tile_Empty;
        }
    } else if(tile >= Tile_FirstActor && tile <= Tile_LastActor) {
        if(engine.gameState != GameState_Loading && !isActorKilled(metadata)) {
            switch(tile) {
            case Tile_Actor_Guard_Hard:
                if(engine.difficulty < Difficulty_Hard) return Tile_Empty;
                __attribute__((fallthrough));
            case Tile_Actor_Guard_Medium:
                if(engine.difficulty < Difficulty_Medium) return Tile_Empty;
                __attribute__((fallthrough));
            case Tile_Actor_Guard_Easy:
                engine.spawnActor(metadata, ActorType_Guard, x, z);
                break;
            case Tile_Actor_SS_Hard:
                if(engine.difficulty < Difficulty_Hard) return Tile_Empty;
                __attribute__((fallthrough));
            case Tile_Actor_SS_Medium:
                if(engine.difficulty < Difficulty_Medium) return Tile_Empty;
                __attribute__((fallthrough));
            case Tile_Actor_SS_Easy:
                engine.spawnActor(metadata, ActorType_SS, x, z);
                break;
            case Tile_Actor_Dog_Hard:
                if(engine.difficulty < Difficulty_Hard) return Tile_Empty;
                __attribute__((fallthrough));
            case Tile_Actor_Dog_Medium:
                if(engine.difficulty < Difficulty_Medium) return Tile_Empty;
                __attribute__((fallthrough));
            case Tile_Actor_Dog_Easy:
                engine.spawnActor(metadata, ActorType_Dog, x, z);
                break;
            case Tile_Actor_Boss:
                engine.spawnActor(metadata, ActorType_Boss, x, z);
                break;
            }
        }
        return Tile_Empty;
    } else if(tile == Tile_SecretPushWall) {
        return Tile_Empty;
    }

    return tile;
}

void Map::updateHorizontalSlice(int8_t offsetZ) {
    streamData(
        engine.streamBuffer, MapRead_Horizontal, bufferX, bufferZ + offsetZ, MAP_BUFFER_SIZE);

    int8_t targetZ = WRAP_TILE_BUFFER_SIZE(bufferZ + offsetZ);

    for(int8_t x = 0; x < MAP_BUFFER_SIZE; x++) {
        int8_t targetX = WRAP_TILE_BUFFER_SIZE(bufferX + x);
        uint8_t read = streamIn(
            engine.streamBuffer[x * 2],
            engine.streamBuffer[x * 2 + 1],
            bufferX + x,
            bufferZ + offsetZ);

        m_mapBuffer[targetZ * MAP_BUFFER_SIZE + targetX] = read;
    }
}

void Map::updateVerticalSlice(int8_t offsetX) {
    streamData(engine.streamBuffer, MapRead_Vertical, bufferX + offsetX, bufferZ, MAP_BUFFER_SIZE);

    int8_t targetX = WRAP_TILE_BUFFER_SIZE(bufferX + offsetX);

    for(int8_t z = 0; z < MAP_BUFFER_SIZE; z++) {
        int8_t targetZ = WRAP_TILE_BUFFER_SIZE(bufferZ + z);
        uint8_t read = streamIn(
            engine.streamBuffer[z * 2],
            engine.streamBuffer[z * 2 + 1],
            bufferX + offsetX,
            bufferZ + z);

        m_mapBuffer[targetZ * MAP_BUFFER_SIZE + targetX] = read;
    }
}

void Map::updateEntireBuffer() {
    for(int8_t n = 0; n < MAX_ACTIVE_ACTORS; n++) {
        if(engine.actors[n].type != ActorType_Empty) {
            engine.actors[n].updateFrozenState();
        }
    }

    for(int8_t n = 0; n < MAP_BUFFER_SIZE; n++) {
        updateHorizontalSlice(n);
    }
}

void Map::updateBufferPosition(int8_t newX, int8_t newZ) {
    if(newX < 0) newX = 0;
    if(newZ < 0) newZ = 0;
    if(newX > MAP_SIZE - MAP_BUFFER_SIZE) newX = MAP_SIZE - MAP_BUFFER_SIZE;
    if(newZ > MAP_SIZE - MAP_BUFFER_SIZE) newZ = MAP_SIZE - MAP_BUFFER_SIZE;

    if(engine.gameState == GameState_Loading || newX <= bufferX - MAP_BUFFER_SIZE ||
       newX >= bufferX + MAP_BUFFER_SIZE || newZ <= bufferZ - MAP_BUFFER_SIZE ||
       newZ >= bufferZ + MAP_BUFFER_SIZE) {
        bufferX = newX;
        bufferZ = newZ;
        WARNING("Updating entire buffer at %d %d\n", bufferX, bufferZ);
        updateEntireBuffer();
        return;
    }

    if(bufferX == newX && bufferZ == newZ) return;

    while(bufferX < newX) {
        bufferX++;
        updateVerticalSlice(MAP_BUFFER_SIZE - 1);
    }
    while(bufferX > newX) {
        bufferX--;
        updateVerticalSlice(0);
    }
    while(bufferZ < newZ) {
        bufferZ++;
        updateHorizontalSlice(MAP_BUFFER_SIZE - 1);
    }
    while(bufferZ > newZ) {
        bufferZ--;
        updateHorizontalSlice(0);
    }

    WARNING(
        "Updating buffer at %d %d\nPlayer position is at: %d %d\n",
        bufferX,
        bufferZ,
        engine.player.x / CELL_SIZE,
        engine.player.z / CELL_SIZE);
}

void Map::updateDoors() {
    for(int8_t n = 0; n < MAX_DOORS; n++) {
        if(doors[n].type != DoorType_None) {
            doors[n].update();
        }
    }
}

Door* Map::streamInDoor(uint8_t type, uint8_t metadata, int8_t x, int8_t z) {
    int8_t freeIndex = -1;

    // if(type == DoorType_SecretPushWall)
    // 	WARNING("Creating secret push wall at %d %d!\n", x, z);

    for(int8_t n = 0; n < MAX_DOORS; n++) {
        if(freeIndex == -1 && doors[n].type == DoorType_None) {
            freeIndex = n;
        }
        if(doors[n].type != DoorType_None && doors[n].x == x && doors[n].z == z) {
            // Already streamed in
            return &doors[n];
        }
    }

    if(freeIndex == -1) {
        for(int8_t n = 0; n < MAX_DOORS; n++) {
            if(doors[n].type != DoorType_SecretPushWall &&
               (!isValid(doors[n].x, doors[n].z) || !doors[n].open)) {
                freeIndex = n;
                break;
            }
        }
    }

    if(freeIndex == -1) {
        for(int8_t n = 0; n < MAX_DOORS; n++) {
            if(doors[n].type != DoorType_SecretPushWall &&
               engine.renderer.isFrustrumClipped(doors[n].x, doors[n].z)) {
                freeIndex = n;
                break;
            }
        }
    }

    if(freeIndex == -1) {
        WARNING("No room to spawn door!\n");
        return nullptr;
    }

    doors[freeIndex].x = x;
    doors[freeIndex].z = z;
    doors[freeIndex].open = 0;
    doors[freeIndex].state = DoorState_Idle;
    doors[freeIndex].type = type;
    doors[freeIndex].texture = metadata;

    return &doors[freeIndex];
}

void Map::openDoorsAt(int8_t x, int8_t z, int8_t direction, bool isPlayer) {
    uint8_t tileType = getTile(x, z);

    if(tileType == Tile_ExitSwitchWall) {
        if(direction != Direction_None) {
            Platform.playSound(LEVELDONESND);
            engine.finishLevel();
        }
        return;
    }

    Door* door = getDoor(x, z);

    if(!door && (tileType >= Tile_FirstDoor && tileType <= Tile_LastDoor)) {
        // Try stream in door
        door = streamInDoor(tileType - Tile_FirstDoor + 1, getDoorTexture(tileType), x, z);
    }

    if(door) {
        if(door->type == DoorType_SecretPushWall) {
            if(direction != Direction_None && door->state == DoorState_Idle) {
                int8_t offX = pgm_read_byte(&PushWallDirections[(direction) * 2]);
                int8_t offZ = pgm_read_byte(&PushWallDirections[(direction) * 2 + 1]);

                if(engine.map.isValid(x + offX, z + offZ) &&
                   !engine.map.isSolid(x + offX, z + offZ)) {
                    door->state = DoorState_FirstPushWallState + direction;
                    Platform.playSound(PUSHWALLSND);
                }
            }
        } else {
            if((door->type == DoorType_Locked1Horizontal ||
                door->type == DoorType_Locked1Vertical) &&
               !engine.player.inventory.hasKey1) {
                if(isPlayer) {
                    if(!engine.player.blinkKeyTimer) {
                        Platform.playSound(NOWAYSND);
                    }
                    engine.player.blinkKeyTimer = TARGET_FRAMERATE;
                }
                return;
            }

            if(door->state != DoorState_Opening && door->open == 0) {
                Platform.playSound(OPENDOORSND);
            }
            door->state = DoorState_Opening;
        }
    }
}

void Door::update() {
    switch(state) {
    case DoorState_PushNorth:
    case DoorState_PushEast:
    case DoorState_PushSouth:
    case DoorState_PushWest:
        open++;
        if(open == CELL_SIZE) {
            open = 0;
            int8_t offX =
                pgm_read_byte(&PushWallDirections[(state - DoorState_FirstPushWallState) * 2]);
            int8_t offZ =
                pgm_read_byte(&PushWallDirections[(state - DoorState_FirstPushWallState) * 2 + 1]);

            x += offX;
            z += offZ;
            uint8_t tile = engine.map.getTile(x + offX, z + offZ);
            if(tile == MAP_OUT_OF_BOUNDS ||
               (tile >= Tile_FirstBlockingDecoration && tile <= Tile_LastBlockingDecoration) ||
               (tile >= Tile_FirstWall && tile <= Tile_LastWall)) {
                state = DoorState_Pushed;
                engine.player.secretsFound++;
            }
        }
        break;
    case DoorState_Opening:
        if(open < DOOR_MAX_OPEN) {
            open++;
        } else
            state = DoorState_Closing;
        break;
    case DoorState_Closing:
        if(open > 0) {
            open--;
            if(open == 16) {
                Platform.playSound(CLOSEDOORSND);
            }
        } else
            state = DoorState_Idle;
        break;
    }
}

bool Map::isClearLine(int16_t x1, int16_t z1, int16_t x2, int16_t z2)
/*{
	int cellX = x1 / CELL_SIZE;
	int cellZ = z1 / CELL_SIZE;
	int targetCellX = x2 / CELL_SIZE;
	int targetCellZ = z2 / CELL_SIZE;
	int16_t x = x1;
	int16_t z = z1;
	int16_t dx = x2 - x1;
	int16_t dz = z2 - z1;
	int16_t tx, tz;

	while(1)
	{
		if(cellX == targetCellX && cellZ == targetCellZ)
			return true;

		if(dx < 0)
		{
			tx = (x - cellX * CELL_SIZE) / -dx;
		}
		else if(dx > 0)
		{
			tx = ((cellX + 1) * CELL_SIZE - x) / dx;
		}
		else tx = 0;

		if(dz < 0)
		{
			tz = (z - cellZ * CELL_SIZE) / -dz;
		}
		else if(dz > 0)
		{
			tz = ((cellZ + 1) * CELL_SIZE - z) / dz;
		}
		else tz = 0;

	}

	return true;
}
*/
{
    /*    int         x1,y1,xt1,yt1,x2,y2,xt2,yt2;
    int         x,y;
    int         xdist,ydist,xstep,ystep;
    int         partial,delta;
    int32_t     ltemp;
    int         xfrac,yfrac,deltafrac;
    unsigned    value,intercept;
	*/
    int cellX1 = WORLD_TO_CELL(x1);
    int cellX2 = WORLD_TO_CELL(x2);
    int cellZ1 = WORLD_TO_CELL(z1);
    int cellZ2 = WORLD_TO_CELL(z2);

    int xdist = mabs(cellX2 - cellX1);

    int partial, delta;
    int deltafrac;
    int xfrac, zfrac;
    int xstep, zstep;
    int32_t ltemp;
    int x, z;

    if(xdist > 0) {
        if(cellX2 > cellX1) {
            partial = (CELL_TO_WORLD(cellX1 + 1) - x1);
            xstep = 1;
        } else {
            partial = (x1 - CELL_TO_WORLD(cellX1));
            xstep = -1;
        }

        deltafrac = mabs(x2 - x1);
        delta = z2 - z1;
        ltemp = ((int32_t)delta * CELL_SIZE) / deltafrac;
        if(ltemp > 0x7fffl)
            zstep = 0x7fff;
        else if(ltemp < -0x7fffl)
            zstep = -0x7fff;
        else
            zstep = ltemp;
        zfrac = z1 + (((int32_t)zstep * partial) / CELL_SIZE);

        x = cellX1 + xstep;
        cellX2 += xstep;
        do {
            z = WORLD_TO_CELL(zfrac);
            zfrac += zstep;

            uint8_t tile = getTile(x, z);

            if(tile) {
                if(tile >= Tile_FirstWall && tile <= Tile_LastWall) return false;

                if(tile >= Tile_FirstDoor && tile <= Tile_LastDoor) {
                    Door* door = getDoor(x, z);
                    if(!door || !door->open) return false;
                }
            }

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

    int zdist = mabs(cellZ2 - cellZ1);

    if(zdist > 0) {
        if(cellZ2 > cellZ1) {
            partial = (CELL_TO_WORLD(cellZ1 + 1) - z1);
            zstep = 1;
        } else {
            partial = (z1 - CELL_TO_WORLD(cellZ1));
            zstep = -1;
        }

        deltafrac = mabs(z2 - z1);
        delta = x2 - x1;
        ltemp = ((int32_t)delta * CELL_SIZE) / deltafrac;
        if(ltemp > 0x7fffl)
            xstep = 0x7fff;
        else if(ltemp < -0x7fffl)
            xstep = -0x7fff;
        else
            xstep = ltemp;
        xfrac = x1 + (((int32_t)xstep * partial) / CELL_SIZE);

        z = cellZ1 + zstep;
        cellZ2 += zstep;
        do {
            x = WORLD_TO_CELL(xfrac);
            xfrac += xstep;

            uint8_t tile = getTile(x, z);

            if(tile) {
                if(tile >= Tile_FirstWall && tile <= Tile_LastWall) return false;

                if(tile >= Tile_FirstDoor && tile <= Tile_LastDoor) {
                    Door* door = getDoor(x, z);
                    if(!door || !door->open) return false;
                }
            }

            z += zstep;

            //
            // see if the door is open enough
            //
            /*value &= ~0x80;
            intercept = xfrac-xstep/2;

            if (intercept>doorposition[value])
                return false;*/
        } while(z != cellZ2);
    }

    return true;
}

bool Map::dropItem(uint8_t type, int8_t x, int8_t z) {
    int8_t slot = -1;

    for(int8_t n = 0; n < MAX_ACTIVE_ITEMS; n++) {
        if(items[n].type == 0) {
            slot = n;
        }
    }

    if(slot == -1) {
        for(int8_t n = 0; n < MAX_ACTIVE_ITEMS; n++) {
            if(!isValid(items[n].x, items[n].z) && items[n].type != Tile_Item_Key1 &&
               items[n].type != Tile_Item_Key2) {
                slot = n;
                break;
            }
        }
    }

    if(slot == -1 && (type == Tile_Item_Key1 || type == Tile_Item_Key2)) {
        for(int8_t n = 0; n < MAX_ACTIVE_ITEMS; n++) {
            if(engine.renderer.isFrustrumClipped(items[n].x, items[n].z) &&
               items[n].type != Tile_Item_Key1 && items[n].type != Tile_Item_Key2) {
                slot = n;
                break;
            }
        }

        if(slot == -1) {
            for(int8_t n = 0; n < MAX_ACTIVE_ITEMS; n++) {
                if(items[n].type != Tile_Item_Key1 && items[n].type != Tile_Item_Key2) {
                    slot = n;
                    break;
                }
            }
        }
    }

    if(slot == -1) {
        WARNING("No room to spawn item!\n");
        return false;
    }

    items[slot].type = type;
    items[slot].x = x;
    items[slot].z = z;

    return true;
}

void Map::markItemCollectedAt(int8_t x, int8_t z) {
    if(!isValid(x, z)) {
        return;
    }

    streamData(engine.streamBuffer, MapRead_Horizontal, x, z, 1);
    uint8_t spawnId = engine.streamBuffer[1];
    markItemCollected(spawnId);

    x = WRAP_TILE_BUFFER_SIZE(x);
    z = WRAP_TILE_BUFFER_SIZE(z);
    m_mapBuffer[z * MAP_BUFFER_SIZE + x] = Tile_Empty;
}

uint8_t Map::getDoorTexture(uint8_t tile) {
    switch(tile) {
    case Tile_Door_Locked1_Horizontal:
    case Tile_Door_Locked1_Vertical:
        return DOOR_LOCKED1_TEXTURE;
    case Tile_Door_Locked2_Horizontal:
    case Tile_Door_Locked2_Vertical:
        return DOOR_LOCKED2_TEXTURE;
    case Tile_Door_Elevator_Horizontal:
    case Tile_Door_Elevator_Vertical:
        return DOOR_ELEVATOR_TEXTURE;

    default:
        return DOOR_GENERIC_TEXTURE;
    }
}
