// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT
//
// Custom views: list with icons and subtitles, entry card, 2FA code, busy screen.
#pragma once

#include <gui/canvas.h>
#include <gui/icon.h>
#include <gui/view.h>

#define VF_HEADER_H 11

/* Black title bar with white bold text and an optional right-aligned note. */
void vf_draw_header(Canvas* canvas, const char* title, const char* note, const Icon* icon);
/* Draw text cut to max_width pixels (adds ".." when cut). */
void vf_draw_str_fit(Canvas* canvas, int x, int y, int max_width, const char* text);

/* ---------- list ---------- */

typedef enum {
    VfListPlain, /* one line per row */
    VfListOpen, /* the selected row opens and shows its subtitle */
    VfListDetail, /* every row shows title and subtitle */
} VfListStyle;

typedef struct {
    const char* title;
    const char* subtitle; /* NULL = single line */
    const Icon* icon; /* NULL = no icon */
    const char* value; /* right-aligned on the title line, e.g. a 2FA code */
} VfListItem;

/* Fill item for index; sub_buf may be used for a generated subtitle. */
typedef void (
    *VfListItemFn)(void* ctx, uint32_t index, VfListItem* item, char* sub_buf, size_t sub_size);
typedef void (*VfListSelectFn)(void* ctx, uint32_t index);

typedef struct VfList VfList;

VfList* vf_list_alloc(void);
void vf_list_free(VfList* list);
View* vf_list_get_view(VfList* list);
void vf_list_set(
    VfList* list,
    const char* header,
    const Icon* header_icon,
    uint32_t count,
    VfListStyle style,
    VfListItemFn item_fn,
    VfListSelectFn select_fn,
    void* ctx,
    uint32_t selected);
uint32_t vf_list_get_selected(VfList* list);
/* Redraw, e.g. after the data behind item_fn changed. */
void vf_list_refresh(VfList* list);
/* Header note instead of the position (NULL = position) and a countdown line
 * under the header, 0..128 px wide (0 = none). Redraws. */
void vf_list_set_status(VfList* list, const char* note, uint8_t bar);

/* ---------- entry card ---------- */

typedef enum {
    VfCardTypeUser,
    VfCardTypePass,
    VfCardTypeLogin,
    VfCardMore,
} VfCardAction;

typedef void (*VfCardFn)(void* ctx, VfCardAction action);

typedef struct VfCard VfCard;

VfCard* vf_card_alloc(void);
void vf_card_free(VfCard* card);
View* vf_card_get_view(VfCard* card);
/* Strings must stay valid while the card is shown. */
void vf_card_set(
    VfCard* card,
    const char* name,
    const char* user,
    const char* pass,
    const char* uri,
    const char* note,
    const char* labels[4], /* user, pass, login, hidden-password hint */
    VfCardFn fn,
    void* ctx);
void vf_card_hide_password(VfCard* card);

/* ---------- 2FA code ---------- */

typedef enum {
    VfCodeType,
    VfCodePrev,
    VfCodeNext,
} VfCodeAction;

typedef void (*VfCodeFn)(void* ctx, VfCodeAction action);

typedef struct VfCode VfCode;

VfCode* vf_code_alloc(void);
void vf_code_free(VfCode* code);
View* vf_code_get_view(VfCode* code);
/* Strings must stay valid while the view is shown. cycle: Left/Right switch entries. */
void vf_code_set(
    VfCode* code,
    const char* name,
    const char* user,
    const char* note,
    const char* type_label,
    bool cycle,
    VfCodeFn fn,
    void* ctx);
/* Current code (or a short message when message is true), seconds left, period. */
void vf_code_update(VfCode* code, const char* text, bool message, uint32_t left, uint32_t period);
void vf_code_clear(VfCode* code);

/* ---------- busy screen ---------- */

typedef struct VfBusy VfBusy;

VfBusy* vf_busy_alloc(void);
void vf_busy_free(VfBusy* busy);
View* vf_busy_get_view(VfBusy* busy);
void vf_busy_set(VfBusy* busy, const char* text, const Icon* icon);
