#pragma once

#include "../seos_i.h"

/* Shared parts of the scenes that are only a popup with a timeout.
 *
 * Four of them were the same forty lines apart from an icon, a caption and
 * where they returned to.
 */

typedef struct {
    const Icon* icon; /* optional */
    int icon_x;
    int icon_y;
    const char* header;
    int header_x;
    int header_y;
    const char* text; /* optional second line */
    int text_x;
    int text_y;
    Align horizontal;
    Align vertical;
    uint32_t timeout_ms;
} SeosScenePopup;

/* Shows the popup and arranges for the timeout to end the scene. */
void seos_scene_popup_enter(Seos* seos, const SeosScenePopup* config);

/* Returns to `target_scene` when the popup times out. */
bool seos_scene_popup_event(Seos* seos, SceneManagerEvent event, uint32_t target_scene);

/* Clears the popup. */
void seos_scene_popup_exit(Seos* seos);
