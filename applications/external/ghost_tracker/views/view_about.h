#pragma once

#include <gui/view.h>

/**
 * "About" screen: a quick pin reference, two pages switchable with
 * LEFT/RIGHT - page 1 is a plain pinout list (no diagrams/explanations,
 * those live in the project README instead), page 2 is a QR code linking
 * to the GitHub repo.
 */

typedef struct AboutView AboutView;

AboutView* about_view_alloc(void);
void about_view_free(AboutView* instance);
View* about_view_get_view(AboutView* instance);
