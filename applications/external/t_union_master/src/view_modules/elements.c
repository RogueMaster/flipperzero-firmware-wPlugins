#include "elements.h"
#include <gui/canvas.h>
#include <t_union_master_icons.h>

struct Icon {
    const uint16_t width;
    const uint16_t height;
    const uint8_t frame_count;
    const uint8_t frame_rate;
    const uint8_t* const* frames;

    Icon* original;
};

void elements_draw_str_utf8(Canvas* canvas, uint8_t x, uint8_t y, const char* str) {
    Unicode* uni = uni_alloc_decode_utf8(str);
    if(uni == NULL) return;
    uni_decode_utf8(str, uni);
    cfont_draw_utf16_str(canvas, x, y, uni);
    uni_free(uni);
}

void elements_draw_str_aligned_utf16(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const Unicode* uni) {
    furi_assert(canvas);
    CFontInfo_t font_info = cfont_get_info(cfont->font_size);

    switch(horizontal) {
    case AlignLeft:
        break;
    case AlignRight:
        x -= cfont_get_utf16_str_width(uni);
        break;
    case AlignCenter:
        x -= (cfont_get_utf16_str_width(uni) / 2);
        break;
    default:
        furi_crash();
        break;
    }

    switch(vertical) {
    case AlignTop:
        y += font_info.px;
        break;
    case AlignBottom:
        break;
    case AlignCenter:
        y += (font_info.px / 2);
        break;
    default:
        furi_crash();
        break;
    }
    cfont_draw_utf16_str(canvas, x, y, uni);
}

void elements_draw_str_aligned_utf8(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* str) {
    Unicode* uni = uni_alloc_decode_utf8(str);
    if(uni == NULL) return;
    elements_draw_str_aligned_utf16(canvas, x, y, horizontal, vertical, uni);
    uni_free(uni);
}

static size_t elements_get_max_chars_to_fit_utf16(
    Canvas* canvas,
    Align horizontal,
    const Unicode* text,
    uint8_t x) {
    const Unicode* end = uni_strchr(text, '\n');
    if(end == NULL) {
        end = text + uni_strlen(text);
    }
    size_t text_size = end - text;
    Unicode* str = uni_alloc(uni_strlen(text));
    uni_strcpy(str, text);
    uni_left(str, text_size);
    size_t result = 0;

    uint16_t len_px = cfont_get_utf16_str_width(str);
    uint8_t px_left = 0;
    if(horizontal == AlignCenter) {
        if(x > (canvas_width(canvas) / 2)) {
            px_left = (canvas_width(canvas) - x) * 2;
        } else {
            px_left = x * 2;
        }
    } else if(horizontal == AlignLeft) {
        px_left = canvas_width(canvas) - x;
    } else if(horizontal == AlignRight) {
        px_left = x;
    } else {
        furi_crash();
    }

    if(len_px > px_left) {
        size_t excess_symbols_approximately =
            ceilf((float)(len_px - px_left) / ((float)len_px / (float)text_size));
        // reduce to 5 to be sure dash fit, and next line will be at least 5 symbols long
        if(excess_symbols_approximately > 0) {
            if(text_size >= excess_symbols_approximately) {
                result = text_size - excess_symbols_approximately;
            } else {
                result = 1;
            }
        } else {
            result = text_size;
        }
    } else {
        result = text_size;
    }
    uni_free(str);

    return result;
}

void elements_multiline_text_aligned_utf16(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const Unicode* uni) {
    furi_assert(canvas);
    furi_assert(uni);

    CFontInfo_t font_info = cfont_get_info(cfont->font_size);

    uint8_t lines_count = 0;
    uint8_t font_height = font_info.px;

    /* go through text line by line and count lines */
    for(const Unicode* start = uni; start[0];) {
        size_t chars_fit = elements_get_max_chars_to_fit_utf16(canvas, horizontal, start, x);
        ++lines_count;
        start += chars_fit;
        start += start[0] == '\n' ? 1 : 0;
        if(lines_count > 6) break;
    }

    if(vertical == AlignBottom) {
        y -= font_height * (lines_count - 1);
    } else if(vertical == AlignCenter) {
        y -= (font_height * (lines_count - 1)) / 2;
    }

    Unicode* line;
    /* go through text line by line and print them */
    for(const Unicode* start = uni; start[0];) {
        size_t chars_fit = elements_get_max_chars_to_fit_utf16(canvas, horizontal, start, x);
        line = uni_alloc(chars_fit);

        if((start[chars_fit] == '\n') || (start[chars_fit] == '\0')) {
            uni_strncpy(line, start, chars_fit);
        } else {
            uni_strncpy(line, start, chars_fit);
            uni_cat_printf(line, "\n");
        }
        elements_draw_str_aligned_utf16(canvas, x, y, horizontal, vertical, line);
        uni_free(line);
        y += font_height + 1;
        if(y > canvas_height(canvas)) {
            break;
        }

        start += chars_fit;
        start += start[0] == '\n' ? 1 : 0;
    }
}

void elements_multiline_text_aligned_utf8(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* text) {
    Unicode* uni = uni_alloc_decode_utf8(text);
    if(uni == NULL) return;
    elements_multiline_text_aligned_utf16(canvas, x, y, horizontal, vertical, uni);
    uni_free(uni);
}

void elements_button_left_utf8(Canvas* canvas, const char* str) {
    Unicode* uni = uni_alloc_decode_utf8(str);
    if(uni == NULL) return;

    const uint8_t button_height = 14;
    const uint8_t vertical_offset = 1;
    const uint8_t horizontal_offset = 3;
    const uint8_t string_width = cfont_get_utf16_str_width(uni);
    const Icon* icon = &I_ButtonLeft_4x7;
    const uint8_t icon_h_offset = 3;
    const uint8_t icon_width_with_offset = icon->width + icon_h_offset;
    const uint8_t icon_v_offset = icon->height + vertical_offset + 2;
    const uint8_t button_width = string_width + horizontal_offset * 2 + icon_width_with_offset;

    const uint8_t x = 0;
    const uint8_t y = canvas_height(canvas);

    canvas_draw_box(canvas, x, y - button_height, button_width, button_height);
    canvas_draw_line(canvas, x + button_width + 0, y, x + button_width + 0, y - button_height + 0);
    canvas_draw_line(canvas, x + button_width + 1, y, x + button_width + 1, y - button_height + 1);
    canvas_draw_line(canvas, x + button_width + 2, y, x + button_width + 2, y - button_height + 2);

    canvas_invert_color(canvas);
    canvas_draw_icon(canvas, x + horizontal_offset, y - icon_v_offset, icon);
    cfont_draw_utf16_str(
        canvas, x + horizontal_offset + icon_width_with_offset, y - vertical_offset, uni);
    canvas_invert_color(canvas);
    uni_free(uni);
}

void elements_button_right_utf8(Canvas* canvas, const char* str) {
    Unicode* uni = uni_alloc_decode_utf8(str);
    if(uni == NULL) return;

    const uint8_t button_height = 14;
    const uint8_t vertical_offset = 1;
    const uint8_t horizontal_offset = 3;
    const uint8_t string_width = cfont_get_utf16_str_width(uni);
    const Icon* icon = &I_ButtonRight_4x7;
    const uint8_t icon_h_offset = 3;
    const uint8_t icon_width_with_offset = icon->width + icon_h_offset;
    const uint8_t icon_v_offset = icon->height + vertical_offset + 2;
    const uint8_t button_width = string_width + horizontal_offset * 2 + icon_width_with_offset;

    const uint8_t x = canvas_width(canvas);
    const uint8_t y = canvas_height(canvas);

    canvas_draw_box(canvas, x - button_width, y - button_height, button_width, button_height);
    canvas_draw_line(canvas, x - button_width - 1, y, x - button_width - 1, y - button_height + 0);
    canvas_draw_line(canvas, x - button_width - 2, y, x - button_width - 2, y - button_height + 1);
    canvas_draw_line(canvas, x - button_width - 3, y, x - button_width - 3, y - button_height + 2);

    canvas_invert_color(canvas);
    cfont_draw_utf16_str(canvas, x - button_width + horizontal_offset, y - vertical_offset, uni);
    canvas_draw_icon(canvas, x - horizontal_offset - icon->width, y - icon_v_offset, icon);
    canvas_invert_color(canvas);
    uni_free(uni);
}

void elements_button_center_utf8(Canvas* canvas, const char* str) {
    Unicode* uni = uni_alloc_decode_utf8(str);
    if(uni == NULL) return;

    const uint8_t button_height = 14;
    const uint8_t vertical_offset = 1;
    const uint8_t horizontal_offset = 1;
    const uint8_t string_width = cfont_get_utf16_str_width(uni);
    const Icon* icon = &I_ButtonCenter_7x7;
    const uint8_t icon_h_offset = 3;
    const uint8_t icon_width_with_offset = icon->width + icon_h_offset;
    const uint8_t icon_v_offset = icon->height + vertical_offset + 2;
    const uint8_t button_width = string_width + horizontal_offset * 2 + icon_width_with_offset;

    const uint8_t x = (canvas_width(canvas) - button_width) / 2;
    const uint8_t y = canvas_height(canvas);

    canvas_draw_box(canvas, x, y - button_height, button_width, button_height);

    canvas_draw_line(canvas, x - 1, y, x - 1, y - button_height + 0);
    canvas_draw_line(canvas, x - 2, y, x - 2, y - button_height + 1);
    canvas_draw_line(canvas, x - 3, y, x - 3, y - button_height + 2);

    canvas_draw_line(canvas, x + button_width + 0, y, x + button_width + 0, y - button_height + 0);
    canvas_draw_line(canvas, x + button_width + 1, y, x + button_width + 1, y - button_height + 1);
    canvas_draw_line(canvas, x + button_width + 2, y, x + button_width + 2, y - button_height + 2);

    canvas_invert_color(canvas);
    canvas_draw_icon(canvas, x + horizontal_offset, y - icon_v_offset, icon);
    cfont_draw_utf16_str(
        canvas, x + horizontal_offset + icon_width_with_offset, y - vertical_offset, uni);
    canvas_invert_color(canvas);
    uni_free(uni);
}
