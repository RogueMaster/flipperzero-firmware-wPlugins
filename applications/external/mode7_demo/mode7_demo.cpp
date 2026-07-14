#include <furi.h>
#include <furi_hal_gpio.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <storage/storage.h>

#include <toolbox/stream/file_stream.h>

#include "util/pbm.h"

#include <cinttypes>

#include <array>
#include <utility>

#define TAG "Mode7"

// This is simplified compared to the "traditional" bit band alias access, since we only occupy the bottom 256KB of SRAM anyway
#define BIT_BAND_ALIAS(var) \
    (reinterpret_cast<uint32_t*>((reinterpret_cast<uintptr_t>(var) << 5) | 0x22000000))

static constexpr uint32_t MAIN_VIEW = 0;

static constexpr int16_t SCREEN_WIDTH = 128;
static constexpr int16_t SCREEN_HEIGHT = 64;
static constexpr int16_t EYE_DISTANCE = 150;
static constexpr int16_t HORIZON = 15;

static uint32_t exit_app(void*) {
    return VIEW_NONE;
}

static float g_offset_x = 0.0f;
static float g_offset_y = 0.0f;
static int16_t g_rotation = 0;

static int16_t g_scale_x = 16;
static int16_t g_scale_y = 16;

static FuriMutex* g_background_switch_mutex;

static uint8_t g_current_background_id = 0;
static Pbm* g_current_background_pbm;

static const char* g_backgrounds[] = {
    APP_ASSETS_PATH("floor.pbm"),
    APP_ASSETS_PATH("grid.pbm"),
    APP_ASSETS_PATH("cookie_monster.pbm"),
    EXT_PATH("mode7_demo/background.pbm")};

static uint8_t* screen_buffer_space;
static uint8_t* back_buffer[2];
static uint8_t current_backbuffer = 0;

static FuriEventFlag* presentation_flag;

static double g_fps = 0.0;
static uint32_t g_last_tick_time;

static void reload_background() {
    if(furi_mutex_acquire(g_background_switch_mutex, FuriWaitForever) != FuriStatusOk) {
        return;
    }

    if(g_current_background_pbm != nullptr) {
        pbm_free(g_current_background_pbm);
    }

    Storage* storage = static_cast<Storage*>(furi_record_open(RECORD_STORAGE));

    g_current_background_pbm = pbm_load_file(storage, g_backgrounds[g_current_background_id]);
    if(g_current_background_pbm == nullptr) {
        // Try the other backgrounds, give up if none of them work (this shouldn't be the case ever)
        const uint8_t last_background_id = g_current_background_id;
        do {
            g_current_background_id = (g_current_background_id + 1) % std::size(g_backgrounds);
            if(g_current_background_id == last_background_id) {
                furi_crash();
            }
            g_current_background_pbm =
                pbm_load_file(storage, g_backgrounds[g_current_background_id]);
        } while(g_current_background_pbm == nullptr);
    }
    furi_record_close(RECORD_STORAGE);

    furi_mutex_release(g_background_switch_mutex);
}

static void load_scales() {
    Storage* storage = static_cast<Storage*>(furi_record_open(RECORD_STORAGE));
    Stream* file_stream = file_stream_alloc(storage);
    if(file_stream_open(
           file_stream, EXT_PATH("mode7_demo/scales.txt"), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line_string = furi_string_alloc();
        if(stream_read_line(file_stream, line_string)) {
            g_scale_x = 1;
            g_scale_y = 1;
            sscanf(
                furi_string_get_cstr(line_string), "%" SCNi16 " %" SCNi16, &g_scale_x, &g_scale_y);
        }
        furi_string_free(line_string);
    }
    file_stream_close(file_stream);

    stream_free(file_stream);
    furi_record_close(RECORD_STORAGE);
}

static void draw_test_checkerboard(Canvas* canvas, void* model) {
    UNUSED(model);

    const uint8_t buffer_to_use = current_backbuffer;
    furi_event_flag_clear(presentation_flag, 1 << buffer_to_use);
    canvas_draw_xbm(canvas, 0, 0, 128, 64, back_buffer[buffer_to_use]);
    furi_event_flag_set(presentation_flag, 1 << buffer_to_use);

    FuriString* fps_text = furi_string_alloc_printf("%0.f", g_fps);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 124, 8, AlignRight, AlignBottom, furi_string_get_cstr(fps_text));
    furi_string_free(fps_text);
}

static bool input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        g_current_background_id = (g_current_background_id + 1) % std::size(g_backgrounds);
        reload_background();
        return true;
    }
    return false;
}

static int mod(int x, int divisor) {
    int m = x % divisor;
    return m + ((m >> 31) & divisor);
}

static uint32_t sample_background(
    const uint32_t* bitmap,
    int32_t sample_x,
    int32_t sample_y,
    uint32_t pitch,
    int32_t width,
    int32_t height) {
    // Repeat mode for now
    sample_x = mod(sample_x, width);
    sample_y = mod(sample_y, height);

    return bitmap[sample_y * pitch + sample_x];
}

static void handle_inputs() {
    // TODO: This should react to events and cache the input buttons state, not this
    float x = 0.0f, y = 0.0f;

    if(!furi_hal_gpio_read(&gpio_button_right)) {
        x++;
    } else if(!furi_hal_gpio_read(&gpio_button_left)) {
        x--;
    }
    if(!furi_hal_gpio_read(&gpio_button_down)) {
        y++;
    } else if(!furi_hal_gpio_read(&gpio_button_up)) {
        y--;
    }

    float angle_sin, angle_cos;
    sincosf((g_rotation * M_PI) / 180.0f, &angle_sin, &angle_cos);
    g_offset_x += x * angle_cos - y * angle_sin;
    g_offset_y += x * angle_sin + y * angle_cos;

    if(furi_hal_gpio_read(&gpio_button_ok)) {
        g_rotation = (g_rotation + 1) % 360;
    }
}

static void tick_callback(void* context) {
    View* view = static_cast<View*>(context);

    const uint32_t current_tick = furi_get_tick();
    const uint32_t last_tick = std::exchange(g_last_tick_time, current_tick);

    const uint32_t tick_delta = (current_tick - last_tick) * 1000;
    g_fps = 1000.0 / (tick_delta / furi_kernel_get_tick_frequency());

    handle_inputs();

    const uint8_t next_backbuffer = (current_backbuffer + 1) % 2;

    float angle_sin, angle_cos;
    sincosf((g_rotation * M_PI) / 180.0f, &angle_sin, &angle_cos);

    furi_event_flag_wait(
        presentation_flag,
        1 << next_backbuffer,
        FuriFlagWaitAny | FuriFlagNoClear,
        FuriWaitForever);

    if(furi_mutex_acquire(g_background_switch_mutex, FuriWaitForever) != FuriStatusOk) {
        return;
    }

    // "Rasterize" the background
    const int32_t background_width = g_current_background_pbm->width;
    const uint32_t background_pitch = pbm_get_pitch(g_current_background_pbm);
    const int32_t background_height = g_current_background_pbm->height;
    const uint32_t* background_bitmap = BIT_BAND_ALIAS(g_current_background_pbm->bitmap);

    // This is "slow" but simulates how backgrounds are rasterized. Use it for now.
    // This method also allows for easy repeat modes
    uint32_t* screen_bitmap = BIT_BAND_ALIAS(back_buffer[next_backbuffer]);
    for(int32_t y = -HORIZON + 1; y < SCREEN_HEIGHT / 2; ++y) {
        for(int32_t x = -SCREEN_WIDTH / 2; x < SCREEN_WIDTH / 2; ++x) {
            int32_t dx = x + (SCREEN_WIDTH / 2);
            int32_t dy = y + (SCREEN_HEIGHT / 2);

            int32_t px = x;
            int32_t py = y + EYE_DISTANCE;
            int32_t pz = y + HORIZON;

            float sx = static_cast<float>(px) / pz;
            float sy = static_cast<float>(-py) / pz;
            float rsx = sx * angle_cos - sy * angle_sin;
            float rsy = sx * angle_sin + sy * angle_cos;

            screen_bitmap[dy * 128 + dx] = sample_background(
                background_bitmap,
                (rsx * g_scale_x) + g_offset_x,
                (rsy * g_scale_y) + g_offset_y,
                background_pitch,
                background_width,
                background_height);
        }
    }

    furi_mutex_release(g_background_switch_mutex);

    current_backbuffer = next_backbuffer;
    if(view != nullptr) {
        view_commit_model(view, true);
    }
}

extern "C" int32_t mode7_demo_app(void* p) {
    UNUSED(p);

    g_background_switch_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    reload_background();
    load_scales();

    furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);

    presentation_flag = furi_event_flag_alloc();
    furi_event_flag_set(presentation_flag, 0b11u);
    screen_buffer_space = static_cast<uint8_t*>(malloc(16 * 64 * 2));
    back_buffer[0] = screen_buffer_space;
    back_buffer[1] = screen_buffer_space + (16 * 64);

    Gui* gui = static_cast<Gui*>(furi_record_open(RECORD_GUI));

    ViewDispatcher* view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    View* test_view = view_alloc();
    view_set_previous_callback(test_view, exit_app);
    view_set_draw_callback(test_view, draw_test_checkerboard);
    view_set_input_callback(test_view, input_callback);

    view_dispatcher_set_event_callback_context(view_dispatcher, test_view);

    view_dispatcher_add_view(view_dispatcher, MAIN_VIEW, test_view);
    view_dispatcher_switch_to_view(view_dispatcher, MAIN_VIEW);

    FuriTimer* tick_timer = furi_timer_alloc(tick_callback, FuriTimerTypePeriodic, test_view);
    furi_timer_start(tick_timer, furi_kernel_get_tick_frequency() / 60);

    view_dispatcher_run(view_dispatcher);

    furi_timer_stop(tick_timer);
    furi_timer_free(tick_timer);

    view_dispatcher_remove_view(view_dispatcher, MAIN_VIEW);
    view_free(test_view);
    view_dispatcher_free(view_dispatcher);

    furi_record_close(RECORD_GUI);

    // This shouldn't be needed, but I was getting an use-after-free when freeing
    // the screen buffers - possibly the draw callback is still running after freeing the view?
    // Just in case, wait for all buffer accesses to finish
    furi_event_flag_wait(presentation_flag, 0b11u, FuriFlagWaitAll, FuriWaitForever);
    free(screen_buffer_space);
    furi_event_flag_free(presentation_flag);

    furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);

    pbm_free(g_current_background_pbm);
    furi_mutex_free(g_background_switch_mutex);

    return 0;
}
