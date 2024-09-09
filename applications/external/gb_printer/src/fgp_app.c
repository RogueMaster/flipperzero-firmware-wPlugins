#include <furi.h>

#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>

#include <src/include/fgp_app.h>
#include <src/scenes/include/fgp_scene.h>

#include <gblink.h>
#include <protocols/printer_proto.h>

bool fgp_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    struct fgp_app* fgp = context;

    return scene_manager_handle_custom_event(fgp->scene_manager, event);
}

bool fgp_back_event_callback(void* context) {
    furi_assert(context);
    struct fgp_app* fgp = context;
    return scene_manager_handle_back_event(fgp->scene_manager);
}

static struct fgp_app* fgp_alloc(void) {
    struct fgp_app* fgp = NULL;

    fgp = malloc(sizeof(struct fgp_app));

    fgp->gblink_handle = gblink_alloc();
    fgp->printer_handle = printer_alloc(fgp->gblink_handle, &fgp->data);

    // View Dispatcher
    fgp->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(fgp->view_dispatcher, fgp);
    view_dispatcher_set_custom_event_callback(fgp->view_dispatcher, fgp_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(fgp->view_dispatcher, fgp_back_event_callback);
    view_dispatcher_attach_to_gui(
        fgp->view_dispatcher, (Gui*)furi_record_open(RECORD_GUI), ViewDispatcherTypeFullscreen);

    // Submenu
    fgp->submenu = submenu_alloc();
    view_dispatcher_add_view(fgp->view_dispatcher, fgpViewSubmenu, submenu_get_view(fgp->submenu));

    // Receive
    //fgp->receive_handle = fgp_receive_view_alloc(fgp->gblink_handle);
    //view_dispatcher_add_view(fgp->view_dispatcher, fgpViewReceive, fgp_receive_view_get(fgp->receive_handle));

    // Scene manager
    fgp->scene_manager = scene_manager_alloc(&fgp_scene_handlers, fgp);
    scene_manager_next_scene(fgp->scene_manager, fgpSceneMenu);

    return fgp;
}

static void fgp_free(struct fgp_app* fgp) {
    // Scene manager
    scene_manager_free(fgp->scene_manager);

    // submenu
    view_dispatcher_remove_view(fgp->view_dispatcher, fgpViewSubmenu);
    submenu_free(fgp->submenu);

    // View dispatcher
    view_dispatcher_free(fgp->view_dispatcher);

    printer_free(fgp->printer_handle);
    free(fgp->data);
    gblink_free(fgp->gblink_handle);

    free(fgp);
}

int32_t fgp_app(void* p) {
    UNUSED(p);
    struct fgp_app* fgp = NULL;

    fgp = fgp_alloc();

    view_dispatcher_run(fgp->view_dispatcher);

    fgp_free(fgp);

    return 0;
}
