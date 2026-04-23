#include "weapon.hpp"
#include "pico-game-engine/engine/game.hpp"
#include "general.hpp"

Weapon::Weapon(WeaponType type, float height, Vector position) : Entity("Weapon", ENTITY_NPC, position, Vector(0, height), nullptr)
{
    this->currentProjectile = nullptr;
    this->held = false;
    this->cooldown = 0;
    this->weaponType = type;
    //
    sprite_3d_type = SPRITE_3D_CUSTOM;
    sprite_3d = ENGINE_MEM_NEW Sprite3D();
    if (!sprite_3d)
    {
        ENGINE_LOG_INFO("[Weapon:Weapon] Failed to create Sprite3D instance for weapon.");
        return;
    }
    // follow same init setps as Sprite3D::initializeAsHumanoid
    sprite_3d->setPosition(position);
    sprite_3d->setRotation(sprite_rotation);
    sprite_3d->clearTriangles();
    sprite_3d->setActive(true);
    // base stats
    // each enemy starts with 100 health
    switch (type)
    {
    case WEAPON_RIFLE:
        damage = 15.0f;
        ammo = 30;
        cooldown_max = SPEED_SCALE(20); // 20 ticks between shots
        projectileType = PROJECTILE_BULLET;
        makeRifle(height);
        break;
    case WEAPON_SHOTGUN:
        damage = 20.0f;
        ammo = 10;
        cooldown_max = SPEED_SCALE(40); // 40 ticks between shots
        projectileType = PROJECTILE_BULLET;
        makeShotgun(height);
        break;
    case WEAPON_ROCKET_LAUNCHER:
        damage = 50.0f;
        ammo = 5;
        cooldown_max = SPEED_SCALE(100); // 100 ticks between shots
        projectileType = PROJECTILE_ROCKET;
        makeRocketLauncher(height);
        break;
    case WEAPON_CROSSBOW:
        damage = 35.0f;
        cooldown_max = SPEED_SCALE(60); // 60 ticks between shots
        projectileType = PROJECTILE_ARROW;
        ammo = 15;
        makeCrossbow(height);
        break;
    default:
        damage = 0.0f;
        ammo = 0;
        cooldown_max = 0;
        projectileType = PROJECTILE_NONE;
        break;
    };
    sprite_3d->setWireframe(WIREFRAME_ENABLED);
}

Weapon::~Weapon()
{
    if (currentProjectile)
    {
        // no need to delete, level already did if allocated and added to level
        currentProjectile = nullptr;
    }
}

void Weapon::addAmmo(uint16_t amount)
{
    ammo += amount;
}

bool Weapon::canFire() const
{
    return cooldown == 0 && ammo > 0;
}

bool Weapon::fire(Level *level)
{
    if (!level || !canFire() || currentProjectile)
    {
        return false;
    }
    // create and add projectile to level
    currentProjectile = ENGINE_MEM_NEW Projectile(projectileType);
    if (!currentProjectile)
    {
        ENGINE_LOG_INFO("[Weapon:fire] Failed to create projectile for weapon: %s", name);
        return false;
    }
    currentProjectile->setDamage(damage);
    currentProjectile->direction = this->direction;
    currentProjectile->start_position = this->position;
    currentProjectile->position = this->position;
    currentProjectile->setMotion(true);
    if (currentProjectile->has3DSprite())
    {
        currentProjectile->update3DSpritePosition();
        currentProjectile->set3DSpriteRotation(this->sprite_rotation);
    }
    level->entity_add(currentProjectile);
    ammo--;
    cooldown = cooldown_max;
    return true;
}

uint16_t Weapon::getAmmo() const
{
    return ammo;
}

float Weapon::getDamage() const
{
    return damage;
}

bool Weapon::isHeld() const
{
    return held;
}

WeaponType Weapon::getWeaponType() const
{
    return weaponType;
}

void Weapon::makeCrossbow(float height)
{
    this->name = "Crossbow";
    const float s = height / 2.0f;
    const uint16_t wood = rgb565(0x6b4a22);
    const uint16_t metalC = rgb565(0x555566);
    const uint16_t str = rgb565(0xaaaaaa);
    const uint16_t darkW = rgb565(0x4a3018);

    // Rail / tiller (long stock)
    sprite_3d->createCube(0, 0.30f * s, 0.02f * s, 0.06f * s, 0.06f * s, 0.60f * s, wood);
    // Stock (rear)
    sprite_3d->createCube(0, 0.28f * s, -0.30f * s, 0.08f * s, 0.08f * s, 0.12f * s, darkW);
    // Stock butt
    sprite_3d->createCube(0, 0.26f * s, -0.38f * s, 0.10f * s, 0.10f * s, 0.06f * s, darkW);
    // Grip
    sprite_3d->createCube(0, 0.14f * s, -0.15f * s, 0.08f * s, 0.22f * s, 0.10f * s, wood);
    // Trigger mechanism
    sprite_3d->createCube(0, 0.26f * s, -0.04f * s, 0.05f * s, 0.05f * s, 0.08f * s, metalC);
    // Trigger
    sprite_3d->createCube(0, 0.20f * s, -0.02f * s, 0.025f * s, 0.06f * s, 0.02f * s, rgb565(0x111111));
    // Prod / bow limb (horizontal bar at front)
    sprite_3d->createCube(0, 0.31f * s, 0.30f * s, 0.80f * s, 0.03f * s, 0.05f * s, metalC);
    // Limb tips (reinforced)
    sprite_3d->createCube(-0.39f * s, 0.31f * s, 0.30f * s, 0.06f * s, 0.05f * s, 0.06f * s, rgb565(0x444455));
    sprite_3d->createCube(0.39f * s, 0.31f * s, 0.30f * s, 0.06f * s, 0.05f * s, 0.06f * s, rgb565(0x444455));
    // Limb curve (slight forward bend at tips)
    sprite_3d->createCube(-0.40f * s, 0.31f * s, 0.34f * s, 0.04f * s, 0.03f * s, 0.04f * s, metalC);
    sprite_3d->createCube(0.40f * s, 0.31f * s, 0.34f * s, 0.04f * s, 0.03f * s, 0.04f * s, metalC);
    // Bowstring (left side)
    sprite_3d->addTriangle(-0.41f * s, 0.30f * s, 0.34f * s, 0.0f, 0.30f * s, 0.16f * s, 0.0f, 0.32f * s, 0.16f * s, str);
    // Bowstring (right side)
    sprite_3d->addTriangle(0.41f * s, 0.30f * s, 0.34f * s, 0.0f, 0.30f * s, 0.16f * s, 0.0f, 0.32f * s, 0.16f * s, str);
    // Bolt groove (top rail)
    sprite_3d->createCube(0, 0.34f * s, 0.10f * s, 0.02f * s, 0.01f * s, 0.30f * s, rgb565(0x333333));
    // Stirrup at front (foot brace)
    sprite_3d->createCube(0, 0.20f * s, 0.32f * s, 0.14f * s, 0.04f * s, 0.04f * s, metalC);
    sprite_3d->createCube(-0.06f * s, 0.14f * s, 0.32f * s, 0.04f * s, 0.14f * s, 0.04f * s, metalC);
    sprite_3d->createCube(0.06f * s, 0.14f * s, 0.32f * s, 0.04f * s, 0.14f * s, 0.04f * s, metalC);
    sprite_3d->createCube(0, 0.08f * s, 0.32f * s, 0.16f * s, 0.04f * s, 0.04f * s, metalC);
}

void Weapon::makeRifle(float height)
{
    this->name = "Rifle";
    const float s = height / 2.0f;
    const uint16_t metal = rgb565(0x2a2a3a);
    const uint16_t wood = rgb565(0x5c3d11);
    const uint16_t darkM = rgb565(0x1a1a2a);
    const uint16_t sight = rgb565(0x555566);

    // Stock (wooden butt)
    sprite_3d->createCube(0, 0.30f * s, -0.50f * s, 0.08f * s, 0.16f * s, 0.34f * s, wood);
    // Stock pad
    sprite_3d->createCube(0, 0.30f * s, -0.68f * s, 0.10f * s, 0.18f * s, 0.04f * s, rgb565(0x222222));
    // Receiver / body
    sprite_3d->createCube(0, 0.34f * s, -0.02f * s, 0.10f * s, 0.12f * s, 0.38f * s, metal);
    // Barrel
    sprite_3d->createCube(0, 0.36f * s, 0.44f * s, 0.04f * s, 0.04f * s, 0.52f * s, darkM);
    // Muzzle brake
    sprite_3d->createCube(0, 0.36f * s, 0.72f * s, 0.06f * s, 0.06f * s, 0.06f * s, rgb565(0x222222));
    // Magazine
    sprite_3d->createCube(0, 0.18f * s, 0.02f * s, 0.07f * s, 0.18f * s, 0.06f * s, metal);
    // Grip
    sprite_3d->createCube(0, 0.14f * s, -0.14f * s, 0.08f * s, 0.22f * s, 0.10f * s, wood);
    // Trigger
    sprite_3d->createCube(0, 0.18f * s, -0.06f * s, 0.025f * s, 0.06f * s, 0.02f * s, rgb565(0x111111));
    // Scope body
    sprite_3d->createCube(0, 0.44f * s, 0.06f * s, 0.04f * s, 0.06f * s, 0.20f * s, sight);
    // Scope front lens
    sprite_3d->createCube(0, 0.44f * s, 0.17f * s, 0.05f * s, 0.05f * s, 0.02f * s, rgb565(0x224488));
    // Scope rear lens
    sprite_3d->createCube(0, 0.44f * s, -0.04f * s, 0.045f * s, 0.045f * s, 0.02f * s, rgb565(0x224488));
    // Front sight post
    sprite_3d->createCube(0, 0.42f * s, 0.68f * s, 0.02f * s, 0.06f * s, 0.02f * s, rgb565(0xff4400));
    // Handguard
    sprite_3d->createCube(0, 0.32f * s, 0.22f * s, 0.09f * s, 0.08f * s, 0.18f * s, rgb565(0x333333));
}

void Weapon::makeRocketLauncher(float height)
{
    this->name = "Rocket Launcher";
    const float s = height / 2.0f;
    const uint16_t tube = rgb565(0x4a5a3a);
    const uint16_t grip = rgb565(0x222222);
    const uint16_t sightC = rgb565(0x666666);

    // Main tube body
    sprite_3d->createCube(0, 0.40f * s, 0.0f, 0.18f * s, 0.18f * s, 1.30f * s, tube);
    // Inner bore (darker, inset slightly at front)
    sprite_3d->createCube(0, 0.40f * s, 0.68f * s, 0.12f * s, 0.12f * s, 0.04f * s, rgb565(0x1a2a1a));
    // Front flare / blast shield
    sprite_3d->createCube(0, 0.40f * s, 0.66f * s, 0.22f * s, 0.22f * s, 0.06f * s, rgb565(0x3a4a2a));
    // Rear exhaust flare
    sprite_3d->createCube(0, 0.40f * s, -0.64f * s, 0.20f * s, 0.20f * s, 0.06f * s, rgb565(0x3a4a2a));
    // Rear exhaust bore
    sprite_3d->createCube(0, 0.40f * s, -0.66f * s, 0.14f * s, 0.14f * s, 0.04f * s, rgb565(0x1a2a1a));
    // Grip / pistol handle
    sprite_3d->createCube(0, 0.18f * s, -0.10f * s, 0.08f * s, 0.28f * s, 0.10f * s, grip);
    // Trigger guard
    sprite_3d->createCube(0, 0.22f * s, -0.02f * s, 0.06f * s, 0.03f * s, 0.10f * s, grip);
    // Trigger
    sprite_3d->createCube(0, 0.20f * s, -0.02f * s, 0.025f * s, 0.05f * s, 0.02f * s, rgb565(0x111111));
    // Shoulder rest
    sprite_3d->createCube(0, 0.34f * s, -0.58f * s, 0.16f * s, 0.14f * s, 0.12f * s, rgb565(0x333333));
    // Shoulder pad
    sprite_3d->createCube(0, 0.34f * s, -0.65f * s, 0.18f * s, 0.16f * s, 0.04f * s, rgb565(0x1a1a1a));
    // Iron sight (front)
    sprite_3d->createCube(0, 0.54f * s, 0.30f * s, 0.03f * s, 0.06f * s, 0.03f * s, sightC);
    // Iron sight (rear)
    sprite_3d->createCube(0, 0.54f * s, -0.10f * s, 0.05f * s, 0.06f * s, 0.03f * s, sightC);
    // Carrying handle
    sprite_3d->createCube(0, 0.54f * s, 0.10f * s, 0.04f * s, 0.04f * s, 0.24f * s, rgb565(0x333333));
    sprite_3d->createCube(-0.02f * s, 0.52f * s, -0.02f * s, 0.04f * s, 0.04f * s, 0.04f * s, rgb565(0x333333));
    sprite_3d->createCube(-0.02f * s, 0.52f * s, 0.22f * s, 0.04f * s, 0.04f * s, 0.04f * s, rgb565(0x333333));
    // Warning stripe
    sprite_3d->createCube(0.091f * s, 0.40f * s, 0.30f * s, 0.005f * s, 0.10f * s, 0.08f * s, rgb565(0xccaa00));
}

void Weapon::makeShotgun(float height)
{
    this->name = "Shotgun";
    const float s = height / 2.0f;
    const uint16_t metal = rgb565(0x3a3a44);
    const uint16_t wood = rgb565(0x6b4a22);
    const uint16_t darkM = rgb565(0x222233);

    // Stock
    sprite_3d->createCube(0, 0.28f * s, -0.55f * s, 0.10f * s, 0.16f * s, 0.40f * s, wood);
    // Stock pad
    sprite_3d->createCube(0, 0.28f * s, -0.76f * s, 0.12f * s, 0.18f * s, 0.04f * s, rgb565(0x1a1a1a));
    // Receiver
    sprite_3d->createCube(0, 0.32f * s, -0.06f * s, 0.12f * s, 0.14f * s, 0.34f * s, metal);
    // Barrel (wider than rifle)
    sprite_3d->createCube(0, 0.35f * s, 0.44f * s, 0.07f * s, 0.07f * s, 0.60f * s, darkM);
    // Barrel rib (top rail)
    sprite_3d->createCube(0, 0.40f * s, 0.44f * s, 0.03f * s, 0.02f * s, 0.58f * s, metal);
    // Pump grip (forend)
    sprite_3d->createCube(0, 0.28f * s, 0.30f * s, 0.11f * s, 0.10f * s, 0.20f * s, wood);
    // Grip
    sprite_3d->createCube(0, 0.14f * s, -0.16f * s, 0.09f * s, 0.20f * s, 0.10f * s, wood);
    // Trigger guard
    sprite_3d->createCube(0, 0.19f * s, -0.06f * s, 0.06f * s, 0.04f * s, 0.12f * s, metal);
    // Trigger
    sprite_3d->createCube(0, 0.17f * s, -0.04f * s, 0.025f * s, 0.05f * s, 0.02f * s, rgb565(0x111111));
    // Front bead sight
    sprite_3d->createCube(0, 0.42f * s, 0.72f * s, 0.02f * s, 0.03f * s, 0.02f * s, rgb565(0xff4400));
}

void Weapon::reset(Level *level)
{
    cooldown = 0;
    switch (weaponType)
    {
    case WEAPON_RIFLE:
        ammo = 30;
        break;
    case WEAPON_SHOTGUN:
        ammo = 10;
        break;
    case WEAPON_ROCKET_LAUNCHER:
        ammo = 5;
        break;
    case WEAPON_CROSSBOW:
        ammo = 15;
        break;
    default:
        ammo = 0;
        break;
    };

    if (currentProjectile)
    {
        level->entity_remove(currentProjectile);
        // no need to delete, level already did
        currentProjectile = nullptr;
    }
}

void Weapon::setAmmo(uint16_t ammo)
{
    this->ammo = ammo;
}

void Weapon::setCooldown(uint16_t cooldown)
{
    this->cooldown_max = cooldown;
}

void Weapon::setDamage(float damage)
{
    this->damage = damage;
    if (currentProjectile)
    {
        currentProjectile->setDamage(damage);
    }
}

void Weapon::setHeld(bool held)
{
    this->held = held;
}

void Weapon::setWeaponType(WeaponType type)
{
    this->weaponType = type;
}

void Weapon::update(Game *game)
{
    if (cooldown > 0)
        cooldown--;

    // we'll add rotation/animation here like the enemy sprites

    Level *currentLevel = game->current_level;
    if (!currentLevel)
        return;

    // check for dead projectile
    if (currentProjectile != nullptr && !currentProjectile->is_active)
    {
        currentLevel->entity_remove(currentProjectile);
        // no need to delete, level already did
        currentProjectile = nullptr;
    }
}
