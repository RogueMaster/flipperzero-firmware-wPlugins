#include "vector.h"

#include <math.h>
#include "helpers.h"

Vector vector_copy(Vector* const other) {
    return (Vector){.x = other->x, .y = other->y};
}

//basic math

void vector_add(Vector* const a, Vector* const b, Vector* target) {
    target->x = a->x + b->x;
    target->y = a->y + b->y;
}

void vector_sub(Vector* const a, Vector* const b, Vector* target) {
    target->x = a->x - b->x;
    target->y = a->y - b->y;
}

void vector_mul(Vector* const a, Vector* const b, Vector* target) {
    target->x = a->x * b->x;
    target->y = a->y * b->y;
}

void vector_div(Vector* const a, Vector* const b, Vector* target) {
    target->x = a->x / b->x;
    target->y = a->y / b->y;
}

// computations

float vector_length_sqrt(Vector* const a) {
    return a->x * a->x + a->y * a->y;
}

float vector_length(Vector* const v) {
    return sqrtf(vector_length_sqrt(v));
}

float vector_distance(Vector* const a, Vector* const b) {
    Vector v;
    vector_sub(a, b, &v);
    return vector_length(&v);
}

void vector_normalized(Vector* const v, Vector* target) {
    float length = vector_length(v);
    if(length == 0) {
        target->x = 0;
        target->y = 0;
    } else {
        target->x = v->x / length;
        target->y = v->y / length;
    }
}

void vector_inverse(Vector* const v, Vector* target) {
    target->x = -v->x;
    target->y = -v->y;
}

float vector_dot(Vector* const a, Vector* const b) {
    return a->x * b->x + a->y * b->y;
}

void vector_rotate(Vector* const v, float deg, Vector* target) {
    float tx = v->x;
    float ty = v->y;
    target->x = (float)(cosf(deg) * tx - sinf(deg) * ty);
    target->y = (float)(sinf(deg) * tx + cosf(deg) * ty);
}

void vector_rounded(Vector* const source, Vector* target) {
    target->x = roundf(source->x);
    target->y = roundf(source->y);
}

float vector_cross(Vector* const a, Vector* const b) {
    return a->x * b->y - a->y * b->x;
}

void vector_perpendicular(Vector* const v, Vector* target) {
    target->x = -v->y;
    target->y = v->x;
}

void vector_project(
    Vector* point,
    Vector* const line_a,
    Vector* const line_b,
    Vector* result,
    bool* success) {
    Vector ab;
    vector_sub(line_b, line_a, &ab);
    Vector ap;
    vector_sub(point, line_a, &ap);
    float dot = vector_dot(&ap, &ab);
    float length_squared = vector_length_sqrt(&ab);
    float t = dot / length_squared;
    if(t < 0 || t > 1) {
        *success = false;
    } else {
        result->x = line_a->x + ab.x * t;
        result->y = line_a->y + ab.y * t;
        *success = true;
    }
}

void vector_lerp(Vector* const start, Vector* const end, float time, Vector* result) {
    result->x = lerp_number(start->x, end->x, time);
    result->y = lerp_number(start->y, end->y, time);
}

void vector_quadratic(
    Vector* const start,
    Vector* const control,
    Vector* const end,
    float time,
    Vector* result) {
    Vector a;
    vector_lerp(start, control, time, &a);
    Vector b;
    vector_lerp(control, end, time, &b);
    vector_lerp(&a, &b, time, result);
}
