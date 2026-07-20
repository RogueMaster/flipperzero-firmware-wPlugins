#pragma once

#include <gui/view.h>
#include "../helpers/emitter_classify.h"
#include "../helpers/field_detector.h"

typedef struct FingerprintView FingerprintView;

typedef void (*FingerprintViewCallback)(void* context);

FingerprintView* fingerprint_view_alloc(void);
void fingerprint_view_free(FingerprintView* v);
View* fingerprint_view_get_view(FingerprintView* v);

/* Short OK saves the current fingerprint to the logbook; long OK restarts the
 * measurement. */
void fingerprint_view_set_save_callback(FingerprintView* v, FingerprintViewCallback cb, void* ctx);
void fingerprint_view_set_reset_callback(FingerprintView* v, FingerprintViewCallback cb, void* ctx);

void fingerprint_view_update(FingerprintView* v, const FieldStats* stats);

/* Briefly overprint a word in the header ("LOGGED", "RESET"). */
void fingerprint_view_flash(FingerprintView* v, const char* msg);

/* Advance animation / decay the flash (call on the UI tick). */
void fingerprint_view_tick(FingerprintView* v);
