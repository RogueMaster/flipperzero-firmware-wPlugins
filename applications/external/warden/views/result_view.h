#pragma once

#include <gui/view.h>
#include "../helpers/grader.h"

/* The verdict card: a big letter grade, a segmented risk meter, the card name
 * and a one-line headline. OK opens the full breakdown; Right re-scans. */

typedef struct ResultView ResultView;

typedef enum {
    ResultEventDetails, // OK pressed
    ResultEventRescan, // Right pressed
} ResultEvent;

typedef void (*ResultViewCallback)(void* context, ResultEvent event);

ResultView* result_view_alloc(void);
void result_view_free(ResultView* v);
View* result_view_get_view(ResultView* v);

void result_view_set_callback(ResultView* v, ResultViewCallback cb, void* context);
void result_view_set_grade(ResultView* v, const CardGrade* grade);
