#include "../gauntlet_i.h"
#include <string.h>

/* Compose the scrollable body: prompt, then the payload, then (if revealed)
 * the hint. Kept out of the view so the view stays a pure renderer. */
static void build_body(GauntletApp* app, CtfChallenge* c, char* out, size_t sz) {
    out[0] = '\0';
    size_t len = 0;

    /* prompt */
    for(const char* p = c->prompt; *p && len < sz - 1; p++) out[len++] = *p;

    /* payload */
    if(c->has_data) {
        const char* hdr = "\n\n>> DATA\n";
        for(const char* p = hdr; *p && len < sz - 1; p++) out[len++] = *p;
        for(const char* p = c->data; *p && len < sz - 1; p++) out[len++] = *p;
    }

    /* hint (only once revealed) */
    if(app->hint_shown && c->has_hint) {
        const char* hdr = "\n\n>> HINT\n";
        for(const char* p = hdr; *p && len < sz - 1; p++) out[len++] = *p;
        for(const char* p = c->hint; *p && len < sz - 1; p++) out[len++] = *p;
    }
    out[len] = '\0';
}

static void refresh(GauntletApp* app) {
    CtfChallenge* c = gauntlet_current_challenge(app);
    if(!c) return;
    char body[CTF_PROMPT_LEN + CTF_DATA_LEN + CTF_HINT_LEN + 40];
    build_body(app, c, body, sizeof(body));
    challenge_view_set(
        app->challenge_view,
        c->title,
        body,
        c->category,
        c->difficulty,
        c->points,
        ctf_progress_is_solved(app->progress, c->id),
        c->has_hint,
        app->hint_shown);
}

static void gauntlet_challenge_cb(void* context, ChallengeEvent event) {
    GauntletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)event + 200);
}

void gauntlet_scene_challenge_on_enter(void* context) {
    GauntletApp* app = context;
    challenge_view_set_callback(app->challenge_view, gauntlet_challenge_cb, app);
    refresh(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GauntletViewChallenge);
}

bool gauntlet_scene_challenge_on_event(void* context, SceneManagerEvent event) {
    GauntletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        CtfChallenge* c = gauntlet_current_challenge(app);
        switch(event.event - 200) {
        case ChallengeEventFlag:
            app->text_buf[0] = '\0';
            scene_manager_next_scene(app->scene_manager, GauntletSceneSolve);
            consumed = true;
            break;
        case ChallengeEventTools:
            if(c) {
                strncpy(app->tool_src, c->data, sizeof(app->tool_src) - 1);
                app->tool_src[sizeof(app->tool_src) - 1] = '\0';
                app->tool_from_challenge = true;
                scene_manager_next_scene(app->scene_manager, GauntletSceneToolkit);
            }
            consumed = true;
            break;
        case ChallengeEventHint:
            if(c && c->has_hint) {
                app->hint_shown = !app->hint_shown;
                if(app->hint_shown && !ctf_progress_is_solved(app->progress, c->id))
                    app->hint_used = true;
                refresh(app);
                gauntlet_notify_blip(app);
            }
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void gauntlet_scene_challenge_on_exit(void* context) {
    UNUSED(context);
}
