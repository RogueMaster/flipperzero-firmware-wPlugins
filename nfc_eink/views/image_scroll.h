#pragma once

#include <stdint.h>
#include <gui/view.h>

typedef struct ImageScroll ImageScroll;

ImageScroll* image_scroll_alloc(void);
void image_scroll_free(ImageScroll* instance);
View* image_scroll_get_view(ImageScroll* instance);

void image_scroll_reset(ImageScroll* instance);
void image_scroll_set_image(
    ImageScroll* instance,
    uint16_t width,
    uint16_t height,
    const uint8_t* image,
    bool invert_image);
