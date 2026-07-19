/**
 * Gauntlet - the challenge reader.
 *
 * The hero screen for a single challenge: an inverted header strip with the
 * category, difficulty pips and points, the title, and a scrollable body that
 * holds the prompt, the payload (DATA) and - once revealed - the hint. OK opens
 * flag entry, Right opens the Toolkit, Left toggles the hint, Up/Down scroll.
 */
#pragma once

#include <gui/view.h>
#include "../helpers/ctf_pack.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ChallengeEventFlag, // OK    -> enter a flag
    ChallengeEventTools, // Right -> open the Toolkit
    ChallengeEventHint, // Left  -> toggle the hint
} ChallengeEvent;

typedef void (*ChallengeViewCallback)(void* context, ChallengeEvent event);

typedef struct ChallengeView ChallengeView;

ChallengeView* challenge_view_alloc(void);
void challenge_view_free(ChallengeView* v);
View* challenge_view_get_view(ChallengeView* v);
void challenge_view_set_callback(ChallengeView* v, ChallengeViewCallback cb, void* context);

/** Load a challenge into the view. `body` is the composed, ready-to-wrap text
 *  (prompt + data + optional hint). Resets the scroll to the top. */
void challenge_view_set(
    ChallengeView* v,
    const char* title,
    const char* body,
    CtfCategory category,
    CtfDifficulty difficulty,
    uint16_t points,
    bool solved,
    bool has_hint,
    bool hint_shown);

#ifdef __cplusplus
}
#endif
