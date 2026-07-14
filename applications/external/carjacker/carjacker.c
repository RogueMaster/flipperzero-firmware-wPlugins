#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <stdlib.h>
#include <string.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <math.h>

#define MAX_CARS             100
#define HACKER_LINES         6
#define SCAN_DURATION_TICKS  60000
#define POPUP_DURATION_TICKS 3500
#define MAX_PEEN_PARTICLES   5

const char* high_end_cars[] = {"Bugatti Chiron",      "Lamborghini Huracan",
                               "Ferrari SF90",        "Porsche 918",
                               "Maserati MC20",       "McLaren P1",
                               "Aston Martin DBS",    "Pagani Huayra",
                               "Bentley Continental", "Rolls Royce Ghost",
                               "Koenigsegg Jesko",    "Lucid Air Sapphire",
                               "Mercedes AMG GT",     "Audi R8",
                               "Tesla Roadster",      "Nissan GT-R Nismo",
                               "Corvette Z06",        "BMW i8",
                               "Jaguar F-Type SVR",   "Cadillac CT5-V"};

const char* silly_cars[] = {
    "Kia Rio",
    "Toyota Prius",
    "Rusty Tractor",
    "Shopping Cart",
    "Moped 50cc",
    "Clown Car",
    "Golf Cart",
    "U-Haul Van",
    "Harley Davidson",
    "Amazon Drone",
    "Skateboard",
    "Riding Lawn Mower",
    "Broken Hoverboard",
    "1998 Corolla",
    "RC Car",
    "Segway",
    "Tardis",
    "Gas Tanker"};

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    bool active;
    float rotation;
    float rotation_speed;
    float angle;
} PeenParticle;

typedef struct {
    bool steal_mode;
    bool running;
    bool scanning;
    bool show_popup;
    bool peen_fountain_active;
    uint8_t car_count;
    char* captured_cars[MAX_CARS];
    uint8_t anim_step;
    uint8_t scan_anim_frame;
    uint8_t scroll_offset;
    uint32_t hacker_tick;
    uint32_t scan_start;
    uint32_t popup_start;
    uint32_t last_detection;
    uint32_t peen_start;
    PeenParticle* peen_particles;
    uint8_t popup_anim_frame;
    bool first_detection_done;
} CarJackerApp;

static void draw_popup(Canvas* canvas, CarJackerApp* app) {
    canvas_clear(canvas);

    uint8_t offset = (app->popup_anim_frame / 2) % 8;
    for(int i = 0; i < 128; i += 8) {
        canvas_draw_box(canvas, i + offset - 8, 0, 4, 2);
        canvas_draw_box(canvas, i + offset - 8, 62, 4, 2);
    }
    for(int i = 0; i < 64; i += 8) {
        canvas_draw_box(canvas, 0, i + offset - 8, 2, 4);
        canvas_draw_box(canvas, 126, i + offset - 8, 2, 4);
    }

    canvas_draw_frame(canvas, 4, 4, 120, 56);
    canvas_draw_frame(canvas, 6, 6, 116, 52);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignCenter, "ROCKETGOD");
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "WAS HERE");

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, "betaskynet.com");

    if(app->popup_anim_frame % 20 < 10) {
        canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, "[ ELITE HAX0R ]");
    }
}

static void draw_rotated_peen(Canvas* canvas, int x, int y, float rotation) {
    float cos_r = cosf(rotation);
    float sin_r = sinf(rotation);

    int cx = x + 10;
    int cy = y + 5;

#define ROTATE_X(px, py) (cx + (int)((px - cx) * cos_r - (py - cy) * sin_r))
#define ROTATE_Y(px, py) (cy + (int)((px - cx) * sin_r + (py - cy) * cos_r))

    for(int i = 0; i < 7; i++) {
        int x1 = ROTATE_X(x, y + 1 + i);
        int y1 = ROTATE_Y(x, y + 1 + i);
        int x2 = ROTATE_X(x + 18, y + 1 + i);
        int y2 = ROTATE_Y(x + 18, y + 1 + i);
        canvas_draw_line(canvas, x1, y1, x2, y2);
    }

    int head_x = ROTATE_X(x + 20, y + 4);
    int head_y = ROTATE_Y(x + 20, y + 4);
    canvas_draw_circle(canvas, head_x, head_y, 5);
    canvas_draw_dot(canvas, head_x + 1, head_y);
    canvas_draw_dot(canvas, head_x + 2, head_y);

    int ball1_x = ROTATE_X(x - 4, y + 3);
    int ball1_y = ROTATE_Y(x - 4, y + 3);
    int ball2_x = ROTATE_X(x - 4, y + 7);
    int ball2_y = ROTATE_Y(x - 4, y + 7);
    canvas_draw_circle(canvas, ball1_x, ball1_y, 4);
    canvas_draw_circle(canvas, ball2_x, ball2_y, 4);

    int vein1_x1 = ROTATE_X(x + 5, y + 2);
    int vein1_y1 = ROTATE_Y(x + 5, y + 2);
    int vein1_x2 = ROTATE_X(x + 8, y + 3);
    int vein1_y2 = ROTATE_Y(x + 8, y + 3);
    canvas_draw_line(canvas, vein1_x1, vein1_y1, vein1_x2, vein1_y2);

    int vein2_x1 = ROTATE_X(x + 11, y + 6);
    int vein2_y1 = ROTATE_Y(x + 11, y + 6);
    int vein2_x2 = ROTATE_X(x + 14, y + 5);
    int vein2_y2 = ROTATE_Y(x + 14, y + 5);
    canvas_draw_line(canvas, vein2_x1, vein2_y1, vein2_x2, vein2_y2);

#undef ROTATE_X
#undef ROTATE_Y
}

static bool check_peen_collision(PeenParticle* p1, PeenParticle* p2) {
    if(!p1->active || !p2->active) return false;

    float dx = p1->x - p2->x;
    float dy = p1->y - p2->y;
    float dist = dx * dx + dy * dy;

    return dist < 600;
}

static void update_peen_physics(CarJackerApp* app) {
    const float gravity = 0.03f;
    const float damping = 0.998f;
    const float bounce = 0.85f;
    const float spin_damping = 0.95f;
    const float max_rotation_speed = 0.05f;

    for(int i = 0; i < MAX_PEEN_PARTICLES; i++) {
        PeenParticle* p = &app->peen_particles[i];
        if(!p->active) continue;

        p->vy += gravity;

        p->angle = atan2f(p->vy, p->vx);

        p->vx += (float)(rand() % 100 - 50) / 5000.0f;
        p->vy += (float)(rand() % 100 - 50) / 5000.0f;

        p->x += p->vx;
        p->y += p->vy;

        p->rotation += p->rotation_speed;
        p->rotation_speed *= spin_damping;

        if(p->rotation_speed > max_rotation_speed) p->rotation_speed = max_rotation_speed;
        if(p->rotation_speed < -max_rotation_speed) p->rotation_speed = -max_rotation_speed;

        for(int j = i + 1; j < MAX_PEEN_PARTICLES; j++) {
            PeenParticle* p2 = &app->peen_particles[j];
            if(check_peen_collision(p, p2)) {
                float dx = p2->x - p->x;
                float dy = p2->y - p->y;
                float dist = sqrtf(dx * dx + dy * dy);

                dx /= dist;
                dy /= dist;

                float rvx = p->vx - p2->vx;
                float rvy = p->vy - p2->vy;
                float speed = rvx * dx + rvy * dy;

                p->vx -= speed * dx * bounce;
                p->vy -= speed * dy * bounce;
                p2->vx += speed * dx * bounce;
                p2->vy += speed * dy * bounce;

                p->rotation_speed += (rand() % 100 - 50) / 2000.0f;
                p2->rotation_speed -= (rand() % 100 - 50) / 2000.0f;

                p->x -= dx * 10;
                p->y -= dy * 10;
                p2->x += dx * 10;
                p2->y += dy * 10;
            }
        }

        if(p->x < 5) {
            p->x = 5;
            p->vx = fabsf(p->vx) * bounce + 0.2f;
            p->rotation_speed += 0.02f;
        }
        if(p->x > 100) {
            p->x = 100;
            p->vx = -fabsf(p->vx) * bounce - 0.2f;
            p->rotation_speed -= 0.02f;
        }

        if(p->y > 48) {
            p->y = 48;
            p->vy = -fabsf(p->vy) * bounce - 0.3f;
            p->rotation_speed += (p->vx > 0 ? 0.03f : -0.03f);
        }
        if(p->y < 5) {
            p->y = 5;
            p->vy = fabsf(p->vy) * bounce + 0.3f;
            p->rotation_speed += (p->vx > 0 ? -0.03f : 0.03f);
        }

        p->vx *= damping;
        p->vy *= damping;

        float speed = sqrtf(p->vx * p->vx + p->vy * p->vy);
        if(speed < 0.2f) {
            float boost = 0.3f / (speed + 0.001f);
            p->vx *= boost;
            p->vy *= boost;
        }
    }
}

static void init_peen_particles(CarJackerApp* app) {
    for(int i = 0; i < MAX_PEEN_PARTICLES; i++) {
        PeenParticle* p = &app->peen_particles[i];
        p->active = true;
        p->x = 20 + (i * 18);
        p->y = 10 + (rand() % 25);

        float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
        float speed = 1.0f + (float)(rand() % 100) / 100.0f;
        p->vx = cosf(angle) * speed;
        p->vy = sinf(angle) * speed;

        p->rotation = (float)(rand() % 360) * 3.14159f / 180.0f;
        p->rotation_speed = (float)(rand() % 100 - 50) / 1500.0f;
        p->angle = angle;
    }
}

static void draw_peen_fountain(Canvas* canvas, CarJackerApp* app) {
    canvas_clear(canvas);

    update_peen_physics(app);

    for(int i = 0; i < MAX_PEEN_PARTICLES; i++) {
        PeenParticle* p = &app->peen_particles[i];
        if(p->active) {
            draw_rotated_peen(canvas, (int)p->x, (int)p->y, p->rotation);
        }
    }
}

static void draw_main_scene(Canvas* canvas, CarJackerApp* app) {
    canvas_clear(canvas);

    if(app->show_popup) {
        draw_popup(canvas, app);
        return;
    }

    if(app->peen_fountain_active) {
        draw_peen_fountain(canvas, app);
        return;
    }

    elements_bold_rounded_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);

    if(app->steal_mode && app->car_count > 0) {
        const char* title = "CAR STEALING MODE";
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, title);

        canvas_set_font(canvas, FontSecondary);
        for(uint8_t i = 0; i < HACKER_LINES; i++) {
            char line[22];
            snprintf(line, sizeof(line), "[%02X] %08X", rand() % 256, rand());
            canvas_draw_str(canvas, 4, 22 + i * 7, line);
        }
        canvas_draw_str(canvas, 10, 62, ">> Injecting packets...");
    } else {
        const char* title = "CAR SCANNER MODE";
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, title);

        if(app->car_count > 0) {
            canvas_set_font(canvas, FontSecondary);
            uint8_t visible = (app->car_count < 5) ? app->car_count : 5;

            for(uint8_t i = 0; i < visible; i++) {
                uint8_t idx = app->car_count - 1 - (app->scroll_offset + i);
                if(idx < app->car_count) {
                    canvas_draw_str_aligned(
                        canvas, 64, 20 + i * 7, AlignCenter, AlignBottom, app->captured_cars[idx]);
                }
            }

            if(app->car_count > app->scroll_offset + 5) {
                canvas_draw_str(canvas, 120, 50, "v");
            }
            if(app->scroll_offset > 0) {
                canvas_draw_str(canvas, 120, 18, "^");
            }
        }

        if(app->scanning) {
            canvas_set_font(canvas, FontSecondary);
            const char* scan_frames[] = {
                "[>    ]",
                "[=>   ]",
                "[==>  ]",
                "[===> ]",
                "[====>]",
                "[ <===]",
                "[  <==]",
                "[   <=]"};
            canvas_draw_str_aligned(
                canvas, 64, 58, AlignCenter, AlignBottom, scan_frames[app->scan_anim_frame % 8]);
        } else if(app->car_count == 0) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignBottom, "Press OK to scan");
        } else {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, 62, "OK=scan ->=steal <=fun ^v=scroll");
        }
    }
}

static void input_callback(InputEvent* event, void* context) {
    CarJackerApp* app = context;

    if(app->show_popup) return;

    if(app->peen_fountain_active) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->peen_fountain_active = false;
        }
        return;
    }

    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk && !app->steal_mode) {
            if(!app->scanning) {
                app->scanning = true;
                app->scan_start = furi_get_tick();
                app->last_detection = app->scan_start;
                app->scan_anim_frame = 0;
                app->first_detection_done = false;
            } else {
                app->scanning = false;
            }
        } else if(event->key == InputKeyOk && app->steal_mode && app->car_count > 0) {
            app->anim_step = 1;
            app->hacker_tick = furi_get_tick();
        } else if(event->key == InputKeyRight) {
            app->steal_mode = !app->steal_mode;
        } else if(event->key == InputKeyLeft) {
            app->peen_fountain_active = true;
            app->peen_start = furi_get_tick();
            init_peen_particles(app);
        } else if(event->key == InputKeyBack) {
            app->running = false;
        } else if(event->key == InputKeyUp && !app->steal_mode) {
            if(app->scroll_offset > 0) {
                app->scroll_offset--;
            }
        } else if(event->key == InputKeyDown && !app->steal_mode) {
            if(app->car_count > app->scroll_offset + 5) {
                app->scroll_offset++;
            }
        }
    }
}

int32_t carjacker_app(void* p) {
    (void)p;

    CarJackerApp* app = malloc(sizeof(CarJackerApp));
    if(!app) return -1;

    app->car_count = 0;
    app->steal_mode = false;
    app->anim_step = 0;
    app->running = true;
    app->scanning = false;
    app->show_popup = true;
    app->peen_fountain_active = false;
    app->scroll_offset = 0;
    app->scan_anim_frame = 0;
    app->popup_anim_frame = 0;
    app->popup_start = furi_get_tick();
    app->first_detection_done = false;
    memset(app->captured_cars, 0, sizeof(app->captured_cars));

    app->peen_particles = malloc(sizeof(PeenParticle) * MAX_PEEN_PARTICLES);
    if(!app->peen_particles) {
        free(app);
        return -1;
    }

    for(int i = 0; i < MAX_PEEN_PARTICLES; i++) {
        app->peen_particles[i].active = false;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, (ViewPortDrawCallback)draw_main_scene, app);
    view_port_input_callback_set(view_port, input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    while(app->running) {
        uint32_t current_tick = furi_get_tick();

        if(app->show_popup) {
            app->popup_anim_frame++;
            if(current_tick - app->popup_start > POPUP_DURATION_TICKS) {
                app->show_popup = false;
            }
        }

        if(app->anim_step > 0 && current_tick - app->hacker_tick > 1000) {
            for(uint8_t i = 0; i < MAX_CARS; i++) {
                if(app->captured_cars[i]) {
                    free(app->captured_cars[i]);
                    app->captured_cars[i] = NULL;
                }
            }
            app->car_count = 0;
            app->anim_step = 0;
            app->scroll_offset = 0;
            notification_message(notifications, &sequence_error);
        }

        if(app->scanning) {
            app->scan_anim_frame++;

            uint32_t elapsed_since_last = current_tick - app->last_detection;
            uint32_t detect_interval;

            if(!app->first_detection_done) {
                detect_interval = 5000;
            } else {
                detect_interval = 5000 + (rand() % 3000);
            }

            if(elapsed_since_last >= detect_interval && app->car_count < MAX_CARS) {
                bool is_real = (app->car_count == 0) || (rand() % 3 > 0);
                const char* selected = is_real ? high_end_cars[rand() % 20] :
                                                 silly_cars[rand() % 18];

                app->captured_cars[app->car_count] = strdup(selected);
                app->car_count++;
                app->last_detection = current_tick;
                app->first_detection_done = true;

                app->scroll_offset = 0;

                notification_message(notifications, &sequence_single_vibro);
                notification_message(notifications, &sequence_success);
            }
        }

        view_port_update(view_port);
        furi_delay_ms(50);
    }

    for(uint8_t i = 0; i < MAX_CARS; i++) {
        if(app->captured_cars[i]) free(app->captured_cars[i]);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app->peen_particles);
    free(app);

    return 0;
}
