#include "enemy.hpp"
#include <math.h>
#include "general.hpp"

Enemy::Enemy(const char *name, Vector position, EnemyType enemyType, float height, float width, float rotation, Vector endPosition)
    : Entity(name, ENTITY_ENEMY, position, Vector(width, height), nullptr,
             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, false, SPRITE_3D_CUSTOM, 0x0000)
{
    // sprite3D is nullptr, so lets set our stuff here
    sprite_3d_type = SPRITE_3D_CUSTOM;
    sprite_3d = ENGINE_MEM_NEW Sprite3D();
    if (!sprite_3d)
    {
        ENGINE_LOG_INFO("[Enemy:Enemy] Failed to create Sprite3D instance for enemy: %s", name);
        return;
    }
    // follow same init setps as Sprite3D::initializeAsHumanoid
    sprite_3d->setPosition(position);
    sprite_3d->setRotation(rotation);
    sprite_3d->clearTriangles();
    sprite_3d->setActive(true);
    switch (enemyType)
    {
    case ENEMY_BULLY:
        this->makeBully(height);
        this->strength = 10.0f;
        this->speed = SPEED_SCALE(0.1f);
        this->attack_timer = SPEED_SCALE(50.0f); // attack every 50 ticks
        break;
    case ENEMY_CREEPER:
        this->makeCreeper(height);
        this->strength = 20.0f;
        this->speed = SPEED_SCALE(0.05f);
        this->attack_timer = SPEED_SCALE(80.0f); // attack every 80 ticks
        break;
    case ENEMY_PUNK:
        this->makePunk(height);
        this->strength = 15.0f;
        this->speed = SPEED_SCALE(0.07f);
        this->attack_timer = SPEED_SCALE(60.0f); // attack every 60 ticks
        break;
    default:
        break;
    }
    set3DSpriteRotation(rotation);
    start_position = position;
    end_position = endPosition == Vector(-1, -1) ? position : endPosition;
    state = ENTITY_IDLE;
    this->health = ENEMY_HEALTH_BASE;
    this->max_health = ENEMY_HEALTH_BASE;
    sprite_3d->setWireframe(WIREFRAME_ENABLED);
}

Vector Enemy::getPlayerPosition(Game *game)
{
    // Find the player entity in the current level
    for (int i = 0; i < game->current_level->getEntityCount(); i++)
    {
        Entity *entity = game->current_level->getEntity(i);
        if (entity->is_player)
        {
            return entity->position;
        }
    }
    return this->end_position;
}

void Enemy::makeBully(float height)
{
    if (!sprite_3d)
    {
        ENGINE_LOG_INFO("[Enemy:makeBully] Sprite3D instance is null for enemy: %s", this->name);
        return;
    }
    const float hr = height * 0.12f, tw = height * 0.20f, th = height * 0.35f;
    const float lh = height * 0.45f, al = height * 0.25f;
    const float lw = tw * 0.45f, aw = tw * 0.35f;
    const uint16_t skin = rgb565(0x6b8c4a);
    const uint16_t shirt = rgb565(0x3a3a5a);
    const uint16_t pants = rgb565(0x2a2a40);
    const uint16_t shoe = rgb565(0x1a1a1a);
    const uint16_t eye = rgb565(0xff2200);

    // head
    sprite_3d->createSphere(0, height - hr, 0, hr, 4, skin);

    // torso
    sprite_3d->createCube(0, lh + th / 2, 0, tw, th, tw * 0.8f, shirt);

    // arms — rotated outward (reaching forward): shift Z forward
    sprite_3d->createCube(-tw * 0.8f, lh + th / 2, al * 0.6f, aw, aw, al, skin);
    sprite_3d->createCube(tw * 0.8f, lh + th / 2, al * 0.6f, aw, aw, al, skin);

    // legs
    sprite_3d->createCube(-lw * 0.7f, lh / 2, 0, lw, lh, lw, pants);
    sprite_3d->createCube(lw * 0.7f, lh / 2, 0, lw, lh, lw, pants);

    // shoes
    sprite_3d->createCube(-lw * 0.7f, lw * 0.15f, lw * 0.2f, lw * 1.1f, lw * 0.3f, lw * 1.4f, shoe);
    sprite_3d->createCube(lw * 0.7f, lw * 0.15f, lw * 0.2f, lw * 1.1f, lw * 0.3f, lw * 1.4f, shoe);

    // red eyes on face
    sprite_3d->createCube(-hr * 0.35f, height - hr * 0.85f, hr * 0.88f, hr * 0.28f, hr * 0.2f, hr * 0.1f, eye);
    sprite_3d->createCube(hr * 0.35f, height - hr * 0.85f, hr * 0.88f, hr * 0.28f, hr * 0.2f, hr * 0.1f, eye);
}

void Enemy::makePunk(float height)
{
    if (!sprite_3d)
    {
        ENGINE_LOG_INFO("[Enemy:makeBully] Sprite3D instance is null for enemy: %s", this->name);
        return;
    }
    const float hr = height * 0.13f, tw = height * 0.22f, th = height * 0.28f;
    const float lh = height * 0.35f, al = height * 0.30f;
    const float lw = tw * 0.45f, aw = tw * 0.35f;
    const uint16_t skin = rgb565(0x5a7a3a);
    const uint16_t shirt = rgb565(0x5a3a2a);
    const uint16_t pants = rgb565(0x2a1a10);
    const uint16_t shoe = rgb565(0x111111);
    const uint16_t eye = rgb565(0xddeeaa);

    // Head pitched forward (Z offset)
    sprite_3d->createSphere(0, lh + th + hr * 0.6f, hr * 0.9f, hr, 4, skin);

    // Torso — hunched, pushed forward
    sprite_3d->createCube(0, lh + th * 0.4f, th * 0.15f, tw, th, tw * 0.85f, shirt);

    // Left arm — hanging low (dragging)
    sprite_3d->createCube(-tw * 0.85f, lh * 0.3f, al * 0.1f, aw, al * 1.3f, aw, skin);

    // Right arm — raised and reaching
    sprite_3d->createCube(tw * 0.85f, lh + th * 0.6f, al * 0.3f, aw, al, aw, skin);

    // Legs — wide squat
    sprite_3d->createCube(-lw * 0.85f, lh * 0.45f, lw * 0.2f, lw, lh * 0.9f, lw, pants);
    sprite_3d->createCube(lw * 0.85f, lh * 0.45f, lw * 0.2f, lw, lh * 0.9f, lw, pants);

    // Shoes
    sprite_3d->createCube(-lw * 0.85f, lw * 0.15f, lw * 0.4f, lw * 1.1f, lw * 0.3f, lw * 1.3f, shoe);
    sprite_3d->createCube(lw * 0.85f, lw * 0.15f, lw * 0.4f, lw * 1.1f, lw * 0.3f, lw * 1.3f, shoe);

    // Glazed white-yellow eyes
    sprite_3d->createCube(-hr * 0.32f, lh + th + hr * 0.55f, hr * 1.05f, hr * 0.25f, hr * 0.18f, hr * 0.1f, eye);
    sprite_3d->createCube(hr * 0.32f, lh + th + hr * 0.55f, hr * 1.05f, hr * 0.25f, hr * 0.18f, hr * 0.1f, eye);
}

void Enemy::makeCreeper(float height)
{
    if (!sprite_3d)
    {
        ENGINE_LOG_INFO("[Enemy:makeBully] Sprite3D instance is null for enemy: %s", this->name);
        return;
    }
    const float hr = height * 0.10f, tw = height * 0.18f, th = height * 0.38f;
    const float lh = height * 0.50f, al = height * 0.32f;
    const float lw = tw * 0.40f, aw = tw * 0.30f;
    const uint16_t skin = rgb565(0x7a9a5a);
    const uint16_t coat = rgb565(0x1e2e1e);
    const uint16_t pants = rgb565(0x151f28);
    const uint16_t shoe = rgb565(0x0a0a0a);
    const uint16_t eyeC = rgb565(0xff6600);
    const uint16_t hat = rgb565(0x111111);

    // Head
    sprite_3d->createSphere(0, height - hr, 0, hr, 4, skin);

    // Top hat: wide flat brim + tall crown
    sprite_3d->createCube(0, height - hr * 0.1f, 0, tw * 1.6f, hr * 0.18f, tw * 1.6f, hat);
    sprite_3d->createCube(0, height + hr * 0.85f, 0, tw * 0.9f, hr * 1.40f, tw * 0.9f, hat);

    // Long coat torso
    sprite_3d->createCube(0, lh + th / 2, 0, tw, th, tw * 0.75f, coat);

    // Arms — one high (raised), one low (dangling)
    sprite_3d->createCube(-tw * 0.8f, lh + th * 0.75f, 0, aw, al, aw, skin);
    sprite_3d->createCube(tw * 0.8f, lh + th * 0.25f, 0, aw, al, aw, skin);

    // Long legs
    sprite_3d->createCube(-lw * 0.65f, lh / 2, 0, lw, lh, lw, pants);
    sprite_3d->createCube(lw * 0.65f, lh / 2, 0, lw, lh, lw, pants);

    // Shoes (long, pointy)
    sprite_3d->createCube(-lw * 0.65f, lw * 0.15f, lw * 0.2f, lw * 1.0f, lw * 0.3f, lw * 1.5f, shoe);
    sprite_3d->createCube(lw * 0.65f, lw * 0.15f, lw * 0.2f, lw * 1.0f, lw * 0.3f, lw * 1.5f, shoe);

    // Orange glowing eyes
    sprite_3d->createCube(-hr * 0.32f, height - hr * 0.82f, hr * 0.9f, hr * 0.26f, hr * 0.19f, hr * 0.1f, eyeC);
    sprite_3d->createCube(hr * 0.32f, height - hr * 0.82f, hr * 0.9f, hr * 0.26f, hr * 0.19f, hr * 0.1f, eyeC);
}

void Enemy::update(Game *game)
{
    if (this->state == ENTITY_DEAD)
    {
        return; // don't update if dead
    }

    // Only move if start and end positions are different
    if (start_position.x == end_position.x && start_position.y == end_position.y)
    {
        return; // No movement needed
    }

    this->elapsed_attack_timer += 1.0f; // Increment attack timer
    this->elapsed_move_timer += 1.0f;   // Increment move timer

    if (this->move_timer > 0 && this->elapsed_move_timer < this->move_timer)
    {
        return; // still in move cooldown
    }
    this->move_timer = 0; // reset move timer

    const float DISTANCE_THRESHOLD = 0.2f; // Minimum distance to consider "reached"

    // move towards the player during the night
    // go back home during the day?
    // for now this should make sure the sprite follows the player
    if (state == ENTITY_MOVING_TO_END)
    {
        // Update the enemy's state
        // always follow the main player
        this->end_position = getPlayerPosition(game);

        float dx = end_position.x - position.x;
        float dy = end_position.y - position.y;
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance > DISTANCE_THRESHOLD)
        {
            // Normalize direction and apply speed
            float move_x = (dx / distance) * this->speed;
            float move_y = (dy / distance) * this->speed;

            // Don't overshoot the target
            if (distance < this->speed)
            {
                move_x = dx;
                move_y = dy;
            }

            position_set(position.x + move_x, position.y + move_y);

            // Update 3D sprite position and rotation to match camera direction
            if (has3DSprite())
            {
                update3DSpritePosition();
                direction.x = dx / distance;
                direction.y = dy / distance;
                float rotation_angle = atan2f(direction.y, direction.x) - M_PI_2;
                set3DSpriteRotation(rotation_angle);
            }
        }
        else
        {
            // Snap to exact position when close enough
            position_set(end_position);
            // state = ENTITY_MOVING_TO_START;
        }
    }
    else if (state == ENTITY_MOVING_TO_START)
    {
        float dx = start_position.x - position.x;
        float dy = start_position.y - position.y;
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance > DISTANCE_THRESHOLD)
        {
            // Normalize direction and apply speed
            float move_x = (dx / distance) * this->speed;
            float move_y = (dy / distance) * this->speed;

            // Don't overshoot the target
            if (distance < this->speed)
            {
                move_x = dx;
                move_y = dy;
            }

            position_set(position.x + move_x, position.y + move_y);

            // Update 3D sprite position and rotation to match camera direction
            if (has3DSprite())
            {
                update3DSpritePosition();
                direction.x = dx / distance;
                direction.y = dy / distance;
                float rotation_angle = atan2f(direction.y, direction.x) - M_PI_2;
                set3DSpriteRotation(rotation_angle);
            }
        }
        else
        {
            // Snap to exact position when close enough
            position_set(start_position);
            // state = ENTITY_MOVING_TO_END;
        }
    }
}