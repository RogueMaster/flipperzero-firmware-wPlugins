#include "level_imu.h"

/**** IMU Debug ****/

typedef struct {
    float pitch;
    float roll;
    float yaw;

    float pitch_offset;
    float roll_offset;
    float yaw_offset;

    bool imu_present;
} IMUDebugContext;

static void imu_debug_start(Entity* self, GameManager* manager, void* ctx) {
    UNUSED(self);
    IMUDebugContext* context = ctx;
    context->pitch = 0;
    context->roll = 0;
    context->yaw = 0;
    context->pitch_offset = 0;
    context->roll_offset = 0;
    context->yaw_offset = 0;
    GameContext* game_context = game_manager_game_context_get(manager);
    context->imu_present = game_context->imu_present;
}

static void imu_debug_update(Entity* self, GameManager* manager, void* ctx) {
    UNUSED(self);
    IMUDebugContext* context = (IMUDebugContext*)ctx;
    GameContext* game_context = game_manager_game_context_get(manager);

    if(game_context->imu_present) {
        context->pitch = imu_pitch_get(game_context->imu);
        context->roll = imu_roll_get(game_context->imu);
        context->yaw = imu_yaw_get(game_context->imu);
    }

    InputState input = game_manager_input_get(manager);
    if(input.pressed & GameKeyOk) {
        context->pitch_offset = context->pitch;
        context->roll_offset = context->roll;
        context->yaw_offset = context->yaw;
    }

    if(input.pressed & GameKeyBack) {
        game_manager_game_stop(manager);
    }

    if(game_context->imu_present) {
        context->pitch -= context->pitch_offset;
        context->roll -= context->roll_offset;
        context->yaw -= context->yaw_offset;
        if(context->pitch > 90) {
            context->pitch -= 90;
        } else if(context->pitch < -90) {
            context->pitch += 90;
        }
        if(context->roll > 180) {
            context->roll -= 360;
        } else if(context->roll < -180) {
            context->roll += 360;
        }
        if(context->yaw > 180) {
            context->yaw -= 360;
        } else if(context->yaw < -180) {
            context->yaw += 360;
        }
    }
}

static void imu_debug_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(self);
    UNUSED(manager);

    Vector pos = {32, 32};
    const float radius = 30;
    const float max_angle = 45;
    const float bubble_radius = 3;

    canvas_draw_line(canvas, pos.x - radius, pos.y, pos.x + radius, pos.y);
    canvas_draw_line(canvas, pos.x, pos.y - radius, pos.x, pos.y + radius);
    canvas_draw_circle(canvas, pos.x, pos.y, radius);

    IMUDebugContext* imu_debug_context = context;
    if(imu_debug_context->imu_present) {
        const float pitch = -CLAMP(imu_debug_context->pitch, max_angle, -max_angle);
        const float roll = -CLAMP(imu_debug_context->roll, max_angle, -max_angle);
        const float max_bubble_len = radius - bubble_radius - 2;

        Vector ball = {
            max_bubble_len * (pitch / max_angle),
            max_bubble_len * (roll / max_angle),
        };

        float bubble_len = sqrtf(ball.x * ball.x + ball.y * ball.y);
        if(bubble_len > max_bubble_len) {
            ball.x = ball.x * max_bubble_len / bubble_len;
            ball.y = ball.y * max_bubble_len / bubble_len;
        }

        ball = vector_add(pos, ball);

        canvas_draw_disc(canvas, ball.x, ball.y, bubble_radius);

        canvas_printf(canvas, 60, 8, "@CodeAllNight");
        canvas_printf(canvas, 70, 23, "Pitch: %3.1f", (double)imu_debug_context->pitch);
        canvas_printf(canvas, 70, 35, "Roll: %3.1f", (double)imu_debug_context->roll);
        canvas_printf(canvas, 70, 47, "Yaw: %d", (int)imu_debug_context->yaw);
        canvas_printf(canvas, 60, 63, "OK to center");
    } else {
        canvas_draw_str_aligned(canvas, pos.x, pos.y + 1, AlignCenter, AlignCenter, "No IMU");
    }
}

static const EntityDescription imu_debug_desc = {
    .start = imu_debug_start,
    .stop = NULL,
    .update = imu_debug_update,
    .render = imu_debug_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(IMUDebugContext),
};

/**** Level  ****/

static void level_imu_alloc(Level* level, GameManager* manager, void* ctx) {
    UNUSED(ctx);
    UNUSED(manager);
    level_add_entity(level, &imu_debug_desc);
}

const LevelBehaviour level_imu = {
    .alloc = level_imu_alloc,
    .free = NULL,
    .start = NULL,
    .stop = NULL,
    .context_size = 0,
};
