#include "game/Engine.h"
#include "game/Actor.h"
#include "game/Sounds.h"

#include "game/Generated/Data_Guard.h"
#include "game/Generated/Data_SS.h"
#include "game/Generated/Data_Dog.h"
#include "game/Generated/Data_Boss.h"
#include "game/Generated/fxdata.h"
#include "game/TileTypes.h"

void Actor::init(uint8_t id, uint8_t actorType, uint8_t cellX, uint8_t cellZ) {
    spawnId = id;
    type = actorType;
    state = ActorState_Idle;
    x = CELL_TO_WORLD(cellX) + CELL_SIZE / 2;
    z = CELL_TO_WORLD(cellZ) + CELL_SIZE / 2;
    targetCellX = cellX;
    targetCellZ = cellZ;
    flags.frozen = false;
    frame = 1;

    switch(actorType) {
    case ActorType_Dog:
        hp = 1;
        break;
    case ActorType_Guard:
        hp = 25;
        break;
    case ActorType_SS:
        hp = 100;
        break;
    case ActorType_Boss:
        switch(engine.difficulty) {
        case Difficulty_Baby:
            hp = 850;
            break;
        case Difficulty_Easy:
            hp = 950;
            break;
        case Difficulty_Medium:
            hp = 1050;
            break;
        case Difficulty_Hard:
            hp = 1200;
            break;
        }
        break;
    }
}

void Actor::update() {
    if(type == ActorType_Empty) return;

    updateFrozenState();

    if(flags.frozen) return;

    bool updateFrame = (engine.frameCount & 0x3) == 0;

    switch(state) {
    case ActorState_Idle: {
        bool wakeFrame = (engine.frameCount & 15) == 0;
        if(wakeFrame && engine.map.isClearLine(x, z, engine.player.x, engine.player.z)) {
            switchState(ActorState_Waking);

            switch(type) {
            case ActorType_Dog:
                Platform.playSound(DOGBARKSND);
                break;
            case ActorType_Guard:
                Platform.playSound(HALTSND);
                break;
            case ActorType_SS:
                Platform.playSound(SCHUTZADSND);
                break;
            case ActorType_Boss:
                Platform.playSound(BOSSACTIVESND);
                break;
            }
        }
    } break;
    case ActorState_Waking: {
        if(updateFrame) {
            switchState(ActorState_Active);
        }
    } break;
    case ActorState_Active: {
        bool movementSucceeded = tryMove();
        if(shouldShootPlayer(movementSucceeded)) {
            switchState(ActorState_Aiming);
        }
    } break;
    case ActorState_Aiming:
        if(updateFrame) {
            switchState(ActorState_Shooting);
        }
        break;
    case ActorState_Shooting:
        if(updateFrame) {
            switchState(ActorState_Recoiling);
            shootPlayer();
        }
        break;
    case ActorState_Recoiling:
        if(updateFrame) {
            switch(type) {
            case ActorType_SS:
                if((getRandomNumber() & 0x3) &&
                   engine.map.isClearLine(x, z, engine.player.x, engine.player.z)) {
                    switchState(ActorState_Shooting);
                } else {
                    switchState(ActorState_Active);
                }
                break;
            case ActorType_Boss:
                if(engine.map.isClearLine(x, z, engine.player.x, engine.player.z)) {
                    switchState(ActorState_Shooting);
                } else {
                    switchState(ActorState_Active);
                }
                break;
            default:
                switchState(ActorState_Active);
                break;
            }
        }
        break;
    case ActorState_Injured:
        if(updateFrame) {
            switchState(ActorState_Active);
        }
        break;
    case ActorState_Dying:
        if(updateFrame) {
            frame++;
            if(frame == 9) switchState(ActorState_Dead);
        }
        break;
    case ActorState_Dead:
        break;
    default:
        frame = 0;
        break;
    }

    engine.map.openDoorsAt(x / CELL_SIZE, z / CELL_SIZE, Direction_None);
    //	if((engine.frameCount & 0x3) == 0)
    //	frame = (frame + 1) % 4;
}

void Actor::draw() {
    switch(type) {
    case ActorType_Guard:
        engine.renderer.queueSprite(
            (SpriteFrame*)&Data_guardSprite_frames[frame], Data_guardSprite, x, z);
        break;
    case ActorType_Dog:
        engine.renderer.queueSprite(
            (SpriteFrame*)&Data_dogSprite_frames[frame], Data_dogSprite, x, z);
        break;
    case ActorType_SS:
        engine.renderer.queueSprite(
            (SpriteFrame*)&Data_ssSprite_frames[frame], Data_ssSprite, x, z);
        break;
    case ActorType_Boss:
        engine.renderer.queueSprite(
            (SpriteFrame*)&Data_bossSprite_frames[frame], Data_bossSprite, x, z);
        break;
    }
}

void Actor::updateFrozenState() {
    int cellX = WORLD_TO_CELL(x);
    int cellZ = WORLD_TO_CELL(z);

    flags.frozen = cellX < engine.map.bufferX || cellZ < engine.map.bufferZ ||
                   cellX >= engine.map.bufferX + MAP_BUFFER_SIZE ||
                   cellZ >= engine.map.bufferZ + MAP_BUFFER_SIZE;
}

void Actor::damage(int amount) {
    if(hp == 0) return;

    if(amount > hp) {
        hp = 0;
    } else
        hp -= amount;

    if(hp == 0) {
        switchState(ActorState_Dying);
        engine.map.markActorKilled(spawnId);
        engine.player.enemiesKilled++;

        switch(type) {
        case ActorType_Guard:
            dropItem(Tile_Item_Clip);
            if(!(getRandomNumber() & 1)) {
                Platform.playSound(DEATHSCREAM2SND);
            } else {
                Platform.playSound(DEATHSCREAM3SND);
            }
            engine.player.givePoints(100);
            break;
        case ActorType_SS:
            dropItem(Tile_Item_MachineGun);
            Platform.playSound(LEBENSND);
            engine.player.givePoints(500);
            break;
        case ActorType_Boss:
            dropItem(Tile_Item_Key1);
            Platform.playSound(MUTTISND);
            engine.player.givePoints(5000);
            break;
        case ActorType_Dog:
            Platform.playSound(DOGDEATHSND);
            engine.player.givePoints(200);
            break;
        }
    } else {
        if(type != ActorType_Dog && type != ActorType_Boss) {
            switchState(ActorState_Injured);
        }
    }
}

void Actor::switchState(uint8_t newState) {
    state = newState;

    switch(newState) {
    case ActorState_Injured:
        frame = 5;
        break;
    case ActorState_Dying:
        frame = 5;
        break;
    case ActorState_Dead:
        frame = 9;
        break;
    case ActorState_Aiming:
    case ActorState_Recoiling:
    case ActorState_Shooting:
        frame = 3;
        break;
    default:
        break;
    }
}

void Actor::dropItem(uint8_t itemType) {
    int cellX = WORLD_TO_CELL(x);
    int cellZ = WORLD_TO_CELL(z);

    if(tryDropItem(itemType, cellX, cellZ)) return;

    for(int i = cellX - 1; i < cellX + 1; i++) {
        for(int j = cellZ - 1; j < cellZ + 1; j++) {
            if(tryDropItem(itemType, i, j)) return;
        }
    }
}

bool Actor::tryDropItem(uint8_t itemType, int cellX, int cellZ) {
    uint8_t tile = engine.map.getTile(cellX, cellZ);
    if(tile == 0) {
        engine.map.dropItem(itemType, cellX, cellZ);
        return true;
    }
    return false;
}

bool Actor::isPlayerColliding() {
    if(x >= engine.player.x - MIN_ACTOR_DISTANCE && x <= engine.player.x + MIN_ACTOR_DISTANCE &&
       z >= engine.player.z - MIN_ACTOR_DISTANCE && z <= engine.player.z + MIN_ACTOR_DISTANCE) {
        return true;
    }
    return false;
}

bool Actor::tryMove() {
    int movement = type == ActorType_Dog ? 2 : 1;

    frame = 1;

    if(engine.map.isBlocked(targetCellX, targetCellZ)) {
        if(type != ActorType_Dog) {
            engine.map.openDoorsAt(targetCellX, targetCellZ, Direction_None);
        }
        return false;
    }

    int16_t targetX = CELL_TO_WORLD(targetCellX) + CELL_SIZE / 2;
    int16_t targetZ = CELL_TO_WORLD(targetCellZ) + CELL_SIZE / 2;

    int8_t deltaX = clamp(targetX - x, -movement, movement);
    int8_t deltaZ = clamp(targetZ - z, -movement, movement);

    x += deltaX;
    z += deltaZ;

    if(isPlayerColliding()) {
        x -= deltaX;
        z -= deltaZ;
        return false;
    }

    // Select animation frame
    frame = (engine.frameCount >> 2) & 0x3;
    if(frame == 3) frame = 1;

    if(type != ActorType_Boss) {
        // Do we need to make this a side angle view?
        int16_t cameraRelativeDirection =
            engine.renderer.view.rotSin * deltaX + engine.renderer.view.rotCos * deltaZ;

        if(cameraRelativeDirection < -48 * movement) {
            frame += 10;
        } else if(cameraRelativeDirection > 48 * movement) {
            frame += 13;
        }
    }

    if(x == targetX && z == targetZ) {
        pickNewTargetCell();
    }
    return true;
}

bool Actor::tryPickCell(int8_t newX, int8_t newZ) {
    if(engine.map.isBlocked(newX, newZ) && !engine.map.isDoor(newX, newZ)) return false;
    if(engine.map.isBlocked(targetCellX, newZ) && !engine.map.isDoor(targetCellX, newZ))
        return false;
    if(engine.map.isBlocked(newX, targetCellZ) && !engine.map.isDoor(newX, targetCellZ))
        return false;

    for(int n = 0; n < MAX_ACTIVE_ACTORS; n++) {
        if(this != &engine.actors[n] && engine.actors[n].type != ActorType_Empty &&
           engine.actors[n].hp > 0) {
            if(engine.actors[n].targetCellX == newX && engine.actors[n].targetCellZ == newZ)
                return false;
        }
    }

    targetCellX = newX;
    targetCellZ = newZ;

    return true;
}

bool Actor::tryPickCells(int8_t deltaX, int8_t deltaZ) {
    return tryPickCell(targetCellX + deltaX, targetCellZ + deltaZ) ||
           tryPickCell(targetCellX + deltaX, targetCellZ) ||
           tryPickCell(targetCellX, targetCellZ + deltaZ) ||
           tryPickCell(targetCellX - deltaX, targetCellZ + deltaZ) ||
           tryPickCell(targetCellX + deltaX, targetCellZ - deltaZ);
}

void Actor::pickNewTargetCell() {
    int8_t deltaX = clamp(WORLD_TO_CELL(engine.player.x) - targetCellX, -1, 1);
    int8_t deltaZ = clamp(WORLD_TO_CELL(engine.player.z) - targetCellZ, -1, 1);
    uint8_t dodgeChance = getRandomNumber();

    if(deltaX == 0) {
        if(dodgeChance < 64) {
            deltaX = -1;
        } else if(dodgeChance < 128) {
            deltaX = 1;
        }
    } else if(deltaZ == 0) {
        if(dodgeChance < 64) {
            deltaZ = -1;
        } else if(dodgeChance < 128) {
            deltaZ = 1;
        }
    }

    tryPickCells(deltaX, deltaZ);
}

int8_t Actor::getPlayerCellDistance() {
    int8_t dx = WORLD_TO_CELL(mabs(engine.player.x - x));
    int8_t dz = WORLD_TO_CELL(mabs(engine.player.z - z));
    return max(dx, dz);
}

bool Actor::shouldShootPlayer(bool movementSucceeded) {
    if(engine.map.isClearLine(x, z, engine.player.x, engine.player.z)) {
        int playerCellDistance = getPlayerCellDistance();
        if(type == ActorType_Dog) {
            return playerCellDistance == 1 && !movementSucceeded;
        }

        int chance = 16 / playerCellDistance;

        return getRandomNumber() < chance;
    }
    return false;
}

void Actor::shootPlayer() {
    switch(type) {
    case ActorType_Dog:
        Platform.playSound(DOGATTACKSND);
        break;
    case ActorType_Guard:
        Platform.playSound(NAZIFIRESND);
        break;
    case ActorType_SS:
        Platform.playSound(SSFIRESND);
        break;
    case ActorType_Boss:
        Platform.playSound(BOSSFIRESND);
        break;
    }

    frame = 4;

    if(engine.map.isClearLine(x, z, engine.player.x, engine.player.z)) {
        int8_t dist = getPlayerCellDistance();
        int hitchance = 256 - dist * 16;

        if(type == ActorType_Dog && dist > 1) return;

        if(getRandomNumber() < hitchance) {
            uint8_t damage;

            if(dist < 2)
                damage = getRandomNumber() >> 2;
            else if(dist < 4)
                damage = getRandomNumber() >> 3;
            else
                damage = getRandomNumber() >> 4;

            if(damage > 0) {
                engine.player.damage(damage);
                if(engine.player.hp == 0) {
                    for(int8_t id = 0; id < MAX_ACTIVE_ACTORS; id++) {
                        if(this == &engine.actors[id]) {
                            engine.player.killer = id;
                            break;
                        }
                    }
                }
            }
        }
    }
}
