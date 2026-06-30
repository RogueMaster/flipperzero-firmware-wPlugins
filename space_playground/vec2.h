#ifndef VEC2_H
#define VEC2_H

#include <stdlib.h>

typedef struct Vec2 {
    double x;
    double y;
} Vec2;

Vec2* new_vec2(double x, double y) {
    Vec2* v = (Vec2*)malloc(sizeof(Vec2));
    if(v) {
        v->x = x;
        v->y = y;
    }
    return v;
}

void free_vec2(Vec2* v) {
    if(v) {
        free(v);
    }
}

Vec2 vec2(double x, double y) {
    Vec2 v;
    v.x = x;
    v.y = y;
    return v;
}

Vec2 vec2_add(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}

#endif
