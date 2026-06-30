#pragma once
#include <furi.h>

typedef struct {
    float x, y;
} Vector;

#define VECTOR_ZERO    \
    (Vector) {         \
        .x = 0, .y = 0 \
    }

Vector vector_copy(Vector* const other);

//basic math

void vector_add(Vector* const a, Vector* const b, Vector* target);

void vector_sub(Vector* const a, Vector* const b, Vector* target);

void vector_mul(Vector* const a, Vector* const b, Vector* target);

void vector_div(Vector* const a, Vector* const b, Vector* target);

// computations

float vector_length_sqrt(Vector* const a);

float vector_length(Vector* const v);

float vector_distance(Vector* const a, Vector* const b);

void vector_normalized(Vector* const v, Vector* target);

void vector_inverse(Vector* const v, Vector* target);

float vector_dot(Vector* const a, Vector* const b);

void vector_rotate(Vector* const v, float deg, Vector* target);

void vector_rounded(Vector* const source, Vector* target);

float vector_cross(Vector* const a, Vector* const b);

void vector_perpendicular(Vector* const v, Vector* target);

void vector_project(
    Vector* point,
    Vector* const line_a,
    Vector* const line_b,
    Vector* result,
    bool* success);

void vector_lerp(Vector* const start, Vector* const end, float time, Vector* result);

void vector_quadratic(
    Vector* const start,
    Vector* const control,
    Vector* const end,
    float time,
    Vector* result);
