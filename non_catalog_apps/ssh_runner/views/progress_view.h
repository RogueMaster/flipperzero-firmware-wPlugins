/* Progress screen: the stock Popup layout (title on top, icon, centred status lines) with
 * the title in FontKeyboard when it is an address or a name (it reads better in a fixed-width
 * font; the stock Popup draws every title in FontPrimary). No allocation after alloc: the
 * connect spinner redraws it 4 times a second. SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <gui/view.h>

typedef struct ProgressView ProgressView;

ProgressView* progress_view_alloc(void);
void progress_view_free(ProgressView* pv);
View* progress_view_get_view(ProgressView* pv);

/* Everything is copied. `icon` (may be NULL) is centred at row 18; `text` may hold two
 * lines (status, hint). */
void progress_view_set(
    ProgressView* pv,
    const char* title,
    bool title_mono,
    const Icon* icon,
    const char* text);
void progress_view_reset(ProgressView* pv);
