#include <stdbool.h>
#include <math.h>

#include "vec2.h"

// Returns the closest point to the line segment ab and p
Vec2 Vec2_closest(const Vec2& a, const Vec2& b, const Vec2& p) {
    // vector along line ab
    Vec2 ab = b - a;
    float t = ab.dot(ab);
    if(t == 0.0f) {
        return a;
    }
    t = fmax(0.0f, fmin(1.0f, (p.dot(ab) - a.dot(ab)) / t));
    return a + ab * t;
}
