#pragma once

#include <gui/view.h>

typedef struct AboutView AboutView;

AboutView* about_view_alloc(void);

void about_view_free(AboutView* instance);

View* about_view_get_view(AboutView* instance);
