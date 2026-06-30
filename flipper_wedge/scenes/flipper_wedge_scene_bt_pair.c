#include "../flipper_wedge.h"
#include <gui/elements.h>

typedef struct {
    Widget* widget;
} BtPairSceneContext;

static void flipper_wedge_scene_bt_pair_rebuild_widget(FlipperWedge* app, Widget* widget) {
    widget_reset(widget);

    // Title
    widget_add_string_element(
        widget,
        64,
        4,
        AlignCenter,
        AlignTop,
        FontPrimary,
        "Bluetooth Pairing");

    // Instructions
    widget_add_string_element(
        widget,
        4,
        18,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "1. Open Bluetooth settings");
    widget_add_string_element(
        widget,
        4,
        28,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "   on your device");
    widget_add_string_element(
        widget,
        4,
        38,
        AlignLeft,
        AlignTop,
        FontSecondary,
        "2. Select 'HID-[name]'");

    // Connection status
    bool bt_connected = flipper_wedge_hid_is_bt_connected(flipper_wedge_get_hid(app));
    if(bt_connected) {
        widget_add_string_element(
            widget,
            4,
            52,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "Status: Connected!");
    } else {
        widget_add_string_element(
            widget,
            4,
            52,
            AlignLeft,
            AlignTop,
            FontSecondary,
            "Status: Waiting...");
    }
}

void flipper_wedge_scene_bt_pair_on_enter(void* context) {
    FlipperWedge* app = context;

    // Keep display backlight on while pairing
    notification_message(app->notification, &sequence_display_backlight_enforce_on);

    // Allocate scene context
    BtPairSceneContext* scene_ctx = malloc(sizeof(BtPairSceneContext));
    scene_ctx->widget = widget_alloc();

    // Build initial widget content
    flipper_wedge_scene_bt_pair_rebuild_widget(app, scene_ctx->widget);

    // Add view and switch to it
    view_dispatcher_add_view(
        app->view_dispatcher,
        FlipperWedgeViewIdBtPair,
        widget_get_view(scene_ctx->widget));
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperWedgeViewIdBtPair);

    // Store scene context
    scene_manager_set_scene_state(
        app->scene_manager,
        FlipperWedgeSceneBtPair,
        (uint32_t)scene_ctx);
}

bool flipper_wedge_scene_bt_pair_on_event(void* context, SceneManagerEvent event) {
    FlipperWedge* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        // Update widget on tick to show connection status changes
        BtPairSceneContext* scene_ctx = (BtPairSceneContext*)scene_manager_get_scene_state(
            app->scene_manager,
            FlipperWedgeSceneBtPair);

        if(scene_ctx && scene_ctx->widget) {
            flipper_wedge_scene_bt_pair_rebuild_widget(app, scene_ctx->widget);
        }
        consumed = true;
    }

    return consumed;
}

void flipper_wedge_scene_bt_pair_on_exit(void* context) {
    FlipperWedge* app = context;

    // Retrieve scene context
    BtPairSceneContext* scene_ctx = (BtPairSceneContext*)scene_manager_get_scene_state(
        app->scene_manager,
        FlipperWedgeSceneBtPair);

    if(scene_ctx) {
        if(scene_ctx->widget) {
            view_dispatcher_remove_view(app->view_dispatcher, FlipperWedgeViewIdBtPair);
            widget_free(scene_ctx->widget);
        }
        free(scene_ctx);
    }

    scene_manager_set_scene_state(app->scene_manager, FlipperWedgeSceneBtPair, 0);

    // Return backlight to auto mode
    notification_message(app->notification, &sequence_display_backlight_enforce_auto);
}
