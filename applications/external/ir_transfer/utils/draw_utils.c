#include "draw_utils.h"
#include <string.h>
#include <stdio.h>

void draw_truncated_text(Canvas* canvas, int x, int y, const char* text, int max_width) {
    int text_width = canvas_string_width(canvas, text);

    if(text_width <= max_width) {
        canvas_draw_str(canvas, x, y, text);
        return;
    }

    const char* ellipsis = "...";
    size_t ellipsis_width = canvas_string_width(canvas, ellipsis);
    size_t available_width = max_width - ellipsis_width;
    size_t text_len = strlen(text);
    size_t start_len = 0;
    size_t end_len = 0;
    int start_width = 0;
    int end_width = 0;

    while(start_len < text_len && start_width + canvas_glyph_width(canvas, text[start_len]) <=
                                      (size_t)(available_width / 2) + 1) {
        start_width += canvas_glyph_width(canvas, text[start_len]);
        start_len++;
    }

    while(end_len < text_len - start_len &&
          end_width + canvas_glyph_width(canvas, text[text_len - 1 - end_len]) <=
              (size_t)(available_width / 2) + 1) {
        end_width += canvas_glyph_width(canvas, text[text_len - 1 - end_len]);
        end_len++;
    }

    char truncated_text[128];
    snprintf(
        truncated_text,
        sizeof(truncated_text),
        "%.*s%s%.*s",
        (int)start_len,
        text,
        ellipsis,
        (int)end_len,
        text + (text_len - end_len));
    canvas_draw_str(canvas, x, y, truncated_text);
}
