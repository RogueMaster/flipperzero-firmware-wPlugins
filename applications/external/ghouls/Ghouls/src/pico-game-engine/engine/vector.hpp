#pragma once
#include <stdint.h>

class Vector
{
public:
    float x;
    float y;
    float z;
    bool integer;

    Vector()
    {
        x = 0;
        y = 0;
        z = 0;
        integer = false;
    };

    Vector(float x, float y, float z = 0, bool integer = false)
    {
        this->x = x;
        this->y = y;
        this->z = z;
        this->integer = integer;
    }

    Vector addf(Vector a, float b);                       // Add scalar to each component
    Vector divf(Vector a, float b);                       // Divide each component by scalar
    Vector mulf(Vector a, float b);                       // Multiply each component by scalar
    Vector rotateY(float angle) const;                    // Rotate vertex around Y axis (for sprite facing)
    Vector scale(float sx, float sy, float sz) const;     // Scale vertex
    Vector subf(Vector a, float b);                       // Subtract scalar from each component
    Vector translate(float dx, float dy, float dz) const; // Translate vertex

    Vector operator+(const Vector &other) const
    {
        return Vector(x + other.x, y + other.y, z + other.z);
    }
    Vector operator-(const Vector &other) const
    {
        return Vector(x - other.x, y - other.y, z - other.z);
    }
    Vector operator*(const Vector &other) const
    {
        return Vector(x * other.x, y * other.y, z * other.z);
    }
    Vector operator/(const Vector &other) const
    {
        return Vector(x / other.x, y / other.y, z / other.z);
    }
    bool operator!=(const Vector &other) const
    {
        return (x != other.x) || (y != other.y) || (z != other.z);
    }
    bool operator==(const Vector &other) const
    {
        return (x == other.x) && (y == other.y) && (z == other.z);
    }
};
