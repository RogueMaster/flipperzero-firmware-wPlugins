/*
 * Survival Manual — offline reference reader for Flipper Zero.
 *
 * Content: the libre Survival Manual (github.com/ligi/SurvivalManual),
 * derived from US Army field manual FM 21-76.
 *
 * Architecture:
 *   Pages submenu  ->  Sections submenu  ->  TextBox reader
 * Text lives on the SD card as assets (NOT compiled into the binary).
 * Each section is loaded on demand by byte offset, so RAM use stays tiny
 * (largest section ~3.5 KB) no matter how big the chapter is.
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <storage/storage.h>
#include <string.h>
#include <stdlib.h>

#include "pages.h"

#define ASSETS_DIR "/ext/apps_assets/survival_manual/"
#define MAX_SECTIONS 160
#define COUNT_OF(x) (sizeof(x) / sizeof((x)[0]))

typedef enum {
    ViewPages,
    ViewSections,
    ViewText,
} ViewId;

typedef struct {
    uint32_t offset;
    uint32_t length;
    char title[40];
} SectionEntry;

typedef struct {
    Gui* gui;
    ViewDispatcher* vd;
    Submenu* pages_menu;
    Submenu* sections_menu;
    TextBox* text_box;
    Storage* storage;

    int current_page; // index into survival_pages[]
    SectionEntry sections[MAX_SECTIONS];
    int section_count;

    char* text_buf; // current section text on heap, or NULL
    FuriString* path; // scratch path builder
} App;

/* ---- navigation (previous) callbacks: return target view id ---- */
static uint32_t nav_to_pages(void* ctx) {
    UNUSED(ctx);
    return ViewPages;
}
static uint32_t nav_to_sections(void* ctx) {
    UNUSED(ctx);
    return ViewSections;
}
static uint32_t nav_exit(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

static void section_cb(void* ctx, uint32_t index); // fwd decl

/* Load a section's text by byte range and show it in the TextBox. */
static void open_section(App* app, int sidx) {
    if(sidx < 0 || sidx >= app->section_count) return;
    SectionEntry* s = &app->sections[sidx];

    if(app->text_buf) {
        free(app->text_buf);
        app->text_buf = NULL;
    }
    app->text_buf = malloc(s->length + 1);
    app->text_buf[0] = '\0';

    furi_string_printf(
        app->path, "%s%s.txt", ASSETS_DIR, survival_pages[app->current_page].file);

    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, furi_string_get_cstr(app->path), FSAM_READ, FSOM_OPENING)) {
        storage_file_seek(f, s->offset, true);
        uint16_t rd = storage_file_read(f, app->text_buf, (uint16_t)s->length);
        app->text_buf[rd] = '\0';
    } else {
        strncpy(app->text_buf, "Could not open chapter file.", s->length);
        app->text_buf[s->length] = '\0';
    }
    storage_file_close(f);
    storage_file_free(f);

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_text(app->text_box, app->text_buf);
    view_dispatcher_switch_to_view(app->vd, ViewText);
}

static void section_cb(void* ctx, uint32_t index) {
    open_section((App*)ctx, (int)index);
}

/* Parse a page's .idx file and build the sections submenu. */
static void open_page(App* app, int pidx) {
    app->current_page = pidx;
    app->section_count = 0;
    submenu_reset(app->sections_menu);
    submenu_set_header(app->sections_menu, survival_pages[pidx].title);

    furi_string_printf(app->path, "%s%s.idx", ASSETS_DIR, survival_pages[pidx].file);

    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, furi_string_get_cstr(app->path), FSAM_READ, FSOM_OPENING)) {
        uint64_t sz = storage_file_size(f);
        char* buf = malloc((size_t)sz + 1);
        uint16_t got = storage_file_read(f, buf, (uint16_t)sz);
        buf[got] = '\0';

        char* save = NULL;
        char* line = strtok_r(buf, "\n", &save);
        while(line && app->section_count < MAX_SECTIONS) {
            // line format: offset<TAB>length<TAB>title
            char* t1 = strchr(line, '\t');
            if(t1) {
                *t1 = '\0';
                char* t2 = strchr(t1 + 1, '\t');
                if(t2) {
                    *t2 = '\0';
                    SectionEntry* e = &app->sections[app->section_count];
                    e->offset = (uint32_t)strtoul(line, NULL, 10);
                    e->length = (uint32_t)strtoul(t1 + 1, NULL, 10);
                    strncpy(e->title, t2 + 1, sizeof(e->title) - 1);
                    e->title[sizeof(e->title) - 1] = '\0';
                    submenu_add_item(
                        app->sections_menu, e->title, app->section_count, section_cb, app);
                    app->section_count++;
                }
            }
            line = strtok_r(NULL, "\n", &save);
        }
        free(buf);
    } else {
        submenu_add_item(app->sections_menu, "(missing assets)", 0, section_cb, app);
    }
    storage_file_close(f);
    storage_file_free(f);

    view_dispatcher_switch_to_view(app->vd, ViewSections);
}

static void page_cb(void* ctx, uint32_t index) {
    open_page((App*)ctx, (int)index);
}

static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->path = furi_string_alloc();

    app->vd = view_dispatcher_alloc();

    app->pages_menu = submenu_alloc();
    submenu_set_header(app->pages_menu, "Survival Manual");
    for(size_t i = 0; i < SURVIVAL_PAGE_COUNT; i++) {
        submenu_add_item(app->pages_menu, survival_pages[i].title, i, page_cb, app);
    }
    app->sections_menu = submenu_alloc();
    app->text_box = text_box_alloc();

    view_dispatcher_add_view(app->vd, ViewPages, submenu_get_view(app->pages_menu));
    view_dispatcher_add_view(app->vd, ViewSections, submenu_get_view(app->sections_menu));
    view_dispatcher_add_view(app->vd, ViewText, text_box_get_view(app->text_box));

    view_set_previous_callback(submenu_get_view(app->pages_menu), nav_exit);
    view_set_previous_callback(submenu_get_view(app->sections_menu), nav_to_pages);
    view_set_previous_callback(text_box_get_view(app->text_box), nav_to_sections);

    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void app_free(App* app) {
    view_dispatcher_remove_view(app->vd, ViewPages);
    view_dispatcher_remove_view(app->vd, ViewSections);
    view_dispatcher_remove_view(app->vd, ViewText);

    submenu_free(app->pages_menu);
    submenu_free(app->sections_menu);
    text_box_free(app->text_box);
    view_dispatcher_free(app->vd);

    if(app->text_buf) free(app->text_buf);
    furi_string_free(app->path);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t survival_manual_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_switch_to_view(app->vd, ViewPages);
    view_dispatcher_run(app->vd);
    app_free(app);
    return 0;
}
