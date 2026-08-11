/*
 * scene_mycard — "this is me": show the node's own contact card so it can be
 * handed to someone who is not on the mesh yet.
 *
 * Two ways off this screen exist in the field, in order of how little work they
 * are: (1) the other person's node hears our advert and adds us with no typing
 * at all — that is what Send advert is for; (2) they have a phone but no node in
 * range, so they need our card out-of-band. EXPORT_CONTACT with a NULL key asks
 * the node for its own card as a meshcore:// link, which is what this shows,
 * next to the raw public key as a fallback the user can read out.
 *
 * The link is the reliable channel. A QR of it would be nicer, but a Flipper has
 * no camera to verify a scan and a 100+ char link is marginal at 64 px, so that
 * is deliberately not here — the text link always works.
 */
#include "../meshcore_cfg.h"

#define MESHCORE_MYCARD_EVENT_DONE 0x490u
#define MESHCORE_MYCARD_WORKER_STACK 2048u

/* The card link the node returned, valid between the worker finishing and the
 * view being built. One card at a time, so a file-static buffer is enough. */
static struct {
    char uri[MC_MAX_DATA + 1];
    bool have_uri;
} mycard;

static const char* meshcore_mycard_run(MeshCoreApp* app) {
    const char* err = meshcore_connect_ensure(app);
    if(err != NULL) return err;

    mycard.have_uri = false;

    uint8_t payload[MC_MAX_PAYLOAD];
    mc_event_t ev;

    /* NULL key == "export my own card". Older firmware may not answer it; that
     * is not fatal, the public key from SELF_INFO is still shown. */
    size_t len = mc_cmd_export_contact(payload, sizeof(payload), NULL);
    if(len != 0 && meshcore_session_request(
                       app->session, payload, len, MC_RESP_CONTACT_URI, &ev,
                       MESHCORE_LINK_TIMEOUT_MS)) {
        size_t n = ev.u.contact_uri.len;
        if(n > MC_MAX_DATA) n = MC_MAX_DATA;
        memcpy(mycard.uri, ev.u.contact_uri.data, n);
        mycard.uri[n] = '\0';
        mycard.have_uri = (n > 0);
    }

    return NULL;
}

static int32_t meshcore_mycard_worker(void* context) {
    MeshCoreApp* app = context;
    app->worker_error = meshcore_mycard_run(app);
    view_dispatcher_send_custom_event(app->view_dispatcher, MESHCORE_MYCARD_EVENT_DONE);
    return 0;
}

static void meshcore_mycard_show(MeshCoreApp* app) {
    /* Room for the header, a 64-char hex key and a full-length link. */
    char text[512];
    size_t off = 0;

    off += snprintf(
        text + off, sizeof(text) - off, "\e#My card\n%.32s\n\n", app->node.name);

    /* Public key as hex: the identity other nodes key us by. Always available
     * once connected, link or no link. */
    off += snprintf(text + off, sizeof(text) - off, "\e*key:\e*\n");
    for(size_t i = 0; i < sizeof(app->node.public_key) && off + 2 < sizeof(text); i++) {
        off += snprintf(text + off, sizeof(text) - off, "%02x", app->node.public_key[i]);
    }
    off += snprintf(text + off, sizeof(text) - off, "\n\n");

    if(mycard.have_uri) {
        snprintf(
            text + off,
            sizeof(text) - off,
            "\e*link:\e*\n%s\n\nType this into the phone's\nMeshCore app to add me. Or\njust Send advert and let a\nnearby node hear me.",
            mycard.uri);
    } else {
        snprintf(
            text + off,
            sizeof(text) - off,
            "No link from this node.\n\nUse Send advert instead: any\nnode in range adds me with\nno typing.");
    }

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
}

void meshcore_scene_mycard_on_enter(void* context) {
    MeshCoreApp* app = context;

    app->worker_stop = false;
    app->worker_error = NULL;
    view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewLoading);

    app->worker = furi_thread_alloc_ex(
        "MeshCoreMyCard", MESHCORE_MYCARD_WORKER_STACK, meshcore_mycard_worker, app);
    furi_thread_start(app->worker);
}

bool meshcore_scene_mycard_on_event(void* context, SceneManagerEvent event) {
    MeshCoreApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event != MESHCORE_MYCARD_EVENT_DONE) return false;

    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }

    if(app->worker_error) {
        char text[192];
        snprintf(
            text,
            sizeof(text),
            "\e#My card\n%s\n\nConnect a node first, then\ntry again.",
            app->worker_error);
        widget_reset(app->widget);
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
        view_dispatcher_switch_to_view(app->view_dispatcher, MeshCoreViewWidget);
    } else {
        meshcore_mycard_show(app);
    }
    return true;
}

void meshcore_scene_mycard_on_exit(void* context) {
    MeshCoreApp* app = context;

    if(app->worker) {
        app->worker_stop = true;
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    widget_reset(app->widget);
}
