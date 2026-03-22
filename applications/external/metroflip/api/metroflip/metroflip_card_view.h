#pragma once

#include <gui/view.h>
#include <gui/canvas.h>
#include <gui/icon.h>
#include <stdint.h>
#include <stdbool.h>

#define METROFLIP_CARD_VIEW_MAX_FIELDS  5
#define METROFLIP_CARD_VIEW_MAX_PAGES   16
#define METROFLIP_CARD_VIEW_LABEL_LEN   16
#define METROFLIP_CARD_VIEW_VALUE_LEN   24
#define METROFLIP_CARD_VIEW_HEADER_LEN  24
#define METROFLIP_CARD_VIEW_TITLE_LEN   20
#define METROFLIP_CARD_VIEW_ANIM_FRAMES 3

typedef struct {
    char label[METROFLIP_CARD_VIEW_LABEL_LEN];
    char value[METROFLIP_CARD_VIEW_VALUE_LEN];
    bool highlight;
} MetroflipCardField;

typedef struct {
    char header[METROFLIP_CARD_VIEW_HEADER_LEN];
    MetroflipCardField fields[METROFLIP_CARD_VIEW_MAX_FIELDS];
    uint8_t field_count;
} MetroflipCardPage;

typedef struct {
    char title[METROFLIP_CARD_VIEW_TITLE_LEN];
    const Icon* icon;
    const Icon* anim[METROFLIP_CARD_VIEW_ANIM_FRAMES]; // animation frames (NULL = no anim)
    uint8_t anim_frame;
    MetroflipCardPage pages[METROFLIP_CARD_VIEW_MAX_PAGES];
    uint8_t page_count;
    uint8_t current_page;
    int8_t scroll_y; // lines scrolled (0 = top)
    uint8_t total_lines; // set by draw callback for scroll clamping
    bool show_save;
    bool show_delete;
} MetroflipCardViewModel;

// Forward declare - full definition is in metroflip_i.h
typedef struct Metroflip Metroflip;

#ifdef __cplusplus
extern "C" {
#endif

// Allocate card view, register with view dispatcher, store in app->card_view
View* metroflip_card_view_alloc(Metroflip* app);

// Set card title shown in header bar
void metroflip_card_view_set_title(View* view, const char* title);

// Set optional icon shown in header bar (top-left)
void metroflip_card_view_set_icon(View* view, const Icon* icon);

// Set animated icon frames (3 frames, cycled automatically)
void metroflip_card_view_set_icon_animation(
    View* view,
    const Icon* frame1,
    const Icon* frame2,
    const Icon* frame3);

// Add a page and return its index (0-based), or UINT8_MAX on failure
uint8_t metroflip_card_view_add_page(View* view, const char* header);

// Add a field to an existing page
void metroflip_card_view_add_field(
    View* view,
    uint8_t page,
    const char* label,
    const char* value,
    bool highlight);

// Show/hide the OK=Save or OK=Delete button
void metroflip_card_view_set_save(View* view, bool show);
void metroflip_card_view_set_delete(View* view, bool show);

// Switch the view dispatcher to display the card view
void metroflip_card_view_show(Metroflip* app);

// Reset card view model (keeps the view alive and registered)
void metroflip_card_view_free(Metroflip* app);

// Actually free the card view - only call during app exit
void metroflip_card_view_destroy(Metroflip* app);

#ifdef __cplusplus
}
#endif
