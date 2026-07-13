#include "labs_list_view.h"

#include <furi.h>
#include <gui/elements.h>

#include "../helpers/pocketlab_content.h"
#include "../helpers/pocketlab_fonts.h"
#include "../helpers/pocketlab_labtext.h"
#include "../helpers/pocketlab_sound.h"

#define LABS_ROW_HEIGHT   16
#define LABS_VISIBLE_ROWS 4
#define LABS_MAX_ITEMS    72 // tracks + labs, with headroom
#define LABS_ANIM_MS      90 // marquee tick for the selected row

struct LabsListView {
    View* view;
    FuriTimer* timer;
    LabsListViewCallback callback;
    void* context;
    NotificationApp* notifications;
    bool sound_enabled;
};

// A display row is either a track header or a lab.
typedef struct {
    bool is_header;
    uint8_t value; // header: track enum; lab: lab index
} LabsItem;

typedef struct {
    uint64_t completed_mask;
    LabsItem items[LABS_MAX_ITEMS];
    size_t item_count;
    size_t selected; // display index, always points at a lab
    size_t offset; // first visible display row
    uint32_t anim; // marquee tick, reset when the selection moves
} LabsListModel;

// Builds the grouped display list: for each track that has labs, a header
// followed by its labs, regardless of the order labs appear in the array.
static void labs_list_view_build(LabsListModel* model) {
    size_t n = 0;
    for(uint8_t track = 0; track < PocketLabTrackCount; track++) {
        bool has = false;
        for(size_t i = 0; i < pocketlab_labs_count; i++) {
            if(pocketlab_labs[i].track == track) {
                has = true;
                break;
            }
        }
        if(!has || n >= LABS_MAX_ITEMS) continue;

        model->items[n].is_header = true;
        model->items[n].value = track;
        n++;

        for(size_t i = 0; i < pocketlab_labs_count && n < LABS_MAX_ITEMS; i++) {
            if(pocketlab_labs[i].track == track) {
                model->items[n].is_header = false;
                model->items[n].value = (uint8_t)i;
                n++;
            }
        }
    }
    model->item_count = n;
}

static size_t labs_list_view_first_lab(const LabsListModel* model) {
    for(size_t i = 0; i < model->item_count; i++) {
        if(!model->items[i].is_header) return i;
    }
    return 0;
}

static void labs_list_view_scroll(LabsListModel* model) {
    if(model->selected < model->offset) {
        model->offset = model->selected;
    } else if(model->selected >= model->offset + LABS_VISIBLE_ROWS) {
        model->offset = model->selected - LABS_VISIBLE_ROWS + 1;
    }
}

static void
    labs_list_view_track_progress(uint8_t track, uint64_t mask, uint8_t* done, uint8_t* total) {
    *done = 0;
    *total = 0;
    for(size_t i = 0; i < pocketlab_labs_count; i++) {
        if(pocketlab_labs[i].track != track) continue;
        (*total)++;
        if(mask & (1ULL << i)) (*done)++;
    }
}

static void labs_list_view_draw_check(Canvas* canvas, uint8_t cx, uint8_t cy) {
    canvas_draw_line(canvas, cx - 2, cy, cx - 1, cy + 2);
    canvas_draw_line(canvas, cx - 1, cy + 2, cx + 2, cy - 2);
}

static void
    labs_list_view_draw_indicator(Canvas* canvas, uint8_t cx, uint8_t cy, bool done, bool selected) {
    const Color foreground = selected ? ColorWhite : ColorBlack;
    const Color background = selected ? ColorBlack : ColorWhite;

    if(!done) {
        canvas_set_color(canvas, foreground);
        canvas_draw_circle(canvas, cx, cy, 4);
    } else if(selected) {
        canvas_set_color(canvas, foreground);
        canvas_draw_circle(canvas, cx, cy, 4);
        labs_list_view_draw_check(canvas, cx, cy);
    } else {
        canvas_set_color(canvas, foreground);
        canvas_draw_disc(canvas, cx, cy, 4);
        canvas_set_color(canvas, background);
        labs_list_view_draw_check(canvas, cx, cy);
    }

    canvas_set_color(canvas, foreground);
}

// Draw the selected row's lab title. If it is too wide it ping-pong scrolls so
// the whole title can be read; overflow is repainted with the row background and
// the indicator is redrawn on top. Non-selected rows are drawn plainly.
static void labs_list_view_draw_title(
    Canvas* canvas,
    const char* title,
    uint8_t y,
    bool selected,
    bool done,
    uint32_t anim) {
    const int x0 = 20;
    const int ty = y + LABS_ROW_HEIGHT / 2 + 4;
    const int right = 122; // keep clear of the scrollbar
    const int avail = right - x0;
    const int tw = canvas_string_width(canvas, title);

    if(!selected || tw <= avail) {
        canvas_draw_str(canvas, x0, ty, title);
        return;
    }

    const int span = tw - avail;
    const int hold = 12; // frames paused at each end
    const int total = 2 * (span + hold);
    const int t = (int)(anim % (uint32_t)total);
    int off;
    if(t < hold) {
        off = 0;
    } else if(t < hold + span) {
        off = t - hold;
    } else if(t < 2 * hold + span) {
        off = span;
    } else {
        off = span - (t - 2 * hold - span);
    }

    canvas_draw_str(canvas, x0 - off, ty, title);
    // Repaint the strips outside [x0, right) with the selected row's black
    // background, then redraw the indicator over the left strip.
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, y, x0, LABS_ROW_HEIGHT);
    canvas_draw_box(canvas, right, y, 128 - right, LABS_ROW_HEIGHT);
    labs_list_view_draw_indicator(canvas, 10, y + LABS_ROW_HEIGHT / 2, done, true);
}

static void labs_list_view_draw_callback(Canvas* canvas, void* context) {
    LabsListModel* model = context;
    canvas_clear(canvas);

    for(size_t r = 0; r < LABS_VISIBLE_ROWS; r++) {
        const size_t idx = model->offset + r;
        if(idx >= model->item_count) break;

        const uint8_t y = r * LABS_ROW_HEIGHT;
        const LabsItem* item = &model->items[idx];

        if(item->is_header) {
            pocketlab_font_apply(canvas, true);
            canvas_draw_str(
                canvas,
                4,
                y + LABS_ROW_HEIGHT - 5,
                pocketlab_tr(pocketlab_track_name(item->value)));

            uint8_t done = 0, total = 0;
            labs_list_view_track_progress(item->value, model->completed_mask, &done, &total);
            char count[8];
            snprintf(count, sizeof(count), "%u/%u", done, total);
            pocketlab_font_apply_small(canvas);
            canvas_draw_str_aligned(
                canvas, 124, y + LABS_ROW_HEIGHT - 4, AlignRight, AlignBottom, count);
            continue;
        }

        const bool selected = idx == model->selected;
        const bool done = (model->completed_mask & (1ULL << item->value)) != 0;

        if(selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y, 128, LABS_ROW_HEIGHT);
        }

        labs_list_view_draw_indicator(canvas, 10, y + LABS_ROW_HEIGHT / 2, done, selected);

        pocketlab_font_apply_small(canvas);
        labs_list_view_draw_title(
            canvas,
            pocketlab_tr(pocketlab_labs[item->value].title),
            y,
            selected,
            done,
            model->anim);

        canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar(canvas, model->selected, model->item_count);
}

static bool labs_list_view_input_callback(InputEvent* event, void* context) {
    LabsListView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    bool consumed = false;
    bool selected = false;
    bool play_nav = false;
    uint32_t lab_index = 0;

    with_view_model(
        instance->view,
        LabsListModel * model,
        {
            const size_t prev = model->selected;
            if(event->key == InputKeyUp) {
                for(size_t j = model->selected; j-- > 0;) {
                    if(!model->items[j].is_header) {
                        model->selected = j;
                        break;
                    }
                }
                labs_list_view_scroll(model);
                consumed = true;
            } else if(event->key == InputKeyDown) {
                for(size_t j = model->selected + 1; j < model->item_count; j++) {
                    if(!model->items[j].is_header) {
                        model->selected = j;
                        break;
                    }
                }
                labs_list_view_scroll(model);
                consumed = true;
            } else if(event->key == InputKeyOk) {
                selected = true;
                lab_index = model->items[model->selected].value;
                consumed = true;
            }
            play_nav = !selected && model->selected != prev;
            if(model->selected != prev) model->anim = 0; // restart the marquee
        },
        true);

    if(play_nav) {
        pocketlab_sound_play(instance->notifications, instance->sound_enabled, PocketLabSoundType);
    }
    if(selected && instance->callback) {
        instance->callback(instance->context, lab_index);
    }

    return consumed;
}

static void labs_list_view_timer_callback(void* context) {
    LabsListView* instance = context;
    with_view_model(instance->view, LabsListModel * model, { model->anim++; }, true);
}

static void labs_list_view_enter_callback(void* context) {
    LabsListView* instance = context;
    with_view_model(instance->view, LabsListModel * model, { model->anim = 0; }, false);
    furi_timer_start(instance->timer, furi_ms_to_ticks(LABS_ANIM_MS));
}

static void labs_list_view_exit_callback(void* context) {
    LabsListView* instance = context;
    furi_timer_stop(instance->timer);
}

LabsListView* labs_list_view_alloc(void) {
    LabsListView* instance = malloc(sizeof(LabsListView));
    instance->view = view_alloc();
    instance->callback = NULL;
    instance->context = NULL;
    instance->notifications = NULL;
    instance->sound_enabled = false;

    view_set_context(instance->view, instance);
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(LabsListModel));
    view_set_draw_callback(instance->view, labs_list_view_draw_callback);
    view_set_input_callback(instance->view, labs_list_view_input_callback);
    view_set_enter_callback(instance->view, labs_list_view_enter_callback);
    view_set_exit_callback(instance->view, labs_list_view_exit_callback);

    instance->timer =
        furi_timer_alloc(labs_list_view_timer_callback, FuriTimerTypePeriodic, instance);

    return instance;
}

void labs_list_view_free(LabsListView* instance) {
    furi_assert(instance);
    furi_timer_stop(instance->timer);
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* labs_list_view_get_view(LabsListView* instance) {
    furi_assert(instance);
    return instance->view;
}

void labs_list_view_set_callback(
    LabsListView* instance,
    LabsListViewCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->context = context;
}

void labs_list_view_configure(
    LabsListView* instance,
    uint64_t completed_mask,
    uint32_t selected,
    NotificationApp* notifications,
    bool sound_enabled) {
    furi_assert(instance);
    instance->notifications = notifications;
    instance->sound_enabled = sound_enabled;
    with_view_model(
        instance->view,
        LabsListModel * model,
        {
            model->completed_mask = completed_mask;
            labs_list_view_build(model);
            if(selected < model->item_count && !model->items[selected].is_header) {
                model->selected = selected;
            } else {
                model->selected = labs_list_view_first_lab(model);
            }
            model->offset = 0;
            labs_list_view_scroll(model);
        },
        true);
}

uint32_t labs_list_view_get_selected(LabsListView* instance) {
    furi_assert(instance);
    uint32_t selected = 0;
    with_view_model(instance->view, LabsListModel * model, { selected = model->selected; }, false);
    return selected;
}
