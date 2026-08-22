#pragma once

#include <gui/view.h>
#include "chord_db.h"

typedef struct ChordView ChordView;

/** Draw just the fretboard diagram (grid, markers, barre, dots). Shared with
 *  the practice screen, which hides the finger row to free up the bottom band. */
void chord_diagram_draw(Canvas* canvas, const Chord* chord, bool with_fingers);

/** delta is -1 or +1: user asked for the previous/next voicing. */
typedef void (*ChordViewStepCallback)(void* context, int delta);

ChordView* chord_view_alloc(void);
void chord_view_free(ChordView* cv);
View* chord_view_get_view(ChordView* cv);

/** Left/Right: previous/next voicing of the chord being shown. */
void chord_view_set_step_callback(ChordView* cv, ChordViewStepCallback cb, void* context);

/** Up/Down: previous/next chord in the current root's list. */
void chord_view_set_chord_step_callback(ChordView* cv, ChordViewStepCallback cb, void* context);

/** Show `chord` as voicing `index` of `count`. */
void chord_view_set_chord(ChordView* cv, const Chord* chord, uint8_t index, uint8_t count);
