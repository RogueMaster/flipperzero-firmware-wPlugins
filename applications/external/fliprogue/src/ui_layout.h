#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    uint8_t first;
    uint8_t page;
    uint8_t pages;
} FrUiPage;

static inline uint8_t fr_ui_pages(uint8_t count, uint8_t rows) {
    if(rows == 0 || count == 0) return 1;
    return (uint8_t)((count + rows - 1u) / rows);
}

static inline uint8_t fr_ui_max_scroll(uint8_t count, uint8_t rows) {
    if(rows == 0 || count <= rows) return 0;
    return (uint8_t)(count - rows);
}

static inline uint8_t fr_ui_clamp_scroll(uint8_t scroll, uint8_t count, uint8_t rows) {
    uint8_t max_scroll = fr_ui_max_scroll(count, rows);
    return scroll > max_scroll ? max_scroll : scroll;
}

static inline FrUiPage fr_ui_page(uint8_t scroll, uint8_t count, uint8_t rows) {
    FrUiPage page;
    page.first = fr_ui_clamp_scroll(scroll, count, rows);
    page.pages = fr_ui_pages(count, rows);
    if(rows == 0 || page.first == 0) {
        page.page = 1;
    } else {
        page.page = (uint8_t)(((page.first + rows - 1u) / rows) + 1u);
        if(page.page > page.pages) page.page = page.pages;
    }
    return page;
}

static inline uint8_t fr_ui_next_page_scroll(uint8_t scroll, uint8_t count, uint8_t rows) {
    uint8_t max_scroll = fr_ui_max_scroll(count, rows);
    uint8_t next = (uint8_t)(scroll + rows);
    return next > max_scroll ? max_scroll : next;
}

static inline uint8_t fr_ui_prev_page_scroll(uint8_t scroll, uint8_t rows) {
    if(scroll <= rows) return 0;
    return (uint8_t)(scroll - rows);
}

static inline uint8_t fr_ui_death_log_lines(
    const char* text,
    char lines[][32],
    uint8_t max_lines,
    uint8_t max_chars) {
    if(max_lines == 0 || max_chars == 0) return 0;
    for(uint8_t i = 0; i < max_lines; i++) lines[i][0] = '\0';
    if(!text || text[0] == '\0') return 0;

    uint8_t line = 0;
    uint8_t used = 0;
    const char* cursor = text;
    while(*cursor != '\0' && line < max_lines) {
        while(*cursor == ' ') cursor++;
        if(*cursor == '\0') break;

        const char* end = cursor;
        while(*end != '\0' && *end != '.' && *end != '!' && *end != '?') end++;
        if(*end != '\0') end++;
        size_t phrase_len = (size_t)(end - cursor);
        while(phrase_len > 0 && cursor[phrase_len - 1] == ' ') phrase_len--;
        if(phrase_len == 0) {
            cursor = end;
            continue;
        }

        uint8_t extra_space = used > 0 ? 1 : 0;
        if(used > 0 && used + extra_space + phrase_len > max_chars && line + 1 < max_lines) {
            line++;
            used = 0;
            extra_space = 0;
        }
        if(line >= max_lines) break;

        if(used > 0 && used < 31) {
            lines[line][used++] = ' ';
            lines[line][used] = '\0';
        }
        size_t room = used < 31 ? (size_t)(31 - used) : 0;
        size_t copy_len = phrase_len < room ? phrase_len : room;
        if(copy_len > 0) {
            memcpy(&lines[line][used], cursor, copy_len);
            used = (uint8_t)(used + copy_len);
            lines[line][used] = '\0';
        }
        cursor = end;
    }

    uint8_t count = 0;
    for(uint8_t i = 0; i < max_lines; i++) {
        if(lines[i][0] != '\0') count = (uint8_t)(i + 1);
    }
    return count;
}
