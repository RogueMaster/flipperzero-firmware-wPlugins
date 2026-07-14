#include "uid_brute_smarter_scene_about.h"
// #include "../version.h"
#include <gui/elements.h>
#include <gui/view_dispatcher.h>
#include <furi.h>
#include <furi_hal.h>

#define ANIMATION_FRAMES   12
#define ANIMATION_SPEED_MS 200

// Particle system for cool effects
#define MAX_PARTICLES 8

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    uint8_t life;
} Particle;

// ASCII art frames for morphing animation - using ONLY basic ASCII that Flipper supports
// Now with 5 lines to fill more screen space
static const char* border_frames[ANIMATION_FRAMES][5] = {
    // Frame 0: Stars in corners
    {"*                 *",
     "                   ",
     "                   ",
     "                   ",
     "*                 *"},
    // Frame 1: Stars move in
    {" *               * ",
     "                   ",
     "                   ",
     "                   ",
     " *               * "},
    // Frame 2: Top/bottom lines appear
    {" *---------------* ",
     "                   ",
     "                   ",
     "                   ",
     " *---------------* "},
    // Frame 3: Full box with stars
    {" *---------------* ",
     " |               | ",
     " |               | ",
     " |               | ",
     " *---------------* "},
    // Frame 4: Double lines
    {" +===============+ ",
     " |               | ",
     " |               | ",
     " |               | ",
     " +===============+ "},
    // Frame 5: Hash corners
    {" #===============# ",
     " |               | ",
     " |               | ",
     " |               | ",
     " #===============# "},
    // Frame 6: Dots expand
    {"  .=============.  ",
     "  |             |  ",
     "  |             |  ",
     "  |             |  ",
     "  .=============.  "},
    // Frame 7: Arrows pointing in
    {"> .=============. <",
     "  |             |  ",
     "  |             |  ",
     "  |             |  ",
     "> .=============. <"},
    // Frame 8: Collapse arrows
    {" >.===========.<  ",
     " ||           ||  ",
     " ||           ||  ",
     " ||           ||  ",
     " >.===========.<  "},
    // Frame 9: Wave effect
    {"~~~~~~~~~~~~~~~~~~",
     " |              | ",
     " |              | ",
     " |              | ",
     "~~~~~~~~~~~~~~~~~~"},
    // Frame 10: Pulse out
    {"  *-----------*   ",
     "  |           |   ",
     "  |           |   ",
     "  |           |   ",
     "  *-----------*   "},
    // Frame 11: Return to start
    {" *             *  ",
     "                  ",
     "                  ",
     "                  ",
     " *             *  "},
};

struct UidBruteSmarterSceneAbout {
    View* view;
    FuriTimer* timer;
};

typedef struct {
    uint8_t frame;
    Particle particles[MAX_PARTICLES];
    uint8_t particle_spawn_counter;
    struct UidBruteSmarterSceneAbout* instance;
} UidBruteSmarterSceneAboutModel;

static void uid_brute_smarter_scene_about_timer_callback(void* context) {
    furi_assert(context);
    struct UidBruteSmarterSceneAbout* instance = context;
    with_view_model(
        instance->view,
        UidBruteSmarterSceneAboutModel * model,
        {
            // Cycle through animation frames
            model->frame = (model->frame + 1) % ANIMATION_FRAMES;

            // Update particles
            for(uint8_t i = 0; i < MAX_PARTICLES; i++) {
                if(model->particles[i].life > 0) {
                    model->particles[i].x += model->particles[i].vx;
                    model->particles[i].y += model->particles[i].vy;
                    model->particles[i].life--;

                    // Bounce off edges
                    if(model->particles[i].x < 0 || model->particles[i].x > 127) {
                        model->particles[i].vx = -model->particles[i].vx;
                    }
                    if(model->particles[i].y < 0 || model->particles[i].y > 63) {
                        model->particles[i].vy = -model->particles[i].vy;
                    }
                }
            }

            // Spawn new particles occasionally
            model->particle_spawn_counter++;
            if(model->particle_spawn_counter >= 3) {
                model->particle_spawn_counter = 0;
                for(uint8_t i = 0; i < MAX_PARTICLES; i++) {
                    if(model->particles[i].life == 0) {
                        // Spawn from corners
                        uint8_t corner = (furi_hal_random_get() % 4);
                        model->particles[i].x = (corner & 1) ? 120.0f : 8.0f;
                        model->particles[i].y = (corner & 2) ? 55.0f : 15.0f;
                        model->particles[i].vx = ((furi_hal_random_get() % 100) - 50) / 50.0f;
                        model->particles[i].vy = ((furi_hal_random_get() % 100) - 50) / 50.0f;
                        model->particles[i].life = 20 + (furi_hal_random_get() % 20);
                        break;
                    }
                }
            }
        },
        true);
}

static void uid_brute_smarter_scene_about_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    UidBruteSmarterSceneAboutModel* model = context;

    canvas_clear(canvas);

    // Draw particles first (background layer)
    for(uint8_t i = 0; i < MAX_PARTICLES; i++) {
        if(model->particles[i].life > 0) {
            uint8_t size = (model->particles[i].life > 10) ? 2 : 1;
            canvas_draw_disc(
                canvas, (uint8_t)model->particles[i].x, (uint8_t)model->particles[i].y, size);
        }
    }

    // Draw title at top
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "UID Brute Smarter");

    // Get current frame
    uint8_t frame = model->frame;

    // Draw animated border (expanded to use full screen)
    canvas_set_font(canvas, FontSecondary);
    const uint8_t box_x = 5;
    const uint8_t box_y = 18;
    const uint8_t line_height = 10;

    // Draw the 5 lines of the border frame (expanded)
    canvas_draw_str(canvas, box_x, box_y, border_frames[frame][0]);
    canvas_draw_str(canvas, box_x, box_y + line_height, border_frames[frame][1]);
    canvas_draw_str(canvas, box_x, box_y + line_height * 2, border_frames[frame][2]);
    canvas_draw_str(canvas, box_x, box_y + line_height * 3, border_frames[frame][3]);
    canvas_draw_str(canvas, box_x, box_y + line_height * 4, border_frames[frame][4]);

    // Draw centered text inside the border with white background for visibility
    canvas_set_font(canvas, FontSecondary);

    // Draw white box behind text for perfect readability
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 18, 28, 92, 26);
    canvas_set_color(canvas, ColorBlack);

    // Draw the GitHub URL centered
    canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignCenter, "github.com/fbettag/");
    canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignCenter, "uid_brute_smarter");

    // Draw version below (from version.c)
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, FAP_VERSION);
}

static bool uid_brute_smarter_scene_about_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    // Let the view dispatcher handle back button via previous_callback
    return false;
}

static void uid_brute_smarter_scene_about_enter_callback(void* context) {
    furi_assert(context);
    struct UidBruteSmarterSceneAbout* instance = context;
    with_view_model(
        instance->view,
        UidBruteSmarterSceneAboutModel * model,
        {
            model->frame = 0;
            model->particle_spawn_counter = 0;
            // Initialize all particles to dead
            for(uint8_t i = 0; i < MAX_PARTICLES; i++) {
                model->particles[i].life = 0;
            }
        },
        true);
    furi_timer_start(instance->timer, ANIMATION_SPEED_MS);
}

static void uid_brute_smarter_scene_about_exit_callback(void* context) {
    furi_assert(context);
    struct UidBruteSmarterSceneAbout* instance = context;
    furi_timer_stop(instance->timer);
}

View* uid_brute_smarter_scene_about_alloc(void) {
    struct UidBruteSmarterSceneAbout* instance = malloc(sizeof(struct UidBruteSmarterSceneAbout));
    if(instance == NULL) return NULL; // Handle allocation failure

    instance->view = view_alloc();
    if(instance->view == NULL) {
        free(instance);
        return NULL;
    }
    view_set_context(instance->view, instance);
    view_allocate_model(
        instance->view, ViewModelTypeLocking, sizeof(UidBruteSmarterSceneAboutModel));
    view_set_draw_callback(instance->view, uid_brute_smarter_scene_about_draw_callback);
    view_set_enter_callback(instance->view, uid_brute_smarter_scene_about_enter_callback);
    view_set_exit_callback(instance->view, uid_brute_smarter_scene_about_exit_callback);
    view_set_input_callback(instance->view, uid_brute_smarter_scene_about_input_callback);

    with_view_model(
        instance->view,
        UidBruteSmarterSceneAboutModel * model,
        {
            model->frame = 0;
            model->particle_spawn_counter = 0;
            model->instance = instance; // Store instance pointer in model for cleanup
            // Initialize all particles to dead
            for(uint8_t i = 0; i < MAX_PARTICLES; i++) {
                model->particles[i].life = 0;
            }
        },
        true);

    instance->timer = furi_timer_alloc(
        uid_brute_smarter_scene_about_timer_callback, FuriTimerTypePeriodic, instance);
    if(instance->timer == NULL) {
        // Handle furi_timer_alloc failure
        view_free(instance->view);
        free(instance);
        return NULL;
    }

    return instance->view;
}

void uid_brute_smarter_scene_about_free(View* view) {
    furi_assert(view);
    struct UidBruteSmarterSceneAbout* instance = NULL;
    with_view_model(
        view,
        UidBruteSmarterSceneAboutModel * model,
        {
            instance = model->instance; // Retrieve instance pointer from model
        },
        false);
    if(instance) {
        furi_timer_free(instance->timer);
        free(instance);
    }
    view_free(view);
}
