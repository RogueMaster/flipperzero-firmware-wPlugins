/**
 * Gauntlet - the result screen.
 *
 * Two faces of one view: a celebratory "FLAG CAPTURED" burst with the points
 * awarded, new score and rank, or a "NOT QUITE" shake when the flag is wrong.
 * Animated off the scene tick. OK (or Back) leaves.
 */
#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ResultEventContinue,
} ResultEvent;

typedef void (*ResultViewCallback)(void* context, ResultEvent event);

typedef struct ResultView ResultView;

ResultView* result_view_alloc(void);
void result_view_free(ResultView* v);
View* result_view_get_view(ResultView* v);
void result_view_set_callback(ResultView* v, ResultViewCallback cb, void* context);

/** Configure the screen. `points`/`score`/`rank` are only shown when correct. */
void result_view_set(
    ResultView* v,
    bool correct,
    uint16_t points,
    uint32_t score,
    const char* rank);

/** Advance the animation one frame (call from the scene tick). */
void result_view_tick(ResultView* v);

#ifdef __cplusplus
}
#endif
