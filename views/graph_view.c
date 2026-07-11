#include "graph_view.h"
#include "../modules/graph_engine.h"
#include <gui/elements.h>
#include <math.h>

struct GraphView {
    View* view;
};

typedef struct {
    Session* session;
    uint16_t selected;
} GraphViewModel;

/* Ellipse layout centred on the canvas, leaving room for the info bar. */
static void node_position(uint16_t i, uint16_t n, int32_t* x, int32_t* y) {
    float angle = (2.0f * (float)M_PI * (float)i) / (float)(n ? n : 1) - (float)M_PI / 2.0f;
    *x = 64 + (int32_t)(48.0f * cosf(angle));
    *y = 26 + (int32_t)(18.0f * sinf(angle));
}

static uint16_t index_of_id(const Session* session, uint16_t id) {
    for(uint16_t i = 0; i < session->asset_count; i++) {
        if(session->assets[i].id == id) return i;
    }
    return RECON_INVALID_INDEX;
}

static void graph_view_draw_callback(Canvas* canvas, void* model) {
    GraphViewModel* m = model;
    canvas_clear(canvas);

    Session* session = m->session;
    if(!session || session->asset_count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "No assets to graph");
        return;
    }

    uint16_t n = session->asset_count;

    /* edges first, so nodes draw on top */
    for(uint16_t e = 0; e < session->relation_count; e++) {
        const Relation* rel = &session->relations[e];
        uint16_t fi = index_of_id(session, rel->from_id);
        uint16_t ti = index_of_id(session, rel->to_id);
        if(fi == RECON_INVALID_INDEX || ti == RECON_INVALID_INDEX) continue;
        int32_t x1, y1, x2, y2;
        node_position(fi, n, &x1, &y1);
        node_position(ti, n, &x2, &y2);
        canvas_draw_line(canvas, x1, y1, x2, y2);

        /* directional arrowhead near the destination node */
        float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
        float len = sqrtf(dx * dx + dy * dy);
        if(len >= 8.0f) {
            float ux = dx / len, uy = dy / len; /* unit along edge */
            float tipx = (float)x2 - ux * 4.0f; /* just outside node radius */
            float tipy = (float)y2 - uy * 4.0f;
            float bl = 4.0f; /* barb length */
            float px = -uy, py = ux; /* perpendicular */
            canvas_draw_line(
                canvas,
                (int)tipx,
                (int)tipy,
                (int)(tipx - ux * bl + px * bl * 0.6f),
                (int)(tipy - uy * bl + py * bl * 0.6f));
            canvas_draw_line(
                canvas,
                (int)tipx,
                (int)tipy,
                (int)(tipx - ux * bl - px * bl * 0.6f),
                (int)(tipy - uy * bl - py * bl * 0.6f));
        }
    }

    /* nodes + short labels */
    canvas_set_font(canvas, FontSecondary);
    for(uint16_t i = 0; i < n; i++) {
        int32_t x, y;
        node_position(i, n, &x, &y);
        if(i == m->selected) {
            canvas_draw_disc(canvas, x, y, 3);
            canvas_draw_circle(canvas, x, y, 5);
        } else {
            canvas_draw_disc(canvas, x, y, 2);
        }
        /* 3-char label, pushed outward from the graph centre to reduce overlap */
        char label[4];
        strncpy(label, session->assets[i].name, 3);
        label[3] = '\0';
        Align ha = (x < 64) ? AlignRight : AlignLeft;
        int lx = (x < 64) ? x - 7 : x + 7;
        canvas_draw_str_aligned(canvas, lx, y, ha, AlignCenter, label);
    }

    /* info bar for the selected asset */
    const Asset* a = &session->assets[m->selected];
    canvas_draw_line(canvas, 0, 52, 128, 52);
    canvas_set_font(canvas, FontSecondary);
    char line[64];
    snprintf(line, sizeof(line), "%s r%u  %s", a->name, a->risk, asset_type_name(a->type));
    canvas_draw_str_aligned(canvas, 2, 58, AlignLeft, AlignCenter, line);
    elements_button_left(canvas, "Prev");
    elements_button_right(canvas, "Next");
}

static bool graph_view_input_callback(InputEvent* event, void* context) {
    GraphView* graph_view = context;
    bool consumed = false;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyLeft || event->key == InputKeyRight) {
            with_view_model(
                graph_view->view,
                GraphViewModel * model,
                {
                    if(model->session && model->session->asset_count > 0) {
                        uint16_t n = model->session->asset_count;
                        if(event->key == InputKeyRight) {
                            model->selected = (model->selected + 1) % n;
                        } else {
                            model->selected = (model->selected + n - 1) % n;
                        }
                    }
                },
                true);
            consumed = true;
        }
    }
    return consumed;
}

GraphView* graph_view_alloc(void) {
    GraphView* graph_view = malloc(sizeof(GraphView));
    graph_view->view = view_alloc();
    view_set_context(graph_view->view, graph_view);
    view_allocate_model(graph_view->view, ViewModelTypeLocking, sizeof(GraphViewModel));
    view_set_draw_callback(graph_view->view, graph_view_draw_callback);
    view_set_input_callback(graph_view->view, graph_view_input_callback);
    return graph_view;
}

void graph_view_free(GraphView* graph_view) {
    furi_check(graph_view);
    view_free(graph_view->view);
    free(graph_view);
}

View* graph_view_get_view(GraphView* graph_view) {
    furi_check(graph_view);
    return graph_view->view;
}

void graph_view_set_session(GraphView* graph_view, Session* session) {
    furi_check(graph_view);
    with_view_model(
        graph_view->view,
        GraphViewModel * model,
        {
            model->session = session;
            model->selected = 0;
        },
        true);
}
