#include "metroflip_card_view.h"
#include "../../metroflip_i.h"
#include "../metroflip/metroflip_api.h"
#include <string.h>
#include <stdlib.h>

/* Icon declarations - these are compiled from images/ by ufbt,
   not exported via API table (table has a ~200 entry limit). */
extern const Icon I_Train_10x10;
extern const Icon I_Train1_10x10;
extern const Icon I_Train2_10x10;
extern const Icon I_Train3_10x10;
extern const Icon I_Wallet_10x10;
extern const Icon I_Wallet1_10x10;
extern const Icon I_Wallet2_10x10;
extern const Icon I_Wallet3_10x10;
extern const Icon I_Ticket_10x10;
extern const Icon I_Ticket1_10x10;
extern const Icon I_Ticket2_10x10;
extern const Icon I_Ticket3_10x10;
extern const Icon I_CardGeneric_10x10;
extern const Icon I_CardGeneric1_10x10;
extern const Icon I_CardGeneric2_10x10;
extern const Icon I_CardGeneric3_10x10;

#define TAG "Metroflip:CardView"

#define CARD_VIEW_HEADER_H    13
#define CARD_VIEW_CONTENT_Y   14
#define CARD_VIEW_FOOTER_Y    53
#define CARD_VIEW_LINE_H      10
#define CARD_VIEW_VIS_LINES   3
#define CARD_VIEW_MAX_WIDTH   120
#define CARD_VIEW_INIT_PAGES  4
#define CARD_VIEW_INIT_FIELDS 4

static uint32_t metroflip_card_view_previous_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void card_view_model_clear(MetroflipCardViewModel* m) {
    if(m->pages) {
        for(uint8_t i = 0; i < m->page_count; i++) {
            free(m->pages[i].fields);
        }
        free(m->pages);
    }
    memset(m, 0, sizeof(MetroflipCardViewModel));
}

static void card_view_draw_line(
    Canvas* canvas,
    uint16_t line_idx,
    int16_t scroll,
    const char* text,
    bool bold) {
    int16_t vis = (int16_t)line_idx - scroll;
    if(vis >= 0 && vis < CARD_VIEW_VIS_LINES) {
        uint8_t y = CARD_VIEW_CONTENT_Y + 8 + vis * CARD_VIEW_LINE_H;
        if(bold)
            canvas_set_font(canvas, FontPrimary);
        else
            canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, y, text);
        canvas_set_font(canvas, FontSecondary);
    }
}

static void metroflip_card_view_draw(Canvas* canvas, void* model) {
    if(!model) return;
    MetroflipCardViewModel* m = (MetroflipCardViewModel*)model;
    canvas_set_bitmap_mode(canvas, true);

    /* ── Header bar (inverted) ── */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, CARD_VIEW_HEADER_H);
    canvas_set_color(canvas, ColorWhite);

    uint8_t title_x = 3;
    /* Animated icon if frames are set, otherwise static */
    const Icon* draw_icon = NULL;
    if(m->anim[0]) {
        draw_icon = m->anim[m->anim_frame % METROFLIP_CARD_VIEW_ANIM_FRAMES];
    } else if(m->icon) {
        draw_icon = m->icon;
    }
    if(draw_icon) {
        canvas_draw_icon(canvas, 2, 1, draw_icon);
        title_x = 15;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, title_x, 10, m->title);

    /* Page indicator - right-aligned in header */
    if(m->page_count > 1) {
        char pg[8];
        snprintf(pg, sizeof(pg), "%d/%d", m->current_page + 1, m->page_count);
        canvas_set_font(canvas, FontSecondary);
        uint16_t pw = canvas_string_width(canvas, pg);
        canvas_draw_str(canvas, (uint8_t)(126 - pw), 10, pg);
    }

    /* ── Content: scrollable field lines ── */
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(m->page_count == 0 || m->current_page >= m->page_count || !m->pages) goto footer;

    MetroflipCardPage* page = &m->pages[m->current_page];
    uint16_t line = 0;

    /* Page header as first bold line */
    if(page->header[0] != '\0') {
        card_view_draw_line(canvas, line, m->scroll_y, page->header, true);
        line++;
    }

    /* Fields - auto-split long "Label: Value" into two lines.
       A field with an empty label renders as plain value text. */
    for(uint8_t i = 0; i < page->field_count; i++) {
        char combined[METROFLIP_CARD_VIEW_LABEL_LEN + METROFLIP_CARD_VIEW_VALUE_LEN + 2];
        if(page->fields[i].label[0] != '\0') {
            snprintf(
                combined, sizeof(combined), "%s: %s", page->fields[i].label, page->fields[i].value);
        } else {
            snprintf(combined, sizeof(combined), "%s", page->fields[i].value);
        }

        canvas_set_font(canvas, page->fields[i].highlight ? FontPrimary : FontSecondary);
        uint16_t w = canvas_string_width(canvas, combined);
        canvas_set_font(canvas, FontSecondary);

        if(w <= CARD_VIEW_MAX_WIDTH || page->fields[i].label[0] == '\0') {
            /* Fits on one line (plain text is never split - it has no
               label to break on) */
            card_view_draw_line(canvas, line, m->scroll_y, combined, page->fields[i].highlight);
            line++;
        } else {
            /* Split: "Label:" on line 1, "  Value" on line 2 */
            char label_line[METROFLIP_CARD_VIEW_LABEL_LEN + 2];
            snprintf(label_line, sizeof(label_line), "%s:", page->fields[i].label);
            card_view_draw_line(canvas, line, m->scroll_y, label_line, page->fields[i].highlight);
            line++;

            char value_line[METROFLIP_CARD_VIEW_VALUE_LEN + 3];
            snprintf(value_line, sizeof(value_line), "  %s", page->fields[i].value);
            card_view_draw_line(canvas, line, m->scroll_y, value_line, page->fields[i].highlight);
            line++;
        }
    }

    /* Store total lines for input callback scroll clamping */
    m->total_lines = line;

    /* ── Scrollbar ── */
    if(line > CARD_VIEW_VIS_LINES) {
        uint8_t sb_h = CARD_VIEW_VIS_LINES * CARD_VIEW_LINE_H;
        uint8_t ind_h = sb_h * CARD_VIEW_VIS_LINES / line;
        if(ind_h < 4) ind_h = 4;
        uint16_t max_sc = line - CARD_VIEW_VIS_LINES;
        uint8_t ind_y = CARD_VIEW_CONTENT_Y;
        if(max_sc > 0) {
            ind_y += (uint8_t)((uint32_t)(sb_h - ind_h) * (uint16_t)m->scroll_y / max_sc);
        }
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_line(canvas, 126, CARD_VIEW_CONTENT_Y, 126, CARD_VIEW_CONTENT_Y + sb_h - 1);
        canvas_draw_box(canvas, 125, ind_y, 3, ind_h);
    }

footer:
    /* ── Footer ── */
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 0, CARD_VIEW_FOOTER_Y, 127, CARD_VIEW_FOOTER_Y);
    canvas_set_font(canvas, FontSecondary);

    if(m->current_page > 0) {
        canvas_draw_str(canvas, 2, 63, "<");
    }
    if(m->page_count > 1 && m->current_page < m->page_count - 1) {
        canvas_draw_str(canvas, 122, 63, ">");
    }

    if(m->show_save) {
        canvas_draw_str(canvas, 42, 63, "OK=Save");
    } else if(m->show_delete) {
        canvas_draw_str(canvas, 42, 63, "OK=Del");
    }
}

static bool metroflip_card_view_input(InputEvent* event, void* context) {
    Metroflip* app = (Metroflip*)context;

    /* Handle Back directly so the view dispatcher never reaches the
       navigation callback (which can cause the app to exit). We consume
       ALL back event types (Press, Short, Release) so a pending release
       after a scene transition doesn't leak through. */
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
        }
        return true;
    }

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp: {
            with_view_model(
                app->card_view,
                MetroflipCardViewModel * m,
                {
                    if(m->scroll_y > 0) m->scroll_y--;
                },
                true);
            return true;
        }
        case InputKeyDown: {
            with_view_model(
                app->card_view,
                MetroflipCardViewModel * m,
                {
                    int16_t max_s = (int16_t)m->total_lines - CARD_VIEW_VIS_LINES;
                    if(max_s < 0) max_s = 0;
                    if(m->scroll_y < max_s) m->scroll_y++;
                },
                true);
            return true;
        }
        case InputKeyLeft: {
            with_view_model(
                app->card_view,
                MetroflipCardViewModel * m,
                {
                    if(m->current_page > 0) {
                        m->current_page--;
                        m->scroll_y = 0;
                    }
                },
                true);
            return true;
        }
        case InputKeyRight: {
            with_view_model(
                app->card_view,
                MetroflipCardViewModel * m,
                {
                    if(m->page_count > 1 && m->current_page < m->page_count - 1) {
                        m->current_page++;
                        m->scroll_y = 0;
                    }
                },
                true);
            return true;
        }
        case InputKeyOk: {
            if(!app->card_view) break;
            bool do_save = false;
            bool do_delete = false;
            with_view_model(
                app->card_view,
                MetroflipCardViewModel * m,
                {
                    do_save = m->show_save;
                    do_delete = m->show_delete;
                },
                false);
            if(do_save) {
                scene_manager_next_scene(app->scene_manager, MetroflipSceneSave);
                return true;
            } else if(do_delete) {
                scene_manager_next_scene(app->scene_manager, MetroflipSceneDelete);
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return false;
}

/* Icon sets that get randomly assigned */
typedef struct {
    const Icon* static_icon;
    const Icon* frames[3];
} CardIconSet;

static void card_view_assign_random_icon(MetroflipCardViewModel* m) {
    const CardIconSet sets[] = {
        {&I_Train_10x10, {&I_Train1_10x10, &I_Train2_10x10, &I_Train3_10x10}},
        {&I_Wallet_10x10, {&I_Wallet1_10x10, &I_Wallet2_10x10, &I_Wallet3_10x10}},
        {&I_Ticket_10x10, {&I_Ticket1_10x10, &I_Ticket2_10x10, &I_Ticket3_10x10}},
        {&I_CardGeneric_10x10,
         {&I_CardGeneric1_10x10, &I_CardGeneric2_10x10, &I_CardGeneric3_10x10}},
    };
    uint8_t idx = rand() % 4;
    m->icon = sets[idx].static_icon;
    m->anim[0] = sets[idx].frames[0];
    m->anim[1] = sets[idx].frames[1];
    m->anim[2] = sets[idx].frames[2];
    m->anim_frame = 0;
}

/* ── Public API ── */

View* metroflip_card_view_alloc(Metroflip* app) {
    /* The card view is persistent - allocated once and reused.
       If it already exists, just reset the model for the new card. */
    if(app->card_view) {
        with_view_model(
            app->card_view,
            MetroflipCardViewModel * m,
            {
                card_view_model_clear(m);
                card_view_assign_random_icon(m);
            },
            false);
        return app->card_view;
    }

    View* view = view_alloc();
    view_set_context(view, app);
    /* Locking model: pages/fields are heap arrays that get reallocated while
       populating (possibly from the NFC worker thread) and freed on reset -
       the draw callback must never observe a stale pointer. */
    view_allocate_model(view, ViewModelTypeLocking, sizeof(MetroflipCardViewModel));
    view_set_draw_callback(view, metroflip_card_view_draw);
    view_set_input_callback(view, metroflip_card_view_input);
    view_set_previous_callback(view, metroflip_card_view_previous_callback);

    with_view_model(
        view,
        MetroflipCardViewModel * m,
        {
            memset(m, 0, sizeof(MetroflipCardViewModel));
            card_view_assign_random_icon(m);
        },
        false);

    app->card_view = view;
    view_dispatcher_add_view(app->view_dispatcher, MetroflipViewCardView, view);

    return view;
}

void metroflip_card_view_set_title(View* view, const char* title) {
    with_view_model(
        view,
        MetroflipCardViewModel * m,
        {
            strncpy(m->title, title, METROFLIP_CARD_VIEW_TITLE_LEN - 1);
            m->title[METROFLIP_CARD_VIEW_TITLE_LEN - 1] = '\0';
        },
        true);
}

void metroflip_card_view_set_icon(View* view, const Icon* icon) {
    with_view_model(view, MetroflipCardViewModel * m, { m->icon = icon; }, true);
}

void metroflip_card_view_set_icon_animation(
    View* view,
    const Icon* frame1,
    const Icon* frame2,
    const Icon* frame3) {
    with_view_model(
        view,
        MetroflipCardViewModel * m,
        {
            m->anim[0] = frame1;
            m->anim[1] = frame2;
            m->anim[2] = frame3;
            m->anim_frame = 0;
        },
        true);
}

uint8_t metroflip_card_view_add_page(View* view, const char* header) {
    uint8_t idx = UINT8_MAX;
    with_view_model(
        view,
        MetroflipCardViewModel * m,
        {
            if(m->page_count < METROFLIP_CARD_VIEW_MAX_PAGES) {
                if(m->page_count == m->page_capacity) {
                    uint16_t new_cap = m->page_capacity ? (uint16_t)m->page_capacity * 2 :
                                                          CARD_VIEW_INIT_PAGES;
                    if(new_cap > METROFLIP_CARD_VIEW_MAX_PAGES) {
                        new_cap = METROFLIP_CARD_VIEW_MAX_PAGES;
                    }
                    MetroflipCardPage* grown =
                        realloc(m->pages, new_cap * sizeof(MetroflipCardPage));
                    if(grown) {
                        m->pages = grown;
                        m->page_capacity = (uint8_t)new_cap;
                    } else {
                        FURI_LOG_E(TAG, "add_page: out of memory");
                    }
                }
                if(m->page_count < m->page_capacity) {
                    idx = m->page_count;
                    MetroflipCardPage* p = &m->pages[idx];
                    memset(p, 0, sizeof(MetroflipCardPage));
                    if(header) {
                        strncpy(p->header, header, METROFLIP_CARD_VIEW_HEADER_LEN - 1);
                        p->header[METROFLIP_CARD_VIEW_HEADER_LEN - 1] = '\0';
                    }
                    m->page_count++;
                }
            }
        },
        true);
    return idx;
}

void metroflip_card_view_add_field(
    View* view,
    uint8_t page,
    const char* label,
    const char* value,
    bool highlight) {
    with_view_model(
        view,
        MetroflipCardViewModel * m,
        {
            if(page < m->page_count) {
                MetroflipCardPage* p = &m->pages[page];
                if(p->field_count < METROFLIP_CARD_VIEW_MAX_FIELDS) {
                    if(p->field_count == p->field_capacity) {
                        uint16_t new_cap = p->field_capacity ? (uint16_t)p->field_capacity * 2 :
                                                               CARD_VIEW_INIT_FIELDS;
                        if(new_cap > METROFLIP_CARD_VIEW_MAX_FIELDS) {
                            new_cap = METROFLIP_CARD_VIEW_MAX_FIELDS;
                        }
                        MetroflipCardField* grown =
                            realloc(p->fields, new_cap * sizeof(MetroflipCardField));
                        if(grown) {
                            p->fields = grown;
                            p->field_capacity = (uint8_t)new_cap;
                        } else {
                            FURI_LOG_E(TAG, "add_field: out of memory, field dropped");
                        }
                    }
                    if(p->field_count < p->field_capacity) {
                        MetroflipCardField* f = &p->fields[p->field_count];
                        strncpy(f->label, label, METROFLIP_CARD_VIEW_LABEL_LEN - 1);
                        f->label[METROFLIP_CARD_VIEW_LABEL_LEN - 1] = '\0';
                        strncpy(f->value, value, METROFLIP_CARD_VIEW_VALUE_LEN - 1);
                        f->value[METROFLIP_CARD_VIEW_VALUE_LEN - 1] = '\0';
                        f->highlight = highlight;
                        p->field_count++;
                    }
                }
            }
        },
        true);
}

void metroflip_card_view_set_save(View* view, bool show) {
    with_view_model(view, MetroflipCardViewModel * m, { m->show_save = show; }, true);
}

void metroflip_card_view_set_delete(View* view, bool show) {
    with_view_model(view, MetroflipCardViewModel * m, { m->show_delete = show; }, true);
}

void metroflip_card_view_show(Metroflip* app) {
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewCardView);
}

void metroflip_card_view_free(Metroflip* app) {
    /* Reset the model (and free its page/field pools) but keep the view
       alive and registered. */
    if(app->card_view) {
        with_view_model(
            app->card_view, MetroflipCardViewModel * m, { card_view_model_clear(m); }, false);
    }
}

void metroflip_card_view_destroy(Metroflip* app) {
    if(app->card_view) {
        view_set_draw_callback(app->card_view, NULL);
        view_set_input_callback(app->card_view, NULL);
        view_set_previous_callback(app->card_view, NULL);
        view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewCardView);
        with_view_model(
            app->card_view, MetroflipCardViewModel * m, { card_view_model_clear(m); }, false);
        view_free_model(app->card_view);
        view_free(app->card_view);
        app->card_view = NULL;
    }
}
