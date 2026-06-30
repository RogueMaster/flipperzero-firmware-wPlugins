#ifndef DEBRI_H
#define DEBRI_H

#include "list.h"
#include "settings.h"
#include "planet.h"
#include "vec2.h"

#include <gui/canvas.h>
#include <string.h>

typedef struct {
    Vec2 pos;
    Vec2 velocity;
} Debri;


List debri_list;

void debri_clear() {
    ListNode* node = debri_list.head;
    while(node) {
        Debri* d = (Debri*)node->data;
        free(d);
        node = node->next;
    }
    list_free(&debri_list);
}

void debri_init() {
    debri_clear();
    debri_list = new_list();
}

void debri_add(double x, double y, double vx, double vy) {
    Debri* d = (Debri*)malloc(sizeof(Debri));
    if(d) {
        d->pos = vec2(x, y);
        d->velocity = vec2(vx, vy);
        list_push(&debri_list, d);
    }
}

void debri_create(double x, double y, double vx, double vy) {
    static const double PI = 3.14159265358979323846;
    double slice = 2 * PI / (double)settings.debri_count;

    for(size_t i = 0; i < settings.debri_count; i++) {
        double angle = slice + rand() * slice;
        double rotated_vx = vx * cos(angle * i) - vy * sin(angle * i);
        double rotated_vy = vx * sin(angle * i) + vy * cos(angle * i);
        debri_add(x, y, rotated_vx, rotated_vy);
    }
}

void debri_update(double dt) {
    ListNode* node = debri_list.head;
    while(node) {
        Debri* d = (Debri*)node->data;

        d->pos.x += d->velocity.x * dt;
        d->pos.y += d->velocity.y * dt;

        const bool is_out_of_bounds = d->pos.x < 0 || d->pos.x > 128 || d->pos.y < 0 ||
                                      d->pos.y > 64;

        if(is_colliding_with_any_planet(&d->pos) || is_out_of_bounds) {
            ListNode* to_remove = node;
            node = node->next;
            list_remove(&debri_list, to_remove);
            free(d);
            continue;
        }
        node = node->next;
    }
}

void debri_draw(Canvas* canvas) {
    for(ListNode* node = debri_list.head; node != NULL; node = node->next) {
        Debri* d = (Debri*)node->data;
        canvas_draw_disc(canvas, (int32_t)d->pos.x, (int32_t)d->pos.y, 1);
    }
}

#endif
