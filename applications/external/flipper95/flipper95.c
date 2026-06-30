#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <cli/cli.h>
#include <toolbox/args.h>
#include <toolbox/strint.h>

#include <furi_hal_power.h>

#include <stdatomic.h>

//#undef MBEDTLS_CONFIG_FILE
//#define MBEDTLS_CONFIG_FILE "mbedtls_cfg.h"
#include <mbedtls/bignum.h>

#define CLI_COMMAND                     "flipper95"
#define CLI_COMMAND_ADVANCE             "advance"
#define CLI_COMMAND_LAST_PRIME          "prime"
#define CLI_COMMAND_LAST_PERFECT_NUMBER "perfect_number"

typedef struct {
    FuriPubSub* input;
    FuriPubSubSubscription* input_subscription;
    Gui* gui;
    Canvas* canvas;
    CliRegistry* cli;
    FuriThreadList* thread_list;

    // These are protected by the mutex
    FuriMutex* state_mutex;
    uint32_t cur_mnumber; // Current Mersenne number analyzed
    uint32_t cur_mprime; // Last Mersenne prime found
    char* mprime_str;
    size_t mprime_str_len;

    atomic_uint_least8_t stop; // Bitmask, 1 = stop app, 2 = stop current number
} Flipper95;

static void gui_input_events_callback(const void* value, void* ctx) {
    furi_assert(value);
    furi_assert(ctx);

    Flipper95* instance = ctx;
    const InputEvent* event = value;

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        atomic_store_explicit(&instance->stop, 1, memory_order_relaxed);
    }
}

static bool thread_signal_callback(uint32_t signal, void* arg, void* context) {
    UNUSED(arg);
    if(signal == FuriSignalExit) {
        Flipper95* instance = context;
        atomic_store_explicit(&instance->stop, 1, memory_order_relaxed);
        return true;
    }
    return false;
}

static void flipper95_cli_print_usage() {
    printf("Stress test your Flipper by calculating Mersenne primes, same as Prime95 does\r\n"
           "\r\n"
           "Usage:\r\n" CLI_COMMAND " <cmd> <args>\r\n"
           "Cmd list:\r\n"
           "\t" CLI_COMMAND_ADVANCE " M<p:int> - Advance calculations to a Mersenne number M_p\r\n"
           "\t" CLI_COMMAND_LAST_PRIME "\t\t - Print the last found Mersenne prime\r\n"
           "\t" CLI_COMMAND_LAST_PERFECT_NUMBER "\t - Print the last found perfect number\r\n");
}

static bool flipper95_cli_set_mnumber(const FuriString* args, Flipper95* instance) {
    // TODO: furi_string_start_withi when it's available
    if(furi_string_size(args) > 2 && furi_string_start_with(args, "M")) {
        uint32_t number;
        if(strint_to_uint32(furi_string_get_cstr(args) + 1, NULL, &number, 10) ==
           StrintParseNoError) {
            furi_check(furi_mutex_acquire(instance->state_mutex, FuriWaitForever) == FuriStatusOk);
            instance->cur_mnumber = number;
            furi_check(furi_mutex_release(instance->state_mutex) == FuriStatusOk);
            atomic_fetch_or_explicit(&instance->stop, 2, memory_order_relaxed);
            return true;
        }
    }
    return false;
}

static bool flipper95_cli_print_prime(const Flipper95* instance) {
    furi_check(furi_mutex_acquire(instance->state_mutex, FuriWaitForever) == FuriStatusOk);
    printf("M%lu = %s\r\n", instance->cur_mprime, instance->mprime_str);
    furi_check(furi_mutex_release(instance->state_mutex) == FuriStatusOk);
    return true;
}

static bool flipper95_cli_print_perfect_number(const Flipper95* instance) {
    furi_check(furi_mutex_acquire(instance->state_mutex, FuriWaitForever) == FuriStatusOk);
    const uint32_t last_prime = instance->cur_mprime;
    furi_check(furi_mutex_release(instance->state_mutex) == FuriStatusOk);

    // Perfect number for M^p = 2^(p - 1) x (2^p  - 1)
    mbedtls_mpi M_p, Two_p;
    mbedtls_mpi_init(&M_p);
    mbedtls_mpi_init(&Two_p);

    mbedtls_mpi_lset(&Two_p, 1);
    mbedtls_mpi_shift_l(&Two_p, last_prime - 1); // 2^(p - 1)

    mbedtls_mpi_copy(&M_p, &Two_p);
    mbedtls_mpi_shift_l(&M_p, 1); // 2^p = 2^(p - 1) << 1
    mbedtls_mpi_sub_int(&M_p, &M_p, 1); // 2^p - 1

    mbedtls_mpi_mul_mpi(&M_p, &M_p, &Two_p);

    // Allocate space
    size_t space_needed = 0;
    mbedtls_mpi_write_string(&M_p, 10, NULL, 0, &space_needed);

    char* buffer = malloc(space_needed);
    furi_check(mbedtls_mpi_write_string(&M_p, 10, buffer, space_needed, &space_needed) == 0);
    printf("Perfect%lu = %s\r\n", last_prime, buffer);
    free(buffer);

    mbedtls_mpi_free(&Two_p);
    mbedtls_mpi_free(&M_p);

    return true;
}

static void flipper95_cli_callback(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    Flipper95* instance = context;

    FuriString* cmd = furi_string_alloc();
    bool success = false;
    if(args_read_string_and_trim(args, cmd)) {
        if(furi_string_equal(cmd, CLI_COMMAND_ADVANCE)) {
            success = flipper95_cli_set_mnumber(args, instance);
        } else if(furi_string_equal(cmd, CLI_COMMAND_LAST_PRIME)) {
            success = flipper95_cli_print_prime(instance);
        } else if(furi_string_equal(cmd, CLI_COMMAND_LAST_PERFECT_NUMBER)) {
            success = flipper95_cli_print_perfect_number(instance);
        }
    }

    if(!success) {
        flipper95_cli_print_usage();
    }
    furi_string_free(cmd);
}

static float flipper95_get_cpu_usage(Flipper95* instance) {
    if(furi_thread_enumerate(instance->thread_list)) {
        const FuriThreadListItem* thread_metrics =
            furi_thread_list_get_or_insert(instance->thread_list, furi_thread_get_current());
        return CLAMP(thread_metrics->cpu, 100.0f, 0.0f);
    }
    return 0.0f;
}

static void flipper95_init(Flipper95* instance) {
    instance->input = furi_record_open(RECORD_INPUT_EVENTS);
    instance->gui = furi_record_open(RECORD_GUI);
    instance->canvas = gui_direct_draw_acquire(instance->gui);
    instance->cli = furi_record_open(RECORD_CLI);

    instance->input_subscription =
        furi_pubsub_subscribe(instance->input, gui_input_events_callback, instance);

    instance->thread_list = furi_thread_list_alloc();

    instance->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    instance->cur_mnumber = 2;
    instance->cur_mprime = 0;
    instance->mprime_str_len = 256;
    instance->mprime_str = malloc(instance->mprime_str_len);
    instance->mprime_str[0] = '\0';

    atomic_init(&instance->stop, 0);
}

static void flipper95_deinit(Flipper95* instance) {
    free(instance->mprime_str);
    furi_mutex_free(instance->state_mutex);

    furi_thread_list_free(instance->thread_list);

    furi_pubsub_unsubscribe(instance->input, instance->input_subscription);

    furi_record_close(RECORD_CLI);
    gui_direct_draw_release(instance->gui);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_INPUT_EVENTS);
}

bool trial_division_is_prime(uint32_t n) {
    if(n == 2) return true;
    if((n % 2) == 0) return false;
    const uint32_t sqrt_n = (unsigned int)sqrt((double)n);
    for(uint32_t x = 3; x <= sqrt_n; x += 2) {
        if((n % x) == 0) {
            return false;
        }
    }
    return true;
}

static void canvas_draw_ascii_str_wrapped_ellipsis(
    Canvas* canvas,
    const int32_t x,
    const int32_t y,
    const int32_t wrap_x,
    const int32_t max_y,
    const char* str) {
    const int32_t font_height = 8;
    int32_t cur_x = x;
    int32_t cur_y = y;

    bool overflow = false;
    const int32_t ellipsis_width = canvas_string_width(canvas, "...");
    while(*str != '\0') {
        const int32_t glyph_width = canvas_glyph_width(canvas, *str);
        const bool last_line = cur_y + font_height > max_y;
        const int32_t max_width_for_line = !last_line ? wrap_x : wrap_x - ellipsis_width;
        if(cur_x + glyph_width > max_width_for_line) {
            if(last_line) {
                overflow = true;
                break;
            }
            cur_x = x;
            cur_y += font_height;
        }
        canvas_draw_glyph(canvas, cur_x, cur_y, *str);
        cur_x += glyph_width;
        str++;
    }

    if(overflow) {
        canvas_draw_str(canvas, cur_x, cur_y, "...");
    }
}

static void flipper95_run(Flipper95* instance) {
    furi_thread_set_current_priority(FuriThreadPriorityIdle);
    furi_thread_set_signal_callback(furi_thread_get_current(), thread_signal_callback, instance);
    cli_registry_add_command(
        instance->cli, CLI_COMMAND, CliCommandFlagParallelSafe, flipper95_cli_callback, instance);
    furi_hal_power_insomnia_enter();

    canvas_reset(instance->canvas);
    canvas_set_font(instance->canvas, FontSecondary);

    mbedtls_mpi M_p, S, temp;
    mbedtls_mpi_init(&M_p);
    mbedtls_mpi_init(&S);
    mbedtls_mpi_init(&temp);

    uint32_t hardware_status_y = 8;
    uint32_t prime_status_y = 16;
    uint32_t prime_display_y = 24;
    bool hardware_status_multiline = true;

    do {
        // Current Mersenne number may have been advanced by the user via CLI when we weren't looking
        furi_check(furi_mutex_acquire(instance->state_mutex, FuriWaitForever) == FuriStatusOk);
        uint32_t p = instance->cur_mnumber;
        furi_check(furi_mutex_release(instance->state_mutex) == FuriStatusOk);
        const uint32_t last_number =
            p; // We'll use this later to check if the user has advanced the calculations themselves

        // Take the next prime p
        while(!trial_division_is_prime(p)) {
            ++p;
        }

        // Print the canvas BEFORE starting the iteration, so the current number is updated before
        // we start heavy calculations.
        char buffer[64];
        snprintf(buffer, sizeof(buffer), ">M%lu", p);
        canvas_draw_str_aligned(
            instance->canvas, 128, prime_status_y, AlignRight, AlignTop, buffer);

        canvas_commit(instance->canvas);
        canvas_clear(instance->canvas);

        mbedtls_mpi_lset(&M_p, 1);
        mbedtls_mpi_shift_l(&M_p, p); // 2^p
        mbedtls_mpi_sub_int(&M_p, &M_p, 1); // 2^p - 1

        // Perform the LL test
        mbedtls_mpi_lset(&S, 4);
        for(uint32_t i = 0;
            i < p - 2 && atomic_load_explicit(&instance->stop, memory_order_relaxed) == 0;
            i++) {
            // S = (S^2 - 2) % M_p
            mbedtls_mpi_mul_mpi(&temp, &S, &S); // temp = S^2
            mbedtls_mpi_sub_int(&temp, &temp, 2); // temp = S^2 - 2
            mbedtls_mpi_mod_mpi(&S, &temp, &M_p); // S = (S^2 - 2) % M_p
        }

        bool is_prime = false;
        // Clear the skip flag, if present, and skip this number if it was set
        if((atomic_fetch_and_explicit(&instance->stop, ~2, memory_order_relaxed) & 2) == 0) {
            // Now update all the values we usually read from under the lock.
            // We can render the Mersenne prime without a lock, as this is the only place where it can update.
            is_prime = mbedtls_mpi_cmp_int(&S, 0) == 0;
            furi_check(furi_mutex_acquire(instance->state_mutex, FuriWaitForever) == FuriStatusOk);
            if(instance->cur_mnumber == last_number) {
                instance->cur_mnumber = p + 1;
            }
            if(is_prime) {
                instance->cur_mprime = p;

                size_t required_buffer_len = 0;
                mbedtls_mpi_write_string(
                    &M_p, 10, instance->mprime_str, instance->mprime_str_len, &required_buffer_len);
                if(required_buffer_len > instance->mprime_str_len) {
                    free(instance->mprime_str);
                    instance->mprime_str = malloc(required_buffer_len);
                    instance->mprime_str_len = required_buffer_len;
                }
                furi_check(
                    mbedtls_mpi_write_string(
                        &M_p,
                        10,
                        instance->mprime_str,
                        instance->mprime_str_len,
                        &required_buffer_len) == 0);
            }
            furi_check(furi_mutex_release(instance->state_mutex) == FuriStatusOk);
        }

        // Shift here as that's where we begin a new "frame" - if we shift displays up here,
        //  >Mxx will go up in the next iteration too
        if(hardware_status_multiline && instance->cur_mprime >= 500) {
            hardware_status_multiline = false;
            hardware_status_y -= 8;
            prime_status_y -= 8;
            prime_display_y -= 8;
        }

        // Just to be safe, opt for shorter display if we are operating on primes big enough
        const bool short_display = p >= 100000 || instance->cur_mprime >= 100000;
        snprintf(
            buffer,
            sizeof(buffer),
            !short_display ? "Last prime: M%lu" : "P: M%lu",
            instance->cur_mprime);
        canvas_draw_str_aligned(instance->canvas, 0, prime_status_y, AlignLeft, AlignTop, buffer);

        // Render the prime one character at a time, with line breaks and ellipsis
        canvas_draw_ascii_str_wrapped_ellipsis(
            instance->canvas, 0, prime_display_y + 8, 128, 64, instance->mprime_str);

        // Once everything else is done, display CPU usage and battery status
        const char* cpu_usage_str;
        const char* battery_consumption_str;
        const char* battery_charging_str;
        const char* battery_full_str;
        const char* battery_level_str;
        if(hardware_status_multiline) {
            canvas_draw_str_aligned(instance->canvas, 0, 0, AlignLeft, AlignTop, "CPU:");
            canvas_draw_str_aligned(instance->canvas, 64, 0, AlignCenter, AlignTop, "Draw:");
            canvas_draw_str_aligned(instance->canvas, 128, 0, AlignRight, AlignTop, "Battery:");

            cpu_usage_str = "%.1f%%";
            battery_consumption_str = "%ldmA";
            battery_level_str = "%u%%";
            battery_charging_str = "Charging";
            battery_full_str = "Charged";
        } else {
            cpu_usage_str = "C: %.1f%%";
            battery_consumption_str = "A: %ldmA";
            battery_level_str = "B: %u%%";
            battery_charging_str = "A: Chrg";
            battery_full_str = "A: Full";
        }

        const int32_t battery_consumption =
            furi_hal_power_get_battery_current(FuriHalPowerICFuelGauge) * 1000;

        snprintf(buffer, sizeof(buffer), cpu_usage_str, (double)flipper95_get_cpu_usage(instance));
        canvas_draw_str_aligned(
            instance->canvas, 0, hardware_status_y, AlignLeft, AlignTop, buffer);
        if(battery_consumption > 0) {
            strlcpy(buffer, battery_charging_str, sizeof(buffer));
        } else if(battery_consumption < -5) {
            snprintf(buffer, sizeof(buffer), battery_consumption_str, -battery_consumption);
        } else {
            strlcpy(buffer, battery_full_str, sizeof(buffer));
        }
        canvas_draw_str_aligned(
            instance->canvas, 64, hardware_status_y, AlignCenter, AlignTop, buffer);
        snprintf(buffer, sizeof(buffer), battery_level_str, furi_hal_power_get_pct());
        canvas_draw_str_aligned(
            instance->canvas, 128, hardware_status_y, AlignRight, AlignTop, buffer);

        furi_thread_yield();
    } while((atomic_load_explicit(&instance->stop, memory_order_relaxed) & 1) == 0);

    mbedtls_mpi_free(&M_p);
    mbedtls_mpi_free(&S);
    mbedtls_mpi_free(&temp);

    furi_hal_power_insomnia_exit();
    cli_registry_delete_command(instance->cli, CLI_COMMAND);
}

int32_t flipper95_app(void* p) {
    UNUSED(p);

    Flipper95 instance;
    flipper95_init(&instance);
    flipper95_run(&instance);
    flipper95_deinit(&instance);

    return 0;
}
