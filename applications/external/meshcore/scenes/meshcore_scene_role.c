/*
 * scene_role — honest about what the Flipper cannot do.
 *
 * A node's role (companion, repeater, room server) is chosen when it is
 * flashed. The companion serial protocol has setters for the name, radio,
 * tx power, position and path-hash mode, but none for the role — so there is
 * nothing to edit here. Rather than a menu item that silently does nothing,
 * this says why, and points at the thing that actually changes it.
 */
#include "../meshcore_cfg.h"

void meshcore_scene_role_on_enter(void* context) {
    MeshCoreApp* app = context;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget,
        0,
        0,
        128,
        64,
        "\e#Role\n"
        "Companion / repeater is set\n"
        "when the node is flashed, and\n"
        "the serial protocol has no\n"
        "command to change it.\n\n"
        "To change the role, reflash\n"
        "the node with that firmware.");
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
}

bool meshcore_scene_role_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void meshcore_scene_role_on_exit(void* context) {
    MeshCoreApp* app = context;
    widget_reset(app->widget);
}
