/*
 * scene_addcontact — how a new node gets into the address book, and how to hand
 * this node out.
 *
 * The field-native way to add a contact needs no typing and no QR: everyone
 * hits "Find peers", each node floods an advert, every node in range hears it
 * and adds the sender. So the first item just sends our advert and drops back
 * into a fresh Contacts read, where the newcomers show up. The second item is
 * for the one case adverts do not cover — handing our identity to someone with
 * a phone but no node in range — and shows our card to read across. The third
 * imports a contact from a pasted meshcore://contact/add link (the documented
 * share format) — a contact you were handed rather than heard.
 *
 * Rendering our own card as a QR is deliberately still not here: a Flipper has
 * no camera to verify a scan, and a 100+ char link is marginal at 64 px. See
 * docs/contacts-and-mesh for the model.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_ADDCONTACT_EVENT_PICK 0x1000u
#define MESHCORE_ADDCONTACT_EVENT_ADVERT_DONE 0x4A0u
#define MESHCORE_ADDCONTACT_WORKER_STACK 2048u

typedef enum {
    MeshCoreAddContactAdvert,
    MeshCoreAddContactMyCard,
    MeshCoreAddContactImport,
} MeshCoreAddContactItem;

static int32_t meshcore_addcontact_advert_worker(void* context) {
    MeshCoreApp* app = context;

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t event;

    app->worker_error = meshcore_connect_ensure(app);
    if(app->worker_error == NULL) {
        size_t len = mc_cmd_send_self_advert(payload, sizeof(payload), MC_ADVERT_FLOOD);
        if(len == 0) {
            app->worker_error = "Could not build the advert.";
        } else if(!meshcore_session_request(
                      app->session, payload, len, MC_RESP_OK, &event, MESHCORE_LINK_TIMEOUT_MS)) {
            app->worker_error = (event.code == MC_RESP_ERR) ? "Node refused the advert." :
                                                              "No answer from the node.";
        } else {
            meshcore_log_printf(app->log, "advert sent from Add contact");
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_ADDCONTACT_EVENT_ADVERT_DONE);
    return 0;
}

static void meshcore_addcontact_callback(void* context, uint32_t index) {
    MeshCoreApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_ADDCONTACT_EVENT_PICK + index);
}

void meshcore_scene_addcontact_on_enter(void* context) {
    MeshCoreApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Add / share");
    submenu_add_item(
        submenu,
        "Find peers (advert)",
        MeshCoreAddContactAdvert,
        meshcore_addcontact_callback,
        app);
    submenu_add_item(
        submenu, "Share my card", MeshCoreAddContactMyCard, meshcore_addcontact_callback, app);
    submenu_add_item(
        submenu, "Import from link", MeshCoreAddContactImport, meshcore_addcontact_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, MeshCoreSceneAddContact));

    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewSubmenu);
}

bool meshcore_scene_addcontact_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == MESHCORE_ADDCONTACT_EVENT_ADVERT_DONE) {
        if(app->worker) {
            furi_thread_join(app->worker);
            furi_thread_free(app->worker);
            app->worker = NULL;
        }
        if(app->worker_error) {
            char text[160];
            snprintf(text, sizeof(text), "\e#Advert\n%s\n\nBack to try again.", app->worker_error);
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
        } else {
            /* Push a fresh Contacts so its on_enter re-reads GET_CONTACTS and any
             * peer that answered our advert shows up. */
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneContacts);
        }
        return true;
    }

    if(event.event >= MESHCORE_ADDCONTACT_EVENT_PICK) {
        size_t index = event.event - MESHCORE_ADDCONTACT_EVENT_PICK;
        scene_manager_set_scene_state(app->scene_manager, MeshCoreSceneAddContact, (uint32_t)index);

        if(index == MeshCoreAddContactAdvert) {
            if(app->worker != NULL) return true;
            widget_reset(app->widget);
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "\e#Find peers\nFlooding advert...\n\nNodes in range will add me,\nand I add any that answer.");
            view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);

            app->worker = furi_thread_alloc_ex(
                "MeshCoreAddAdv",
                MESHCORE_ADDCONTACT_WORKER_STACK,
                meshcore_addcontact_advert_worker,
                app);
            furi_thread_start(app->worker);
            return true;
        }

        if(index == MeshCoreAddContactMyCard) {
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneMyCard);
            return true;
        }

        if(index == MeshCoreAddContactImport) {
            scene_manager_next_scene(app->scene_manager, MeshCoreSceneImport);
            return true;
        }
    }

    return false;
}

void meshcore_scene_addcontact_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        app->worker_stop = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    submenu_reset(app->submenu);
    widget_reset(app->widget);
}
