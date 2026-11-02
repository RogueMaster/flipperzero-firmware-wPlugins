#include "bible_view_action_menu.h"
#include "bible_view_reader.h"
#include "bible_renderer.h"
#include "bible_storage.h"
#include "bible_url.h"
#include "bible_nfc.h"

#include <string.h>

/* ============================================================================
 * ActionMenu layout constants
 * ============================================================================ */
#define ACTION_MENU_HEADER_Y  10
#define ACTION_MENU_DIVIDER_Y 12
#define ACTION_MENU_ITEM_H    14
#define ACTION_MENU_START_Y   24

static const char* READER_ACTION_MENU_ITEMS[] = {
    "Save to collection",
    "Share via NFC",
    "Back to reading",
};

static const char* COLLECTION_ACTION_MENU_ITEMS[] = {
    "Read passage",
    "Share via NFC",
    "Delete passage",
};

#define ACTION_MENU_ITEM_COUNT 3

/* ============================================================================
 * ActionMenu view — draw callback
 * ============================================================================ */
void bible_bsb_view_action_menu_draw(Canvas* canvas, void* ctx) {
    BibleApp* app = bible_app_from_draw(ctx);
    BibleAppState* state = app->state;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    const char** items = READER_ACTION_MENU_ITEMS;
    const char* header = "Menu";

    if(state->action_menu_from_collection) {
        items = COLLECTION_ACTION_MENU_ITEMS;
        if(state->collection && state->collection_selected < state->collection_count &&
           state->collection[state->collection_selected].scripture_display_ref[0] != '\0') {
            header = state->collection[state->collection_selected].scripture_display_ref;
        }
    }

    if(canvas_string_width(canvas, header) > BIBLE_SCREEN_WIDTH - 4) {
        canvas_set_font(canvas, FontSecondary);
    }
    canvas_draw_str(canvas, BIBLE_MARGIN_X, ACTION_MENU_HEADER_Y, header);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_line(canvas, 0, ACTION_MENU_DIVIDER_Y, BIBLE_SCREEN_WIDTH, ACTION_MENU_DIVIDER_Y);

    char goto_buf[28];
    for(uint8_t i = 0; i < ACTION_MENU_ITEM_COUNT; i++) {
        int16_t y = ACTION_MENU_START_Y + ((int16_t)i * ACTION_MENU_ITEM_H);
        bool is_selected = (i == state->action_menu_selection);
        const char* label = items[i];

        if(!state->action_menu_from_collection && i == 2) {
            snprintf(
                goto_buf,
                sizeof(goto_buf),
                is_selected ? "< Go to verse: %u >" : "Go to verse: %u",
                (unsigned int)(state->goto_verse > 0 ? state->goto_verse : 1));
            label = goto_buf;
        }

        if(is_selected) {
            canvas_draw_box(canvas, 0, y - 11, BIBLE_SCREEN_WIDTH, ACTION_MENU_ITEM_H);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_str(canvas, 4, y, label);

        if(is_selected) {
            canvas_set_color(canvas, ColorBlack);
        }
    }
}

/* ============================================================================
 * ActionMenu view — input callback
 * ============================================================================ */
bool bible_bsb_view_action_menu_input(InputEvent* event, void* ctx) {
    BibleApp* app = ctx;
    BibleAppState* state = app->state;

    /* Only process Press, Repeat, Short, Long — ignore Release */
    if(event->type != InputTypePress && event->type != InputTypeRepeat &&
       event->type != InputTypeShort && event->type != InputTypeLong) {
        return false;
    }

    bool consumed = false;

    if(event->key == InputKeyUp &&
       (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        /* Clamp: do not wrap past the first item. */
        if(state->action_menu_selection > 0) {
            state->action_menu_selection -= 1;
        }
        consumed = true;

    } else if(
        event->key == InputKeyDown &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        if(state->action_menu_selection < (ACTION_MENU_ITEM_COUNT - 1)) {
            state->action_menu_selection += 1;
        }
        consumed = true;

    } else if(
        event->key == InputKeyLeft &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        if(!state->action_menu_from_collection && state->action_menu_selection == 2) {
            if(state->goto_verse > 1) {
                state->goto_verse -= 1;
            }
        }
        consumed = true;

    } else if(
        event->key == InputKeyRight &&
        (event->type == InputTypePress || event->type == InputTypeRepeat)) {
        if(!state->action_menu_from_collection && state->action_menu_selection == 2) {
            uint16_t max_v = state->passage.verse_count > 0 ? state->passage.verse_count : 1;
            if(state->goto_verse < max_v) {
                state->goto_verse += 1;
            }
        }
        consumed = true;

    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(state->action_menu_from_collection) {
            switch(state->action_menu_selection) {
            case 0: /* Read passage */
                if(state->collection_selected < state->collection_count) {
                    bool loaded =
                        bible_storage_load_verses_for_entry(state, state->collection_selected);
                    if(loaded || state->show_data_help) {
                        state->action_menu_from_collection = false;
                        scene_manager_next_scene(app->scene_manager, BibleSceneReader);
                    } else {
                        if(!bible_toast_active(&state->toast)) {
                            bible_toast_set(&state->toast, "Load failed");
                        }
                        scene_manager_previous_scene(app->scene_manager);
                    }
                } else {
                    scene_manager_previous_scene(app->scene_manager);
                }
                break;

            case 1: /* Share via NFC */
                if(state->collection && state->collection_selected < state->collection_count) {
                    const BibleCollectionEntry* e = &state->collection[state->collection_selected];
                    BiblePassage share_passage = {
                        .book_index = e->book_index,
                        .chapter = e->chapter,
                        .start_verse = e->start_verse,
                        .end_verse = e->end_verse,
                        .verse_count = 1,
                    };
                    if(bible_build_route_url(
                           &share_passage, state->nfc_url, sizeof(state->nfc_url))) {
                        state->nfc_is_export = false;
                        if(bible_nfc_start_url(state)) {
                            scene_manager_next_scene(app->scene_manager, BibleSceneNfcShare);
                        } else {
                            bible_toast_set(&state->toast, "NFC failed");
                            scene_manager_previous_scene(app->scene_manager);
                        }
                    } else {
                        bible_toast_set(&state->toast, "URL failed");
                        scene_manager_previous_scene(app->scene_manager);
                    }
                } else {
                    scene_manager_previous_scene(app->scene_manager);
                }
                break;

            case 2: /* Delete passage */
                if(state->collection && state->collection_selected < state->collection_count) {
                    for(uint8_t i = state->collection_selected; i + 1 < state->collection_count;
                        i++) {
                        state->collection[i] = state->collection[i + 1];
                    }
                    state->collection_count--;
                    if(state->collection_count > 0 &&
                       state->collection_selected >= state->collection_count) {
                        state->collection_selected = state->collection_count - 1;
                    }
                    if(state->collection_scroll > state->collection_selected) {
                        state->collection_scroll = state->collection_selected;
                    }
                    bible_storage_save_collection(state);
                    bible_toast_set(&state->toast, "Deleted");
                }
                scene_manager_previous_scene(app->scene_manager);
                break;

            default:
                scene_manager_previous_scene(app->scene_manager);
                break;
            }
        } else {
            switch(state->action_menu_selection) {
            case 0: /* Save to collection -> open verse range picker */
                bible_reader_open_verse_select(state, app->scene_manager, false);
                break;

            case 1: /* Share via NFC -> open verse range picker */
                bible_reader_open_verse_select(state, app->scene_manager, true);
                break;

            case 2: /* Go to verse */
            default:
                state->reader_on_chapter_page = false;
                if(state->lines && state->line_count > 0) {
                    state->scroll_offset = 0;
                    for(uint16_t i = 0; i < state->line_count; i++) {
                        if(state->lines[i].verse_number >= state->goto_verse &&
                           state->lines[i].is_verse_number) {
                            state->scroll_offset = i;
                            break;
                        }
                    }
                }
                scene_manager_previous_scene(app->scene_manager);
                break;
            }
        }
        consumed = true;

    } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    bible_app_request_redraw(app);
    return consumed;
}
