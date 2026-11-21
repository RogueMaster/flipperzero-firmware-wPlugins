#include "mock_loaders.h"

#include <furi.h>
#include <stdio.h>
#include <stdlib.h>
#include <storage/storage.h>

static const char* const gta_mock_loader_files[] = {
    "gta.bin",
    "gta_v2.bin",
    "original.bin",
    "rogue.bin",
    "wizard.bin",
    "ranger.bin",
    "paladin.bin",
    "cleric.bin",
    "bard.bin",
    "artificer.bin",
    "druid.bin",
    "monk.bin",
    "sorcerer.bin",
    "warlock.bin",
    "barbarian.bin",
};

enum {
    GtaMockLoaderWidth = 128,
    GtaMockLoaderHeight = 64,
    GtaMockLoaderBytes = GtaMockLoaderWidth * GtaMockLoaderHeight / 8,
    GtaMockLoaderCount = sizeof(gta_mock_loader_files) / sizeof(gta_mock_loader_files[0]),
};

typedef struct {
    uint8_t* bitmap;
    size_t selected;
    bool available;
} GtaMockLoaderModel;

static void gta_mock_loader_read(GtaMockLoaderModel* model) {
    model->available = false;
    if(!model->bitmap) model->bitmap = malloc(GtaMockLoaderBytes);
    if(!model->bitmap) return;

    char path[128];
    snprintf(
        path,
        sizeof(path),
        APP_ASSETS_PATH("mock_loaders/%s"),
        gta_mock_loader_files[model->selected]);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(file) {
        if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            model->available = storage_file_size(file) == GtaMockLoaderBytes &&
                               storage_file_read(file, model->bitmap, GtaMockLoaderBytes) ==
                                   GtaMockLoaderBytes;
            storage_file_close(file);
        }
        storage_file_free(file);
    }
    furi_record_close(RECORD_STORAGE);
}

static void gta_mock_loader_draw(Canvas* canvas, void* context) {
    const GtaMockLoaderModel* model = context;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(model->available) {
        canvas_draw_xbm(canvas, 0, 0, GtaMockLoaderWidth, GtaMockLoaderHeight, model->bitmap);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Image unavailable");
    }
}

static bool gta_mock_loader_input(InputEvent* event, void* context) {
    if(event->type != InputTypeShort ||
       (event->key != InputKeyLeft && event->key != InputKeyRight)) {
        return false;
    }

    View* view = context;
    const size_t step = event->key == InputKeyRight ? 1 : GtaMockLoaderCount - 1;
    with_view_model(
        view,
        GtaMockLoaderModel * model,
        {
            model->selected = (model->selected + step) % GtaMockLoaderCount;
            gta_mock_loader_read(model);
        },
        true);
    return true;
}

static void gta_mock_loader_enter(void* context) {
    View* view = context;
    with_view_model(view, GtaMockLoaderModel * model, { gta_mock_loader_read(model); }, true);
}

static void gta_mock_loader_exit(void* context) {
    View* view = context;
    with_view_model(
        view,
        GtaMockLoaderModel * model,
        {
            free(model->bitmap);
            model->bitmap = NULL;
            model->available = false;
        },
        false);
}

View* gta_mock_loaders_alloc(void) {
    View* view = view_alloc();
    view_allocate_model(view, ViewModelTypeLocking, sizeof(GtaMockLoaderModel));
    with_view_model(
        view, GtaMockLoaderModel * model, { *model = (GtaMockLoaderModel){0}; }, false);
    view_set_context(view, view);
    view_set_draw_callback(view, gta_mock_loader_draw);
    view_set_input_callback(view, gta_mock_loader_input);
    view_set_enter_callback(view, gta_mock_loader_enter);
    view_set_exit_callback(view, gta_mock_loader_exit);
    return view;
}
