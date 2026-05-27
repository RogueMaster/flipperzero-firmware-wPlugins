/* Custom canvas for the About scene: a small description on top and the
 * project's GitHub URL emphasised below. The stock Popup widget can't mix
 * fonts the way we want, so this owns its own draw callback. */

#pragma once

#include <gui/view.h>

typedef struct AboutCanvas AboutCanvas;

AboutCanvas* about_canvas_alloc(void);
void         about_canvas_free(AboutCanvas* a);
View*        about_canvas_get_view(AboutCanvas* a);
