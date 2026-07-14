#include "../metroflip_i.h"
#include <dolphin/dolphin.h>
#include "../api/metroflip/metroflip_api.h"

/* Header icon (compiled from images/, linked into the main app) */
extern const Icon I_Train_10x10;
extern const Icon I_Train1_10x10;
extern const Icon I_Train2_10x10;
extern const Icon I_Train3_10x10;

#define TAG "Metroflip:Scene:About"

void metroflip_scene_about_on_enter(void* context) {
    Metroflip* app = context;

    dolphin_deed(DolphinDeedNfcReadSuccess);

    /* Use the shared card view (titled header, animated icon, paged + scrollable)
       instead of a flat text blob. */
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "Metroflip");
    metroflip_card_view_set_icon(view, &I_Train_10x10);
    metroflip_card_view_set_icon_animation(
        view, &I_Train1_10x10, &I_Train2_10x10, &I_Train3_10x10);

    /* Page 1: what it is */
    uint8_t p = metroflip_card_view_add_page(view, "About");
    metroflip_card_view_add_field(view, p, "", "Multi-protocol", false);
    metroflip_card_view_add_field(view, p, "", "transit card reader", false);
    metroflip_card_view_add_field(view, p, "", "for Flipper Zero.", false);
    metroflip_card_view_add_field(view, p, "Version", "2.0", true);

    /* Page 2: credits & project */
    p = metroflip_card_view_add_page(view, "Credits");
    metroflip_card_view_add_field(view, p, "Author", "luu176", false);
    metroflip_card_view_add_field(view, p, "Based on", "Metrodroid", false);
    metroflip_card_view_add_field(view, p, "", "github.com/", false);
    metroflip_card_view_add_field(view, p, "", "luu176/Metroflip", false);

    metroflip_card_view_show(app);
}

bool metroflip_scene_about_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventTick) {
            /* Advance the header icon animation. No view_get_model() guard:
               the card view uses a Locking model, so that would deadlock. */
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

void metroflip_scene_about_on_exit(void* context) {
    Metroflip* app = context;
    /* Reset the card view model (keeps the persistent view registered). */
    metroflip_card_view_free(app);
}
