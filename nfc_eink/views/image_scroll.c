#include "image_scroll.h"

#define IMAGE_WINDOW_WIDTH    (128)
#define IMAGE_WINDOW_HEIGHT   (64)
#define IMAGE_PIXELS_PER_BYTE (8)

#define IMAGE_WINDOW_BYTES_PER_ROW (IMAGE_WINDOW_WIDTH / IMAGE_PIXELS_PER_BYTE)
#define IMAGE_WINDOW_SIZE_BYTES    (IMAGE_WINDOW_BYTES_PER_ROW * IMAGE_WINDOW_HEIGHT)

struct ImageScroll {
    void* context;
    View* view;
};

typedef struct {
    bool image_set;
    bool invert_image;

    int16_t pos_x;
    int16_t pos_y;

    uint16_t width;
    uint16_t height;
    uint8_t row_size_bytes;
    const uint8_t* image;
    uint8_t view_image[IMAGE_WINDOW_SIZE_BYTES];
} ImageScrollViewModel;

static void scroll_draw_callback(Canvas* canvas, void* model) {
    ImageScrollViewModel* m = model;
    canvas_draw_xbm_ex(
        canvas, 0, 0, IMAGE_WINDOW_WIDTH, IMAGE_WINDOW_HEIGHT, IconRotation180, m->view_image);
}

static inline int16_t image_scroll_calculate_new_pos(int16_t pos, int8_t step, int16_t max_pos) {
    int16_t new_pos = pos + step;

    if(new_pos > max_pos) new_pos = max_pos;
    if(new_pos < 0) new_pos = 0;

    return new_pos;
}

static void
    image_scroll_reverse_copy_row(const uint8_t* array, uint8_t* reverse_array, bool invert_image) {
    for(int i = 0; i < IMAGE_WINDOW_BYTES_PER_ROW; i++) {
        reverse_array[i] = invert_image ? ~array[IMAGE_WINDOW_BYTES_PER_ROW - 1 - i] :
                                          array[IMAGE_WINDOW_BYTES_PER_ROW - 1 - i];
    }
}

static void image_scroll_copy_to_window(ImageScrollViewModel* model) {
    uint16_t image_offset = model->pos_y * model->row_size_bytes + model->pos_x;
    for(uint16_t i = 0, j = 0; j < IMAGE_WINDOW_SIZE_BYTES;
        i += model->row_size_bytes, j += IMAGE_WINDOW_BYTES_PER_ROW)
        image_scroll_reverse_copy_row(
            model->image + image_offset + i, model->view_image + j, model->invert_image);
}

static void image_scroll_move(ImageScroll* instance, int8_t step_x, int8_t step_y) {
    ImageScrollViewModel* model = view_get_model(instance->view);

    if(!model->image_set) return;
    UNUSED(step_x);

    model->pos_x = image_scroll_calculate_new_pos(
        model->pos_x, step_x, model->row_size_bytes - IMAGE_WINDOW_BYTES_PER_ROW);
    model->pos_y =
        image_scroll_calculate_new_pos(model->pos_y, step_y, model->width - IMAGE_WINDOW_HEIGHT);

    image_scroll_copy_to_window(model);
    view_commit_model(instance->view, true);
    view_set_orientation(instance->view, ViewOrientationHorizontalFlip);
}

static bool scroll_input_callback(InputEvent* event, void* context) {
    ImageScroll* scroll = context;

    int8_t x = 0, y = 0;
    bool consumed = false;
    if(event->key == InputKeyUp) {
        y = 1;
        consumed = true;
    } else if(event->key == InputKeyDown) {
        y = -1;
        consumed = true;
    } else if(event->key == InputKeyLeft) {
        x = 1;
        consumed = true;
    } else if(event->key == InputKeyRight) {
        x = -1;
        consumed = true;
    }

    if(event->type == InputTypeRepeat) {
        y *= 10;
        x *= 10;
    }

    image_scroll_move(scroll, x, y);
    return consumed;
}

ImageScroll* image_scroll_alloc(void) {
    ImageScroll* scroll = malloc(sizeof(ImageScroll));

    scroll->view = view_alloc();
    view_allocate_model(scroll->view, ViewModelTypeLockFree, sizeof(ImageScrollViewModel));
    view_set_draw_callback(scroll->view, scroll_draw_callback);
    view_set_input_callback(scroll->view, scroll_input_callback);
    view_set_context(scroll->view, scroll);
    return scroll;
}

void image_scroll_free(ImageScroll* instance) {
    furi_assert(instance);
    view_free(instance->view);
    free(instance);
}

View* image_scroll_get_view(ImageScroll* instance) {
    furi_assert(instance);
    return instance->view;
}

void image_scroll_reset(ImageScroll* instance) {
    furi_assert(instance);
    ImageScrollViewModel* model = view_get_model(instance->view);

    model->image = NULL;
    memset(model->view_image, 0, sizeof(model->view_image));
    model->pos_x = 0;
    model->pos_y = 0;
    model->width = 0;
    model->height = 0;
    model->row_size_bytes = 0;

    model->image_set = false;
}

void image_scroll_set_image(
    ImageScroll* instance,
    uint16_t width,
    uint16_t height,
    const uint8_t* image,
    bool invert_image) {
    furi_assert(instance);
    furi_assert(image);

    ImageScrollViewModel* model = view_get_model(instance->view);
    if(!model->image_set) {
        model->height = height;
        model->width = width;
        model->invert_image = invert_image;
        model->image = image;
        model->pos_x = 0;
        model->pos_y = 0;

        model->row_size_bytes = model->height / IMAGE_PIXELS_PER_BYTE;
        if(model->height % IMAGE_PIXELS_PER_BYTE != 0) model->row_size_bytes++;

        model->image_set = true;

        image_scroll_move(instance, 0, 0);
    }
}
