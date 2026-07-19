#pragma once

#include <gui/view.h>
#include "../helpers/protocols.h"

/* The animated walkthrough. Each protocol is a short deck of steps; every step
 * draws a live, looping diagram of what is happening on the wire plus a one-line
 * caption. Left / Right (or OK) move between steps; Back leaves. The animation
 * is driven by lesson_view_tick() from the scene's UI tick. */

typedef struct LessonView LessonView;

LessonView* lesson_view_alloc(void);
void lesson_view_free(LessonView* v);
View* lesson_view_get_view(LessonView* v);

void lesson_view_set_protocol(LessonView* v, RosettaProtocol p); // resets to step 0
void lesson_view_tick(LessonView* v); // advance one animation frame
