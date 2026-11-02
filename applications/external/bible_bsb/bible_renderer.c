#include "bible_renderer.h"

#include <string.h>

/* ============================================================================
 * Word wrapping — split text on whitespace into lines fitting max_chars.
 * ============================================================================ */

void bible_word_wrap(
    const char* text,
    uint16_t verse_number,
    uint8_t max_chars,
    BibleLine* out_lines,
    uint16_t* out_count,
    uint16_t max_lines) {
    furi_check(text);
    furi_check(out_lines);
    furi_check(out_count);
    if(max_chars < 1) max_chars = 1;
    if(max_chars > 60) max_chars = 60;

    char prefix[8];
    snprintf(prefix, sizeof(prefix), "%u ", (unsigned int)verse_number);
    uint8_t prefix_len = (uint8_t)strlen(prefix);
    if(prefix_len >= sizeof(prefix)) prefix_len = sizeof(prefix) - 1;

    bool first = true;
    char current[60];
    uint8_t current_len = 0;
    uint8_t budget = (max_chars > prefix_len) ? (uint8_t)(max_chars - prefix_len) : 1;

    const char* p = text;
    while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;

    while(*p != '\0') {
        const char* word = p;
        uint8_t word_len = 0;
        while(*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            if(word_len < 60) word_len++;
            p++;
        }
        uint8_t taken = 0;
        while(taken < word_len) {
            uint8_t piece = (uint8_t)(word_len - taken);
            if(piece > budget) piece = budget;
            if(piece == 0) piece = 1;
            if(current_len > 0 && (uint8_t)(current_len + 1 + piece) > budget) {
                if(*out_count < max_lines) {
                    BibleLine* line = &out_lines[*out_count];
                    memset(line, 0, sizeof(*line));
                    uint8_t room = (uint8_t)(sizeof(line->text) - 1);
                    uint8_t n = current_len;
                    if(n > room) n = room;
                    if(first) {
                        uint8_t pl = prefix_len < room ? prefix_len : room;
                        memcpy(line->text, prefix, pl);
                        uint8_t copy = n;
                        if(pl + copy > room) copy = (uint8_t)(room - pl);
                        memcpy(line->text + pl, current, copy);
                        line->text[pl + copy] = '\0';
                        first = false;
                    } else {
                        memcpy(line->text, current, n);
                        line->text[n] = '\0';
                    }
                    line->verse_number = verse_number;
                    line->is_verse_number = false;
                    if(line->text[0] && prefix_len &&
                       memcmp(line->text, prefix, prefix_len) == 0) {
                        line->is_verse_number = true;
                    }
                    (*out_count)++;
                }
                current_len = 0;
                budget = max_chars;
                continue;
            }
            if(current_len > 0 && (size_t)current_len + 1 < sizeof(current)) {
                current[current_len++] = ' ';
            }
            if(piece > sizeof(current) - 1 - current_len) {
                piece = (uint8_t)(sizeof(current) - 1 - current_len);
            }
            if(piece == 0) break;
            memcpy(current + current_len, word + taken, piece);
            current_len = (uint8_t)(current_len + piece);
            taken = (uint8_t)(taken + piece);
        }
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            p++;
    }

    if(current_len > 0 && *out_count < max_lines) {
        BibleLine* line = &out_lines[*out_count];
        memset(line, 0, sizeof(*line));
        uint8_t room = (uint8_t)(sizeof(line->text) - 1);
        uint8_t n = current_len;
        if(n > room) n = room;
        if(first) {
            uint8_t pl = prefix_len < room ? prefix_len : room;
            memcpy(line->text, prefix, pl);
            uint8_t copy = n;
            if(pl + copy > room) copy = (uint8_t)(room - pl);
            memcpy(line->text + pl, current, copy);
            line->text[pl + copy] = '\0';
            line->is_verse_number = true;
        } else {
            memcpy(line->text, current, n);
            line->text[n] = '\0';
            line->is_verse_number = false;
        }
        line->verse_number = verse_number;
        (*out_count)++;
    }
}

/* ============================================================================
 * Scroll indicator — 2px-wide thumb at x=126
 * ============================================================================ */

void bible_draw_scroll_indicator(
    Canvas* canvas,
    uint16_t scroll,
    uint16_t total,
    uint16_t visible,
    uint8_t header_height) {
    if(total <= visible) {
        return;
    }

    /* Thumb height is proportional to visible/total. */
    int32_t avail = BIBLE_SCREEN_HEIGHT - header_height;
    int32_t thumb_height = (visible * BIBLE_SCREEN_HEIGHT / total);
    if(thumb_height < 4) {
        thumb_height = 4;
    }

    /* Position proportional to scroll offset */
    int32_t thumb_y = header_height;
    if(total > visible) {
        thumb_y += (int32_t)scroll * (avail - thumb_height) / (int32_t)(total - visible);
    }

    canvas_draw_box(canvas, 126, thumb_y, 2, thumb_height);
}

/* ============================================================================
 * Toast — centered black box with white text
 * ============================================================================ */

void bible_draw_toast(Canvas* canvas, const BibleToast* toast) {
    if(!toast || toast->timer == 0 || toast->message[0] == '\0') {
        return;
    }

    canvas_set_font(canvas, FontSecondary);

    /* Width based on message length: len * 5 + 4, capped at 20 characters. */
    size_t msg_len = strlen(toast->message);
    if(msg_len > 20) {
        msg_len = 20;
    }
    uint8_t toast_w = (uint8_t)(msg_len * 5) + 4;
    uint8_t toast_x = (BIBLE_SCREEN_WIDTH - toast_w) / 2;

    /* Black background box */
    canvas_draw_box(canvas, toast_x, 24, toast_w, 14);

    /* White text */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, toast_x + 2, 34, toast->message);

    /* Restore black drawing color */
    canvas_set_color(canvas, ColorBlack);
}

/* ============================================================================
 * Page rendering and pagination
 * ============================================================================ */

uint16_t bible_max_visible_lines(int16_t y_offset) {
    int16_t avail = BIBLE_SCREEN_HEIGHT - y_offset - BIBLE_MARGIN_Y;
    if(avail < BIBLE_CHAR_HEIGHT) return 1;
    return (uint16_t)(1 + (avail - BIBLE_CHAR_HEIGHT) / BIBLE_LINE_HEIGHT);
}

void bible_render_page(
    Canvas* canvas,
    const BibleLine* lines,
    uint16_t line_count,
    uint16_t scroll_offset,
    int16_t y_offset) {
    if(!lines || line_count == 0) return;

    uint16_t max_visible = bible_max_visible_lines(y_offset);
    canvas_set_font(canvas, FontPrimary);

    for(uint16_t i = 0; i < max_visible; i++) {
        uint16_t idx = scroll_offset + i;
        if(idx >= line_count) break;

        int16_t y = y_offset + ((int16_t)i * BIBLE_LINE_HEIGHT);
        if(y + BIBLE_CHAR_HEIGHT > BIBLE_SCREEN_HEIGHT - BIBLE_MARGIN_Y) break;

        canvas_draw_str(canvas, BIBLE_MARGIN_X, y + BIBLE_CHAR_HEIGHT - 1, lines[idx].text);
    }
}

uint16_t bible_total_pages(const BibleLine* lines, uint16_t line_count, int16_t y_offset) {
    UNUSED(lines);
    uint16_t max_visible = bible_max_visible_lines(y_offset);
    if(line_count == 0 || max_visible == 0) return 1;
    return (line_count + max_visible - 1) / max_visible;
}

/* ============================================================================
 * Inverted highlight — black box + white text
 * ============================================================================ */

void bible_draw_missing_data(Canvas* canvas) {
    if(!canvas) return;
    canvas_reset(canvas);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 20, "BSB text missing");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 36, "Reinstall Bible [BSB]");
    canvas_draw_str(canvas, 2, 46, "from the app catalog.");
}

void bible_draw_inverted_highlight(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    const char* text,
    uint8_t text_x,
    uint8_t text_y) {
    /* Black filled box */
    canvas_draw_box(canvas, x, y, w, h);

    /* White text */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, text_x, text_y, text);

    /* Restore black drawing color */
    canvas_set_color(canvas, ColorBlack);
}
