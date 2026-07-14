#include "../vk_thermo.h"
#include "vk_thermo_log_view.h"
#include <furi.h>
#include <furi_hal.h>
#include <input/input.h>
#include <gui/elements.h>
#include <datetime/datetime.h>

#define VISIBLE_ENTRIES 4
#define MAX_UNIQUE_UIDS 10

struct VkThermoLogView {
    View* view;
    VkThermoLogViewCallback callback;
    void* context;
};

typedef struct {
    VkThermoLog* log;
    uint32_t temp_unit;
    uint8_t scroll_offset;
    uint8_t unique_uids[MAX_UNIQUE_UIDS][VK_THERMO_UID_LENGTH];
    uint8_t uid_count;
    uint8_t selected_uid_index;
} VkThermoLogViewModel;

void vk_thermo_log_view_set_callback(
    VkThermoLogView* instance,
    VkThermoLogViewCallback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

static bool uids_equal(const uint8_t* uid1, const uint8_t* uid2) {
    return memcmp(uid1, uid2, VK_THERMO_UID_LENGTH) == 0;
}

static bool uid_is_zero(const uint8_t* uid) {
    for(size_t i = 0; i < VK_THERMO_UID_LENGTH; i++) {
        if(uid[i] != 0) return false;
    }
    return true;
}

static void build_uid_list(VkThermoLogViewModel* model) {
    model->uid_count = 0;

    if(!model->log || model->log->count == 0) return;

    for(uint8_t i = 0; i < model->log->count && model->uid_count < MAX_UNIQUE_UIDS; i++) {
        int idx = (model->log->head - model->log->count + i + VK_THERMO_LOG_MAX_ENTRIES) %
                  VK_THERMO_LOG_MAX_ENTRIES;
        VkThermoLogEntry* entry = &model->log->entries[idx];

        if(!entry->valid || uid_is_zero(entry->uid)) continue;

        bool found = false;
        for(uint8_t j = 0; j < model->uid_count; j++) {
            if(uids_equal(model->unique_uids[j], entry->uid)) {
                found = true;
                break;
            }
        }

        if(!found) {
            memcpy(model->unique_uids[model->uid_count], entry->uid, VK_THERMO_UID_LENGTH);
            model->uid_count++;
        }
    }

    if(model->selected_uid_index >= model->uid_count && model->uid_count > 0) {
        model->selected_uid_index = 0;
    }
}

// Count entries matching selected UID (for scroll bounds)
static uint8_t count_entries_for_uid(VkThermoLogViewModel* model) {
    if(!model->log || model->log->count == 0 || model->uid_count == 0) return 0;

    const uint8_t* uid = model->unique_uids[model->selected_uid_index];
    uint8_t count = 0;

    for(uint8_t i = 0; i < model->log->count; i++) {
        int idx = (model->log->head - model->log->count + i + VK_THERMO_LOG_MAX_ENTRIES) %
                  VK_THERMO_LOG_MAX_ENTRIES;
        VkThermoLogEntry* entry = &model->log->entries[idx];
        if(entry->valid && uids_equal(entry->uid, uid)) {
            count++;
        }
    }
    return count;
}

static void format_time_entry(uint32_t timestamp, char* buffer, size_t buffer_size) {
    DateTime dt;
    datetime_timestamp_to_datetime(timestamp, &dt);

    // Get current time to check if it's today
    DateTime now;
    datetime_timestamp_to_datetime(furi_hal_rtc_get_timestamp(), &now);

    if(dt.year == now.year && dt.month == now.month && dt.day == now.day) {
        // Today - show time only
        snprintf(buffer, buffer_size, "%02d:%02d", dt.hour, dt.minute);
    } else {
        // Different day - show date
        snprintf(
            buffer,
            buffer_size,
            "%s %d",
            (dt.month == 1  ? "Jan" :
             dt.month == 2  ? "Feb" :
             dt.month == 3  ? "Mar" :
             dt.month == 4  ? "Apr" :
             dt.month == 5  ? "May" :
             dt.month == 6  ? "Jun" :
             dt.month == 7  ? "Jul" :
             dt.month == 8  ? "Aug" :
             dt.month == 9  ? "Sep" :
             dt.month == 10 ? "Oct" :
             dt.month == 11 ? "Nov" :
                              "Dec"),
            dt.day);
    }
}

static void vk_thermo_log_view_draw(Canvas* canvas, VkThermoLogViewModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(!model->log || model->log->count == 0 || model->uid_count == 0) {
        // Empty state
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "No readings yet");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "Scan an thermo");
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    // Header left: UID selector with index
    const uint8_t* selected_uid = model->unique_uids[model->selected_uid_index];
    char uid_str[8];
    vk_thermo_uid_to_short_string(selected_uid, uid_str, sizeof(uid_str));

    char selector_str[24];
    if(model->uid_count > 1) {
        snprintf(
            selector_str,
            sizeof(selector_str),
            "< %s %d/%d >",
            uid_str,
            model->selected_uid_index + 1,
            model->uid_count);
    } else {
        snprintf(selector_str, sizeof(selector_str), "%s", uid_str);
    }
    canvas_draw_str(canvas, 0, 8, selector_str);

    // Header right: min-max stats for selected UID
    float min_temp = vk_thermo_log_get_min_for_uid(model->log, selected_uid);
    float max_temp = vk_thermo_log_get_max_for_uid(model->log, selected_uid);

    char stats_str[16];
    if(model->temp_unit == VkThermoTempUnitFahrenheit) {
        min_temp = vk_thermo_celsius_to_fahrenheit(min_temp);
        max_temp = vk_thermo_celsius_to_fahrenheit(max_temp);
    } else if(model->temp_unit == VkThermoTempUnitKelvin) {
        min_temp = vk_thermo_celsius_to_kelvin(min_temp);
        max_temp = vk_thermo_celsius_to_kelvin(max_temp);
    }
    snprintf(stats_str, sizeof(stats_str), "%.1f-%.1f", (double)min_temp, (double)max_temp);
    canvas_draw_str_aligned(canvas, 127, 8, AlignRight, AlignBottom, stats_str);

    canvas_draw_line(canvas, 0, 10, 128, 10);

    // Draw filtered entries (newest first), centered time+temp pair
    uint8_t y = 20;
    uint8_t displayed = 0;
    uint8_t skipped = 0; // For scroll offset

    for(int i = model->log->count - 1; i >= 0 && displayed < VISIBLE_ENTRIES; i--) {
        int idx = (model->log->head - model->log->count + i + VK_THERMO_LOG_MAX_ENTRIES) %
                  VK_THERMO_LOG_MAX_ENTRIES;
        VkThermoLogEntry* entry = &model->log->entries[idx];

        if(!entry->valid) continue;
        if(!uids_equal(entry->uid, selected_uid)) continue;

        // Skip entries before scroll offset
        if(skipped < model->scroll_offset) {
            skipped++;
            continue;
        }

        char time_str[16];
        char temp_str[16];

        format_time_entry(entry->timestamp, time_str, sizeof(time_str));

        const char* unit_letter;
        bool has_degree;
        if(model->temp_unit == VkThermoTempUnitFahrenheit) {
            float f = vk_thermo_celsius_to_fahrenheit(entry->temperature_celsius);
            snprintf(temp_str, sizeof(temp_str), "%.1f", (double)f);
            unit_letter = "F";
            has_degree = true;
        } else if(model->temp_unit == VkThermoTempUnitKelvin) {
            float k = vk_thermo_celsius_to_kelvin(entry->temperature_celsius);
            snprintf(temp_str, sizeof(temp_str), "%.2f", (double)k);
            unit_letter = "K";
            has_degree = false;
        } else {
            snprintf(temp_str, sizeof(temp_str), "%.2f", (double)entry->temperature_celsius);
            unit_letter = "C";
            has_degree = true;
        }

        // Centered time+temp pair (time at X=25, temp at X=67)
        canvas_draw_str(canvas, 25, y, time_str);
        canvas_draw_str(canvas, 67, y, temp_str);
        int16_t tx = 67 + canvas_string_width(canvas, temp_str);
        if(has_degree) {
            canvas_draw_circle(canvas, tx + 2, y - 6, 1);
            tx += 5;
        }
        canvas_draw_str(canvas, tx, y, unit_letter);

        y += 10;
        displayed++;
    }

    // Scroll indicators
    uint8_t uid_entry_count = count_entries_for_uid(model);
    if(model->scroll_offset > 0) {
        canvas_draw_str(canvas, 120, 16, "^");
    }
    if(model->scroll_offset + VISIBLE_ENTRIES < uid_entry_count) {
        canvas_draw_str(canvas, 120, 52, "v");
    }

    // No bottom button hints - Back key is standard, UID cycling shown in header
}

static bool vk_thermo_log_view_input(InputEvent* event, void* context) {
    furi_assert(context);
    VkThermoLogView* instance = context;

    // Long-press OK to clear history
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        instance->callback(VkThermoCustomEventLogClear, instance->context);
        return true;
    }

    if(event->type == InputTypeRelease || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyBack:
            if(event->type == InputTypeRelease) {
                instance->callback(VkThermoCustomEventLogBack, instance->context);
            }
            return true;

        case InputKeyUp:
            with_view_model(
                instance->view,
                VkThermoLogViewModel * model,
                {
                    if(model->scroll_offset > 0) {
                        model->scroll_offset--;
                    }
                },
                true);
            return true;

        case InputKeyDown:
            with_view_model(
                instance->view,
                VkThermoLogViewModel * model,
                {
                    uint8_t uid_count = count_entries_for_uid(model);
                    if(model->scroll_offset + VISIBLE_ENTRIES < uid_count) {
                        model->scroll_offset++;
                    }
                },
                true);
            return true;

        case InputKeyRight:
            if(event->type == InputTypeRelease) {
                with_view_model(
                    instance->view,
                    VkThermoLogViewModel * model,
                    {
                        if(model->uid_count > 1) {
                            model->selected_uid_index =
                                (model->selected_uid_index + 1) % model->uid_count;
                            model->scroll_offset = 0;
                        }
                    },
                    true);
            }
            return true;

        case InputKeyLeft:
            if(event->type == InputTypeRelease) {
                with_view_model(
                    instance->view,
                    VkThermoLogViewModel * model,
                    {
                        if(model->uid_count > 1) {
                            model->selected_uid_index =
                                (model->selected_uid_index + model->uid_count - 1) %
                                model->uid_count;
                            model->scroll_offset = 0;
                        }
                    },
                    true);
            }
            return true;

        default:
            break;
        }
    }

    return false;
}

VkThermoLogView* vk_thermo_log_view_alloc(void) {
    VkThermoLogView* instance = malloc(sizeof(VkThermoLogView));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(VkThermoLogViewModel));
    view_set_context(instance->view, instance);
    view_set_draw_callback(instance->view, (ViewDrawCallback)vk_thermo_log_view_draw);
    view_set_input_callback(instance->view, vk_thermo_log_view_input);

    with_view_model(
        instance->view,
        VkThermoLogViewModel * model,
        {
            model->log = NULL;
            model->temp_unit = VkThermoTempUnitCelsius;
            model->scroll_offset = 0;
            model->uid_count = 0;
            model->selected_uid_index = 0;
        },
        true);

    return instance;
}

void vk_thermo_log_view_free(VkThermoLogView* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* vk_thermo_log_view_get_view(VkThermoLogView* instance) {
    furi_assert(instance);
    return instance->view;
}

void vk_thermo_log_view_set_log(VkThermoLogView* instance, VkThermoLog* log) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        VkThermoLogViewModel * model,
        {
            model->log = log;
            model->scroll_offset = 0;
            build_uid_list(model);
        },
        true);
}

void vk_thermo_log_view_set_temp_unit(VkThermoLogView* instance, uint32_t temp_unit) {
    furi_assert(instance);
    with_view_model(
        instance->view, VkThermoLogViewModel * model, { model->temp_unit = temp_unit; }, true);
}
