#pragma once

/* Custom events posted into the view dispatcher. Menu selections use their own
 * small indices, so app events start well clear of them. */
typedef enum {
    ETEventAnswer = 100, /* the highlighted answer was confirmed */
    ETEventPrev, /* move the highlight left */
    ETEventNext, /* move the highlight right */
    ETEventReplay, /* replay the current interval */
    ETEventHint, /* spend a hint */
    ETEventFeedbackDone, /* feedback banner expired */
} EarTrainerEvent;
