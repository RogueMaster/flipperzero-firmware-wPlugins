#include "../badge_wallet.h"

/* Item index used for the placeholder row shown when the filtered list is
 * empty. Real card indices are always < BADGE_WALLET_MAX_CARDS, so this never
 * collides with a selectable card. */
#define BADGE_WALLET_LIST_EMPTY_INDEX (BADGE_WALLET_MAX_CARDS)

static void badge_wallet_scene_list_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void badge_wallet_scene_list_on_enter(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;

    submenu_reset(app->submenu);

    bool favourites_only = (app->list_mode == BadgeWalletListModeFavourites);
    submenu_set_header(app->submenu, favourites_only ? "Favourites" : "All Badges");

    size_t shown = 0;
    char label[BADGE_WALLET_LABEL_MAX + 4];
    for(size_t i = 0; i < app->card_count; i++) {
        BadgeCard* card = &app->cards[i];
        if(favourites_only && !card->favorite) continue;

        /* In the full list, flag favourites with a leading star. */
        if(!favourites_only && card->favorite) {
            snprintf(label, sizeof(label), "* %s", furi_string_get_cstr(card->label));
        } else {
            snprintf(label, sizeof(label), "%s", furi_string_get_cstr(card->label));
        }

        /* The submenu item index IS the card index, so selection maps back
         * directly with no lookup table. */
        submenu_add_item(app->submenu, label, i, badge_wallet_scene_list_submenu_callback, app);
        shown++;
    }

    if(shown == 0) {
        submenu_add_item(
            app->submenu,
            favourites_only ? "No favourites yet" : "No badges in /ext/nfc",
            BADGE_WALLET_LIST_EMPTY_INDEX,
            badge_wallet_scene_list_submenu_callback,
            app);
    }

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, BadgeWalletSceneList));

    view_dispatcher_switch_to_view(app->view_dispatcher, BadgeWalletViewSubmenu);
}

bool badge_wallet_scene_list_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BADGE_WALLET_LIST_EMPTY_INDEX) {
            /* Placeholder row: nothing to open. */
            consumed = true;
        } else if(event.event < app->card_count) {
            app->selected_index = event.event;
            scene_manager_set_scene_state(app->scene_manager, BadgeWalletSceneList, event.event);
            scene_manager_next_scene(app->scene_manager, BadgeWalletSceneCard);
            consumed = true;
        }
    }

    return consumed;
}

void badge_wallet_scene_list_on_exit(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    submenu_reset(app->submenu);
}
