#ifndef VECTOR2_H_
#define VECTOR2_H_

#include <stdint.h>

class Vector2 {
public:
    inline Vector2()
        : x(0)
        , y(0) {
    }
    inline Vector2(int16_t _x, int16_t _y)
        : x(_x)
        , y(_y) {
    }
    inline Vector2 operator+(const Vector2& v) {
        return Vector2(x + v.x, y + v.y);
    }
    inline Vector2 operator-(const Vector2& v) {
        return Vector2(x - v.x, y - v.y);
    }
    inline Vector2 operator*(int16_t scale) {
        return Vector2(x * scale, y * scale);
    }
    inline Vector2 operator/(int16_t scale) {
        return Vector2(x / scale, y / scale);
    }
    inline bool operator==(const Vector2& v) {
        return v.x == x && v.y == y;
    }
    inline bool operator!=(const Vector2& v) {
        return !(*this == v);
    }
    inline Vector2 operator-() const {
        return Vector2(-x, -y);
    }
    inline Vector2 operator+=(const Vector2& v) {
        x += v.x;
        y += v.y;
        return *this;
    }
    inline Vector2 operator-=(const Vector2& v) {
        x -= v.x;
        y -= v.y;
        return *this;
    }
    inline Vector2 operator*=(int16_t scale) {
        x *= scale;
        y *= scale;
        return *this;
    }
    inline Vector2 operator/=(int16_t scale) {
        x /= scale;
        y /= scale;
        return *this;
    }

    int16_t x, y;
};

#endif
