#include <furi.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <gui/elements.h>
#include <input/input.h>
#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <stdio.h>
#include <string.h>

#define TAG        "SmokeFree"
#define START_FILE "/ext/apps/Health/nie_pale_start.bin"
#define LOG_FILE   "/ext/apps/Health/nie_pale_log.txt"

typedef enum {
    NiePaleModeNoSmoke = 0,
    NiePaleModeBuch,
    NiePaleModeExit,
    NiePaleModeCount,
} NiePaleMode;

typedef struct {
    bool is_exit;
    bool running;
    bool menu_active;
    bool confirm_active;
    bool napewno;
    NiePaleMode menu_index;
    Storage* storage;
    uint32_t start_timestamp;
    uint32_t last_milestone;
    char status[96];
    char elapsed[64];
    char next_goal[64];
    char saved_text[64];
    char current_date[32];
    char current_time[32];
    char zegnaj[100];
} NiePaleApp;

static void clear_log(Storage* storage) {
    furi_check(storage);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, LOG_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return;
    }
    storage_file_close(file);
    storage_file_free(file);
}

static void append_log(Storage* storage, const char* text) {
    furi_check(storage);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_free(file);
        return;
    }
    storage_file_write(file, text, strlen(text));
    storage_file_write(file, "\r\n", 2);
    storage_file_close(file);
    storage_file_free(file);
}

static bool save_start_timestamp(Storage* storage, uint32_t timestamp) {
    furi_check(storage);
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, START_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        return false;
    }
    size_t written = storage_file_write(file, &timestamp, sizeof(timestamp));
    storage_file_close(file);

    bool result = (written == sizeof(timestamp));
    storage_file_free(file);
    return result;
}

static bool load_start_timestamp(Storage* storage, uint32_t* timestamp) {
    furi_check(storage);
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, START_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    bool result = storage_file_read(file, timestamp, sizeof(*timestamp)) == sizeof(*timestamp);
    storage_file_close(file);
    storage_file_free(file);
    return result;
}

static void format_datetime(uint32_t timestamp, char* buffer, size_t size) {
    DateTime dt;
    datetime_timestamp_to_datetime(timestamp, &dt);
    snprintf(
        buffer,
        size,
        "%04u-%02u-%02u %02u:%02u:%02u",
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
}

static void format_date_only(uint32_t timestamp, char* buffer, size_t size) {
    DateTime dt;
    datetime_timestamp_to_datetime(timestamp, &dt);
    snprintf(buffer, size, "%04u-%02u-%02u", dt.year, dt.month, dt.day);
}

static void format_time_only(uint32_t timestamp, char* buffer, size_t size) {
    DateTime dt;
    datetime_timestamp_to_datetime(timestamp, &dt);
    snprintf(buffer, size, "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
}

static void format_elapsed(uint32_t elapsed, char* buffer, size_t size) {
    const uint32_t days = elapsed / 86400;
    const uint32_t hours = (elapsed % 86400) / 3600;
    const uint32_t minutes = (elapsed % 3600) / 60;
    const uint32_t seconds = elapsed % 60;

    if(days > 0) {
        snprintf(buffer, size, "%lud %luh %lum %lus", days, hours, minutes, seconds);
    } else if(hours > 0) {
        snprintf(buffer, size, "%luh %lum %lus", hours, minutes, seconds);
    } else if(minutes > 0) {
        snprintf(buffer, size, "%lum %lus", minutes, seconds);
    } else {
        snprintf(buffer, size, "%lus", seconds);
    }
}

static void update_next_goal(uint32_t elapsed, char* buffer, size_t size) {
    if(elapsed < 3600) {
        snprintf(buffer, size, "Next goal: 1 hour");
    } else if(elapsed < 43200) {
        snprintf(buffer, size, "Next goal: 12 hours");
    } else if(elapsed < 86400) {
        snprintf(buffer, size, "Next goal: 1 day");
    } else if(elapsed < 604800) {
        snprintf(buffer, size, "Next goal: 7 days");
    } else {
        snprintf(buffer, size, "You're doing great! Keep it up");
    }
}

static void check_milestone(
    Storage* storage,
    uint32_t elapsed,
    uint32_t* last_milestone,
    char* status,
    size_t status_size) {
    const uint32_t thresholds[] = {604800, 86400, 43200, 3600};
    const char* messages[] = {
        "7 days smoke-free! Brilliant!",
        "1 day smoke-free! Congrats!",
        "12 hours smoke-free! Awesome!",
        "1 hour down! You've got this!",
    };

    for(size_t i = 0; i < sizeof(thresholds) / sizeof(thresholds[0]); i++) {
        if(elapsed >= thresholds[i] && *last_milestone < thresholds[i]) {
            *last_milestone = thresholds[i];
            snprintf(status, status_size, "%s", messages[i]);

            char log_line[128];
            char time_text[64];
            format_datetime(furi_hal_rtc_get_timestamp(), time_text, sizeof(time_text));
            snprintf(log_line, sizeof(log_line), "%s - %s", time_text, messages[i]);

            if(storage) {
                append_log(storage, log_line);
            }
            return;
        }
    }
    status[0] = '\0';
}

static void draw(Canvas* canvas, void* context) {
    NiePaleApp* app = context;
    canvas_clear(canvas);

    if(app->is_exit) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, app->zegnaj);
        return;
    }

    if(app->menu_active) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Smoke Free - Menu");
        canvas_draw_line(canvas, 0, 13, 128, 13);
        canvas_set_font(canvas, FontSecondary);

        if(app->confirm_active) {
            canvas_set_font(canvas, FontPrimary);

            if(!app->napewno) {
                canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "Are you sure?");
            } else {
                canvas_draw_str_aligned(
                    canvas, 64, 20, AlignCenter, AlignCenter, "Are you 100% sure?!");
            }

            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, 64, 42, AlignCenter, AlignCenter, "[OK] Yes  |  [Back] No");
            return;
        }

        if(app->menu_index == NiePaleModeNoSmoke) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 4, 16, 120, 15);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignCenter, "Continue smoke-free");
        canvas_set_color(canvas, ColorBlack);

        if(app->menu_index == NiePaleModeBuch) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 4, 34, 120, 15);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignCenter, "Reset counter (Puff)");
        canvas_set_color(canvas, ColorBlack);

        if(app->menu_index == NiePaleModeExit) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 4, 52, 120, 15);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 64, 59, AlignCenter, AlignCenter, "Exit");
        canvas_set_color(canvas, ColorBlack);

        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "Smoke Free - Counter");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 2, 16, AlignLeft, AlignCenter, app->current_date);
    canvas_draw_str_aligned(canvas, 126, 16, AlignRight, AlignCenter, app->current_time);

    canvas_draw_line(canvas, 0, 22, 128, 22);
    canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignCenter, "Time since last smoke:");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignCenter, app->elapsed);

    canvas_set_font(canvas, FontSecondary);

    if(app->status[0] != '\0') {
        canvas_set_color(canvas, ColorBlack);
        elements_slightly_rounded_box(canvas, 2, 50, 124, 13);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, app->status);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, app->next_goal);
    }
}

static void reset_session(NiePaleApp* app, const char* reason) {
    furi_check(app);
    furi_check(app->storage);
    const uint32_t now = furi_hal_rtc_get_timestamp();
    app->start_timestamp = now;
    app->last_milestone = 0;
    format_elapsed(0, app->elapsed, sizeof(app->elapsed));
    update_next_goal(0, app->next_goal, sizeof(app->next_goal));
    snprintf(app->status, sizeof(app->status), "%s", reason);

    save_start_timestamp(app->storage, now);
    clear_log(app->storage);

    char time_text[64];
    format_datetime(now, time_text, sizeof(time_text));
    char log_line[128];
    snprintf(log_line, sizeof(log_line), "%s - %s", time_text, reason);
    append_log(app->storage, log_line);
}

static void input_callback(InputEvent* event, void* context) {
    NiePaleApp* app = context;

    if(event->type != InputTypePress) {
        return;
    }

    if(app->menu_active) {
        if(app->confirm_active) {
            if(event->key == InputKeyOk) {
                if(!app->napewno) {
                    app->napewno = true;
                } else {
                    reset_session(app, "Puff - counter reset");
                    app->napewno = false;
                    app->confirm_active = false;
                    app->menu_active = false;
                    snprintf(app->saved_text, sizeof(app->saved_text), "Puff saved to SD");
                }
            } else if(event->key == InputKeyBack) {
                if(app->napewno) {
                    app->napewno = false;
                    app->confirm_active = false;
                }
                if(app->confirm_active) {
                    app->confirm_active = false;
                }
            }

            return;
        }

        if(event->key == InputKeyLeft || event->key == InputKeyUp) {
            if(app->menu_index == 0) {
                app->menu_index = NiePaleModeCount - 1;
            } else {
                app->menu_index = (NiePaleMode)(app->menu_index - 1);
            }
        } else if(event->key == InputKeyRight || event->key == InputKeyDown) {
            app->menu_index = (NiePaleMode)((app->menu_index + 1) % NiePaleModeCount);
        } else if(event->key == InputKeyOk) {
            if(app->menu_index == NiePaleModeNoSmoke) {
                app->menu_active = false;
                snprintf(app->saved_text, sizeof(app->saved_text), "Continue smoke-free");
                app->status[0] = '\0';
            } else if(app->menu_index == NiePaleModeBuch) {
                app->confirm_active = true;
                app->napewno = false;
            } else if(app->menu_index == NiePaleModeExit) {
                app->is_exit = true;
            }
        } else if(event->key == InputKeyBack) {
            app->menu_active = false;
            snprintf(app->saved_text, sizeof(app->saved_text), "Selection cancelled");
        }
        return;
    }

    if(event->key == InputKeyOk || event->key == InputKeyBack) {
        app->menu_active = true;
        app->menu_index = NiePaleModeNoSmoke;
    }
}

int32_t nie_pale(void* p) {
    UNUSED(p);

    NiePaleApp app = {
        .running = true,
        .is_exit = false,
        .menu_active = true,
        .menu_index = NiePaleModeNoSmoke,
        .storage = NULL,
        .start_timestamp = 0,
        .last_milestone = 0,
        .status = "",
        .elapsed = "",
        .next_goal = "",
        .saved_text = "Press OK to select",
        .current_date = "",
        .current_time = "",
        .confirm_active = false,
        .napewno = false,
        .zegnaj = "NOT WORTH IT !!!"};

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        app.storage = NULL;
        strncpy(app.saved_text, "Error: cannot open Storage", sizeof(app.saved_text) - 1);
    } else {
        app.storage = storage;
        uint32_t start_ts = 0;

        if(load_start_timestamp(storage, &start_ts)) {
            app.start_timestamp = start_ts;
            snprintf(app.saved_text, sizeof(app.saved_text), "Loaded previous start from SD");
        } else {
            app.start_timestamp = furi_hal_rtc_get_timestamp();
            save_start_timestamp(storage, app.start_timestamp);

            char time_text[64];
            format_datetime(app.start_timestamp, time_text, sizeof(time_text));
            char log_line[128];
            snprintf(log_line, sizeof(log_line), "%s - Started new smoke-free session", time_text);
            append_log(storage, log_line);
            snprintf(app.saved_text, sizeof(app.saved_text), "New session saved to SD");
        }
    }

    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();

    view_port_draw_callback_set(view_port, draw, &app);
    view_port_input_callback_set(view_port, input_callback, &app);

    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(app.running) {
        uint32_t now = furi_hal_rtc_get_timestamp();
        format_date_only(now, app.current_date, sizeof(app.current_date));
        format_time_only(now, app.current_time, sizeof(app.current_time));

        if(app.is_exit) {
            view_port_update(view_port);
            furi_delay_ms(3000);
            app.running = false;
            continue;
        }

        if(!app.menu_active) {
            if(now < app.start_timestamp) {
                now = app.start_timestamp;
            }
            uint32_t elapsed = now - app.start_timestamp;
            format_elapsed(elapsed, app.elapsed, sizeof(app.elapsed));
            update_next_goal(elapsed, app.next_goal, sizeof(app.next_goal));
            check_milestone(
                app.storage, elapsed, &app.last_milestone, app.status, sizeof(app.status));
        }

        view_port_update(view_port);
        furi_delay_ms(200);
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    if(storage) {
        furi_record_close(RECORD_STORAGE);
    }

    return 0;
}
