#include "badge_wallet.h"

#define SIDECAR_FILETYPE "Badge Wallet Sidecar"
#define SIDECAR_VERSION  (1U)

/* ------------------------------------------------------------------ */
/* Data model: card scanning + sidecar metadata                        */
/* ------------------------------------------------------------------ */

void badge_wallet_free_cards(BadgeWalletApp* app) {
    furi_assert(app);
    for(size_t i = 0; i < app->card_count; i++) {
        furi_string_free(app->cards[i].path);
        furi_string_free(app->cards[i].label);
        app->cards[i].path = NULL;
        app->cards[i].label = NULL;
    }
    app->card_count = 0;
}

size_t badge_wallet_favourite_count(BadgeWalletApp* app) {
    furi_assert(app);
    size_t count = 0;
    for(size_t i = 0; i < app->card_count; i++) {
        if(app->cards[i].favorite) count++;
    }
    return count;
}

/* Read the sidecar (if any) and apply Favorite/Label onto already-scanned
 * cards. The sidecar is a flat FlipperFormat file of repeating blocks:
 *
 *   Card: /ext/nfc/office.nfc
 *   Favorite: 1
 *   Label: Office
 *
 * FlipperFormat reads advance a forward cursor, so reading Card -> Favorite ->
 * Label in a loop walks each block in order. Entries whose path no longer
 * matches a scanned card are simply ignored. */
static void badge_wallet_apply_sidecar(BadgeWalletApp* app) {
    FlipperFormat* ff = flipper_format_file_alloc(app->storage);

    do {
        if(!flipper_format_file_open_existing(ff, BADGE_WALLET_SIDECAR)) break;

        FuriString* header = furi_string_alloc();
        uint32_t version = 0;
        bool header_ok = flipper_format_read_header(ff, header, &version);
        furi_string_free(header);
        if(!header_ok) break;

        FuriString* card_path = furi_string_alloc();
        FuriString* label = furi_string_alloc();

        while(flipper_format_read_string(ff, "Card", card_path)) {
            bool fav = false;
            /* Favorite/Label are optional per block; absence keeps defaults. */
            flipper_format_read_bool(ff, "Favorite", &fav, 1);
            bool has_label = flipper_format_read_string(ff, "Label", label);

            for(size_t i = 0; i < app->card_count; i++) {
                if(furi_string_equal(app->cards[i].path, card_path)) {
                    app->cards[i].favorite = fav;
                    if(has_label && !furi_string_empty(label)) {
                        furi_string_set(app->cards[i].label, label);
                    }
                    break;
                }
            }
        }

        furi_string_free(card_path);
        furi_string_free(label);
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
}

void badge_wallet_load_cards(BadgeWalletApp* app) {
    furi_assert(app);
    badge_wallet_free_cards(app);

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, BADGE_WALLET_NFC_DIR)) {
        FileInfo info;
        char name[BADGE_WALLET_NAME_MAX];
        while(app->card_count < BADGE_WALLET_MAX_CARDS &&
              storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;

            FuriString* fname = furi_string_alloc_set(name);
            bool is_nfc = furi_string_end_with_str(fname, BADGE_WALLET_NFC_EXT);
            furi_string_free(fname);
            if(!is_nfc) continue;

            BadgeCard* card = &app->cards[app->card_count];
            card->path = furi_string_alloc_printf("%s/%s", BADGE_WALLET_NFC_DIR, name);
            card->label = furi_string_alloc();
            path_extract_filename_no_ext(name, card->label);
            card->favorite = false;
            app->card_count++;
        }
    }
    /* storage_dir_close must be called even if open failed. */
    storage_dir_close(dir);
    storage_file_free(dir);

    badge_wallet_apply_sidecar(app);
}

void badge_wallet_save_sidecar(BadgeWalletApp* app) {
    furi_assert(app);
    storage_common_mkdir(app->storage, BADGE_WALLET_DATA_DIR);

    FlipperFormat* ff = flipper_format_file_alloc(app->storage);
    do {
        if(!flipper_format_file_open_always(ff, BADGE_WALLET_SIDECAR)) break;
        if(!flipper_format_write_header_cstr(ff, SIDECAR_FILETYPE, SIDECAR_VERSION)) break;

        for(size_t i = 0; i < app->card_count; i++) {
            BadgeCard* card = &app->cards[i];
            bool fav = card->favorite;
            if(!flipper_format_write_string(ff, "Card", card->path)) break;
            if(!flipper_format_write_bool(ff, "Favorite", &fav, 1)) break;
            if(!flipper_format_write_string(ff, "Label", card->label)) break;
        }
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);
}

/* ------------------------------------------------------------------ */
/* App lifecycle                                                       */
/* ------------------------------------------------------------------ */

static bool badge_wallet_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool badge_wallet_back_event_callback(void* context) {
    furi_assert(context);
    BadgeWalletApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static BadgeWalletApp* badge_wallet_alloc(void) {
    BadgeWalletApp* app = malloc(sizeof(BadgeWalletApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    storage_common_mkdir(app->storage, BADGE_WALLET_DATA_DIR);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&badge_wallet_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, badge_wallet_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, badge_wallet_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    app->popup = popup_alloc();
    app->text_input = text_input_alloc();
    app->widget = widget_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher, BadgeWalletViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, BadgeWalletViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, BadgeWalletViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, BadgeWalletViewWidget, widget_get_view(app->widget));

    app->card_count = 0;
    app->list_mode = BadgeWalletListModeAll;
    app->selected_index = 0;
    app->nfc = NULL;
    app->emulate_device = NULL;
    app->listener = NULL;

    badge_wallet_load_cards(app);

    return app;
}

static void badge_wallet_free(BadgeWalletApp* app) {
    furi_assert(app);

    /* The Emulate scene owns the NFC lifecycle and frees it on exit.
     * Guard here in case of an abnormal teardown so the HAL is released. */
    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }
    if(app->emulate_device) {
        nfc_device_free(app->emulate_device);
        app->emulate_device = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    badge_wallet_free_cards(app);

    view_dispatcher_remove_view(app->view_dispatcher, BadgeWalletViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, BadgeWalletViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, BadgeWalletViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, BadgeWalletViewWidget);

    submenu_free(app->submenu);
    popup_free(app->popup);
    text_input_free(app->text_input);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t badge_wallet_app(void* p) {
    UNUSED(p);
    BadgeWalletApp* app = badge_wallet_alloc();

    scene_manager_next_scene(app->scene_manager, BadgeWalletSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    badge_wallet_free(app);
    return 0;
}
