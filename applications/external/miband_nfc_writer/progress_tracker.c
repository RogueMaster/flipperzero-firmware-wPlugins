/**
 * @file progress_tracker.c
 * @brief Progress tracker implementation
 */

#include "progress_tracker.h"
#include <furi_hal_rtc.h>

/**
 * @brief Progress tracker internal structure
 */
struct ProgressTracker {
    uint32_t total_items;
    uint32_t completed_items;
    char operation_name[32];
    uint32_t start_time;
    uint32_t last_update_time;
    FuriString* display_buffer; // Persistent buffer for popup text
};

ProgressTracker* progress_tracker_alloc(uint32_t total_items, const char* operation_name) {
    ProgressTracker* tracker = malloc(sizeof(ProgressTracker));
    if(!tracker) return NULL;

    tracker->total_items = total_items;
    tracker->completed_items = 0;
    tracker->start_time = furi_get_tick();
    tracker->last_update_time = tracker->start_time;

    strncpy(tracker->operation_name, operation_name, 31);
    tracker->operation_name[31] = '\0';

    tracker->display_buffer = furi_string_alloc();
    if(!tracker->display_buffer) {
        free(tracker);
        return NULL;
    }

    return tracker;
}

void progress_tracker_free(ProgressTracker* tracker) {
    if(tracker) {
        if(tracker->display_buffer) furi_string_free(tracker->display_buffer);
        free(tracker);
    }
}

void progress_tracker_update(ProgressTracker* tracker, uint32_t completed_items) {
    tracker->completed_items = completed_items;
    tracker->last_update_time = furi_get_tick();
}

void progress_tracker_increment(ProgressTracker* tracker) {
    tracker->completed_items++;
    tracker->last_update_time = furi_get_tick();
}

uint8_t progress_tracker_get_percentage(const ProgressTracker* tracker) {
    if(tracker->total_items == 0) return 0;
    return (tracker->completed_items * 100) / tracker->total_items;
}

FuriString* progress_tracker_get_text(const ProgressTracker* tracker) {
    FuriString* text = furi_string_alloc();
    uint8_t percentage = progress_tracker_get_percentage(tracker);

    furi_string_printf(
        text,
        "%s: %lu/%lu (%u%%)",
        tracker->operation_name,
        tracker->completed_items,
        tracker->total_items,
        percentage);

    return text;
}

FuriString* progress_tracker_get_bar(const ProgressTracker* tracker, uint8_t width) {
    FuriString* bar = furi_string_alloc();
    uint8_t percentage = progress_tracker_get_percentage(tracker);
    uint8_t filled = (percentage * width) / 100;

    furi_string_cat_str(bar, "[");
    for(uint8_t i = 0; i < width; i++) {
        if(i < filled) {
            furi_string_cat_str(bar, "=");
        } else if(i == filled && percentage < 100) {
            furi_string_cat_str(bar, ">");
        } else {
            furi_string_cat_str(bar, " ");
        }
    }
    furi_string_cat_str(bar, "]");

    return bar;
}

uint32_t progress_tracker_get_eta_seconds(const ProgressTracker* tracker) {
    if(tracker->completed_items == 0) return 0;

    uint32_t elapsed_ms = furi_get_tick() - tracker->start_time;
    uint32_t ms_per_item = elapsed_ms / tracker->completed_items;
    uint32_t remaining_items = tracker->total_items - tracker->completed_items;
    uint32_t eta_ms = ms_per_item * remaining_items;

    return eta_ms / 1000; // Convert to seconds
}

void progress_tracker_update_popup(ProgressTracker* tracker, Popup* popup, const char* header) {
    if(header) {
        popup_set_header(popup, header, 64, 2, AlignCenter, AlignTop);
    }

    // FIX: Use persistent display_buffer instead of local FuriString.
    // popup_set_text stores a pointer, NOT a copy - freeing a local
    // string would leave a dangling pointer in the popup.
    furi_string_reset(tracker->display_buffer);

    // Progress text
    uint8_t percentage = progress_tracker_get_percentage(tracker);
    furi_string_printf(
        tracker->display_buffer,
        "%s: %lu/%lu (%u%%)",
        tracker->operation_name,
        tracker->completed_items,
        tracker->total_items,
        percentage);

    furi_string_cat_str(tracker->display_buffer, "\n\n");

    // Progress bar inline
    uint8_t filled = (percentage * 20) / 100;
    furi_string_cat_str(tracker->display_buffer, "[");
    for(uint8_t i = 0; i < 20; i++) {
        if(i < filled) {
            furi_string_cat_str(tracker->display_buffer, "=");
        } else if(i == filled && percentage < 100) {
            furi_string_cat_str(tracker->display_buffer, ">");
        } else {
            furi_string_cat_str(tracker->display_buffer, " ");
        }
    }
    furi_string_cat_str(tracker->display_buffer, "]");

    // ETA if available
    uint32_t eta = progress_tracker_get_eta_seconds(tracker);
    if(eta > 0 && tracker->completed_items > 5) {
        furi_string_cat_printf(tracker->display_buffer, "\n\nETA: %lu seconds", eta);
    }

    popup_set_text(
        popup, furi_string_get_cstr(tracker->display_buffer), 4, 18, AlignLeft, AlignTop);
}
