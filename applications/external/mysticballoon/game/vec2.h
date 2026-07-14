#ifndef _VEC2_
#define _VEC2_

struct vec2 {
    int x;
    int y;

    vec2()
        : x(0)
        , y(0) {
    }
    vec2(int vx, int vy)
        : x(vx)
        , y(vy) {
    }
    vec2(const vec2& other) = default;

    vec2& operator+=(const vec2& rhs) {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    vec2& operator-=(const vec2& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    vec2& operator=(const vec2& rhs) {
        x = rhs.x;
        y = rhs.y;
        return *this;
    }
};

vec2 operator+(vec2 lhs, const vec2& rhs) {
    //vec2 t = lhs;
    //t += rhs;
    return (lhs += rhs);
}

vec2 operator-(vec2 lhs, const vec2& rhs) {
    //vec2 t = lhs;
    //t -= rhs;
    return (lhs -= rhs);
}

vec2 operator<<(vec2 lhs, const int& rhs) {
    lhs.x <<= rhs;
    lhs.y <<= rhs;
    return lhs;
}

vec2 operator>>(vec2 lhs, const int& rhs) {
    lhs.x >>= rhs;
    lhs.y >>= rhs;
    return lhs;
}

#endif
