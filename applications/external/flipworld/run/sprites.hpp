#pragma once
#include "furi.h"
#include "engine/entity.hpp"
#include "engine/vector.hpp"
#include "engine/game.hpp"

class FlipWorldRun;

class Sprite : public Entity {
private:
    FlipWorldRun* flipWorldRun = nullptr;
    void syncMultiplayerState(bool hostOnly = true, Entity* other = nullptr);

public:
    Sprite(
        const char* name,
        EntityType type,
        Vector position,
        Vector endPosition,
        float move_timer,
        float speed,
        float attack_timer,
        float strength,
        float health);
    ~Sprite() = default;
    void collision(Entity* other, Game* game) override;
    void drawUsername(Vector pos, Game* game);
    void render(Draw* canvas, Game* game) override;
    void update(Game* game) override;
    void setFlipWorldRun(FlipWorldRun* run) {
        flipWorldRun = run;
    } // Set the FlipWorldRun instance
};
