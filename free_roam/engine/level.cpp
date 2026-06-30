#include "entity.hpp"
#include "game.hpp"
#include "level.hpp"
#include "sprite3d.hpp"
#include "engine_config.hpp"
#include ENGINE_LCD_INCLUDE
#include <math.h>

// Default Constructor
Level::Level()
    : name(""),
      size(Vector(0, 0)),
      clearAllowed(true),
      gameRef(nullptr),
      entity_count(0),
      entities(nullptr),
      renderOrder(nullptr),
      _start{},
      _stop{}
{
}

// Parameterized Constructor
Level::Level(const char *name, const Vector &size, Game *game, CallbackLevel start, CallbackLevel stop)
    : name(name),
      size(size),
      clearAllowed(true),
      gameRef(game),
      entity_count(0),
      entities(nullptr),
      renderOrder(nullptr),
      _start(start),
      _stop(stop)
{
}

// Destructor
Level::~Level()
{
    clear();
}

// Clear all entities
void Level::clear()
{
    for (int i = 0; i < entity_count; i++)
    {
        if (entities[i] != nullptr)
        {
            entities[i]->stop(this->gameRef);
            // Only delete entities that are not players (players are managed externally)
            if (!entities[i]->is_player)
            {
                ENGINE_MEM_DELETE entities[i];
            }
            entities[i] = nullptr;
        }
    }
    // Free the dynamic array
    ENGINE_MEM_DELETE[] entities;
    entities = nullptr;
    entity_count = 0;
    ENGINE_MEM_FREE(renderOrder);
    renderOrder = nullptr;
}

// Get list of collisions for a given entity
Entity **Level::collision_list(Entity *entity, int &count) const
{
    count = 0;
    if (entity_count == 0)
    {
        return nullptr;
    }

    Entity **result = ENGINE_MEM_NEW Entity *[entity_count];
    for (int i = 0; i < entity_count; i++)
    {
        if (entities[i] != nullptr &&
            entities[i] != entity && // Skip self
            is_collision(entity, entities[i]))
        {
            result[count++] = entities[i];
        }
    }
    return result;
}

// Add an entity to the level
void Level::entity_add(Entity *entity)
{
    if (!entity)
    {
        return;
    }

    if (!this->gameRef)
    {
        return;
    }

    // Allocate a new array with size one greater than the current count
    Entity **newEntities = ENGINE_MEM_NEW Entity * [entity_count + 1];
    if (!newEntities)
    {
        return;
    }

    // Copy the existing entity pointers (if any)
    for (int i = 0; i < entity_count; i++)
    {
        newEntities[i] = entities[i];
    }
    newEntities[entity_count] = entity;

    // Delete the old array
    ENGINE_MEM_DELETE[] entities;
    entities = newEntities;
    entity_count++;

    // Grow the depth-sort scratch buffer to match
    ENGINE_MEM_FREE(renderOrder);
    renderOrder = (int *)ENGINE_MEM_MALLOC(entity_count * sizeof(int));

    // Start the new entity
    entity->start(this->gameRef);
    entity->is_active = true;
}

// Remove an entity from the level
void Level::entity_remove(Entity *entity)
{
    if (entity_count == 0)
        return;

    int remove_index = -1;
    for (int i = 0; i < entity_count; i++)
    {
        if (entities[i] == entity)
        {
            remove_index = i;
            break;
        }
    }
    if (remove_index == -1)
        return;

    // Stop and delete the entity (only if it's not a player - players are managed externally)
    entities[remove_index]->stop(this->gameRef);
    if (!entities[remove_index]->is_player)
    {
        ENGINE_MEM_DELETE entities[remove_index];
    }

    // Allocate a new array with one fewer slot (if any remain)
    Entity **newEntities = (entity_count - 1 > 0) ? ENGINE_MEM_NEW Entity * [entity_count - 1] : nullptr;
    // Copy over all pointers except the removed one
    for (int i = 0, j = 0; i < entity_count; i++)
    {
        if (i == remove_index)
            continue;
        newEntities[j++] = entities[i];
    }

    // Free the old array and update state
    ENGINE_MEM_DELETE[] entities;
    entities = newEntities;
    entity_count--;

    // Shrink the depth-sort scratch buffer to match
    ENGINE_MEM_FREE(renderOrder);
    renderOrder = entity_count > 0 ? (int *)ENGINE_MEM_MALLOC(entity_count * sizeof(int)) : nullptr;
}

// Check if any entity has collided with the given entity
bool Level::has_collided(Entity *entity) const
{
    for (int i = 0; i < entity_count; i++)
    {
        if (entities[i] != nullptr &&
            entities[i] != entity &&
            is_collision(entity, entities[i]))
        {
            return true;
        }
    }
    return false;
}

// Determine if two entities are colliding
bool Level::is_collision(const Entity *a, const Entity *b) const
{
    return a->position.x < b->position.x + b->size.x &&
           a->position.x + a->size.x > b->position.x &&
           a->position.y < b->position.y + b->size.y &&
           a->position.y + a->size.y > b->position.y;
}

void Level::project3DTo2D(Vector vertex, Vector player_pos, Vector player_dir, float view_height, Vector screen_size, Vector &result)
{
    // Transform world coordinates to camera coordinates
    float world_dx = vertex.x - player_pos.x;
    float world_dz = vertex.z - player_pos.y; // player_pos.y is actually the Z coordinate in world space
    float world_dy = vertex.y - view_height;  // Height difference from camera

    // Transform to camera space with camera coordinate system
    float camera_x = world_dx * -player_dir.y + world_dz * player_dir.x;
    float camera_z = world_dx * player_dir.x + world_dz * player_dir.y;

    // Prevent division by zero and reject points behind camera
    if (camera_z <= 0.1f)
    {
        result.x = -1;
        result.y = -1;
        result.z = 0.0f;
        return; // Invalid point (behind camera)
    }

    // Project to screen coordinates - scale based on screen size
    result.x = (camera_x / camera_z) * screen_size.y + (screen_size.x / 2.0f);
    result.y = (-world_dy / camera_z) * screen_size.y + (screen_size.y / 2.0f);
    result.z = camera_z; // Store depth
}

// Render all active entities
void Level::render(Game *game)
{
    // clear the screen and render the entities
    if (clearAllowed)
    {
        game->draw->fillScreen(game->bg_color);
    }

    Camera *gameCamera = game->getCamera();

    // If using third person perspective, calculate camera from player
    if (gameCamera->perspective == CAMERA_THIRD_PERSON)
    {
        // Find the player entity to calculate 3rd person camera
        Entity *player = nullptr;
        for (int i = 0; i < entity_count; i++)
        {
            if (entities[i] != nullptr && entities[i]->is_player)
            {
                player = entities[i];
                break;
            }
        }

        if (player != nullptr)
        {
            // Calculate 3rd person camera position behind the player
            // Normalize direction vector to ensure consistent behavior
            float dir_length = sqrtf(player->direction.x * player->direction.x + player->direction.y * player->direction.y);
            if (dir_length < 0.001f)
            {
                // Fallback if direction is zero
                dir_length = 1.0f;
                player->direction.x = 1; // Default forward direction
                player->direction.y = 0; // Default forward direction
            }
            float normalized_dir_x = player->direction.x / dir_length;
            float normalized_dir_y = player->direction.y / dir_length;

            gameCamera->position.x = player->position.x - normalized_dir_x * gameCamera->distance;
            gameCamera->position.y = player->position.y - normalized_dir_y * gameCamera->distance;
            gameCamera->direction.x = normalized_dir_x;
            gameCamera->direction.y = normalized_dir_y;
            gameCamera->plane.x = player->plane.x;
            gameCamera->plane.y = player->plane.y;
        }
    }

    if (entity_count == 0)
    {
        return; // Nothing to render
    }

    // Painter's algorithm (back-to-front) for third-person
    const bool use_depth_order = (gameCamera->perspective == CAMERA_THIRD_PERSON && entity_count > 0);
    if (use_depth_order && renderOrder != nullptr)
    {
        for (int i = 0; i < entity_count; i++)
            renderOrder[i] = i;
        // Insertion sort: furthest entities first so closer ones overdraw them
        for (int i = 1; i < entity_count; i++)
        {
            int key_idx = renderOrder[i];
            float key_dist = 0.0f;
            if (entities[key_idx] != nullptr)
            {
                float dx = entities[key_idx]->position.x - gameCamera->position.x;
                float dy = entities[key_idx]->position.y - gameCamera->position.y;
                key_dist = dx * dx + dy * dy;
            }
            int j = i - 1;
            while (j >= 0)
            {
                int cmp_idx = renderOrder[j];
                float cmp_dist = 0.0f;
                if (entities[cmp_idx] != nullptr)
                {
                    float dx = entities[cmp_idx]->position.x - gameCamera->position.x;
                    float dy = entities[cmp_idx]->position.y - gameCamera->position.y;
                    cmp_dist = dx * dx + dy * dy;
                }
                if (cmp_dist < key_dist)
                {
                    renderOrder[j + 1] = renderOrder[j];
                    j--;
                }
                else
                {
                    break;
                }
            }
            renderOrder[j + 1] = key_idx;
        }
    }

    for (int i = 0; i < entity_count; i++)
    {
        Entity *ent = entities[use_depth_order && renderOrder ? renderOrder[i] : i];

        if (ent != nullptr && ent->is_active)
        {
            ent->render(game->draw, game);

            if (!ent->is_visible)
            {
                continue; // Skip rendering if entity is not visible
            }

            // Only draw the 2D sprite if it exists
            if (ent->sprite != nullptr)
            {
                ent->sprite->render(game->draw, ent->position.x - game->pos.x, ent->position.y - game->pos.y);
            }

            // Render 3D sprite if it exists
            if (ent->has3DSprite())
            {
                if (gameCamera->perspective == CAMERA_FIRST_PERSON)
                {
                    // First person: render from player's own perspective
                    if (ent->is_player)
                    {
                        // Use entity's own direction and plane for rendering
                        render3DSprite(ent->sprite_3d, game->draw, ent->position, ent->direction, gameCamera->height);
                    }
                    else
                    {
                        // For non-player entities, render from the player's perspective
                        // We need to find the player entity to get the view parameters
                        Entity *player = nullptr;
                        for (int j = 0; j < entity_count; j++)
                        {
                            if (entities[j] != nullptr && entities[j]->is_player)
                            {
                                player = entities[j];
                                break;
                            }
                        }

                        if (player != nullptr)
                        {
                            render3DSprite(ent->sprite_3d, game->draw, player->position, player->direction, gameCamera->height);
                        }
                    }
                }
                else if (gameCamera->perspective == CAMERA_THIRD_PERSON)
                {
                    // Third person: render ALL entities (including player) from the external camera perspective
                    render3DSprite(ent->sprite_3d, game->draw, gameCamera->position, gameCamera->direction, gameCamera->height);
                }
            }
        }
    }

    if (clearAllowed)
    {
// send newly drawn pixels to the display
#ifdef ENGINE_LCD_SWAP
        ENGINE_LCD_SWAP();
#endif
    }
}

void Level::render3DSprite(const Sprite3D *sprite3d, Draw *draw, Vector player_pos, Vector player_dir, float view_height)
{
    if (!sprite3d)
        return;

    // Get triangles from the 3D sprite and render them
    // changed from static because it gave us size issues
    // with compiling in micropython
    Vector screen_points[3];
    Vector vertex;
    Triangle3D triangle;
    //
    const uint8_t triangle_count = sprite3d->getTriangleCount();
    for (uint8_t i = 0; i < triangle_count; i++)
    {
        triangle = sprite3d->getTransformedTriangle(i, player_pos);
        if (!triangle.set)
            continue;

        // Only render triangles facing the camera
        if (triangle.isFacingCamera(player_pos))
        {
            // Project 3D vertices to 2D screen coordinates
            bool any_visible = false;
            bool has_behind = false;

            for (uint8_t j = 0; j < 3; j++)
            {
                switch (j)
                {
                case 0:
                    vertex.x = triangle.x1;
                    vertex.y = triangle.y1;
                    vertex.z = triangle.z1;
                    break;
                case 1:
                    vertex.x = triangle.x2;
                    vertex.y = triangle.y2;
                    vertex.z = triangle.z2;
                    break;
                case 2:
                    vertex.x = triangle.x3;
                    vertex.y = triangle.y3;
                    vertex.z = triangle.z3;
                    break;
                default:
                    vertex.x = 0;
                    vertex.y = 0;
                    vertex.z = 0;
                    break;
                };

                project3DTo2D(vertex, player_pos, player_dir, view_height, draw->getDisplaySize(), screen_points[j]);

                // Check if behind camera
                if (screen_points[j].x == -1 && screen_points[j].y == -1)
                {
                    has_behind = true;
                }
                else
                {
                    any_visible = true;
                }
            }

            if (any_visible && !has_behind)
            {
                draw->fillTriangle(screen_points[0], screen_points[1], screen_points[2], triangle.color);
                if (triangle.wireframe)
                {
                    // Compute a lighter outline color from the fill color
                    uint8_t r = (uint8_t)((triangle.color >> 11) & 0x1F);
                    uint8_t g = (uint8_t)((triangle.color >> 5) & 0x3F);
                    uint8_t b = (uint8_t)(triangle.color & 0x1F);
                    r = r + ((0x1F - r) >> 1);
                    g = g + ((0x3F - g) >> 1);
                    b = b + ((0x1F - b) >> 1);
                    const uint16_t outline_color = ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
                    draw->triangle(screen_points[0], screen_points[1], screen_points[2], outline_color);
                }
            }
        }
    }
}

// Start the level
void Level::start()
{
    if (_start)
    {
        _start(*this);
    }
}

// Stop the level
void Level::stop()
{
    if (_stop)
    {
        _stop(*this);
    }
}

// Update all active entities
void Level::update(Game *game)
{
    for (int i = 0; i < entity_count; i++)
    {
        Entity *ent = entities[i];

        if (ent != nullptr && ent->is_active)
        {
            ent->update(game);

            for (int j = 0; j < entity_count; j++)
            {
                if (entities[j] != nullptr &&
                    entities[j] != ent &&
                    is_collision(ent, entities[j]))
                {
                    ent->collision(entities[j], game);
                }
            }
        }
    }
}
