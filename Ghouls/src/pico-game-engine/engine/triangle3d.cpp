#include "triangle3d.hpp"

Vector Triangle3D::getCenter() const
{
    return Vector(
        (this->x1 + this->x2 + this->x3) / 3.0f,
        (this->y1 + this->y2 + this->y3) / 3.0f,
        (this->z1 + this->z2 + this->z3) / 3.0f);
}

bool Triangle3D::isFacingCamera(const Vector &camera_pos) const
{
    // Calculate triangle normal using cross product
    Vector v1 = Vector(this->x2 - this->x1, this->y2 - this->y1, this->z2 - this->z1);
    Vector v2 = Vector(this->x3 - this->x1, this->y3 - this->y1, this->z3 - this->z1);

    // Cross product to get normal (right-hand rule)
    Vector normal = Vector(
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x);

    // Vector from triangle center to camera
    Vector center = getCenter();
    Vector to_camera = Vector(
        camera_pos.x - center.x,
        0.5f - center.y,        // Camera height
        camera_pos.y - center.z // camera_pos.y is Z in world space
    );

    // Dot product - if positive, triangle faces camera
    float dot = normal.x * to_camera.x + normal.y * to_camera.y + normal.z * to_camera.z;
    return dot > 0.0f;
}
