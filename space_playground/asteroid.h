#ifndef ASTEROID_H
#define ASTEROID_H

#include "list.h"
#include "settings.h"
#include "vec2.h"
#include "planet.h"
#include "debri.h"

#include <stdbool.h>
#include <stdlib.h>

const size_t ASTEROID_MIN_SPEED = 5;
const size_t ASTEROID_MAX_SPEED = 50;

static double elapsed = 0.0;

typedef struct Asteroid {
    Vec2 p;
    Vec2 v;
    bool active;
    List trail;
} Asteroid;

List asteroids;

void asteroid_free(Asteroid* a) {
    if(a) {
        ListNode* node = a->trail.head;
        while(node) {
            Vec2* point = (Vec2*)node->data;
            free_vec2(point);
            node = node->next;
        }
        list_free(&a->trail);
        free(a);
    }
}

void asteroid_clear() {
    ListNode* node = asteroids.head;
    while(node) {
        Asteroid* a = (Asteroid*)node->data;
        asteroid_free(a);
        node = node->next;
    }
    list_free(&asteroids);
}

void asteroid_init() {
    asteroid_clear();

    asteroids = new_list();
    elapsed = 0.0;
}

Asteroid* asteroid_create(double x, double y, double vx, double vy) {
    Asteroid* a = (Asteroid*)malloc(sizeof(Asteroid));
    a->p = vec2(x, y);
    a->v = vec2(vx, vy);
    a->active = true;
    a->trail = new_list();
    list_push(&asteroids, a);
    return a;
}

void asteroid_update_asteroids(double dt) {
    elapsed += dt;

    int canvas_width = 128; // Assuming a fixed canvas width
    int canvas_height = 64; // Assuming a fixed canvas height

    if(elapsed > (double)0.5) {
        elapsed = 0.0;

        int origin = rand() % 4; // 0: left, 1: right, 2: top, 3: bottom
        double x, y;
        double vx =
            settings.asteroid_speed *
            (double)(rand() % (ASTEROID_MAX_SPEED - ASTEROID_MIN_SPEED + 1) + ASTEROID_MIN_SPEED);
        double vy =
            settings.asteroid_speed *
            (double)(rand() % (ASTEROID_MAX_SPEED - ASTEROID_MIN_SPEED + 1) + ASTEROID_MIN_SPEED);

        if(origin == 0) {
            // Left side
            x = -2.0;
            y = (double)(rand() % canvas_height);
            vy = (rand() % 2 == 0) ? vy : -vy;
        } else if(origin == 1) {
            // Right side
            x = (double)(canvas_width + 2);
            y = (double)(rand() % canvas_height);
            vx = -vx;
            vy = (rand() % 2 == 0) ? vy : -vy;
        } else if(origin == 2) {
            // Top side
            x = (double)(rand() % canvas_width);
            y = -2.0;
            vx = (rand() % 2 == 0) ? vx : -vx;
        } else {
            // Bottom side
            x = (double)(rand() % canvas_width);
            y = (double)(canvas_height + 2);
            vy = -vy;
        }

        asteroid_create(x, y, vx, vy);
    }

    ListNode* node = asteroids.head;
    while(node) {
        Asteroid* a = (Asteroid*)node->data;

        if(a->active) {
            if(is_colliding_with_any_planet(&a->p)) {
                a->active = false;
                debri_create(a->p.x, a->p.y, a->v.x, a->v.y);
                continue;
            }

            for(ListNode* other_node = asteroids.head; other_node != NULL;
                other_node = other_node->next) {
                if(other_node == node) continue; // Skip self

                Asteroid* other = (Asteroid*)other_node->data;
                if(other->active) {
                    double dx = other->p.x - a->p.x;
                    double dy = other->p.y - a->p.y;
                    double dist_sq = dx * dx + dy * dy;
                    double radius_sum = 4.0; // Assuming both asteroids have radius 1.0
                    if(dist_sq <= (radius_sum * radius_sum)) {
                        // Collision detected
                        a->active = false;
                        other->active = false;
                        debri_create(a->p.x, a->p.y, a->v.x, a->v.y);
                        debri_create(other->p.x, other->p.y, other->v.x, other->v.y);
                        continue;
                    }
                }
            }

            Vec2 acc = planet_get_combined_acceleration(&a->p);

            // Update velocity with acceleration
            a->v.x += acc.x * dt;
            a->v.y += acc.y * dt;

            // Clamp velocity
            double speed = sqrt(a->v.x * a->v.x + a->v.y * a->v.y);
            if(speed > ASTEROID_MAX_SPEED) {
                double scale = ASTEROID_MAX_SPEED / speed;
                a->v.x *= scale;
                a->v.y *= scale;
            }

            a->p.x += a->v.x * dt;
            a->p.y += a->v.y * dt;

            Vec2* trail_point = new_vec2(a->p.x, a->p.y);
            list_push(&a->trail, trail_point);

            if(a->trail.size > settings.trail_duration * 60) {
                // Remove oldest point if trail exceeds max length
                ListNode* tail_node = a->trail.head;
                if(tail_node) {
                    free_vec2((Vec2*)tail_node->data);
                    list_pop_head(&a->trail);
                }
            }
        } else {
            if(a->trail.size > 0) {
                void* data = list_pop_head(&a->trail);
                if(data) {
                    free_vec2((Vec2*)data);
                }
            }

            // If no trail left, free asteroid
            if(a->trail.size == 0) {
                asteroid_free(a);
                ListNode* to_remove = node;
                node = node->next; // Move to next before removing
                list_remove(&asteroids, to_remove);
                continue;
            }
        }

        const bool is_too_far_out = a->p.x < -canvas_width || a->p.x > 2 * canvas_width ||
                                    a->p.y < -canvas_height || a->p.y > 2 * canvas_height;

        if(is_too_far_out) {
            // Remove asteroid if it goes too far out of bounds
            asteroid_free(a);
            ListNode* to_remove = node;
            node = node->next; // Move to next before removing
            list_remove(&asteroids, to_remove);
            continue;
        }

        node = node->next;
    }
}

void asteroid_draw_asteroids(Canvas* canvas) {
    for(ListNode* node = asteroids.head; node != NULL; node = node->next) {
        Asteroid* a = (Asteroid*)node->data;

        for(ListNode* trail_node = a->trail.head; trail_node != NULL;
            trail_node = trail_node->next) {
            Vec2* point = (Vec2*)trail_node->data;
            canvas_draw_dot(canvas, (int)point->x, (int)point->y);
        }

        if(a->active) {
            canvas_draw_disc(canvas, (int)a->p.x, (int)a->p.y, 2);
        }
    }
}

#endif
