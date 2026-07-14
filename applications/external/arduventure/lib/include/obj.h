#pragma once

#ifdef __cplusplus
struct Rect {
    int x;
    int y;
    int width;
    int height;

    Rect() = default;
    constexpr Rect(int x, int y, int width, int height)
        : x(x)
        , y(y)
        , width(width)
        , height(height) {
    }
};

struct Point {
    int x;
    int y;
    Point() = default;
    constexpr Point(int x, int y)
        : x(x)
        , y(y) {
    }
};
#else
typedef struct {
    int x;
    int y;
    int width;
    int height;
} Rect;

typedef struct {
    int x;
    int y;
} Point;
#endif
