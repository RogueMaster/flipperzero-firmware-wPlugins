#include "vector.hpp"
#include <math.h>

Vector Vector::addf(Vector a, float b)
{
    return (Vector){this->x = a.x + b, this->y = a.y + b, this->z = a.z + b};
}

Vector Vector::divf(Vector a, float b)
{
    return (Vector){this->x = a.x / b, this->y = a.y / b, this->z = a.z / b};
}

Vector Vector::mulf(Vector a, float b)
{
    return (Vector){this->x = a.x * b, this->y = a.y * b, this->z = a.z * b};
}

Vector Vector::rotateY(float angle) const
{
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    return Vector(
        x * cos_a - z * sin_a,
        y,
        x * sin_a + z * cos_a);
}

Vector Vector::scale(float sx, float sy, float sz) const
{
    return Vector(x * sx, y * sy, z * sz);
}

Vector Vector::subf(Vector a, float b)
{
    return (Vector){this->x = a.x - b, this->y = a.y - b, this->z = a.z - b};
}

Vector Vector::translate(float dx, float dy, float dz) const
{
    return Vector(x + dx, y + dy, z + dz);
}