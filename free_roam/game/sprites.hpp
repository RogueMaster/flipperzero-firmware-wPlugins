#pragma once
#include "furi.h"
#include "engine/entity.hpp"
#include "engine/vector.hpp"
#include "engine/game.hpp"

class Sprite : public Entity
{
public:
    Sprite(const char *name, Vector position, Sprite3DType spriteType,
           float height = 2.0f, float width = 1.0f, float rotation = 0.0f, Vector endPosition = Vector(-1, -1));
    ~Sprite() = default;
    void collision(Entity *other, Game *game) override;
    void update(Game *game) override;
};
