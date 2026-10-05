/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * A report from an earlier read.
 *
 * The file is read in small pieces into one string with a hard ceiling: a
 * report is a few kilobytes, but nothing stops somebody pointing this at a
 * larger file, and the heap here is shared with everything else.
 */
#include "../emrtd_i.h"

/** As much of a report as is worth holding at once. */
#define EMRTD_REPORT_MAX_BYTES (4096)
#define EMRTD_REPORT_CHUNK     (128)

/** Appended when the report is longer than the ceiling. */
#define EMRTD_REPORT_TRUNCATED "\n... the rest is on the card."

/*
 * The most of a file the loop below can take in: the last read may carry it up
 * to a chunk past the ceiling.
 */
#define EMRTD_REPORT_READ_MAX (EMRTD_REPORT_MAX_BYTES + EMRTD_REPORT_CHUNK)

static void emrtd_scene_saved_detail_load(Emrtd* app) {
    FuriString* text = app->text_box_store;
    furi_string_reset(text);

    File* file = storage_file_alloc(app->storage);

    if(!storage_file_open(
           file, furi_string_get_cstr(app->file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        furi_string_printf(text, "Cannot open\n%s", furi_string_get_cstr(app->file_path));
    } else {
        /*
         * One allocation of the most the text can reach: what the loop can
         * take in, the note and the terminator. A string that grows by
         * appending is reallocated each time it outgrows its buffer. For a
         * full report that is seven allocations, six of them while the one
         * before is still held (docs/platform.md, item 24). The file's own
         * size keeps a short one from taking the whole ceiling.
         */
        const uint64_t file_size = storage_file_size(file);
        furi_string_reserve(
            text,
            (size_t)MIN(file_size, (uint64_t)EMRTD_REPORT_READ_MAX) +
                sizeof(EMRTD_REPORT_TRUNCATED));

        char chunk[EMRTD_REPORT_CHUNK + 1];
        size_t total = 0;

        while(total < EMRTD_REPORT_MAX_BYTES) {
            const size_t read = storage_file_read(file, chunk, sizeof(chunk) - 1);
            if(read == 0) {
                break;
            }

            /* A report is text, but the file on the card is whatever is on the
             * card; anything unprintable is replaced so that the text box is
             * given a string and not a surprise. */
            for(size_t i = 0; i < read; i++) {
                if(chunk[i] == '\0' || (chunk[i] < ' ' && chunk[i] != '\n' && chunk[i] != '\t')) {
                    chunk[i] = '.';
                }
            }
            chunk[read] = '\0';

            furi_string_cat_str(text, chunk);
            total += read;
        }

        if(total >= EMRTD_REPORT_MAX_BYTES && !storage_file_eof(file)) {
            furi_string_cat_str(text, EMRTD_REPORT_TRUNCATED);
        }
        if(total == 0) {
            furi_string_set_str(text, "The file is empty.");
        }

        storage_file_close(file);
    }

    storage_file_free(file);
}

void emrtd_scene_saved_detail_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    emrtd_scene_saved_detail_load(app);

    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    /* The text box keeps the pointer it is given, so the string it points at
     * stays untouched until on_exit. */
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewTextBox);
}

bool emrtd_scene_saved_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_saved_detail_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
