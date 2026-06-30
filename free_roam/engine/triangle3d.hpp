#pragma once
#include "vector.hpp"

class Triangle3D
{
public:
    float x1 = 0.0, y1 = 0.0, z1 = 0.0, x2 = 0.0, y2 = 0.0, z2 = 0.0, x3 = 0.0, y3 = 0.0, z3 = 0.0;
    bool visible;   // Whether the triangle is visible (facing the camera)
    float distance; // For depth sorting
    bool set;       // Indicates if the triangle has been initialized with valid vertices
    uint16_t color; // Color of the triangle
    bool wireframe; // If true, draw outline and fill; if false, only fill

    Vector getCenter() const;                            // Calculate triangle center for distance sorting
    bool isFacingCamera(const Vector &camera_pos) const; // Check if triangle is facing the camera

    Triangle3D() : visible(true), distance(0), set(false), color(0x0000), wireframe(true) {}

    Triangle3D(const Vector &v1, const Vector &v2, const Vector &v3, uint16_t color = 0x0000)
        : visible(true), distance(0), set(true), color(color), wireframe(true)
    {
        x1 = v1.x;
        y1 = v1.y;
        z1 = v1.z;
        x2 = v2.x;
        y2 = v2.y;
        z2 = v2.z;
        x3 = v3.x;
        y3 = v3.y;
        z3 = v3.z;
    }

    Triangle3D(float x1, float y1, float z1,
               float x2, float y2, float z2,
               float x3, float y3, float z3, uint16_t color = 0x0000)
        : x1(x1), y1(y1), z1(z1),
          x2(x2), y2(y2), z2(z2),
          x3(x3), y3(y3), z3(z3),
          visible(true), distance(0), set(true), color(color), wireframe(true) {}
};