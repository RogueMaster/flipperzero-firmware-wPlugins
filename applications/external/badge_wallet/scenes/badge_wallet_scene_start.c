#include "../badge_wallet.h"

typedef enum {
    BadgeWalletStartItemFavourites,
    BadgeWalletStartItemAll,
} BadgeWalletStartItem;

static void badge_wallet_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void badge_wallet_scene_start_on_enter(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Badge Wallet");

    char label[48];
    snprintf(label, sizeof(label), "Favourites (%u)", (unsigned)badge_wallet_favourite_count(app));
    submenu_add_item(
        app->submenu,
        label,
        BadgeWalletStartItemFavourites,
        badge_wallet_scene_start_submenu_callback,
        app);

    snprintf(label, sizeof(label), "All Badges (%u)", (unsigned)app->card_count);
    submenu_add_item(
        app->submenu,
        label,
        BadgeWalletStartItemAll,
        badge_wallet_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, BadgeWalletSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, BadgeWalletViewSubmenu);
}

bool badge_wallet_scene_start_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BadgeWalletSceneStart, event.event);
        if(event.event == BadgeWalletStartItemFavourites) {
            app->list_mode = BadgeWalletListModeFavourites;
            scene_manager_next_scene(app->scene_manager, BadgeWalletSceneList);
            consumed = true;
        } else if(event.event == BadgeWalletStartItemAll) {
            app->list_mode = BadgeWalletListModeAll;
            scene_manager_next_scene(app->scene_manager, BadgeWalletSceneList);
            consumed = true;
        }
    }

    return consumed;
}

void badge_wallet_scene_start_on_exit(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    submenu_reset(app->submenu);
}
