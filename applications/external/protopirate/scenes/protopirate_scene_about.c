// scenes/protopirate_scene_about.c
#include "../protopirate_app_i.h"
#include <gui/elements.h>
#include <stdlib.h>

#define CREDITS_START_Y    28
#define CREDITS_END_Y      52
#define CREDIT_LINE_HEIGHT 10
#define SCROLL_SPEED       1

static const char* credits[] = {
    "",
    "-=> App Development by",
    "RocketGod",
    "MMX",
    "Leeroy",
    "gullradriel",
    "Skorp's Weather App",
    "Vadim's Radio Driver",
    "-=> Protocol Magic by",
    "L0rdDiakon",
    "Leeroy",
    "Li0ard",
    "MMX",
    "YougZ",
    "DoobTheGoober",
    "RocketGod",
    "Skorp",
    "Slackware",
    "Trikk",
    "Wootini",
    "-=> RE Support",
    "DoobTheGoober",
    "Li0ard",
    "MMX",
    "NeedNotApply",
    "RocketGod",
    "Slackware",
    "Trikk",
    // can add more
};

#define CREDITS_COUNT (sizeof(credits) / sizeof(credits[0]))

typedef struct {
    uint8_t frame;
    uint8_t seed;
    int16_t scroll_offset;
} GlitchState;

static GlitchState g_state = {0};

static void draw_noise_pixels(Canvas* canvas, uint8_t density) {
    for(uint8_t i = 0; i < density; i++) {
        canvas_draw_dot(canvas, rand() % 128, rand() % 64);
    }
}

static void about_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    srand(g_state.seed);
    canvas_clear(canvas);

    // Light background static
    canvas_set_color(canvas, ColorBlack);
    draw_noise_pixels(canvas, 6 + (rand() % 6));

    // Occasional subtle x-jitter
    int8_t x_off = (rand() % 15 == 0) ? ((rand() % 4) - 2) : 0;

    // Animated TPP decoration (centered)
    canvas_set_font(canvas, FontKeyboard);
    if(g_state.frame % 8 < 4) {
        canvas_draw_str_aligned(
            canvas, 64, 18, AlignCenter, AlignBottom, ">>>=================<<<");
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 18, AlignCenter, AlignBottom, ">>>======[TPP]======<<<");
    }

    // Draw credits region (clip area)
    canvas_set_font(canvas, FontSecondary);

    // Calculate total scroll height
    int16_t total_height = CREDITS_COUNT * CREDIT_LINE_HEIGHT;

    // Draw scrolling credits
    for(size_t i = 0; i < CREDITS_COUNT; i++) {
        int16_t y = CREDITS_START_Y + (i * CREDIT_LINE_HEIGHT) - g_state.scroll_offset;

        // Wrap around for endless scroll
        while(y < CREDITS_START_Y - CREDIT_LINE_HEIGHT) {
            y += total_height;
        }
        while(y > CREDITS_START_Y + total_height) {
            y -= total_height;
        }

        // Only draw if in visible region
        if(y >= CREDITS_START_Y - CREDIT_LINE_HEIGHT && y <= CREDITS_END_Y) {
            canvas_draw_str(canvas, x_off, y, credits[i]);
        }
    }

    // Draw fade/mask bars at top and bottom of credits area
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, 128, CREDITS_START_Y - CREDIT_LINE_HEIGHT);
    canvas_draw_box(canvas, 0, CREDITS_END_Y, 128, 14);

    // Redraw header over mask
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x_off, 10, "ProtoPirate v" FAP_VERSION);

    canvas_set_font(canvas, FontKeyboard);
    if(g_state.frame % 8 < 4) {
        canvas_draw_str_aligned(
            canvas, 64, 18, AlignCenter, AlignBottom, ">>>=================<<<");
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 18, AlignCenter, AlignBottom, ">>>======[TPP]======<<<");
    }

    // Redraw static in header area
    srand(g_state.seed + 1);
    for(uint8_t i = 0; i < 3; i++) {
        canvas_draw_dot(canvas, rand() % 128, rand() % (CREDITS_START_Y - CREDIT_LINE_HEIGHT));
    }

    // Footer: The Pirate's Plunder Discord
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, "discord.gg/thepirates");

    // Rare subtle glitch bar
    if(rand() % 30 == 0) {
        canvas_set_color(canvas, ColorXOR);
        uint8_t y = rand() % 60;
        canvas_draw_box(canvas, 0, y, 128, 2);
    }
}

static bool about_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void protopirate_scene_about_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    if(!protopirate_ensure_view_about(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    g_state.frame = 0;
    g_state.seed = furi_get_tick() & 0xFF;
    g_state.scroll_offset = 0;

    view_set_draw_callback(app->view_about, about_draw_callback);
    view_set_input_callback(app->view_about, about_input_callback);
    view_set_context(app->view_about, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewAbout);
}

bool protopirate_scene_about_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        g_state.frame++;
        g_state.seed = rand();

        if(g_state.frame % 2 == 0) {
            g_state.scroll_offset += SCROLL_SPEED;
            int16_t total_height = CREDITS_COUNT * CREDIT_LINE_HEIGHT;
            if(g_state.scroll_offset >= total_height) {
                g_state.scroll_offset = 0;
            }
        }

        view_commit_model(app->view_about, true);
        consumed = true;
    }

    return consumed;
}

void protopirate_scene_about_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    view_set_draw_callback(app->view_about, NULL);
    view_set_input_callback(app->view_about, NULL);
}
