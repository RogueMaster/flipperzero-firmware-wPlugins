#pragma once

#include "../flippass.h"

typedef enum {
    FlipPassEditorIconsUnhandled = 0,
    FlipPassEditorIconsHandled,
    FlipPassEditorIconsClosed,
} FlipPassEditorIconsEventResult;

/* These callbacks belong to EditorCrud, pinned only while the borrowed browser
 * is visible. Detach/reset, return to Editor, unpin/unmap, then rebuild the form. */
void flippass_editor_icons_open(App* app);
FlipPassEditorIconsEventResult flippass_editor_icons_on_event(App* app, SceneManagerEvent event);
void flippass_editor_icons_reset(App* app);
