#include "../badge_wallet.h"

typedef enum {
    BadgeWalletCardItemEmulate,
    BadgeWalletCardItemToggleFav,
    BadgeWalletCardItemEditLabel,
    BadgeWalletCardItemInfo,
} BadgeWalletCardItem;

static void badge_wallet_scene_card_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void badge_wallet_scene_card_populate(BadgeWalletApp* app) {
    BadgeCard* card = &app->cards[app->selected_index];

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, furi_string_get_cstr(card->label));

    submenu_add_item(
        app->submenu,
        "Emulate",
        BadgeWalletCardItemEmulate,
        badge_wallet_scene_card_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        card->favorite ? "Unfavourite" : "Favourite",
        BadgeWalletCardItemToggleFav,
        badge_wallet_scene_card_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Edit Label",
        BadgeWalletCardItemEditLabel,
        badge_wallet_scene_card_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Info",
        BadgeWalletCardItemInfo,
        badge_wallet_scene_card_submenu_callback,
        app);
}

void badge_wallet_scene_card_on_enter(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;

    badge_wallet_scene_card_populate(app);
    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, BadgeWalletSceneCard));
    view_dispatcher_switch_to_view(app->view_dispatcher, BadgeWalletViewSubmenu);
}

bool badge_wallet_scene_card_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BadgeWalletSceneCard, event.event);
        switch(event.event) {
        case BadgeWalletCardItemEmulate:
            scene_manager_next_scene(app->scene_manager, BadgeWalletSceneEmulate);
            consumed = true;
            break;
        case BadgeWalletCardItemToggleFav: {
            BadgeCard* card = &app->cards[app->selected_index];
            card->favorite = !card->favorite;
            badge_wallet_save_sidecar(app);
            /* Refresh the menu in place so the toggle label updates. */
            badge_wallet_scene_card_populate(app);
            submenu_set_selected_item(app->submenu, BadgeWalletCardItemToggleFav);
            consumed = true;
            break;
        }
        case BadgeWalletCardItemEditLabel:
            scene_manager_next_scene(app->scene_manager, BadgeWalletSceneEditLabel);
            consumed = true;
            break;
        case BadgeWalletCardItemInfo:
            scene_manager_next_scene(app->scene_manager, BadgeWalletSceneInfo);
            consumed = true;
            break;
        default:
            break;
        }
    }

    return consumed;
}

void badge_wallet_scene_card_on_exit(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    submenu_reset(app->submenu);
}
