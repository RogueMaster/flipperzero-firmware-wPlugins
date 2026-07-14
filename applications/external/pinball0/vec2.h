#pragma once
#include <stdbool.h>
#include <cmath>

#define VEC2_EPSILON (float)0.001

class Vec2 {
public:
    float x;
    float y;

    Vec2()
        : x(0)
        , y(0) {
    }
    Vec2(float x_, float y_)
        : x(x_)
        , y(y_) {
    }

    Vec2 operator+(const Vec2& rhs) const {
        return Vec2(x + rhs.x, y + rhs.y);
    }
    Vec2 operator+(float s) const {
        return Vec2(x + s, y + s);
    }
    void operator+=(const Vec2& rhs) {
        x += rhs.x;
        y += rhs.y;
    }
    Vec2 operator-(const Vec2& rhs) const {
        return Vec2(x - rhs.x, y - rhs.y);
    }
    Vec2 operator-(float s) const {
        return Vec2(x - s, y - s);
    }
    void operator-=(const Vec2& rhs) {
        x -= rhs.x;
        y -= rhs.y;
    }
    Vec2 operator*(float s) const {
        return Vec2(x * s, y * s);
    }
    void operator*=(float s) {
        x *= s;
        y *= s;
    }

    Vec2 operator/(float s) const {
        return Vec2(x / s, y / s);
    }

    bool operator==(const Vec2& rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    // Magnitude / length of vector
    float mag() const {
        return sqrtf(x * x + y * y);
    }
    // Magnitude squared
    float mag2() const {
        return x * x + y * y;
    }

    // Dot product: this.x * v.x + this.y * v.y
    float dot(const Vec2& v) const {
        return x * v.x + y * v.y;
    }

    // Cross product
    float cross(const Vec2& v) const {
        return x * v.y - y * v.x;
    }

    void normalize(void) {
        float len = mag();
        if(len > VEC2_EPSILON) {
            float inverse_len = 1.0f / len;
            x *= inverse_len;
            y *= inverse_len;
        }
    }

    // Distance squared between this and next
    float dist2(const Vec2& v) const {
        float dx = x - v.x;
        float dy = y - v.y;
        return dx * dx + dy * dy;
    }
    // Distance between tihs and next
    float dist(const Vec2& v) const {
        return sqrtf(dist2(v));
    }
};

inline Vec2 operator*(float s, const Vec2& v) {
    return Vec2(s * v.x, s * v.y);
}

// // Returns the closest point to the line segment ab and p
Vec2 Vec2_closest(const Vec2& a, const Vec2& b, const Vec2& p);
