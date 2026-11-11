#include "ir_maker_draft.h"

#include <flipper_format/flipper_format.h>
#include <infrared_worker.h> // MAX_TIMINGS_AMOUNT

/* On-disk format: identical to the stock Infrared app so the file is a normal
 * playable .ir remote. */
#define IR_MAKER_FILE_HEADER  "IR signals file"
#define IR_MAKER_FILE_VERSION (1U)

/* Number of raw timings a signal contributes to the shared budget (0 for a
 * parsed signal). */
static size_t ir_maker_signal_raw_cost(const InfraredSignal* signal) {
    if(!infrared_signal_is_raw(signal)) return 0;
    return infrared_signal_get_raw_signal(signal)->timings_size;
}

IrMakerDraft* ir_maker_draft_alloc(void) {
    IrMakerDraft* draft = malloc(sizeof(IrMakerDraft));
    draft->count = 0;
    draft->raw_timings_used = 0;
    draft->remote_name = furi_string_alloc();
    for(size_t i = 0; i < IR_MAKER_MAX_BUTTONS; i++) {
        draft->items[i].name = NULL;
        draft->items[i].signal = NULL;
    }
    return draft;
}

void ir_maker_draft_reset(IrMakerDraft* draft) {
    furi_assert(draft);
    for(size_t i = 0; i < draft->count; i++) {
        if(draft->items[i].name) {
            furi_string_free(draft->items[i].name);
            draft->items[i].name = NULL;
        }
        if(draft->items[i].signal) {
            infrared_signal_free(draft->items[i].signal);
            draft->items[i].signal = NULL;
        }
    }
    draft->count = 0;
    draft->raw_timings_used = 0;
}

void ir_maker_draft_free(IrMakerDraft* draft) {
    furi_assert(draft);
    ir_maker_draft_reset(draft);
    furi_string_free(draft->remote_name);
    free(draft);
}

bool ir_maker_draft_add(IrMakerDraft* draft, const char* name, const InfraredSignal* signal) {
    furi_assert(draft);
    furi_assert(signal);

    if(draft->count >= IR_MAKER_MAX_BUTTONS) return false;

    const size_t raw_cost = ir_maker_signal_raw_cost(signal);
    if(draft->raw_timings_used + raw_cost > IR_MAKER_RAW_BUDGET) return false;

    IrMakerButton* button = &draft->items[draft->count];
    button->name = furi_string_alloc_set(name);
    button->signal = infrared_signal_alloc();
    infrared_signal_set_signal(button->signal, signal);

    draft->raw_timings_used += raw_cost;
    draft->count++;
    return true;
}

void ir_maker_draft_set_name(IrMakerDraft* draft, size_t idx, const char* name) {
    furi_assert(draft);
    furi_check(idx < draft->count);
    furi_string_set(draft->items[idx].name, name);
}

bool ir_maker_draft_set_signal(IrMakerDraft* draft, size_t idx, const InfraredSignal* signal) {
    furi_assert(draft);
    furi_assert(signal);
    furi_check(idx < draft->count);

    const size_t old_cost = ir_maker_signal_raw_cost(draft->items[idx].signal);
    const size_t new_cost = ir_maker_signal_raw_cost(signal);

    /* Guard against a raw-for-raw swap that pushes the total over budget. */
    if(draft->raw_timings_used - old_cost + new_cost > IR_MAKER_RAW_BUDGET) return false;

    infrared_signal_set_signal(draft->items[idx].signal, signal);
    draft->raw_timings_used = draft->raw_timings_used - old_cost + new_cost;
    return true;
}

void ir_maker_draft_delete(IrMakerDraft* draft, size_t idx) {
    furi_assert(draft);
    furi_check(idx < draft->count);

    draft->raw_timings_used -= ir_maker_signal_raw_cost(draft->items[idx].signal);
    furi_string_free(draft->items[idx].name);
    infrared_signal_free(draft->items[idx].signal);

    for(size_t i = idx; i + 1 < draft->count; i++) {
        draft->items[i] = draft->items[i + 1];
    }
    draft->count--;
    draft->items[draft->count].name = NULL;
    draft->items[draft->count].signal = NULL;
}

void ir_maker_draft_move(IrMakerDraft* draft, size_t idx, size_t new_idx) {
    furi_assert(draft);
    if(draft->count == 0) return;
    if(idx >= draft->count) return;
    if(new_idx >= draft->count) return;
    if(idx == new_idx) return;

    IrMakerButton moved = draft->items[idx];
    if(new_idx > idx) {
        for(size_t i = idx; i < new_idx; i++) {
            draft->items[i] = draft->items[i + 1];
        }
    } else {
        for(size_t i = idx; i > new_idx; i--) {
            draft->items[i] = draft->items[i - 1];
        }
    }
    draft->items[new_idx] = moved;
}

InfraredErrorCode ir_maker_draft_save(IrMakerDraft* draft, Storage* storage, const char* path) {
    furi_assert(draft);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    InfraredErrorCode error = InfraredErrorCodeNone;

    do {
        if(!flipper_format_file_open_always(ff, path)) {
            error = InfraredErrorCodeFileOperationFailed;
            break;
        }
        if(!flipper_format_write_header_cstr(ff, IR_MAKER_FILE_HEADER, IR_MAKER_FILE_VERSION)) {
            error = InfraredErrorCodeFileOperationFailed;
            break;
        }

        for(size_t i = 0; i < draft->count; i++) {
            const IrMakerButton* button = &draft->items[i];
            /* Skip any button that somehow holds an invalid signal rather than
             * emitting a malformed record. */
            if(!infrared_signal_is_valid(button->signal)) continue;

            error = infrared_signal_save(button->signal, ff, furi_string_get_cstr(button->name));
            if(INFRARED_ERROR_PRESENT(error)) break;
        }
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
    return error;
}

InfraredErrorCode ir_maker_draft_load(IrMakerDraft* draft, Storage* storage, const char* path) {
    furi_assert(draft);
    FlipperFormat* ff = flipper_format_buffered_file_alloc(storage);
    FuriString* header = furi_string_alloc();
    FuriString* name = furi_string_alloc();
    InfraredSignal* signal = infrared_signal_alloc();
    InfraredErrorCode error = InfraredErrorCodeNone;

    do {
        if(!flipper_format_buffered_file_open_existing(ff, path)) {
            error = InfraredErrorCodeFileOperationFailed;
            break;
        }

        uint32_t version = 0;
        if(!flipper_format_read_header(ff, header, &version)) {
            error = InfraredErrorCodeFileOperationFailed;
            break;
        }
        if(!furi_string_equal_str(header, IR_MAKER_FILE_HEADER)) {
            error = InfraredErrorCodeWrongFileType;
            break;
        }

        while(true) {
            const InfraredErrorCode read_error = infrared_signal_read(signal, ff, name);
            if(INFRARED_ERROR_PRESENT(read_error)) break; // out of signals / EOF

            if(!ir_maker_draft_add(draft, furi_string_get_cstr(name), signal)) {
                break; // hit MAX_BUTTONS or the raw budget: keep what we have
            }
        }
    } while(false);

    infrared_signal_free(signal);
    furi_string_free(name);
    furi_string_free(header);
    flipper_format_free(ff);
    return error;
}
