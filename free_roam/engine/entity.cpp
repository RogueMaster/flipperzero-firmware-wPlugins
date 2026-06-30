#include "entity.hpp"
#include "game.hpp"
#include "sprite3d.hpp"
#include "image.hpp"

Entity::Entity(
    const char *name,
    EntityType type,
    Vector position,
    Vector size,
    Image *sprite_data,
    Image *sprite_left_data,
    Image *sprite_right_data,
    CallbackEntityGame start,
    CallbackEntityGame stop,
    CallbackEntityGame update,
    CallbackEntityDrawGame render,
    CallbackEntityEntityGame collision,
    bool is_8bit_sprite,
    Sprite3DType sprite_3d_type,
    uint16_t sprite_3d_color)
{
    this->name = name;
    this->type = type;
    this->position = position;
    this->old_position = position;
    this->direction = Vector(1, 0, 0, true);
    this->plane = Vector(0, 0);
    this->size = size;
    this->sprite = sprite_data;
    this->sprite_left = sprite_left_data;
    this->sprite_right = sprite_right_data;
    this->_start = start;
    this->_stop = stop;
    this->_update = update;
    this->_render = render;
    this->_collision = collision;
    this->is_active = false;
    this->is_visible = true;
    this->is_player = false;
    this->is_8bit = is_8bit_sprite;

    // initialize additional properties
    this->state = ENTITY_IDLE;
    this->start_position = position;
    this->end_position = position;
    this->move_timer = 0;
    this->elapsed_move_timer = 0;
    this->radius = this->size.x / 2;
    this->speed = 0;
    this->attack_timer = 0;
    this->elapsed_attack_timer = 0;
    this->strength = 0;
    this->health = 0;
    this->max_health = 0;
    this->level = 0;
    this->xp = 0;
    this->health_regen = 0;
    this->elapsed_health_regen = 0;

    // Initialize 3D sprite properties
    this->sprite_3d = nullptr;
    this->sprite_3d_type = sprite_3d_type;
    this->sprite_rotation = 0.0f;
    this->sprite_scale = 1.0f;

    // Create 3D sprite if type is specified
    if (sprite_3d_type != SPRITE_3D_NONE)
    {
        create3DSprite(sprite_3d_type, size.y, size.x, 0.0f, sprite_3d_color);
    }
}

Entity::~Entity()
{
    // Clean up 3D sprite first
    destroy3DSprite();

    if (this->sprite != NULL)
    {
        ENGINE_MEM_DELETE this->sprite;
        this->sprite = NULL;
    }
    if (this->sprite_left != NULL)
    {
        ENGINE_MEM_DELETE this->sprite_left;
        this->sprite_left = NULL;
    }
    if (this->sprite_right != NULL)
    {
        ENGINE_MEM_DELETE this->sprite_right;
        this->sprite_right = NULL;
    }
}

void Entity::collision(Entity *other, Game *game)
{
    if (this->_collision)
    {
        this->_collision(this, other, game);
    }
}

void Entity::create3DSprite(Sprite3DType type, float height, float width, float rotation, uint16_t color)
{
    // Clean up any existing sprite first
    destroy3DSprite();

    sprite_3d_type = type;
    sprite_rotation = rotation;

    switch (type)
    {
    case SPRITE_3D_HUMANOID:
        sprite_3d = ENGINE_MEM_NEW Sprite3D();
        sprite_3d->initializeAsHumanoid(position, height, rotation, color);
        break;

    case SPRITE_3D_TREE:
        sprite_3d = ENGINE_MEM_NEW Sprite3D();
        sprite_3d->initializeAsTree(position, height, color);
        break;

    case SPRITE_3D_HOUSE:
        sprite_3d = ENGINE_MEM_NEW Sprite3D();
        sprite_3d->initializeAsHouse(position, width, height, rotation, color);
        break;

    case SPRITE_3D_PILLAR:
        sprite_3d = ENGINE_MEM_NEW Sprite3D();
        sprite_3d->initializeAsPillar(position, height, width, color);
        break;

    case SPRITE_3D_CUSTOM:
    case SPRITE_3D_NONE:
    default:
        sprite_3d = nullptr;
        break;
    }
}

void Entity::destroy3DSprite()
{
    if (sprite_3d != nullptr)
    {
        ENGINE_MEM_DELETE sprite_3d;
        sprite_3d = nullptr;
    }
    sprite_3d_type = SPRITE_3D_NONE;
}

bool Entity::has3DSprite() const
{
    return sprite_3d != nullptr && sprite_3d_type != SPRITE_3D_NONE;
}

bool Entity::hasChangedPosition() const
{
    return (this->position.x != this->old_position.x) || (this->position.y != this->old_position.y);
}

Vector Entity::position_get()
{
    return this->position;
}

void Entity::position_set(Vector value)
{
    this->old_position.x = this->position.x;
    this->old_position.y = this->position.y;
    this->old_position.z = this->position.z;
    this->old_position.integer = this->position.integer;
    this->position.x = value.x;
    this->position.y = value.y;
    this->position.z = value.z;
    this->position.integer = value.integer;

    // Automatically update 3D sprite position if it exists
    if (sprite_3d != nullptr)
    {
        sprite_3d->setPosition(position);
    }
}

void Entity::position_set(float x, float y, float z, bool integer)
{
    this->old_position.x = this->position.x;
    this->old_position.y = this->position.y;
    this->old_position.z = this->position.z;
    this->old_position.integer = this->position.integer;
    this->position.x = x;
    this->position.y = y;
    this->position.z = z;
    this->position.integer = integer;

    // Automatically update 3D sprite position if it exists
    if (sprite_3d != nullptr)
    {
        sprite_3d->setPosition(position);
    }
}

void Entity::render(Draw *draw, Game *game)
{
    if (this->_render)
    {
        this->_render(this, draw, game);
    }
}

void Entity::set3DSpriteRotation(float rotation)
{
    sprite_rotation = rotation;
    if (sprite_3d != nullptr)
    {
        sprite_3d->setRotation(rotation);
    }
}

void Entity::set3DSpriteScale(float scale)
{
    sprite_scale = scale;
    if (sprite_3d != nullptr)
    {
        sprite_3d->setScale(scale);
    }
}

void Entity::start(Game *game)
{
    if (!game)
    {
        return;
    }

    if (this->_start)
    {
        this->_start(this, game);
    }
    this->is_active = true;
}

void Entity::stop(Game *game)
{
    if (this->_stop)
    {
        this->_stop(this, game);
    }
    this->is_active = false;
}

void Entity::update(Game *game)
{
    if (this->_update)
    {
        this->_update(this, game);
    }

    // Update 3D sprite position if it exists
    if (has3DSprite())
    {
        update3DSpritePosition();
    }
}

void Entity::update3DSpritePosition()
{
    if (sprite_3d != nullptr)
    {
        sprite_3d->setPosition(position);
    }
}