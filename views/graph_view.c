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
    bool move_mode; /* reposition the selected node (floorplan layout) */
} GraphViewModel;

/* Node position: stored floorplan coordinate if set, else auto ellipse layout. */
static void node_position(const Session* session, uint16_t i, uint16_t n, int32_t* x, int32_t* y) {
    const Asset* a = &session->assets[i];
    if(a->gx != RECON_COORD_AUTO && a->gy != RECON_COORD_AUTO) {
        *x = a->gx;
        *y = a->gy;
        return;
    }
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

    bool on_path[RECON_MAX_RELATIONS];
    graph_attack_path(session, on_path);

    /* edges first, so nodes draw on top */
    for(uint16_t e = 0; e < session->relation_count; e++) {
        const Relation* rel = &session->relations[e];
        uint16_t fi = index_of_id(session, rel->from_id);
        uint16_t ti = index_of_id(session, rel->to_id);
        if(fi == RECON_INVALID_INDEX || ti == RECON_INVALID_INDEX) continue;
        int32_t x1, y1, x2, y2;
        node_position(session, fi, n, &x1, &y1);
        node_position(session, ti, n, &x2, &y2);
        canvas_draw_line(canvas, x1, y1, x2, y2);
        if(on_path[e]) {
            canvas_draw_line(canvas, x1, y1 + 1, x2, y2 + 1);
            canvas_draw_line(canvas, x1 + 1, y1, x2 + 1, y2);
        }

        /* directional arrowhead near the destination node */
        float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
        float len = sqrtf(dx * dx + dy * dy);
        if(len >= 8.0f) {
            float ux = dx / len, uy = dy / len;
            float tipx = (float)x2 - ux * 4.0f;
            float tipy = (float)y2 - uy * 4.0f;
            float bl = 4.0f;
            float px = -uy, py = ux;
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
        node_position(session, i, n, &x, &y);
        if(i == m->selected) {
            canvas_draw_disc(canvas, x, y, 3);
            canvas_draw_circle(canvas, x, y, 5);
        } else {
            canvas_draw_disc(canvas, x, y, 2);
        }
        char label[4];
        strncpy(label, session->assets[i].name, 3);
        label[3] = '\0';
        Align ha = (x < 64) ? AlignRight : AlignLeft;
        int lx = (x < 64) ? x - 7 : x + 7;
        canvas_draw_str_aligned(canvas, lx, y, ha, AlignCenter, label);
    }

    /* info bar */
    const Asset* a = &session->assets[m->selected];
    canvas_draw_line(canvas, 0, 52, 128, 52);
    canvas_set_font(canvas, FontSecondary);
    char line[64];
    snprintf(line, sizeof(line), "%s r%u  %s", a->name, a->risk, asset_type_name(a->type));
    canvas_draw_str_aligned(canvas, 2, 58, AlignLeft, AlignCenter, line);
    if(m->move_mode) {
        canvas_draw_str_aligned(canvas, 126, 58, AlignRight, AlignCenter, "MOVE");
    } else {
        elements_button_left(canvas, "Prev");
        elements_button_right(canvas, "Next");
    }
}

static bool graph_view_input_callback(InputEvent* event, void* context) {
    GraphView* graph_view = context;
    bool consumed = false;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    with_view_model(
        graph_view->view,
        GraphViewModel * model,
        {
            Session* s = model->session;
            if(!s || s->asset_count == 0) {
                consumed = false;
            } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                /* toggle floorplan move mode; seed coords from current position */
                model->move_mode = !model->move_mode;
                if(model->move_mode) {
                    Asset* a = &s->assets[model->selected];
                    if(a->gx == RECON_COORD_AUTO || a->gy == RECON_COORD_AUTO) {
                        int32_t x;
                        int32_t y;
                        node_position(s, model->selected, s->asset_count, &x, &y);
                        a->gx = (uint8_t)x;
                        a->gy = (uint8_t)y;
                    }
                }
                consumed = true;
            } else if(model->move_mode) {
                Asset* a = &s->assets[model->selected];
                int nx = a->gx;
                int ny = a->gy;
                if(event->key == InputKeyLeft) nx -= 3;
                if(event->key == InputKeyRight) nx += 3;
                if(event->key == InputKeyUp) ny -= 3;
                if(event->key == InputKeyDown) ny += 3;
                if(nx < 6) nx = 6;
                if(nx > 122) nx = 122;
                if(ny < 6) ny = 6;
                if(ny > 48) ny = 48;
                a->gx = (uint8_t)nx;
                a->gy = (uint8_t)ny;
                session_touch(s);
                consumed = true;
            } else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
                uint16_t nn = s->asset_count;
                if(event->key == InputKeyRight) {
                    model->selected = (model->selected + 1) % nn;
                } else {
                    model->selected = (model->selected + nn - 1) % nn;
                }
                consumed = true;
            }
        },
        true);
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
            model->move_mode = false;
        },
        true);
}
