// scenes/protopirate_scene_timing_tuner.c
#include "../protopirate_app_i.h"
#ifdef ENABLE_TIMING_TUNER_SCENE
#include "../protocols/protocol_items.h"
#include <gui/elements.h>
#include <math.h>

#define TAG "ProtoPirateTimingTuner"

#define MAX_TIMING_SAMPLES       512
#define VISIBLE_LINES            6
#define LINE_HEIGHT              9
#define SUBGHZ_RAW_THRESHOLD_MIN -90.0f

typedef struct {
    // Ring buffer for timing capture
    int32_t samples[MAX_TIMING_SAMPLES];
    size_t write_idx; // Next write position
    size_t sample_count; // Total samples captured (may exceed MAX)
    bool buffer_wrapped; // True if we've wrapped around

    // Timing statistics
    size_t short_count;
    size_t long_count;

    // Calculated stats
    int32_t avg_short;
    int32_t avg_long;
    int32_t min_short;
    int32_t max_short;
    int32_t min_long;
    int32_t max_long;

    // Protocol match info
    const char* matched_protocol;
    const ProtoPirateProtocolTiming* timing_info;

    // State
    bool is_receiving;
    bool has_match;
    uint16_t animation_frame;
    float rssi;
    uint8_t scroll_offset;
    uint8_t total_lines;

    // App reference for callback
    ProtoPirateApp* app;
} TimingTunerContext;

static TimingTunerContext* g_timing_ctx = NULL;

static void calculate_timing_stats(TimingTunerContext* ctx) {
    size_t num_samples = ctx->buffer_wrapped ? MAX_TIMING_SAMPLES : ctx->write_idx;

    if(num_samples < 10) {
        FURI_LOG_W(TAG, "Not enough samples: %zu", num_samples);
        return;
    }

    FURI_LOG_I(
        TAG,
        "Analyzing %zu samples (total captured: %zu, wrapped: %d)",
        num_samples,
        ctx->sample_count,
        ctx->buffer_wrapped);

    ctx->short_count = 0;
    ctx->long_count = 0;
    ctx->min_short = INT32_MAX;
    ctx->max_short = 0;
    ctx->min_long = INT32_MAX;
    ctx->max_long = 0;

    int64_t short_sum = 0;
    int64_t long_sum = 0;

    int32_t threshold;
    int32_t min_valid;
    int32_t max_valid;

    if(ctx->timing_info) {
        int32_t te_short = (int32_t)ctx->timing_info->te_short;
        int32_t te_long = (int32_t)ctx->timing_info->te_long;
        int32_t te_delta = (int32_t)ctx->timing_info->te_delta;

        // Threshold halfway between expected short and long
        threshold = (te_short + te_long) / 2;

        // Valid pulse range - use 2x delta as bounds
        min_valid = te_short - (te_delta * 2);
        if(min_valid < 100) min_valid = 100;
        max_valid = te_long + (te_delta * 2);

        FURI_LOG_I(
            TAG, "Protocol timing: threshold=%ld valid=%ld-%ld", threshold, min_valid, max_valid);

    } else {
        // No protocol info - use reasonable defaults
        threshold = 400;
        min_valid = 100;
        max_valid = 1200;

        FURI_LOG_I(
            TAG,
            "No protocol ref, using defaults: threshold=%ld valid=%ld-%ld",
            threshold,
            min_valid,
            max_valid);
    }

    // Analyze all samples in the ring buffer
    for(size_t i = 0; i < num_samples; i++) {
        int32_t dur = ctx->samples[i];
        if(dur < 0) dur = -dur;

        // Filter out noise and gaps
        if(dur < min_valid || dur > max_valid) continue;

        if(dur < threshold) {
            short_sum += dur;
            ctx->short_count++;
            if(dur < ctx->min_short) ctx->min_short = dur;
            if(dur > ctx->max_short) ctx->max_short = dur;
        } else {
            long_sum += dur;
            ctx->long_count++;
            if(dur < ctx->min_long) ctx->min_long = dur;
            if(dur > ctx->max_long) ctx->max_long = dur;
        }
    }

    // Calculate averages
    if(ctx->short_count > 0) {
        ctx->avg_short = (int32_t)(short_sum / (int64_t)ctx->short_count);
    } else {
        ctx->avg_short = 0;
        ctx->min_short = 0;
        ctx->max_short = 0;
    }

    if(ctx->long_count > 0) {
        ctx->avg_long = (int32_t)(long_sum / (int64_t)ctx->long_count);
    } else {
        ctx->avg_long = 0;
        ctx->min_long = 0;
        ctx->max_long = 0;
    }

    // Log results
    FURI_LOG_I(
        TAG,
        "MEASURED SHORT: avg=%ld min=%ld max=%ld n=%zu",
        ctx->avg_short,
        ctx->min_short,
        ctx->max_short,
        ctx->short_count);
    FURI_LOG_I(
        TAG,
        "MEASURED LONG: avg=%ld min=%ld max=%ld n=%zu",
        ctx->avg_long,
        ctx->min_long,
        ctx->max_long,
        ctx->long_count);

    if(ctx->timing_info && ctx->short_count > 0 && ctx->long_count > 0) {
#ifndef REMOVE_LOGS
        int32_t short_diff = ctx->avg_short - (int32_t)ctx->timing_info->te_short;
        int32_t long_diff = ctx->avg_long - (int32_t)ctx->timing_info->te_long;
        FURI_LOG_I(
            TAG,
            "DIFFERENCE: short=%+ld long=%+ld (tolerance +/-%lu)",
            short_diff,
            long_diff,
            ctx->timing_info->te_delta);
#endif
    }
}

static void timing_tuner_draw_listening(Canvas* canvas, TimingTunerContext* ctx) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Timing Tuner");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 18, AlignCenter, AlignTop, "Listening for signals...");

    int wave_y = 38;
    ctx->animation_frame++;
    for(int x = 0; x < 128; x++) {
        float phase = (float)(x + ctx->animation_frame * 3) * 0.15f;
        int y_offset = (int)(sinf(phase) * 8.0f);
        canvas_draw_dot(canvas, x, wave_y + y_offset);
    }

    {
        //RSSI Signal Bar
        uint8_t spacer = 0;
        for(uint8_t i = 1; i < (uint8_t)(ctx->rssi - SUBGHZ_RAW_THRESHOLD_MIN); i++) {
            if(i % 5) {
                uint8_t j = 46 + i + spacer;
                canvas_draw_dot(canvas, j, 58);
                canvas_draw_dot(canvas, j + 1, 59);
                canvas_draw_dot(canvas, j, 60);
            } else
                spacer++;
        }
    }

    canvas_set_font(canvas, FontSecondary);
    char rssi_str[24];
    snprintf(rssi_str, sizeof(rssi_str), "%.0f", (double)ctx->rssi);
    canvas_draw_str_aligned(canvas, 127, 62, AlignRight, AlignBottom, rssi_str);
    elements_button_left(canvas, "Config");
}

// Get a specific line of content
static bool
    get_result_line(TimingTunerContext* ctx, uint8_t line_idx, char* buf, size_t buf_size) {
    int32_t short_diff = 0;
    int32_t long_diff = 0;
    bool short_ok = false;
    bool long_ok = false;
    bool short_exact = false;
    bool long_exact = false;
    int32_t short_jitter = 0;
    int32_t long_jitter = 0;

    if(ctx->timing_info) {
        short_diff = ctx->avg_short - (int32_t)ctx->timing_info->te_short;
        long_diff = ctx->avg_long - (int32_t)ctx->timing_info->te_long;
        short_ok = (ctx->short_count > 0) &&
                   (abs(short_diff) <= (int32_t)ctx->timing_info->te_delta);
        long_ok = (ctx->long_count > 0) && (abs(long_diff) <= (int32_t)ctx->timing_info->te_delta);
        short_exact = (abs(short_diff) <= 15);
        long_exact = (abs(long_diff) <= 15);
    }
    short_jitter = ctx->max_short - ctx->min_short;
    long_jitter = ctx->max_long - ctx->min_long;

    if(ctx->timing_info) {
        switch(line_idx) {
        case 0:
            snprintf(buf, buf_size, "PROTOCOL DEFINITION:");
            return true;
        case 1:
            snprintf(buf, buf_size, "  Short: %lu us", ctx->timing_info->te_short);
            return true;
        case 2:
            snprintf(buf, buf_size, "  Long: %lu us", ctx->timing_info->te_long);
            return true;
        case 3:
            snprintf(buf, buf_size, "  Tolerance: +/-%lu us", ctx->timing_info->te_delta);
            return true;
        case 4:
            buf[0] = '\0';
            return true;
        case 5:
            snprintf(buf, buf_size, "RECEIVED SIGNAL:");
            return true;
        case 6:
            snprintf(buf, buf_size, "  Short Avg: %ld us", ctx->avg_short);
            return true;
        case 7:
            snprintf(buf, buf_size, "  Short Min: %ld us", ctx->min_short);
            return true;
        case 8:
            snprintf(buf, buf_size, "  Short Max: %ld us", ctx->max_short);
            return true;
        case 9:
            snprintf(buf, buf_size, "  Short Samples: %zu", ctx->short_count);
            return true;
        case 10:
            snprintf(buf, buf_size, "  Long Avg: %ld us", ctx->avg_long);
            return true;
        case 11:
            snprintf(buf, buf_size, "  Long Min: %ld us", ctx->min_long);
            return true;
        case 12:
            snprintf(buf, buf_size, "  Long Max: %ld us", ctx->max_long);
            return true;
        case 13:
            snprintf(buf, buf_size, "  Long Samples: %zu", ctx->long_count);
            return true;
        case 14:
            buf[0] = '\0';
            return true;
        case 15:
            snprintf(buf, buf_size, "ANALYSIS:");
            return true;
        case 16:
            snprintf(buf, buf_size, "  Short Diff: %+ld us", short_diff);
            return true;
        case 17:
            snprintf(buf, buf_size, "  Long Diff: %+ld us", long_diff);
            return true;
        case 18:
            snprintf(buf, buf_size, "  Short Jitter: %ld us", short_jitter);
            return true;
        case 19:
            snprintf(buf, buf_size, "  Long Jitter: %ld us", long_jitter);
            return true;
        case 20:
            buf[0] = '\0';
            return true;
        case 21:
            snprintf(buf, buf_size, "CONCLUSION:");
            return true;
        case 22:
            if(short_exact) {
                snprintf(buf, buf_size, "  Short: EXCELLENT");
            } else if(short_ok) {
                snprintf(buf, buf_size, "  Short: OK (%+ld)", short_diff);
            } else if(short_diff > 0) {
                snprintf(buf, buf_size, "  Short: HIGH by %ld", short_diff);
            } else {
                snprintf(buf, buf_size, "  Short: LOW by %ld", -short_diff);
            }
            return true;
        case 23:
            if(long_exact) {
                snprintf(buf, buf_size, "  Long: EXCELLENT");
            } else if(long_ok) {
                snprintf(buf, buf_size, "  Long: OK (%+ld)", long_diff);
            } else if(long_diff > 0) {
                snprintf(buf, buf_size, "  Long: HIGH by %ld", long_diff);
            } else {
                snprintf(buf, buf_size, "  Long: LOW by %ld", -long_diff);
            }
            return true;
        case 24:
            buf[0] = '\0';
            return true;
        case 25:
            if(short_exact && long_exact) {
                snprintf(buf, buf_size, "Timing matches fob!");
            } else if(short_ok && long_ok) {
                snprintf(buf, buf_size, "Within tolerance.");
            } else {
                snprintf(buf, buf_size, "NEEDS ADJUSTMENT:");
            }
            return true;
        case 26:
            if(short_exact && long_exact) {
                snprintf(buf, buf_size, "No changes needed.");
            } else if(short_ok && long_ok) {
                snprintf(buf, buf_size, "Consider fine-tuning.");
            } else if(!short_ok && !long_ok) {
                snprintf(buf, buf_size, "te_short=%ld", ctx->avg_short);
            } else if(!short_ok) {
                snprintf(buf, buf_size, "Set te_short=%ld", ctx->avg_short);
            } else {
                snprintf(buf, buf_size, "Set te_long=%ld", ctx->avg_long);
            }
            return true;
        case 27:
            if(!short_ok && !long_ok) {
                snprintf(buf, buf_size, "te_long=%ld", ctx->avg_long);
            } else {
                buf[0] = '\0';
            }
            return true;
        case 28:
            buf[0] = '\0';
            return true;
        case 29:
            snprintf(buf, buf_size, "OK:Retry  <:Config");
            return true;
        default:
            return false;
        }
    } else {
        // No timing reference
        switch(line_idx) {
        case 0:
            snprintf(buf, buf_size, "NO PROTOCOL REFERENCE");
            return true;
        case 1:
            buf[0] = '\0';
            return true;
        case 2:
            snprintf(buf, buf_size, "RECEIVED SIGNAL:");
            return true;
        case 3:
            snprintf(buf, buf_size, "  Short Avg: %ld us", ctx->avg_short);
            return true;
        case 4:
            snprintf(buf, buf_size, "  Short Min: %ld us", ctx->min_short);
            return true;
        case 5:
            snprintf(buf, buf_size, "  Short Max: %ld us", ctx->max_short);
            return true;
        case 6:
            snprintf(buf, buf_size, "  Short Samples: %zu", ctx->short_count);
            return true;
        case 7:
            snprintf(buf, buf_size, "  Long Avg: %ld us", ctx->avg_long);
            return true;
        case 8:
            snprintf(buf, buf_size, "  Long Min: %ld us", ctx->min_long);
            return true;
        case 9:
            snprintf(buf, buf_size, "  Long Max: %ld us", ctx->max_long);
            return true;
        case 10:
            snprintf(buf, buf_size, "  Long Samples: %zu", ctx->long_count);
            return true;
        case 11:
            buf[0] = '\0';
            return true;
        case 12:
            snprintf(buf, buf_size, "Short Jitter: %ld us", short_jitter);
            return true;
        case 13:
            snprintf(buf, buf_size, "Long Jitter: %ld us", long_jitter);
            return true;
        case 14:
            buf[0] = '\0';
            return true;
        case 15:
            snprintf(buf, buf_size, "Add to protocol_items.c");
            return true;
        case 16:
            buf[0] = '\0';
            return true;
        case 17:
            snprintf(buf, buf_size, "OK:Retry  <:Config");
            return true;
        default:
            return false;
        }
    }
}

static uint8_t count_result_lines(TimingTunerContext* ctx) {
    if(ctx->timing_info) {
        return 30; // Lines 0-29
    } else {
        return 18; // Lines 0-17
    }
}

static void timing_tuner_draw_results(Canvas* canvas, TimingTunerContext* ctx) {
    char line_buf[32];

    uint8_t total_lines = count_result_lines(ctx);
    ctx->total_lines = total_lines;

    uint8_t max_scroll = 0;
    if(total_lines > VISIBLE_LINES) {
        max_scroll = total_lines - VISIBLE_LINES;
    }
    if(ctx->scroll_offset > max_scroll) {
        ctx->scroll_offset = max_scroll;
    }

    // Draw header (protocol name)
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, ctx->matched_protocol);

    // Draw content lines
    canvas_set_font(canvas, FontSecondary);
    uint8_t y = 10;
    for(uint8_t i = 0; i < VISIBLE_LINES; i++) {
        uint8_t line_idx = ctx->scroll_offset + i;
        if(get_result_line(ctx, line_idx, line_buf, sizeof(line_buf))) {
            canvas_draw_str(canvas, 1, y + LINE_HEIGHT, line_buf);
        }
        y += LINE_HEIGHT;
    }

    // Scroll indicators
    if(total_lines > VISIBLE_LINES) {
        if(ctx->scroll_offset > 0) {
            canvas_draw_str_aligned(canvas, 126, 14, AlignRight, AlignTop, "^");
        }
        if(ctx->scroll_offset < max_scroll) {
            canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "v");
        }
    }
}

static void timing_tuner_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    TimingTunerContext* ctx = g_timing_ctx;
    if(!ctx) return;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(!ctx->has_match) {
        timing_tuner_draw_listening(canvas, ctx);
    } else {
        timing_tuner_draw_results(canvas, ctx);
    }
}

static bool timing_tuner_input_callback(InputEvent* event, void* context) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyBack:
            if(event->type == InputTypeShort) {
                view_dispatcher_send_custom_event(app->view_dispatcher, 0);
                consumed = true;
            }
            break;
        case InputKeyOk:
            if(event->type == InputTypeShort && g_timing_ctx && g_timing_ctx->has_match) {
                g_timing_ctx->has_match = false;
                g_timing_ctx->write_idx = 0;
                g_timing_ctx->sample_count = 0;
                g_timing_ctx->buffer_wrapped = false;
                g_timing_ctx->timing_info = NULL;
                g_timing_ctx->scroll_offset = 0;
                consumed = true;
            }
            break;
        case InputKeyLeft:
            if(event->type == InputTypeShort) {
                view_dispatcher_send_custom_event(app->view_dispatcher, 1);
                consumed = true;
            }
            break;
        case InputKeyUp:
            if(g_timing_ctx && g_timing_ctx->has_match && g_timing_ctx->scroll_offset > 0) {
                g_timing_ctx->scroll_offset--;
                consumed = true;
            }
            break;
        case InputKeyDown:
            if(g_timing_ctx && g_timing_ctx->has_match) {
                uint8_t max_scroll = 0;
                if(g_timing_ctx->total_lines > VISIBLE_LINES) {
                    max_scroll = g_timing_ctx->total_lines - VISIBLE_LINES;
                }
                if(g_timing_ctx->scroll_offset < max_scroll) {
                    g_timing_ctx->scroll_offset++;
                }
                consumed = true;
            }
            break;
        default:
            break;
        }
    }

    return consumed;
}

static void timing_tuner_rx_callback(
    SubGhzReceiver* receiver,
    SubGhzProtocolDecoderBase* decoder_base,
    void* context) {
    UNUSED(receiver);
    ProtoPirateApp* app = context;
    TimingTunerContext* ctx = g_timing_ctx;

    if(!ctx || ctx->has_match) return;

    const char* protocol_name = decoder_base->protocol->name;
    ctx->matched_protocol = protocol_name;

    FURI_LOG_I(TAG, "Matched protocol: %s", protocol_name);

    ctx->timing_info = protopirate_get_protocol_timing(protocol_name);

    if(ctx->timing_info) {
        FURI_LOG_I(
            TAG,
            "Found timing for %s: short=%lu, long=%lu, delta=%lu",
            ctx->timing_info->name,
            ctx->timing_info->te_short,
            ctx->timing_info->te_long,
            ctx->timing_info->te_delta);
    } else {
        FURI_LOG_W(TAG, "No timing info found for protocol: %s", protocol_name);
    }

    calculate_timing_stats(ctx);

    ctx->has_match = true;
    ctx->scroll_offset = 0;

    notification_message(app->notifications, &sequence_success);
}

static void timing_tuner_pair_callback(void* context, bool level, uint32_t duration) {
    UNUSED(context);
    TimingTunerContext* ctx = g_timing_ctx;

    if(ctx && !ctx->has_match) {
        // Ring buffer - always write, wrap around if needed
        ctx->samples[ctx->write_idx] = level ? (int32_t)duration : -(int32_t)duration;
        ctx->write_idx++;
        ctx->sample_count++;

        if(ctx->write_idx >= MAX_TIMING_SAMPLES) {
            ctx->write_idx = 0;
            ctx->buffer_wrapped = true;
        }
    }

    if(ctx && ctx->app && ctx->app->txrx && ctx->app->txrx->receiver) {
        subghz_receiver_decode(ctx->app->txrx->receiver, level, duration);
    }
}

void protopirate_scene_timing_tuner_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    FURI_LOG_I(TAG, "Entering Timing Tuner");

    g_timing_ctx = malloc(sizeof(TimingTunerContext));
    if(!g_timing_ctx) {
        FURI_LOG_E(TAG, "Failed to allocate timing tuner context");
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    memset(g_timing_ctx, 0, sizeof(TimingTunerContext));
    g_timing_ctx->is_receiving = false;
    g_timing_ctx->has_match = false;
    g_timing_ctx->rssi = -127.0f;
    g_timing_ctx->timing_info = NULL;
    g_timing_ctx->scroll_offset = 0;
    g_timing_ctx->total_lines = 0;
    g_timing_ctx->write_idx = 0;
    g_timing_ctx->sample_count = 0;
    g_timing_ctx->buffer_wrapped = false;
    g_timing_ctx->app = app;

    if(app->radio_initialized) {
        protopirate_rx_stack_resume_after_tx(app);
    }

    view_set_draw_callback(app->view_about, timing_tuner_draw_callback);
    view_set_input_callback(app->view_about, timing_tuner_input_callback);
    view_set_context(app->view_about, app);

    // Allocate worker
    if(!app->txrx->worker) {
        app->txrx->worker = subghz_worker_alloc();
        if(!app->txrx->worker) {
            FURI_LOG_E(TAG, "Failed to allocate worker!");
            return;
        }
        // Set up worker callbacks
        subghz_worker_set_overrun_callback(
            app->txrx->worker, (SubGhzWorkerOverrunCallback)subghz_receiver_reset);
        subghz_worker_set_pair_callback(
            app->txrx->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);
        subghz_worker_set_context(app->txrx->worker, app->txrx->receiver);
    }

    subghz_receiver_set_rx_callback(app->txrx->receiver, timing_tuner_rx_callback, app);

    subghz_worker_set_pair_callback(
        app->txrx->worker, (SubGhzWorkerPairCallback)timing_tuner_pair_callback);

    protopirate_begin(app, app->txrx->preset->data);
    protopirate_rx(app, app->txrx->preset->frequency);
    g_timing_ctx->is_receiving = true;

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewAbout);
}

bool protopirate_scene_timing_tuner_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 0) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == 1) {
            if(g_timing_ctx && g_timing_ctx->is_receiving) {
                protopirate_rx_end(app);
                g_timing_ctx->is_receiving = false;
            }
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneReceiverConfig);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(g_timing_ctx && g_timing_ctx->is_receiving && !g_timing_ctx->has_match) {
            if(app->txrx->radio_device) {
                g_timing_ctx->rssi = subghz_devices_get_rssi(app->txrx->radio_device);
            }
            // Blink the light like the SubGHZ app
            notification_message(app->notifications, &sequence_blink_cyan_10);
        }
        view_commit_model(app->view_about, true);
        consumed = true;
    }

    return consumed;
}

void protopirate_scene_timing_tuner_on_exit(void* context) {
    ProtoPirateApp* app = context;

    FURI_LOG_I(TAG, "Exiting Timing Tuner");

    if(g_timing_ctx && g_timing_ctx->is_receiving) {
        protopirate_rx_end(app);
    }

    subghz_worker_set_pair_callback(
        app->txrx->worker, (SubGhzWorkerPairCallback)subghz_receiver_decode);

    if(app->txrx->worker) {
        FURI_LOG_D(TAG, "Freeing worker %p", app->txrx->worker);
        subghz_worker_free(app->txrx->worker);
        app->txrx->worker = NULL;
    } else {
        FURI_LOG_D(TAG, "Worker was NULL, skipping free");
    }

    view_set_draw_callback(app->view_about, NULL);
    view_set_input_callback(app->view_about, NULL);

    if(g_timing_ctx) {
        free(g_timing_ctx);
        g_timing_ctx = NULL;
    }
}
#endif
