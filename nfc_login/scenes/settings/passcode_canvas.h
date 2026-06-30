#pragma once

#include "nfc_login_app.h"
#include <gui/view.h>

View* passcode_canvas_view_alloc(App* app);
void passcode_canvas_view_free(View* view);

