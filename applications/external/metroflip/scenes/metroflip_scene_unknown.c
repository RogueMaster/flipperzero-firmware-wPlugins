#include "../metroflip_i.h"
#include <dolphin/dolphin.h>
#include "../api/metroflip/metroflip_api.h"

extern const Icon I_Cross_10x10;
extern const Icon I_Cross1_10x10;
extern const Icon I_Cross2_10x10;
extern const Icon I_Cross3_10x10;

#define TAG "Metroflip:Scene:UnknownCard"

void metroflip_scene_unknown_on_enter(void* context) {
    Metroflip* app = context;

    dolphin_deed(DolphinDeedNfcReadSuccess);

    FURI_LOG_I(TAG, "ct 2 %s", app->card_type ? app->card_type : "NULL");

    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "Unsupported");
    metroflip_card_view_set_icon(view, &I_Cross_10x10);
    metroflip_card_view_set_icon_animation(
        view, &I_Cross1_10x10, &I_Cross2_10x10, &I_Cross3_10x10);

    uint8_t p = metroflip_card_view_add_page(view, "");

    if(app->card_type && app->card_type[0] != '\0') {
        metroflip_card_view_add_field(view, p, "Detected", app->card_type, false);
    }

    if(app->is_desfire) {
        metroflip_card_view_add_field(view, p, "Protocol", "DESFire", false);
        metroflip_card_view_add_field(view, p, "Status", "Locked", true);
    } else {
        metroflip_card_view_add_field(view, p, "Status", "Not supported", true);
    }

    uint8_t p2 = metroflip_card_view_add_page(view, "Report This Card");
    metroflip_card_view_add_field(view, p2, "GitHub", "luu176/Metroflip", false);
    metroflip_card_view_add_field(view, p2, "Section", "/issues", false);

    metroflip_card_view_show(app);
}

bool metroflip_scene_unknown_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventTick) {
            /* Note: no view_get_model() here - on a Locking model it would
               take the model mutex and never release it (deadlock). */
            if(app->card_view) {
                with_view_model(
                    app->card_view,
                    MetroflipCardViewModel * m,
                    {
                        if(m->anim[0]) {
                            m->anim_frame = (m->anim_frame + 1) % METROFLIP_CARD_VIEW_ANIM_FRAMES;
                        }
                    },
                    true);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }
    return consumed;
}

void metroflip_scene_unknown_on_exit(void* context) {
    UNUSED(context);
    /* Card view cleanup is handled by parse scene's on_exit */
}
