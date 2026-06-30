#ifndef PLANET_H
#define PLANET_H

#include "list.h"
#include "settings.h"
#include <gui/canvas.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>

#include "vec2.h"

static const u_int8_t PLANET_MIN_RADIUS = 2;
static const u_int8_t PLANET_MAX_RADIUS = 3;

static const u_int8_t PLANET_MIN_X = 3 * PLANET_MIN_RADIUS;
static const u_int8_t PLANET_MIN_Y = 3 * PLANET_MIN_RADIUS;

static const u_int8_t PLANET_MAX_X = 128 - 3 * PLANET_MIN_RADIUS;
static const u_int8_t PLANET_MAX_Y = 64 - 3 * PLANET_MIN_RADIUS;

typedef struct Planet {
    Vec2 pos;
    float r;
} Planet;

List planets;

Vec2 planet_get_acceleration(const Planet* planet, const Vec2* pos) {
    Vec2 acc = vec2(0, 0);
    double dx = planet->pos.x - pos->x;
    double dy = planet->pos.y - pos->y;
    double dist_sq = dx * dx + dy * dy;

    if(dist_sq > 0) {
        double dist = sqrt(dist_sq);
        double force = settings.gravity_force * 100 * ((double)planet->r * (double)planet->r) /
                       dist_sq; // Simplified gravitational force
        acc.x = force * (dx / dist);
        acc.y = force * (dy / dist);
    }
    return acc;
}

bool is_colliding_with_planet(const Planet* planet, const Vec2* pos) {
    double dx = planet->pos.x - pos->x;
    double dy = planet->pos.y - pos->y;
    double dist_sq = dx * dx + dy * dy;
    double radius_sum = (double)planet->r + (double)2.0;

    return dist_sq <= (radius_sum * radius_sum);
}

bool is_colliding_with_any_planet(const Vec2* pos) {
    for(ListNode* node = planets.head; node != NULL; node = node->next) {
        Planet* planet = (Planet*)node->data;
        if(is_colliding_with_planet(planet, pos)) {
            return true;
        }
    }
    return false;
}

Vec2 planet_get_combined_acceleration(const Vec2* pos) {
    Vec2 acc = vec2(0, 0);
    for(ListNode* node = planets.head; node != NULL; node = node->next) {
        Planet* planet = (Planet*)node->data;
        Vec2 planet_acc = planet_get_acceleration(planet, pos);
        acc.x += planet_acc.x;
        acc.y += planet_acc.y;
    }
    return acc;
}

void planet_clear() {
    list_free(&planets);
}

void planet_init() {
    planet_clear();

    double section_width = (double)128.0 / (double)settings.num_planets;
    for(size_t i = 0; i < settings.num_planets; i++) {
        double r =
            (double)(rand() % (PLANET_MAX_RADIUS - PLANET_MIN_RADIUS + 1) + PLANET_MIN_RADIUS);

        double x = (double)(i * section_width + (section_width / (double)2.0));
        double y = (double)(rand() % (PLANET_MAX_Y - PLANET_MIN_Y + 1) + PLANET_MIN_Y);

        Planet* planet = (Planet*)malloc(sizeof(Planet));
        if(planet) {
            planet->pos = vec2(x, y);
            planet->r = r;
            list_push(&planets, planet);
        }
    }
}

void planet_update_planets(double dt) {
    UNUSED(dt);
}

void planet_draw_planets(Canvas* canvas) {
    for(ListNode* node = planets.head; node != NULL; node = node->next) {
        Planet* planet = (Planet*)node->data;
        canvas_draw_circle(canvas, (int)planet->pos.x, (int)planet->pos.y, 2 * (int)planet->r);
        canvas_draw_disc(canvas, (int)planet->pos.x, (int)planet->pos.y, (int)planet->r);
    }
}

#endif
