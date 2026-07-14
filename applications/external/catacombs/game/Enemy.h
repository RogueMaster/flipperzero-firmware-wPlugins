#pragma once

#include <stdint.h>
#include "game/Entity.h"
#include "game/Defines.h"
#include "game/Draw.h"

enum class EnemyType : uint8_t {
    Skeleton,
    Mage,
    Bat,
    Spider,
    NumEnemyTypes,
    None = NumEnemyTypes,
    Exit,
};

enum class EnemyState : uint8_t {
    Idle,
    Moving,
    Attacking,
    Stunned,
    Dying,
    Dead
};

struct EnemyArchetype {
    const uint16_t* spriteData;

    uint8_t hp;
    uint8_t movementSpeed;
    uint8_t attackStrength;
    uint8_t attackDuration;
    uint8_t stunDuration;
    uint8_t isRanged;
    uint8_t spriteScale;
    AnchorType spriteAnchor;

    const uint16_t* GetSpriteData() const {
        return static_cast<const uint16_t*>(pgm_read_ptr_safe(&spriteData));
    }
    uint8_t GetHP() const {
        return pgm_read_byte(&hp);
    }
    uint8_t GetMovementSpeed() const {
        return pgm_read_byte(&movementSpeed);
    }
    uint8_t GetAttackStrength() const {
        return pgm_read_byte(&attackStrength);
    }
    uint8_t GetAttackDuration() const {
        return pgm_read_byte(&attackDuration);
    }
    uint8_t GetStunDuration() const {
        return pgm_read_byte(&stunDuration);
    }
    bool GetIsRanged() const {
        return pgm_read_byte(&isRanged) != 0;
    }
    uint8_t GetSpriteScale() const {
        return pgm_read_byte(&spriteScale);
    }
    AnchorType GetSpriteAnchor() const {
        return (AnchorType)pgm_read_byte(&spriteAnchor);
    }
};

class Enemy : public Entity {
public:
    void Init(EnemyType type, int16_t x, int16_t y);
    void Tick();
    bool IsValid() const {
        return type != EnemyType::None;
    }
    void Damage(uint8_t amount);
    void Clear() {
        type = EnemyType::None;
    }
    const EnemyArchetype* GetArchetype() const;
    EnemyState GetState() const {
        return state;
    }
    EnemyType GetType() const {
        return type;
    }

private:
    static const EnemyArchetype archetypes[(int)EnemyType::NumEnemyTypes];

    bool ShouldFireProjectile() const;
    bool FireProjectile(uint8_t angle);
    bool TryMove();
    void StunMove();
    bool TryFireProjectile();
    void PickNewTargetCell();
    bool TryPickCells(int8_t deltaX, int8_t deltaY);
    bool TryPickCell(int8_t newX, int8_t newY);
    uint8_t GetPlayerCellDistance() const;

    EnemyType type     : 3;
    EnemyState state   : 3;
    uint8_t frameDelay : 2;
    uint8_t hp;
    uint8_t targetCellX, targetCellY;
};

class EnemyManager {
public:
    static constexpr int maxEnemies = 24; //24;
    static Enemy enemies[maxEnemies];

    static void Spawn(EnemyType enemyType, int16_t x, int16_t y);
    static void SpawnEnemies();

    static Enemy* GetOverlappingEnemy(Entity& entity);
    static Enemy* GetOverlappingEnemy(int16_t x, int16_t y);

    static void Init();
    static void Draw();
    static void Update();
};
