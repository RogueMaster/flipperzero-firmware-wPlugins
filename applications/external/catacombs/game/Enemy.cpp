#include "game/Enemy.h"
#include "game/Defines.h"
#include "game/Draw.h"
#include "game/Map.h"
#include "game/FixedMath.h"
#include "game/Game.h"
#include "game/Projectile.h"
#include "game/Generated/SpriteTypes.h"
#include "game/Sounds.h"
#include "game/Platform.h"
#include "game/Particle.h"

Enemy EnemyManager::enemies[maxEnemies];

const EnemyArchetype Enemy::archetypes[(int)EnemyType::NumEnemyTypes] PROGMEM = {
    {
        // Skeleton
        skeletonSpriteData,
        50, // hp
        4, // speed
        20, // attackStrength
        3, // attackDuration
        2, // stunDuration
        false, // isRanged
        96, // sprite scale
        AnchorType::Floor // sprite anchor
    },
    {
        // Mage
        mageSpriteData,
        30, // hp
        5, // speed
        20, // attackStrength
        3, // attackDuration
        2, // stunDuration
        true, // isRanged
        96, // sprite scale
        AnchorType::Floor // sprite anchor
    },
    {
        // Bat
        batSpriteData,
        20, // hp
        7, // speed
        10, // attackStrength
        2, // attackDuration
        0, // stunDuration
        false, // isRanged
        80, // sprite scale
        AnchorType::Center // sprite anchor
    },
    {
        // Spider
        spiderSpriteData,
        10, // hp
        7, // speed
        5, // attackStrength
        1, // attackDuration
        0, // stunDuration
        false, // isRanged
        50, // sprite scale
        AnchorType::Floor // sprite anchor
    }};

void Enemy::Init(EnemyType initType, int16_t initX, int16_t initY) {
    state = EnemyState::Idle;
    type = initType;
    x = initX;
    y = initY;
    frameDelay = 0;
    targetCellX = x / CELL_SIZE;
    targetCellY = y / CELL_SIZE;
    hp = GetArchetype()->GetHP();
}

void Enemy::Damage(uint8_t amount) {
    if(amount >= hp) {
        Game::stats.enemyKills[(int)type]++;
        type = EnemyType::None;
        Platform::PlaySound(Sounds::Kill);
        ParticleSystemManager::CreateExplosion(x, y, true);
    } else {
        hp -= amount;
        Platform::PlaySound(Sounds::Hit);
        state = EnemyState::Stunned;
        frameDelay = GetArchetype()->GetStunDuration();
    }
}

const EnemyArchetype* Enemy::GetArchetype() const {
    if(type == EnemyType::None) return nullptr;
    return &archetypes[(int)type];
}

int16_t Clamp(int16_t x, int16_t min, int16_t max) {
    if(x < min) return min;
    if(x > max) return max;
    return x;
}

bool Enemy::TryPickCell(int8_t newX, int8_t newY) {
    if(Map::IsBlocked(newX, newY)) // && !engine.map.isDoor(newX, newZ))
        return false;
    if(Map::IsBlocked(targetCellX, newY)) // && !engine.map.isDoor(targetCellX, newZ))
        return false;
    if(Map::IsBlocked(newX, targetCellY)) // && !engine.map.isDoor(newX, targetCellZ))
        return false;

    for(Enemy& other : EnemyManager::enemies) {
        if(this != &other && other.IsValid()) {
            if(other.targetCellX == newX && other.targetCellY == newY) return false;
        }
    }

    targetCellX = newX;
    targetCellY = newY;

    return true;
}

bool Enemy::TryPickCells(int8_t deltaX, int8_t deltaY) {
    return TryPickCell(targetCellX + deltaX, targetCellY + deltaY) ||
           TryPickCell(targetCellX + deltaX, targetCellY) ||
           TryPickCell(targetCellX, targetCellY + deltaY) ||
           TryPickCell(targetCellX - deltaX, targetCellY + deltaY) ||
           TryPickCell(targetCellX + deltaX, targetCellY - deltaY);
}

uint8_t Enemy::GetPlayerCellDistance() const {
    uint8_t dx = ABS(Game::player.x - x) / CELL_SIZE;
    uint8_t dy = ABS(Game::player.y - y) / CELL_SIZE;
    return dx > dy ? dx : dy;
}

void Enemy::PickNewTargetCell() {
    int8_t deltaX = (int8_t)Clamp((Game::player.x / CELL_SIZE) - targetCellX, -1, 1);
    int8_t deltaY = (int8_t)Clamp((Game::player.y / CELL_SIZE) - targetCellY, -1, 1);
    uint8_t dodgeChance = (uint8_t)Random();

    if(GetArchetype()->GetIsRanged() && GetPlayerCellDistance() < 3) {
        deltaX = -deltaX;
        deltaY = -deltaY;
    }

    if(deltaX == 0) {
        if(dodgeChance < 64) {
            deltaX = -1;
        } else if(dodgeChance < 128) {
            deltaX = 1;
        }
    } else if(deltaY == 0) {
        if(dodgeChance < 64) {
            deltaY = -1;
        } else if(dodgeChance < 128) {
            deltaY = 1;
        }
    }

    TryPickCells(deltaX, deltaY);
}

void Enemy::StunMove() {
    //int16_t targetX = Game::player.x;
    //int16_t targetY = Game::player.y;
    //
    //int16_t maxDelta = 3;
    //
    //int16_t deltaX = Clamp(targetX - x, -maxDelta, maxDelta);
    //int16_t deltaY = Clamp(targetY - y, -maxDelta, maxDelta);
    //
    //x -= deltaX;
    //y -= deltaY;

    //	int16_t deltaX = (Random() % 16) - 8;
    //	int16_t deltaY = (Random() % 16) - 8;
    //	x += deltaX;
    //	y += deltaY;
}

bool Enemy::TryMove() {
    if(Map::IsSolid(targetCellX, targetCellY)) {
        //engine.map.openDoorsAt(targetCellX, targetCellZ, Direction_None);
        return false;
    }

    int16_t targetX = (targetCellX * CELL_SIZE) + CELL_SIZE / 2;
    int16_t targetY = (targetCellY * CELL_SIZE) + CELL_SIZE / 2;

    int16_t maxDelta = GetArchetype()->GetMovementSpeed();

    int16_t deltaX = Clamp(targetX - x, -maxDelta, maxDelta);
    int16_t deltaY = Clamp(targetY - y, -maxDelta, maxDelta);

    x += deltaX;
    y += deltaY;

    if(IsOverlappingEntity(Game::player)) {
        if(!GetArchetype()->GetIsRanged()) {
            Game::player.Damage(GetArchetype()->GetAttackStrength());
            if(Game::player.hp == 0) {
                Game::stats.killedBy = type;
            }

            state = EnemyState::Attacking;
            frameDelay = GetArchetype()->GetAttackDuration();
        }

        x -= deltaX;
        y -= deltaY;
        return false;
    }

    if(x == targetX && y == targetY) {
        PickNewTargetCell();
    }
    return true;
}

bool Enemy::FireProjectile(uint8_t angle) {
    return ProjectileManager::FireProjectile(this, x, y, angle) != nullptr;
}

bool Enemy::TryFireProjectile() {
    int8_t deltaX = (Game::player.x - x) / CELL_SIZE;
    int8_t deltaY = (Game::player.y - y) / CELL_SIZE;

    if(deltaX == 0) {
        if(deltaY < 0) {
            return FireProjectile(FIXED_ANGLE_270);
        } else if(deltaY > 0) {
            return FireProjectile(FIXED_ANGLE_90);
        }
    } else if(deltaY == 0) {
        if(deltaX < 0) {
            return FireProjectile(FIXED_ANGLE_180);
        } else if(deltaX > 0) {
            return FireProjectile(0);
        }
    } else if(deltaX == deltaY) {
        if(deltaX > 0) {
            return FireProjectile(FIXED_ANGLE_45);
        } else {
            return FireProjectile(FIXED_ANGLE_180 + FIXED_ANGLE_45);
        }
    } else if(deltaX == -deltaY) {
        if(deltaX > 0) {
            return FireProjectile(FIXED_ANGLE_270 + FIXED_ANGLE_45);
        } else {
            return FireProjectile(FIXED_ANGLE_90 + FIXED_ANGLE_45);
        }
    }

    return false;
}

bool Enemy::ShouldFireProjectile() const {
    uint8_t distance = GetPlayerCellDistance();
    uint8_t chance = 16 / (distance > 0 ? distance : 1);

    return GetArchetype()->GetIsRanged() && (Random() & 0xff) < chance &&
           Map::IsClearLine(x, y, Game::player.x, Game::player.y);
}

void Enemy::Tick() {
    if(state == EnemyState::Stunned) {
        StunMove();
    }

    if(frameDelay > 0) {
        if((Game::globalTickFrame & 0xf) == 0) {
            frameDelay--;
        }
        return;
    }

    switch(state) {
    case EnemyState::Idle:
        if(Map::IsClearLine(x, y, Game::player.x, Game::player.y)) {
            Platform::PlaySound(Sounds::SpotPlayer);
            state = EnemyState::Moving;
        }
        break;
    case EnemyState::Moving:
        TryMove();

        if(ShouldFireProjectile()) {
            if(TryFireProjectile()) {
                Platform::PlaySound(Sounds::Shoot);
                state = EnemyState::Attacking;
                frameDelay = GetArchetype()->GetAttackDuration();
            }
        }
        break;
    case EnemyState::Attacking:
        state = EnemyState::Moving;
        break;
    case EnemyState::Stunned:
        state = EnemyState::Moving;
        break;
    default:
        break;
    }
}

void EnemyManager::Init() {
    for(Enemy& enemy : enemies) {
        enemy.Clear();
    }
}

void EnemyManager::Update() {
    for(Enemy& enemy : enemies) {
        if(enemy.IsValid()) {
            enemy.Tick();
        }
    }
}

void EnemyManager::Draw() {
    for(Enemy& enemy : enemies) {
        if(enemy.IsValid()) {
            bool invert = enemy.GetState() == EnemyState::Stunned &&
                          (Renderer::globalRenderFrame & 1);
            int frameOffset =
                (enemy.GetType() == EnemyType::Bat || enemy.GetState() == EnemyState::Moving) &&
                        (Game::globalTickFrame & 8) == 0 ?
                    32 :
                    0;

            const EnemyArchetype* archetype = enemy.GetArchetype();
            Renderer::DrawObject(
                archetype->GetSpriteData() + frameOffset,
                enemy.x,
                enemy.y,
                archetype->GetSpriteScale(),
                archetype->GetSpriteAnchor(),
                invert);
        }
    }
}

void EnemyManager::Spawn(EnemyType enemyType, int16_t x, int16_t y) {
    for(Enemy& enemy : enemies) {
        if(!enemy.IsValid()) {
            enemy.Init(enemyType, x, y);
            return;
        }
    }
}

void EnemyManager::SpawnEnemies() {
    for(uint8_t y = 0; y < Map::height; y++) {
        for(uint8_t x = 0; x < Map::width; x++) {
            if(Map::GetCellSafe(x, y) == CellType::Monster) {
                EnemyType type = (EnemyType)((Random() % ((int)(EnemyType::NumEnemyTypes))));
                EnemyManager::Spawn(
                    type, x * CELL_SIZE + CELL_SIZE / 2, y * CELL_SIZE + CELL_SIZE / 2);
                Map::SetCell(x, y, CellType::Empty);
                break;
            }
        }
    }
}

Enemy* EnemyManager::GetOverlappingEnemy(Entity& entity) {
    for(Enemy& enemy : enemies) {
        if(enemy.IsValid() && enemy.IsOverlappingEntity(entity)) {
            return &enemy;
        }
    }

    return nullptr;
}

Enemy* EnemyManager::GetOverlappingEnemy(int16_t x, int16_t y) {
    for(Enemy& enemy : enemies) {
        if(enemy.IsValid() && enemy.IsOverlappingPoint(x, y)) {
            return &enemy;
        }
    }

    return nullptr;
}
