#pragma once
#include "triangle3d.hpp"
#include "vector.hpp"
#include "engine_config.hpp"
#include "math.h"

#include ENGINE_MEM_INCLUDE

#define MAX_TRIANGLES_PER_SPRITE 64

typedef enum
{
    SPRITE_HUMANOID = 0,
    SPRITE_TREE = 1,
    SPRITE_HOUSE = 2,
    SPRITE_PILLAR = 3,
    SPRITE_CUSTOM = 4
} SpriteType;

class Sprite3D
{
private:
    Triangle3D *triangles[MAX_TRIANGLES_PER_SPRITE];
    uint8_t triangle_count;
    Vector position;
    float rotation_y;
    float scale_factor;
    SpriteType type;
    bool active;

public:
    Sprite3D();
    ~Sprite3D();

    void addTriangle(const Triangle3D &triangle);
    void addTriangle(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, uint16_t color = 0x0000);
    void clearTriangles();
    void createHumanoid(float height = 1.8f, uint16_t color = 0x0000);
    void createTree(float height = 2.0f, uint16_t color = 0x0000);
    void createHouse(float width = 2.0f, float height = 2.5f, uint16_t color = 0x0000);
    void createPillar(float height = 3.0f, float radius = 0.3f, uint16_t color = 0x0000);
    void createWall(float x, float y, float z, float width = 4.0f, float height = 1.5f, float depth = 0.2f, uint16_t color = 0x0000);
    void createCube(float x, float y, float z, float width, float height, float depth, uint16_t color = 0x0000);
    void createCylinder(float x, float y, float z, float radius, float height, uint8_t segments, uint16_t color = 0x0000);
    void createSphere(float x, float y, float z, float radius, uint8_t segments, uint16_t color = 0x0000);
    void createTriangularPrism(float x, float y, float z, float width, float height, float depth, uint16_t color = 0x0000);
    Vector getPosition() const { return position; }
    float getRotation() const { return rotation_y; }
    float getScale() const { return scale_factor; }
    Triangle3D getTransformedTriangle(uint8_t index, const Vector &camera_pos) const;
    uint8_t getTriangleCount() const { return triangle_count; }
    SpriteType getType() const { return type; }
    bool isActive() const { return active; }
    void setActive(bool state) { active = state; }
    void setPosition(Vector pos) { position = pos; }
    void setRotation(float rot) { rotation_y = rot; }
    void setScale(float scale) { scale_factor = scale; }
    void initializeAsHouse(Vector pos, float width, float height, float rot, uint16_t color = 0x0000);
    void initializeAsHumanoid(Vector pos, float height, float rot, uint16_t color = 0x0000);
    void initializeAsPillar(Vector pos, float height, float radius, uint16_t color = 0x0000);
    void initializeAsTree(Vector pos, float height, uint16_t color = 0x0000);
};
