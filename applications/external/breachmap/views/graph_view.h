#pragma once

#include <gui/view.h>
#include "../models/session.h"

typedef struct GraphView GraphView;

GraphView* graph_view_alloc(void);
void graph_view_free(GraphView* graph_view);
View* graph_view_get_view(GraphView* graph_view);

/* Point the view at a session and select the first node. */
void graph_view_set_session(GraphView* graph_view, Session* session);
