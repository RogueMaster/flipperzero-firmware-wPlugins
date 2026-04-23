#include "projectile.hpp"
#include "pico-game-engine/engine/game.hpp"
#include <math.h>
#include "general.hpp"
#include "player.hpp"
#include "game.hpp"

Projectile::Projectile(ProjectileType type, float height, Vector position) : Entity("Projectile", ENTITY_ICON, position, Vector(0, height), nullptr)
{
    damage = 0.0;
    inMotion = false;
    setProjectileType(type);
    this->sprite_3d_type = SPRITE_3D_CUSTOM;
    switch (type)
    {
    case PROJECTILE_BULLET:
        makeBullet(height);
        speed = SPEED_SCALE(0.5f);
        break;
    case PROJECTILE_ARROW:
        makeArrow(height);
        speed = SPEED_SCALE(0.3f);
        break;
    case PROJECTILE_ROCKET:
        makeRocket(height);
        speed = SPEED_SCALE(0.1f);
        break;
    default:
        speed = SPEED_SCALE(0.05f);
        break;
    };
    if (sprite_3d)
        sprite_3d->setWireframe(WIREFRAME_ENABLED);
}

Projectile::~Projectile()
{
    // nothing to do...
}

void Projectile::collision(Entity *other, Game *game)
{
    if (other->type == ENTITY_PLAYER)
    {
        // projectiles should not collide with the player who fired them, so ignore
        return;
    }

    if (other->type == ENTITY_ENEMY)
    {
        other->health -= this->damage;
        other->move_timer = SPEED_SCALE(50.0f);      // add a short move cooldown to enemies hit by projectiles
        other->elapsed_move_timer = 0;               // reset move timer to start cooldown immediately
        const float enemyStrength = other->strength; // save before possible removal
        if (other->health <= 0)
        {
            other->health = 0;
            other->state = ENTITY_DEAD;

            // remove the enemy from the level
            game->current_level->entity_remove(other);

            Player *p = static_cast<Player *>(getPlayer(game));
            if (p && p->getGhoulsGame())
            {
                p->getGhoulsGame()->onGhoulDied();
            }
        }
        Player *player = static_cast<Player *>(getPlayer(game));
        if (player)
        {
            player->increaseXP(enemyStrength); // increase player XP by the enemy's strength
        }
    }
    // delete the projectile on collision
    this->position_set(-100, -100); // move it off-screen first to avoid multiple collisions
    this->inMotion = false;
    this->is_active = false;
    this->is_visible = false;
}

Entity *Projectile::getPlayer(Game *game) const
{
    for (int i = 0; i < game->current_level->getEntityCount(); i++)
    {
        Entity *e = game->current_level->getEntity(i);
        if (e && e->type == ENTITY_PLAYER && e->is_player)
        {
            return e;
        }
    }
    return nullptr;
}

void Projectile::makeBullet(float height)
{
    sprite_3d = ENGINE_MEM_NEW Sprite3D();
    if (!sprite_3d)
        return;
    sprite_3d->clearTriangles();
    sprite_3d->setActive(true);

    const float s = height / 2.0f;
    const uint16_t brass = rgb565(0xcc8833);
    const uint16_t lead = rgb565(0x888899);
    const uint16_t primer = rgb565(0xaa5522);

    // Casing body
    sprite_3d->createCube(0, 0.10f * s, -0.04f * s, 0.06f * s, 0.06f * s, 0.14f * s, brass);

    // Casing rim (base)
    sprite_3d->createCube(0, 0.10f * s, -0.12f * s, 0.07f * s, 0.07f * s, 0.02f * s, rgb565(0xaa7722));

    // Primer (rear center)
    sprite_3d->createCube(0, 0.10f * s, -0.13f * s, 0.03f * s, 0.03f * s, 0.01f * s, primer);

    // Bullet head (ogive shape)
    sprite_3d->createCube(0, 0.10f * s, 0.06f * s, 0.055f * s, 0.055f * s, 0.06f * s, lead);
    sprite_3d->createCube(0, 0.10f * s, 0.11f * s, 0.04f * s, 0.04f * s, 0.05f * s, lead);
    sprite_3d->createCube(0, 0.10f * s, 0.15f * s, 0.025f * s, 0.025f * s, 0.04f * s, lead);

    // Cannelure (crimp groove)
    sprite_3d->createCube(0, 0.10f * s, 0.03f * s, 0.062f * s, 0.062f * s, 0.01f * s, rgb565(0x996633));

    // Tip point (cone from triangles)
    const float tipZ = 0.20f * s, baseZ = 0.17f * s, r = 0.018f * s, cy = 0.10f * s;
    for (int i = 0; i < 6; i++)
    {
        float a1 = i * 2.0f * M_PI / 6;
        float a2 = (i + 1) * 2.0f * M_PI / 6;
        float x1 = r * cosf(a1), y1 = r * sinf(a1);
        float x2 = r * cosf(a2), y2 = r * sinf(a2);
        sprite_3d->addTriangle(x1, cy + y1, baseZ, x2, cy + y2, baseZ, 0, cy, tipZ, lead);
    }
}

void Projectile::makeArrow(float height)
{
    sprite_3d = ENGINE_MEM_NEW Sprite3D();
    if (!sprite_3d)
        return;
    sprite_3d->clearTriangles();
    sprite_3d->setActive(true);

    const float s = height / 2.0f;
    const uint16_t shaft = rgb565(0x8b6914);
    const uint16_t tip = rgb565(0x888899);
    const uint16_t fletchR = rgb565(0xcc2222);
    const uint16_t fletchW = rgb565(0xeeeeee);
    const float cy = 0.10f * s;

    // Shaft (long thin rod)
    sprite_3d->createCube(0, cy, 0.0f, 0.018f * s, 0.018f * s, 0.80f * s, shaft);

    // Arrowhead (diamond shape)
    const float ahw = 0.04f * s, ahh = 0.035f * s, aZ = 0.40f * s, atipZ = 0.50f * s;
    sprite_3d->addTriangle(-ahw, cy, aZ, ahw, cy, aZ, 0, cy, atipZ, tip);
    sprite_3d->addTriangle(0, cy - ahh, aZ, 0, cy + ahh, aZ, 0, cy, atipZ, tip);
    sprite_3d->addTriangle(-ahw, cy, aZ, 0, cy - ahh, aZ, 0, cy, atipZ, tip);
    sprite_3d->addTriangle(ahw, cy, aZ, 0, cy + ahh, aZ, 0, cy, atipZ, tip);

    // Arrowhead base collar
    sprite_3d->createCube(0, cy, 0.395f * s, 0.03f * s, 0.03f * s, 0.02f * s, rgb565(0x555555));

    // Fletching (3 vanes at rear, 120deg apart)
    const float fLen = 0.10f * s, fH = 0.04f * s, fZ = -0.32f * s;

    // Vane 1 (top - red)
    sprite_3d->addTriangle(0, cy, fZ, 0, cy, fZ + fLen, 0, cy + fH, fZ + fLen * 0.5f, fletchR);
    sprite_3d->addTriangle(0, cy, fZ + fLen, 0, cy, fZ, 0, cy + fH, fZ + fLen * 0.5f, fletchR);

    // Vane 2 (bottom-left - white)
    sprite_3d->addTriangle(0, cy, fZ, 0, cy, fZ + fLen, -fH * 0.87f, cy - fH * 0.5f, fZ + fLen * 0.5f, fletchW);
    sprite_3d->addTriangle(0, cy, fZ + fLen, 0, cy, fZ, -fH * 0.87f, cy - fH * 0.5f, fZ + fLen * 0.5f, fletchW);

    // Vane 3 (bottom-right - white)
    sprite_3d->addTriangle(0, cy, fZ, 0, cy, fZ + fLen, fH * 0.87f, cy - fH * 0.5f, fZ + fLen * 0.5f, fletchW);
    sprite_3d->addTriangle(0, cy, fZ + fLen, 0, cy, fZ, fH * 0.87f, cy - fH * 0.5f, fZ + fLen * 0.5f, fletchW);

    // Nock (notch at rear)
    sprite_3d->createCube(0, cy, -0.405f * s, 0.015f * s, 0.03f * s, 0.02f * s, rgb565(0xdddddd));
    sprite_3d->createCube(0, cy, -0.41f * s, 0.006f * s, 0.04f * s, 0.01f * s, rgb565(0xdddddd));
}

void Projectile::makeRocket(float height)
{
    sprite_3d = ENGINE_MEM_NEW Sprite3D();
    if (!sprite_3d)
        return;
    sprite_3d->clearTriangles();
    sprite_3d->setActive(true);

    const float s = height / 2.0f;
    const uint16_t body = rgb565(0xcc3322);
    const uint16_t fin = rgb565(0xaaaaaa);
    const uint16_t nose = rgb565(0xdddddd);
    const uint16_t exhaust = rgb565(0xff6600);
    const uint16_t ring = rgb565(0x888888);
    const float cy = 0.10f * s;

    // Main body
    sprite_3d->createCube(0, cy, 0.14f * s, 0.12f * s, 0.12f * s, 0.62f * s, body);

    // Warhead shoulder taper
    sprite_3d->createCube(0, cy, 0.48f * s, 0.10f * s, 0.10f * s, 0.10f * s, rgb565(0xbb2211));
    sprite_3d->createCube(0, cy, 0.55f * s, 0.07f * s, 0.07f * s, 0.08f * s, rgb565(0xaa1100));

    // Nose cone (stacked tapered cubes)
    sprite_3d->createCube(0, cy, 0.62f * s, 0.05f * s, 0.05f * s, 0.07f * s, nose);
    sprite_3d->createCube(0, cy, 0.68f * s, 0.03f * s, 0.03f * s, 0.05f * s, nose);
    sprite_3d->createCube(0, cy, 0.72f * s, 0.015f * s, 0.015f * s, 0.03f * s, nose);

    // Nose tip (cone from triangles)
    const float tipZ = 0.76f * s, baseZ = 0.745f * s, r = 0.012f * s;
    for (int i = 0; i < 6; i++)
    {
        float a1 = i * 2.0f * M_PI / 6, a2 = (i + 1) * 2.0f * M_PI / 6;
        sprite_3d->addTriangle(r * cosf(a1), cy + r * sinf(a1), baseZ, r * cosf(a2), cy + r * sinf(a2), baseZ, 0, cy, tipZ, nose);
    }

    // Booster section (wider rear)
    sprite_3d->createCube(0, cy, -0.22f * s, 0.14f * s, 0.14f * s, 0.18f * s, rgb565(0x882211));

    // Engine nozzle throat
    sprite_3d->createCube(0, cy, -0.32f * s, 0.10f * s, 0.10f * s, 0.04f * s, ring);

    // Nozzle bell (wider)
    sprite_3d->createCube(0, cy, -0.37f * s, 0.13f * s, 0.13f * s, 0.06f * s, ring);
    sprite_3d->createCube(0, cy, -0.41f * s, 0.16f * s, 0.16f * s, 0.04f * s, ring);

    // Exhaust glow center
    sprite_3d->createCube(0, cy, -0.44f * s, 0.06f * s, 0.06f * s, 0.04f * s, exhaust);
    sprite_3d->createCube(0, cy, -0.47f * s, 0.03f * s, 0.03f * s, 0.04f * s, rgb565(0xffcc00));

    // 4 stabiliser fins (cruciform)
    const float fW = 0.04f * s, fH = 0.14f * s, fZ = -0.18f * s;

    // top & bottom fins
    sprite_3d->createCube(0, cy + 0.13f * s, fZ, fW, fH, fW * 1.5f, fin);
    sprite_3d->createCube(0, cy - 0.13f * s, fZ, fW, fH, fW * 1.5f, fin);

    // left & right fins
    sprite_3d->createCube(-0.13f * s, cy, fZ, fH, fW, fW * 1.5f, fin);
    sprite_3d->createCube(0.13f * s, cy, fZ, fH, fW, fW * 1.5f, fin);

    // Fin root fillets
    sprite_3d->createCube(0, cy + 0.065f * s, fZ + 0.02f * s, fW * 0.6f, 0.04f * s, fW * 2.0f, rgb565(0x993322));
    sprite_3d->createCube(0, cy - 0.065f * s, fZ + 0.02f * s, fW * 0.6f, 0.04f * s, fW * 2.0f, rgb565(0x993322));
    sprite_3d->createCube(-0.065f * s, cy, fZ + 0.02f * s, 0.04f * s, fW * 0.6f, fW * 2.0f, rgb565(0x993322));
    sprite_3d->createCube(0.065f * s, cy, fZ + 0.02f * s, 0.04f * s, fW * 0.6f, fW * 2.0f, rgb565(0x993322));

    // Band markings
    sprite_3d->createCube(0, cy, 0.32f * s, 0.121f * s, 0.121f * s, 0.025f * s, rgb565(0xffcc00));
    sprite_3d->createCube(0, cy, -0.02f * s, 0.121f * s, 0.121f * s, 0.025f * s, rgb565(0xffcc00));
}

void Projectile::setDamage(float damage)
{
    this->damage = damage;
}

void Projectile::setMotion(bool inMotion)
{
    this->inMotion = inMotion;
}

void Projectile::setProjectileType(ProjectileType type)
{
    this->projectileType = type;
}

void Projectile::setSpeed(float speed)
{
    this->speed = speed;
}

void Projectile::update(Game *game)
{
    if (!inMotion || !is_active || !game)
        return;

    // continue to move
    position.x += direction.x * speed;
    position.y += direction.y * speed;

    // if position goes out of bounds of the game world, deactivate the projectile
    if (position.x < 0 || position.x > game->size.x || position.y < 0 || position.y > game->size.y)
    {
        this->position_set(-100, -100);
        inMotion = false;
        is_active = false;
        is_visible = false;
        return;
    }

    // if projectile has traveled its maximum range, deactivate it
    float dx = position.x - start_position.x;
    float dy = position.y - start_position.y;
    if (dx * dx + dy * dy >= (float)(FIELD_OF_VIEW_SQUARED))
    {
        this->position_set(-100, -100);
        inMotion = false;
        is_active = false;
        is_visible = false;
        return;
    }

    if (sprite_3d)
        update3DSpritePosition();
}