#include "sprite3d.hpp"

// Shade an RGB565 color by a factor (< 1.0 darkens, > 1.0 lightens)
static uint16_t shadeColor565(uint16_t color, float factor)
{
    uint8_t r = (uint8_t)(((color >> 11) & 0x1F) * factor);
    uint8_t g = (uint8_t)(((color >> 5) & 0x3F) * factor);
    uint8_t b = (uint8_t)((color & 0x1F) * factor);
    if (r > 0x1F)
        r = 0x1F;
    if (g > 0x3F)
        g = 0x3F;
    if (b > 0x1F)
        b = 0x1F;
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | b;
}

Sprite3D::Sprite3D() : triangle_count(0), position(Vector(0, 0)), rotation_y(0),
                       scale_factor(1.0f), type(SPRITE_CUSTOM), active(false)
{
    memset(triangles, 0, sizeof(triangles));
}

Sprite3D::~Sprite3D()
{
    clearTriangles();
}

void Sprite3D::addTriangle(const Triangle3D &triangle)
{
    if (triangle_count < MAX_TRIANGLES_PER_SPRITE)
    {
        triangles[triangle_count] = ENGINE_MEM_NEW Triangle3D(triangle);
        triangle_count++;
    }
}

void Sprite3D::addTriangle(float x1, float y1, float z1,
                           float x2, float y2, float z2,
                           float x3, float y3, float z3, uint16_t color)
{
    if (triangle_count < MAX_TRIANGLES_PER_SPRITE)
    {
        triangles[triangle_count] = ENGINE_MEM_NEW Triangle3D(x1, y1, z1, x2, y2, z2, x3, y3, z3, color);
        triangle_count++;
    }
}

void Sprite3D::clearTriangles()
{
    for (uint8_t i = 0; i < triangle_count; i++)
    {
        ENGINE_MEM_DELETE triangles[i];
        triangles[i] = nullptr;
    }
    triangle_count = 0;
}

void Sprite3D::createCube(float x, float y, float z, float width, float height, float depth, uint16_t color)
{
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;

    // Render 4 most important faces (skip top and bottom to save triangles)
    // This gives 8 triangles per cube instead of 12

    // Front face (2 triangles)
    addTriangle(Triangle3D(
        x - hw, y - hh, z + hd,
        x + hw, y - hh, z + hd,
        x + hw, y + hh, z + hd, color));
    addTriangle(Triangle3D(
        x - hw, y - hh, z + hd,
        x + hw, y + hh, z + hd,
        x - hw, y + hh, z + hd, color));

    // Back face (2 triangles)
    addTriangle(Triangle3D(
        x + hw, y - hh, z - hd,
        x - hw, y - hh, z - hd,
        x - hw, y + hh, z - hd, color));
    addTriangle(Triangle3D(
        x + hw, y - hh, z - hd,
        x - hw, y + hh, z - hd,
        x + hw, y + hh, z - hd, color));

    // Right face (2 triangles)
    addTriangle(Triangle3D(
        x + hw, y - hh, z + hd,
        x + hw, y - hh, z - hd,
        x + hw, y + hh, z - hd, color));
    addTriangle(Triangle3D(
        x + hw, y - hh, z + hd,
        x + hw, y + hh, z - hd,
        x + hw, y + hh, z + hd, color));

    // Left face (2 triangles)
    addTriangle(Triangle3D(
        x - hw, y - hh, z - hd,
        x - hw, y - hh, z + hd,
        x - hw, y + hh, z + hd, color));
    addTriangle(Triangle3D(
        x - hw, y - hh, z - hd,
        x - hw, y + hh, z + hd,
        x - hw, y + hh, z - hd, color));
}

void Sprite3D::createCylinder(float x, float y, float z, float radius, float height, uint8_t segments, uint16_t color)
{
    float hh = height * 0.5f;

    // Limit segments to prevent too many triangles
    if (segments > 6)
        segments = 6;

    // Only side faces - no caps to save triangles
    for (uint8_t i = 0; i < segments; i++)
    {
        float angle1 = (float)i * 2.0f * M_PI / segments;
        float angle2 = (float)(i + 1) * 2.0f * M_PI / segments;

        float x1 = x + radius * cosf(angle1);
        float z1 = z + radius * sinf(angle1);
        float x2 = x + radius * cosf(angle2);
        float z2 = z + radius * sinf(angle2);

        // Side face triangles only
        addTriangle(Triangle3D(
            x1, y - hh, z1,
            x2, y - hh, z2,
            x2, y + hh, z2, color));
        addTriangle(Triangle3D(
            x1, y - hh, z1,
            x2, y + hh, z2,
            x1, y + hh, z1, color));
    }
}

void Sprite3D::createHouse(float width, float height, uint16_t color)
{
    clearTriangles();
    type = SPRITE_HOUSE;

    float wall_height = height * 0.7f;
    float roof_height = height * 0.3f;
    float house_width = width * 1.3f;
    float house_depth = width * 1.1f;

    // House base (cube)
    createCube(0, wall_height / 2, 0, house_width, wall_height, house_depth, color);

    // Roof (triangular prism)
    createTriangularPrism(0, wall_height + roof_height / 2, 0, house_width, roof_height, house_depth, shadeColor565(color, 0.6f));
}

void Sprite3D::createHumanoid(float height, uint16_t color)
{
    clearTriangles();
    type = SPRITE_HUMANOID;

    float head_radius = height * 0.12f;
    float torso_width = height * 0.20f;
    float torso_height = height * 0.35f;
    float leg_height = height * 0.45f;
    float arm_length = height * 0.25f;

    // Head (sphere) - positioned at top
    createSphere(0, height - head_radius, 0, head_radius, 4, shadeColor565(color, 1.25f));

    // Torso - positioned in middle, wider and deeper
    createCube(0, leg_height + torso_height / 2, 0, torso_width, torso_height, torso_width * 0.8f, color);

    // Arms - positioned at shoulder level
    float arm_width = torso_width * 0.35f;
    float arm_y = leg_height + torso_height - arm_length / 2;
    const uint16_t arm_color = shadeColor565(color, 0.75f);
    createCube(-torso_width * 0.8f, arm_y, 0, arm_width, arm_length, arm_width, arm_color);
    createCube(torso_width * 0.8f, arm_y, 0, arm_width, arm_length, arm_width, arm_color);

    // Legs - positioned so their bottoms touch ground (y=0)
    float leg_width = torso_width * 0.45f;
    const uint16_t leg_color = shadeColor565(color, 0.55f);
    createCube(-leg_width * 0.7f, leg_height / 2, 0, leg_width, leg_height, leg_width, leg_color);
    createCube(leg_width * 0.7f, leg_height / 2, 0, leg_width, leg_height, leg_width, leg_color);
}

void Sprite3D::createPillar(float height, float radius, uint16_t color)
{
    clearTriangles();
    type = SPRITE_PILLAR;
    float pillar_radius = radius * 1.5f;

    // Main cylinder - 6 segments = 12 triangles
    createCylinder(0, height / 2, 0, pillar_radius, height, 6, color);

    // Base - 4 segments = 8 triangles
    createCylinder(0, pillar_radius * 0.4f, 0, pillar_radius * 1.4f, pillar_radius * 0.8f, 4, color);

    // Top - 4 segments = 8 triangles
    createCylinder(0, height - pillar_radius * 0.4f, 0, pillar_radius * 1.4f, pillar_radius * 0.8f, 4, color);
}

void Sprite3D::createSphere(float x, float y, float z, float radius, uint8_t segments, uint16_t color)
{
    // limit segments for sphere to prevent triangle explosion
    if (segments > 4)
        segments = 4;

    for (uint8_t lat = 0; lat < segments / 2; lat++)
    {
        float theta1 = (float)lat * M_PI / (segments / 2);
        float theta2 = (float)(lat + 1) * M_PI / (segments / 2);

        for (uint8_t lon = 0; lon < segments; lon++)
        {
            float phi1 = (float)lon * 2.0f * M_PI / segments;
            float phi2 = (float)(lon + 1) * 2.0f * M_PI / segments;

            // Calculate vertices
            float x1 = x + radius * sinf(theta1) * cosf(phi1);
            float y1 = y + radius * cosf(theta1);
            float z1 = z + radius * sinf(theta1) * sinf(phi1);
            //
            float x2 = x + radius * sinf(theta1) * cosf(phi2);
            float y2 = y + radius * cosf(theta1);
            float z2 = z + radius * sinf(theta1) * sinf(phi2);
            //
            float x3 = x + radius * sinf(theta2) * cosf(phi1);
            float y3 = y + radius * cosf(theta2);
            float z3 = z + radius * sinf(theta2) * sinf(phi1);
            //
            float x4 = x + radius * sinf(theta2) * cosf(phi2);
            float y4 = y + radius * cosf(theta2);
            float z4 = z + radius * sinf(theta2) * sinf(phi2);

            // Add triangles
            if (lat > 0)
            {
                addTriangle(Triangle3D(x1, y1, z1, x2, y2, z2, x3, y3, z3, color));
            }
            if (lat < segments / 2 - 1)
            {
                addTriangle(Triangle3D(x2, y2, z2, x4, y4, z4, x3, y3, z3, color));
            }
        }
    }
}

void Sprite3D::createTree(float height, uint16_t color)
{
    clearTriangles();
    type = SPRITE_TREE;

    float trunk_width = height * 0.18f;
    float trunk_height = height * 0.4f;
    float crown_width = height * 0.65f;
    float crown_height = height * 0.6f;

    // Trunk (simple cube) - positioned so bottom touches ground (y=0) - brown
    createCube(0, trunk_height / 2, 0, trunk_width, trunk_height, trunk_width, 0x9A60);

    // Crown (simple cube representing foliage) - positioned on top of trunk
    createCube(0, trunk_height + crown_height / 2, 0, crown_width, crown_height, crown_width, color);
}

void Sprite3D::createTriangularPrism(float x, float y, float z, float width, float height, float depth, uint16_t color)
{
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    float hd = depth * 0.5f;

    // Front triangle
    addTriangle(Triangle3D(
        x - hw, y - hh, z + hd,
        x + hw, y - hh, z + hd,
        x, y + hh, z + hd, color));

    // Back triangle
    addTriangle(Triangle3D(
        x + hw, y - hh, z - hd,
        x - hw, y - hh, z - hd,
        x, y + hh, z - hd, color));

    // Bottom face
    addTriangle(Triangle3D(
        x - hw, y - hh, z - hd,
        x + hw, y - hh, z - hd,
        x + hw, y - hh, z + hd, color));
    addTriangle(Triangle3D(
        x - hw, y - hh, z - hd,
        x + hw, y - hh, z + hd,
        x - hw, y - hh, z + hd, color));

    // Side faces
    addTriangle(Triangle3D(
        x - hw, y - hh, z + hd,
        x, y + hh, z + hd,
        x, y + hh, z - hd, color));
    addTriangle(Triangle3D(
        x - hw, y - hh, z + hd,
        x, y + hh, z - hd,
        x - hw, y - hh, z - hd, color));

    addTriangle(Triangle3D(
        x, y + hh, z + hd,
        x + hw, y - hh, z + hd,
        x + hw, y - hh, z - hd, color));
    addTriangle(Triangle3D(
        x, y + hh, z + hd,
        x + hw, y - hh, z - hd,
        x, y + hh, z - hd, color));
}

void Sprite3D::createWall(float x, float y, float z, float width, float height, float depth, uint16_t color)
{
    // Wall segment using raw triangles, offset by (x, y, z)
    const float hw = width / 2, hh = height / 2, hd = depth / 2;
    // Front face
    addTriangle(x - hw, y - hh, z + hd, x + hw, y - hh, z + hd, x + hw, y + hh, z + hd, color);
    addTriangle(x - hw, y - hh, z + hd, x + hw, y + hh, z + hd, x - hw, y + hh, z + hd, color);
    // Back face
    addTriangle(x + hw, y - hh, z - hd, x - hw, y - hh, z - hd, x - hw, y + hh, z - hd, color);
    addTriangle(x + hw, y - hh, z - hd, x - hw, y + hh, z - hd, x + hw, y + hh, z - hd, color);
    // Left, right, top caps
    addTriangle(x - hw, y - hh, z - hd, x - hw, y - hh, z + hd, x - hw, y + hh, z + hd, color);
    addTriangle(x - hw, y - hh, z - hd, x - hw, y + hh, z + hd, x - hw, y + hh, z - hd, color);
    addTriangle(x + hw, y - hh, z + hd, x + hw, y - hh, z - hd, x + hw, y + hh, z - hd, color);
    addTriangle(x + hw, y - hh, z + hd, x + hw, y + hh, z - hd, x + hw, y + hh, z + hd, color);
    addTriangle(x - hw, y + hh, z + hd, x + hw, y + hh, z + hd, x + hw, y + hh, z - hd, color);
    addTriangle(x - hw, y + hh, z + hd, x + hw, y + hh, z - hd, x - hw, y + hh, z - hd, color);
}

static void scale_vertex(float &x, float &y, float &z, float scale_factor)
{
    x *= scale_factor;
    y *= scale_factor;
    z *= scale_factor;
}

static void rotateY_vertex(float &x, float &z, float angle)
{
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    float orig_x = x; // Store original x
    x = orig_x * cos_a - z * sin_a;
    z = orig_x * sin_a + z * cos_a; // Use original x
}

static void translate_vertex(float &x, float &y, float &z, float dx, float dy, float dz)
{
    x += dx;
    y += dy;
    z += dz;
}

Triangle3D Sprite3D::getTransformedTriangle(uint8_t index, const Vector &camera_pos) const
{
    if (index >= triangle_count)
        return Triangle3D();

    Triangle3D transformed = *triangles[index];

    // Apply transformations to each vertex
    for (uint8_t v = 0; v < 3; v++)
    {
        switch (v)
        {
        case 0:
        {
            // Scale
            scale_vertex(transformed.x1, transformed.y1, transformed.z1, scale_factor);
            // Rotate around Y axis
            rotateY_vertex(transformed.x1, transformed.z1, rotation_y);
            // Translate to world position
            translate_vertex(transformed.x1, transformed.y1, transformed.z1, position.x, 0, position.y);
            break;
        }
        case 1:
        {
            // Scale
            scale_vertex(transformed.x2, transformed.y2, transformed.z2, scale_factor);
            // Rotate around Y axis
            rotateY_vertex(transformed.x2, transformed.z2, rotation_y);
            // Translate to world position
            translate_vertex(transformed.x2, transformed.y2, transformed.z2, position.x, 0, position.y);
            break;
        }
        case 2:
        {
            // Scale
            scale_vertex(transformed.x3, transformed.y3, transformed.z3, scale_factor);
            // Rotate around Y axis
            rotateY_vertex(transformed.x3, transformed.z3, rotation_y);
            // Translate to world position
            translate_vertex(transformed.x3, transformed.y3, transformed.z3, position.x, 0, position.y);
            break;
        }
        default:
            break;
        };
    }

    // Check if triangle should be rendered
    if (transformed.isFacingCamera(camera_pos))
    {
        // Calculate distance for sorting
        Vector center = transformed.getCenter();
        float dx = center.x - camera_pos.x;
        float dz = center.z - camera_pos.y;
        transformed.distance = sqrtf(dx * dx + dz * dz);

        return transformed;
    }
    return Triangle3D(); // Return empty triangle if not facing camera
}

void Sprite3D::initializeAsHouse(Vector pos, float width, float height, float rot, uint16_t color)
{
    position = pos;
    rotation_y = rot;
    clearTriangles();
    type = SPRITE_HOUSE;
    active = true;
    createHouse(width, height, color);
}

void Sprite3D::initializeAsHumanoid(Vector pos, float height, float rot, uint16_t color)
{
    position = pos;
    rotation_y = rot;
    clearTriangles();
    type = SPRITE_HUMANOID;
    active = true;
    createHumanoid(height, color);
}

void Sprite3D::initializeAsPillar(Vector pos, float height, float radius, uint16_t color)
{
    position = pos;
    rotation_y = 0;
    clearTriangles();
    type = SPRITE_PILLAR;
    active = true;
    createPillar(height, radius, color);
}

void Sprite3D::initializeAsTree(Vector pos, float height, uint16_t color)
{
    position = pos;
    rotation_y = 0;
    clearTriangles();
    type = SPRITE_TREE;
    active = true;
    createTree(height, color);
}
