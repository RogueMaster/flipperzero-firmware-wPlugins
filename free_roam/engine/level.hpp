#pragma once
#include "vector.hpp"
#include "camera.hpp"
#include "callback.hpp"

// Forward declarations
class Game;
class Sprite3D;
class Entity;

class Level
{
public:
    Level(); // Default constructor
    Level(const char *name,
          const Vector &size,
          Game *game,
          CallbackLevel start = {},
          CallbackLevel stop = {});
    ~Level();

    // Member Functions
    void clear();
    Entity **collision_list(Entity *entity, int &count) const;
    void entity_add(Entity *entity);
    void entity_remove(Entity *entity);
    Entity *getEntity(int index) const { return entities[index]; }
    int getEntityCount() const { return entity_count; }
    bool has_collided(Entity *entity) const;
    bool isClearAllowed() const noexcept { return clearAllowed; }
    bool is_collision(const Entity *a, const Entity *b) const;
    void project3DTo2D(Vector vertex, Vector player_pos, Vector player_dir, float view_height, Vector screen_size, Vector &result);
    void render(Game *game);
    void render3DSprite(const Sprite3D *sprite3d, Draw *draw, Vector player_pos, Vector player_dir, float view_height);
    void setClearAllowed(bool status) { clearAllowed = status; }
    void start();
    void stop();
    void update(Game *game);

    const char *name;
    Vector size;

private:
    bool clearAllowed;
    Game *gameRef;
    int entity_count;
    Entity **entities;
    int *renderOrder;
    // Callback Functions
    CallbackLevel _start;
    CallbackLevel _stop;
};
