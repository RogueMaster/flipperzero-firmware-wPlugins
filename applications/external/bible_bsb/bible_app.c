#include "bible_app.h"
#include "bible_view_book_list.h"
#include "bible_view_book_filter.h"
#include "bible_view_chapter_list.h"
#include "bible_view_verse_select.h"
#include "bible_view_reader.h"
#include "bible_view_action_menu.h"
#include "bible_view_collection.h"
#include "bible_view_nfc_share.h"
#include "bible_storage.h"
#include "bible_loader.h"
#include "bible_books.h"
#include "bible_nfc.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/view.h>

/* ============================================================================
 * Navigation helpers
 * ============================================================================ */

/**
 * ViewDispatcher navigation callback — routes Back events through SceneManager.
 *
 * When a view's input callback returns false for Back, ViewDispatcher calls
 * this callback, which forwards to scene_manager_handle_back_event().
 * The active scene's on_event handler then receives SceneManagerEventTypeBack.
 */
void bible_app_request_redraw(BibleApp* app) {
    if(!app || !app->state) return;
    uint8_t scene = app->state->current_scene;
    if(scene >= BibleViewCount) return;
    View* view = app->views[scene];
    if(view) {
        /* Lock-free model: commit with update=true queues view_port_update. */
        view_commit_model(view, true);
    }
}

static bool bible_bsb_navigation_event_callback(void* context) {
    furi_check(context);
    BibleApp* app = context;
    bool consumed = scene_manager_handle_back_event(app->scene_manager);
    bible_app_request_redraw(app);
    return consumed;
}

/* ============================================================================
 * Scene handlers — BookList
 * ============================================================================ */

static void bible_bsb_scene_book_list_on_enter(void* context) {
    BibleApp* app = context;
    app->state->current_scene = BibleSceneBookList;
    app->state->bsb_data_present = bible_chapter_data_present();
    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewBookList);
}

static bool bible_bsb_scene_book_list_on_event(void* context, SceneManagerEvent event) {
    BibleApp* app = context;
    BibleAppState* state = app->state;

    if(event.type == SceneManagerEventTypeBack) {
        if(state->book_filter_idx != 0) {
            /* Reset filter to All, select first matching book */
            state->book_filter_idx = 0;
            if(state->genre_mode_enabled && state->selected_genre < BIBLE_GENRE_COUNT) {
                state->selected_book = BIBLE_GENRES[state->selected_genre].start_book;
            } else {
                uint8_t all_filtered[BIBLE_BOOK_COUNT];
                uint8_t all_count = get_filtered_books(0, all_filtered);
                if(all_count > 0) {
                    state->selected_book = all_filtered[0];
                }
            }
            state->book_scroll = 0;
            return true; /* consumed — stay on BookList */
        } else if(state->genre_mode_enabled && state->browse_depth > 0) {
            /* Step back up the hierarchy: Book (2) -> Genre (1) -> Testament (0) */
            state->browse_depth--;
            return true; /* consumed — stay on BookList at parent level */
        } else if(state->genre_mode_enabled) {
            /* From < Testament > (depth 0), step back to < All > */
            state->genre_mode_enabled = false;
            return true; /* consumed — stay on < All > */
        } else {
            /* Already at < All > — quit the app */
            view_dispatcher_stop(app->view_dispatcher);
            return true; /* consumed — app will exit */
        }
    }
    return false;
}

static void bible_bsb_scene_book_list_on_exit(void* context) {
    UNUSED(context);
}

/* ============================================================================
 * Scene handlers — BookFilter
 * ============================================================================ */

static void bible_bsb_scene_book_filter_on_enter(void* context) {
    BibleApp* app = context;
    app->state->current_scene = BibleSceneBookFilter;
    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewBookFilter);
}

static bool bible_bsb_scene_book_filter_on_event(void* context, SceneManagerEvent event) {
    BibleApp* app = context;
    BibleAppState* state = app->state;

    if(event.type == SceneManagerEventTypeBack) {
        /* Apply filter and return to BookList */
        uint8_t filtered[BIBLE_BOOK_COUNT];
        uint8_t filtered_count = get_filtered_books(state->book_filter_idx, filtered);
        if(filtered_count > 0) {
            state->selected_book = filtered[0];
            state->book_scroll = 0;
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

static void bible_bsb_scene_book_filter_on_exit(void* context) {
    UNUSED(context);
}

/* ============================================================================
 * Scene handlers — ChapterList
 * ============================================================================ */

static void bible_bsb_scene_chapter_list_on_enter(void* context) {
    BibleApp* app = context;
    app->state->current_scene = BibleSceneChapterList;
    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewChapterList);
}

static bool bible_bsb_scene_chapter_list_on_event(void* context, SceneManagerEvent event) {
    BibleApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

static void bible_bsb_scene_chapter_list_on_exit(void* context) {
    UNUSED(context);
}

/* ============================================================================
 * Scene handlers — VerseSelect
 * ============================================================================ */

static void bible_bsb_scene_verse_select_on_enter(void* context) {
    BibleApp* app = context;
    app->state->current_scene = BibleSceneVerseSelect;
    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewVerseSelect);
}

static bool bible_bsb_scene_verse_select_on_event(void* context, SceneManagerEvent event) {
    BibleApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

static void bible_bsb_scene_verse_select_on_exit(void* context) {
    BibleApp* app = context;
    if(app && app->state) {
        app->state->show_data_help = false;
    }
}

/* ============================================================================
 * Scene handlers — Reader
 * ============================================================================ */

static void bible_bsb_scene_reader_on_enter(void* context) {
    BibleApp* app = context;
    app->state->current_scene = BibleSceneReader;
    app->state->header_scroll_ticks = 0;
    app->state->header_scroll_key = NULL;
    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewReader);
}

static bool bible_bsb_scene_reader_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

static void bible_bsb_scene_reader_on_exit(void* context) {
    BibleApp* app = context;
    if(app && app->state) {
        bible_storage_save_last_read(app->state);
        app->state->show_data_help = false;
    }
}

/* ============================================================================
 * Scene handlers — ActionMenu
 * ============================================================================ */

static void bible_bsb_scene_action_menu_on_enter(void* context) {
    BibleApp* app = context;
    app->state->current_scene = BibleSceneActionMenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewActionMenu);
}

static bool bible_bsb_scene_action_menu_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

static void bible_bsb_scene_action_menu_on_exit(void* context) {
    UNUSED(context);
}

/* ============================================================================
 * Scene handlers — Collection
 * ============================================================================ */

static void bible_bsb_scene_collection_on_enter(void* context) {
    BibleApp* app = context;
    BibleAppState* state = app->state;
    state->current_scene = BibleSceneCollection;

    /* Ensure collection is loaded from SD on first visit */
    if(!state->collection_loaded) {
        bible_storage_load_collection(state);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewCollection);
}

static bool bible_bsb_scene_collection_on_event(void* context, SceneManagerEvent event) {
    BibleApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, BibleSceneBookList);
        return true;
    }
    return false;
}

static void bible_bsb_scene_collection_on_exit(void* context) {
    BibleApp* app = context;
    if(app && app->state) {
        app->state->show_data_help = false;
    }
}

/* ============================================================================
 * Scene handlers - NFC share
 * ============================================================================ */

static void bible_bsb_scene_nfc_share_on_enter(void* context) {
    BibleApp* app = context;
    app->state->current_scene = BibleSceneNfcShare;
    view_dispatcher_switch_to_view(app->view_dispatcher, BibleViewNfcShare);
}

static bool bible_bsb_scene_nfc_share_on_event(void* context, SceneManagerEvent event) {
    BibleApp* app = context;
    if(!app || !app->state) {
        return false;
    }

    if(event.type == SceneManagerEventTypeBack) {
        bool return_to_collection = app->state->nfc_is_export ||
                                    app->state->action_menu_from_collection;
        bible_nfc_stop(app->state);
        if(return_to_collection) {
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, BibleSceneCollection);
        } else {
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, BibleSceneReader);
        }
        return true;
    }
    return false;
}

static void bible_bsb_scene_nfc_share_on_exit(void* context) {
    BibleApp* app = context;
    if(app && app->state && app->state->nfc_emitting) {
        bible_nfc_stop(app->state);
    }
}

/* ============================================================================
 * SceneManagerHandlers array
 * ============================================================================ */
static const AppSceneOnEnterCallback bible_bsb_on_enter_handlers[BibleSceneCount] = {
    bible_bsb_scene_book_list_on_enter,
    bible_bsb_scene_book_filter_on_enter,
    bible_bsb_scene_chapter_list_on_enter,
    bible_bsb_scene_verse_select_on_enter,
    bible_bsb_scene_reader_on_enter,
    bible_bsb_scene_action_menu_on_enter,
    bible_bsb_scene_collection_on_enter,
    bible_bsb_scene_nfc_share_on_enter,
};

static const AppSceneOnEventCallback bible_bsb_on_event_handlers[BibleSceneCount] = {
    bible_bsb_scene_book_list_on_event,
    bible_bsb_scene_book_filter_on_event,
    bible_bsb_scene_chapter_list_on_event,
    bible_bsb_scene_verse_select_on_event,
    bible_bsb_scene_reader_on_event,
    bible_bsb_scene_action_menu_on_event,
    bible_bsb_scene_collection_on_event,
    bible_bsb_scene_nfc_share_on_event,
};

static const AppSceneOnExitCallback bible_bsb_on_exit_handlers[BibleSceneCount] = {
    bible_bsb_scene_book_list_on_exit,
    bible_bsb_scene_book_filter_on_exit,
    bible_bsb_scene_chapter_list_on_exit,
    bible_bsb_scene_verse_select_on_exit,
    bible_bsb_scene_reader_on_exit,
    bible_bsb_scene_action_menu_on_exit,
    bible_bsb_scene_collection_on_exit,
    bible_bsb_scene_nfc_share_on_exit,
};

static const SceneManagerHandlers bible_bsb_scene_handlers = {
    .on_enter_handlers = bible_bsb_on_enter_handlers,
    .on_event_handlers = bible_bsb_on_event_handlers,
    .on_exit_handlers = bible_bsb_on_exit_handlers,
    .scene_num = BibleSceneCount,
};

/* ============================================================================
 * Custom view draw and input callbacks
 * ============================================================================ */

static void bible_bsb_wrapper_book_filter_draw(Canvas* canvas, void* ctx) {
    bible_bsb_view_book_filter_draw(canvas, ctx);
}

static bool bible_bsb_wrapper_book_filter_input(InputEvent* event, void* ctx) {
    return bible_bsb_view_book_filter_input(event, ctx);
}

static void bible_bsb_wrapper_action_menu_draw(Canvas* canvas, void* ctx) {
    bible_bsb_view_action_menu_draw(canvas, ctx);
}

static bool bible_bsb_wrapper_action_menu_input(InputEvent* event, void* ctx) {
    return bible_bsb_view_action_menu_input(event, ctx);
}

static void bible_bsb_wrapper_collection_draw(Canvas* canvas, void* ctx) {
    bible_bsb_view_collection_draw(canvas, ctx);
}

static bool bible_bsb_wrapper_collection_input(InputEvent* event, void* ctx) {
    return bible_bsb_view_collection_input(event, ctx);
}

static void bible_bsb_wrapper_nfc_share_draw(Canvas* canvas, void* ctx) {
    bible_bsb_view_nfc_share_draw(canvas, ctx);
}

static bool bible_bsb_wrapper_nfc_share_input(InputEvent* event, void* ctx) {
    return bible_bsb_view_nfc_share_input(event, ctx);
}

/* ============================================================================
 * Tick callback — called every frame to update toast timer, animations, etc.
 * ============================================================================ */
static void bible_bsb_tick_callback(void* context) {
    BibleApp* app = context;
    if(app && app->state) {
        bible_toast_tick(&app->state->toast);
        if(app->state->current_scene == BibleSceneReader) {
            app->state->header_scroll_ticks++;
        }
        /* Queue a redraw so toast timers and the NFC pulse stay current. */
        bible_app_request_redraw(app);
    }
}

/* ============================================================================
 * Free — mirror alloc order in reverse.
 * Safe on a partially constructed app: NULL members are skipped.
 * ============================================================================ */
void bible_app_free(BibleApp* app) {
    furi_check(app);

    /* Remove views that were both allocated and registered. */
    if(app->view_dispatcher) {
        for(BibleView v = 0; v < BibleViewCount; v++) {
            View* view = app->views[v];
            if(view != NULL) {
                view_dispatcher_remove_view(app->view_dispatcher, v);
                view_free_model(view);
                view_free(view);
                app->views[v] = NULL;
            }
        }
    } else {
        for(BibleView v = 0; v < BibleViewCount; v++) {
            if(app->views[v] != NULL) {
                view_free(app->views[v]);
                app->views[v] = NULL;
            }
        }
    }

    /* GUI — only if this app opened the record */
    if(app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }

    /* Dispatcher + SceneManager */
    if(app->view_dispatcher) {
        view_dispatcher_set_tick_event_callback(app->view_dispatcher, NULL, 0);
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
    }
    if(app->scene_manager) {
        scene_manager_free(app->scene_manager);
        app->scene_manager = NULL;
    }

    /* State — free dynamic arrays first, then the state struct */
    if(app->state) {
        bible_app_state_deinit(app->state);
        free(app->state);
        app->state = NULL;
    }
    free(app);
}

/**
 * Tear down whatever bible_app_alloc managed to create and return NULL.
 * Callers must not use `app` after this returns.
 */
static BibleApp* bible_app_alloc_fail(BibleApp* app) {
    if(app) {
        bible_app_free(app);
    }
    return NULL;
}

static bool bible_app_register_view(
    BibleApp* app,
    BibleView view_id,
    ViewDrawCallback draw_callback,
    ViewInputCallback input_callback) {
    View* view = view_alloc();
    if(!view) {
        return false;
    }

    view_set_draw_callback(view, draw_callback);
    view_set_input_callback(view, input_callback);
    view_set_context(view, app);
    /* Draw is handed the model, not the context. Store the app pointer there. */
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(BibleApp*));
    BibleApp** model = view_get_model(view);
    if(!model) {
        view_free(view);
        return false;
    }
    *model = app;
    view_commit_model(view, false);
    /* Store only after add so free() removes a view that was registered. */
    view_dispatcher_add_view(app->view_dispatcher, view_id, view);
    app->views[view_id] = view;
    return true;
}

/* ============================================================================
 * Allocation
 * ============================================================================ */
BibleApp* bible_app_alloc(void) {
    BibleApp* app = malloc(sizeof(BibleApp));
    if(!app) {
        return NULL;
    }
    memset(app, 0, sizeof(BibleApp));

    /* App state */
    app->state = malloc(sizeof(BibleAppState));
    if(!app->state) {
        return bible_app_alloc_fail(app);
    }
    bible_app_state_init(app->state);

    /* ViewDispatcher + SceneManager */
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        return bible_app_alloc_fail(app);
    }

    app->scene_manager = scene_manager_alloc(&bible_bsb_scene_handlers, app);
    if(!app->scene_manager) {
        return bible_app_alloc_fail(app);
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, NULL);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bible_bsb_navigation_event_callback);
    /* 33ms ~ 30fps tick for toast timer and NFC animation */
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, bible_bsb_tick_callback, 33);

    /* GUI */
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        return bible_app_alloc_fail(app);
    }
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Custom views — all 8 views registered with ViewDispatcher */
    if(!bible_app_register_view(
           app, BibleViewBookList, bible_bsb_view_book_list_draw, bible_bsb_view_book_list_input) ||
       !bible_app_register_view(
           app,
           BibleViewBookFilter,
           bible_bsb_wrapper_book_filter_draw,
           bible_bsb_wrapper_book_filter_input) ||
       !bible_app_register_view(
           app,
           BibleViewChapterList,
           bible_bsb_view_chapter_list_draw,
           bible_bsb_view_chapter_list_input) ||
       !bible_app_register_view(
           app,
           BibleViewVerseSelect,
           bible_bsb_view_verse_select_draw,
           bible_bsb_view_verse_select_input) ||
       !bible_app_register_view(
           app, BibleViewReader, bible_bsb_view_reader_draw, bible_bsb_view_reader_input) ||
       !bible_app_register_view(
           app,
           BibleViewActionMenu,
           bible_bsb_wrapper_action_menu_draw,
           bible_bsb_wrapper_action_menu_input) ||
       !bible_app_register_view(
           app,
           BibleViewCollection,
           bible_bsb_wrapper_collection_draw,
           bible_bsb_wrapper_collection_input) ||
       !bible_app_register_view(
           app,
           BibleViewNfcShare,
           bible_bsb_wrapper_nfc_share_draw,
           bible_bsb_wrapper_nfc_share_input)) {
        return bible_app_alloc_fail(app);
    }

    return app;
}

/* ============================================================================
 * Entry point
 * ============================================================================ */
static bool bible_boot_signal(uint32_t signal, void* arg, void* context) {
    UNUSED(arg);
    BibleApp* app = context;
    if(signal == FuriSignalExit && app && app->view_dispatcher) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    return false;
}

int32_t bible_bsb_main(void* p) {
    UNUSED(p);

    BibleApp* app = bible_app_alloc();
    if(app == NULL) {
        return -1;
    }

    furi_thread_set_signal_callback(furi_thread_get_current(), bible_boot_signal, app);
    scene_manager_next_scene(app->scene_manager, BibleSceneBookList);
    if(app->state->bsb_data_present && bible_storage_load_last_read(app->state)) {
        if(BOOK_CHAPTER_COUNTS[app->state->selected_book] > 1) {
            scene_manager_next_scene(app->scene_manager, BibleSceneChapterList);
        }
        scene_manager_next_scene(app->scene_manager, BibleSceneReader);
    }
    view_dispatcher_run(app->view_dispatcher);

    if(app->state) {
        bible_storage_save_last_read(app->state);
    }
    bible_app_free(app);
    return 0;
}
