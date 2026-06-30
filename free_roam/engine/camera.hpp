#pragma once
#include "vector.hpp"
#include "engine_config.hpp"

#include ENGINE_MEM_INCLUDE

// Camera perspective types for 3D rendering
typedef enum
{
    CAMERA_FIRST_PERSON, // Default - render from player's own position/view
    CAMERA_THIRD_PERSON  // Render from external camera position
} CameraPerspective;

// Camera parameters for 3D rendering
class Camera
{
public:
    Vector direction;              // Camera direction
    float distance;                // Distance to projection plane (for 3D rendering)
    float height;                  // Camera height
    CameraPerspective perspective; // Camera perspective type
    Vector plane;                  // Camera plane
    Vector position;               // Camera position

    Camera() : direction(1, 0, 0), distance(2.0f), height(1.6f), perspective(CAMERA_FIRST_PERSON), plane(0, 0.66f, 0), position(0, 0, 0) {}
    Camera(Vector pos, Vector dir, Vector pl, float h, float dist, CameraPerspective camera_perspective = CAMERA_FIRST_PERSON)
        : direction(dir), distance(dist), height(h), perspective(camera_perspective), plane(pl), position(pos) {}
};