/*
 * Purpose: Render tiny markdown-ish help text on the Flipper canvas.
 * Owns: inline parsing, wrapping, justification, icons, chrome, and scrolling.
 * Depends on: cw_markdown_widget.h and Canvas text metrics.
 * Tests: tests/test_cw_markdown_widget.c.
 */

#include "cw_markdown_widget.h"

#include <stddef.h>
#include <string.h>

#define CWMD_SCREEN_W            128U
#define CWMD_SCREEN_H            64U
#define CWMD_CHROME_H            13U
#define CWMD_SCROLL_W            1U
#define CWMD_SCROLL_GAP          2U
#define CWMD_MAX_ITEMS           24U
#define CWMD_ITEM_TEXT           24U
#define CWMD_SOFT_BREAKS_MAX     8U
#define CWMD_BULLET_INDENT       8U
#define CWMD_BULLET_SIZE         4U
#define CWMD_JUSTIFY_MIN_NUM     4U
#define CWMD_JUSTIFY_MIN_DEN     5U
#define CWMD_JUSTIFY_MIN_GAPS    2U
#define CWMD_WORD_GAP_MIN        3U
#define CWMD_JUSTIFY_MAX_GAP     12U
#define CWMD_MICROFIT_GAP_MIN    2U
#define CWMD_MICROFIT_MAX_SHRINK 2U
#define CWMD_SCROLL_PX_PER_TICK  1U

typedef enum {
    CwmdItemText = 0,
    CwmdItemGap,
    CwmdItemIcon,
} CwmdItemType;

typedef struct {
    bool bold;
    bool mono;
} CwmdStyle;

typedef struct {
    CwmdItemType type;
    CwmdStyle style;
    const CwmdIcon* icon;
    uint16_t width;
    uint8_t soft_break_count;
    uint8_t soft_breaks[CWMD_SOFT_BREAKS_MAX];
    char text[CWMD_ITEM_TEXT];
} CwmdItem;

typedef struct {
    CwmdItem items[CWMD_MAX_ITEMS];
    uint8_t item_count;
    uint8_t gaps;
    uint16_t width;
    bool bullet;
    bool bullet_mark;
    bool ended_logical;
    bool hard_empty;
    CwmdAlign align;
    CwmdStyle line_style;
    uint8_t microfit_shrink;
} CwmdLine;

typedef struct {
    const char* p;
    bool new_logical;
    bool bullet;
    bool first_bullet_line;
    CwmdAlign align;
    CwmdStyle line_style;
    CwmdStyle inline_style;
    bool has_pending_item;
    CwmdItem pending_item;
#ifdef CWMD_HOST_TEST
    CwmdTestStats* stats;
#endif
} CwmdParser;

/*
 * The parser emits small atoms first; wrapping and drawing happen later.
 * No heap, no browser, no grand civilised Markdown parser. Just enough markup
 * for help cards on a 128x64 screen.
 */
static bool cwmd_ascii_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

static const CwmdIcon* cwmd_find_icon(const CwmdConfig* cfg, uint8_t id) {
    uint8_t i;

    if(cfg == NULL || cfg->icons == NULL || id == 0U) return NULL;
    for(i = 0U; i < cfg->icon_count; i++) {
        if(cfg->icons[i].id == id) return &cfg->icons[i];
    }
    return NULL;
}

static Font cwmd_font(CwmdStyle style) {
    if(style.mono) return FontKeyboard;
    if(style.bold) return FontPrimary;
    return FontSecondary;
}

static uint16_t cwmd_text_width(Canvas* canvas, CwmdStyle style, const char* text) {
    canvas_set_font(canvas, cwmd_font(style));
    return canvas_string_width(canvas, text);
}

static uint16_t cwmd_space_width(Canvas* canvas, CwmdStyle style) {
    uint16_t w = cwmd_text_width(canvas, style, " ");
    return w < CWMD_WORD_GAP_MIN ? CWMD_WORD_GAP_MIN : w;
}

static uint16_t cwmd_effective_width(const CwmdConfig* cfg) {
    uint16_t width;

    if(cfg == NULL) return 0U;
    width = cfg->width;
    if(cfg->scrollbar) {
        uint8_t reserve = CWMD_SCROLL_W + CWMD_SCROLL_GAP;
        width = width > reserve ? (uint16_t)(width - reserve) : 0U;
    }
    return width;
}

void cwmd_config_default(CwmdConfig* cfg, bool bottom_chrome) {
    if(cfg == NULL) return;

    cfg->x = 1U;
    cfg->y = 1U;
    cfg->width = 126U;
    cfg->height = bottom_chrome ? 50U : 62U;
    cfg->line_height = 9U;
    cfg->scrollbar = true;
    cfg->chrome = bottom_chrome ? (CwmdChromeLeft | CwmdChromeCenter | CwmdChromeRight) :
                                  CwmdChromeNone;
    cfg->left_label = "Back";
    cfg->center_label = NULL;
    cfg->right_label = "Next";
    cfg->icons = NULL;
    cfg->icon_count = 0U;
}

static bool cwmd_marker_has_close(const char* p, const char* marker, uint8_t len) {
    while(*p != '\0' && *p != '\n') {
        if(strncmp(p, marker, len) == 0) return true;
        p++;
    }
    return false;
}

static void cwmd_parser_reset(CwmdParser* parser) {
    parser->new_logical = true;
    parser->bullet = false;
    parser->first_bullet_line = false;
    parser->align = CwmdAlignJustify;
    parser->line_style.bold = false;
    parser->line_style.mono = false;
    parser->inline_style.bold = false;
    parser->inline_style.mono = false;
}

static bool cwmd_parse_line_escape(CwmdParser* parser, const CwmdConfig* cfg, CwmdItem* item) {
    const char* p = parser->p;
    uint8_t id;
    uint8_t digits = 0U;
    const CwmdIcon* icon;

    if((uint8_t)*p != 0x1BU) return false;
    p++;
    switch(*p) {
    case 'l':
        parser->align = CwmdAlignLeft;
        parser->p = p + 1;
        return true;
    case 'c':
        parser->align = CwmdAlignCenter;
        parser->p = p + 1;
        return true;
    case 'r':
        parser->align = CwmdAlignRight;
        parser->p = p + 1;
        return true;
    case '#':
        parser->line_style.bold = true;
        parser->p = p + 1;
        return true;
    case '*':
        parser->line_style.mono = true;
        parser->p = p + 1;
        return true;
    case 'x':
        p++;
        id = 0U;
        while(digits < 2U && cwmd_ascii_digit(*p)) {
            id = (uint8_t)((id * 10U) + (uint8_t)(*p - '0'));
            p++;
            digits++;
        }
        parser->p = p;
        if(digits == 0U) return true;
        icon = cwmd_find_icon(cfg, id);
        if(icon == NULL) {
#ifdef CWMD_HOST_TEST
            if(parser->stats) parser->stats->unknown_icons++;
#endif
            return true;
        }
        memset(item, 0, sizeof(*item));
        item->type = CwmdItemIcon;
        item->icon = icon;
        item->width = (uint16_t)icon->left_bearing + icon->width + icon->right_bearing;
        item->style = parser->line_style;
        return true;
    default:
        if(*p != '\0') p++;
        parser->p = p;
        return true;
    }
}

/* Line escapes only count at logical-line start; inline escapes may appear mid-word. */
static void cwmd_prepare_logical(CwmdParser* parser, const CwmdConfig* cfg) {
    (void)cfg;

    if(!parser->new_logical) return;
    parser->new_logical = false;
    parser->bullet = false;
    parser->first_bullet_line = false;
    parser->align = CwmdAlignJustify;
    parser->line_style.bold = false;
    parser->line_style.mono = false;
    parser->inline_style.bold = false;
    parser->inline_style.mono = false;

    while(*parser->p == ' ' || *parser->p == '\t' || (uint8_t)*parser->p == 0x1BU) {
        if(*parser->p == ' ' || *parser->p == '\t') {
            parser->p++;
            continue;
        }
        switch(parser->p[1]) {
        case 'l':
            parser->align = CwmdAlignLeft;
            parser->p += 2;
            continue;
        case 'c':
            parser->align = CwmdAlignCenter;
            parser->p += 2;
            continue;
        case 'r':
            parser->align = CwmdAlignRight;
            parser->p += 2;
            continue;
        case '#':
            parser->line_style.bold = true;
            parser->p += 2;
            continue;
        case '*':
            parser->line_style.mono = true;
            parser->p += 2;
            continue;
        default:
            break;
        }
        break;
    }

    if(parser->p[0] == '-' && parser->p[1] == ' ') {
        parser->bullet = true;
        parser->first_bullet_line = true;
        parser->align = CwmdAlignLeft;
        parser->p += 2;
    }
}

static bool
    cwmd_next_item(Canvas* canvas, const CwmdConfig* cfg, CwmdParser* parser, CwmdItem* item) {
    CwmdStyle style;
    char ch;
    uint8_t n;

    memset(item, 0, sizeof(*item));
    if(parser->has_pending_item) {
        *item = parser->pending_item;
        parser->has_pending_item = false;
        memset(&parser->pending_item, 0, sizeof(parser->pending_item));
        return true;
    }
    if(*parser->p == '\0' || *parser->p == '\n') return false;

    if(*parser->p == ' ' || *parser->p == '\t') {
        while(*parser->p == ' ' || *parser->p == '\t')
            parser->p++;
        item->type = CwmdItemGap;
        item->style = parser->line_style;
        item->width = cwmd_space_width(canvas, item->style);
        return true;
    }

    if((uint8_t)*parser->p == 0x1BU) {
        if(cwmd_parse_line_escape(parser, cfg, item) && item->type == CwmdItemIcon) return true;
        return cwmd_next_item(canvas, cfg, parser, item);
    }

    if(parser->p[0] == '\\' && (parser->p[1] == '_' || parser->p[1] == '`')) {
        parser->p++;
    } else if(parser->p[0] == '_' && parser->p[1] == '_') {
        if(parser->inline_style.bold || cwmd_marker_has_close(parser->p + 2, "__", 2U)) {
            parser->inline_style.bold = !parser->inline_style.bold;
            parser->p += 2;
            return cwmd_next_item(canvas, cfg, parser, item);
        }
    } else if(parser->p[0] == '`') {
        if(parser->inline_style.mono || cwmd_marker_has_close(parser->p + 1, "`", 1U)) {
            parser->inline_style.mono = !parser->inline_style.mono;
            parser->p++;
            return cwmd_next_item(canvas, cfg, parser, item);
        }
    }

    style = parser->line_style;
    if(parser->inline_style.bold) style.bold = true;
    if(parser->inline_style.mono) style.mono = true;

    item->type = CwmdItemText;
    item->style = style;
    n = 0U;
    while(*parser->p != '\0' && *parser->p != '\n' && *parser->p != ' ' && *parser->p != '\t' &&
          (uint8_t)*parser->p != 0x1BU && n + 1U < CWMD_ITEM_TEXT) {
        if(parser->p[0] == '\\' && (parser->p[1] == '_' || parser->p[1] == '`')) {
            parser->p++;
        } else if(
            parser->p[0] == '_' && parser->p[1] == '_' &&
            (parser->inline_style.bold || cwmd_marker_has_close(parser->p + 2, "__", 2U))) {
            break;
        } else if(
            parser->p[0] == '`' &&
            (parser->inline_style.mono || cwmd_marker_has_close(parser->p + 1, "`", 1U))) {
            break;
        }

        if(*parser->p == '~') {
            parser->p++;
            if(*parser->p == '~') {
                ch = '~';
                parser->p++;
            } else {
                if(n > 0U && item->soft_break_count < CWMD_SOFT_BREAKS_MAX) {
                    item->soft_breaks[item->soft_break_count++] = n;
                }
                continue;
            }
        } else {
            ch = *parser->p++;
        }
        if((uint8_t)ch < 32U || (uint8_t)ch > 127U) {
            ch = '?';
#ifdef CWMD_HOST_TEST
            if(parser->stats) parser->stats->sanitized++;
#endif
        }
        item->text[n++] = ch;
    }
    item->text[n] = '\0';
    while(item->soft_break_count > 0U && item->soft_breaks[item->soft_break_count - 1U] >= n) {
        item->soft_break_count--;
    }
    item->width = cwmd_text_width(canvas, style, item->text);
    return n != 0U;
}

static void cwmd_trim_trailing_gaps(CwmdLine* line) {
    while(line->item_count > 0U && line->items[line->item_count - 1U].type == CwmdItemGap) {
        line->item_count--;
    }

    line->width = 0U;
    line->gaps = 0U;
    for(uint8_t i = 0U; i < line->item_count; i++) {
        line->width += line->items[i].width;
        if(line->items[i].type == CwmdItemGap) line->gaps++;
    }
}

static bool cwmd_microfit_is_punctuation(char ch) {
    return ch == '.' || ch == '?' || ch == '!' || ch == ',' || ch == ';' || ch == ':';
}

static bool cwmd_microfit_gap_is_shrinkable(const CwmdLine* line, uint8_t index) {
    return index < line->item_count && line->items[index].type == CwmdItemGap &&
           line->items[index].width > CWMD_MICROFIT_GAP_MIN;
}

static bool cwmd_microfit_gap_after_punctuation(const CwmdLine* line, uint8_t index) {
    const CwmdItem* prev;
    size_t len;

    if(index == 0U || !cwmd_microfit_gap_is_shrinkable(line, index)) return false;
    prev = &line->items[index - 1U];
    if(prev->type != CwmdItemText) return false;
    len = strlen(prev->text);
    if(len == 0U) return false;
    return cwmd_microfit_is_punctuation(prev->text[len - 1U]);
}

static bool cwmd_microfit_plan_has(const uint8_t* plan, uint8_t count, uint8_t index) {
    uint8_t i;

    for(i = 0U; i < count; i++) {
        if(plan[i] == index) return true;
    }
    return false;
}

static bool cwmd_microfit_collect_plan(const CwmdLine* line, uint8_t needed, uint8_t* plan) {
    uint8_t count = 0U;
    int16_t i;

    for(i = (int16_t)line->item_count - 1; i >= 0 && count < needed; i--) {
        if(cwmd_microfit_gap_after_punctuation(line, (uint8_t)i)) {
            plan[count++] = (uint8_t)i;
        }
    }

    for(i = (int16_t)line->item_count - 1; i >= 0 && count < needed; i--) {
        if(cwmd_microfit_gap_is_shrinkable(line, (uint8_t)i) &&
           !cwmd_microfit_plan_has(plan, count, (uint8_t)i)) {
            plan[count++] = (uint8_t)i;
        }
    }

    return count == needed;
}

static bool
    cwmd_microfit_line_for_item(CwmdLine* line, const CwmdItem* item, uint16_t line_width) {
    uint16_t prospective;
    uint16_t overflow_full;
    uint8_t overflow;
    uint8_t remaining;
    uint8_t plan[CWMD_MICROFIT_MAX_SHRINK];
    uint8_t i;

    prospective = (uint16_t)(line->width + item->width);
    if(prospective <= line_width) return true;
    if(item->type == CwmdItemGap) return false;
    if(line->align != CwmdAlignJustify || line->bullet || line->line_style.mono) return false;
    if(line->microfit_shrink >= CWMD_MICROFIT_MAX_SHRINK) return false;

    overflow_full = (uint16_t)(prospective - line_width);
    remaining = (uint8_t)(CWMD_MICROFIT_MAX_SHRINK - line->microfit_shrink);
    if(overflow_full > remaining) return false;
    overflow = (uint8_t)overflow_full;
    if(!cwmd_microfit_collect_plan(line, overflow, plan)) return false;

    for(i = 0U; i < overflow; i++) {
        line->items[plan[i]].width--;
        line->width--;
    }
    line->microfit_shrink = (uint8_t)(line->microfit_shrink + overflow);
    return true;
}

static bool cwmd_soft_split_item(
    Canvas* canvas,
    const CwmdLine* line,
    const CwmdItem* item,
    uint16_t line_width,
    CwmdItem* prefix,
    CwmdItem* suffix) {
    size_t len;
    uint16_t available;
    uint16_t prefix_width;
    uint8_t split_pos;
    int16_t i;
    uint8_t j;

    if(item->type != CwmdItemText || item->soft_break_count == 0U) return false;
    if(line->width >= line_width) return false;
    available = (uint16_t)(line_width - line->width);
    len = strlen(item->text);

    for(i = (int16_t)item->soft_break_count - 1; i >= 0; i--) {
        split_pos = item->soft_breaks[i];
        if(split_pos == 0U || split_pos >= len || split_pos + 2U > CWMD_ITEM_TEXT) continue;

        memset(prefix, 0, sizeof(*prefix));
        prefix->type = CwmdItemText;
        prefix->style = item->style;
        memcpy(prefix->text, item->text, split_pos);
        prefix->text[split_pos] = '-';
        prefix->text[split_pos + 1U] = '\0';
        prefix_width = cwmd_text_width(canvas, item->style, prefix->text);
        if(prefix_width > available) continue;
        prefix->width = prefix_width;

        memset(suffix, 0, sizeof(*suffix));
        suffix->type = CwmdItemText;
        suffix->style = item->style;
        strncpy(suffix->text, item->text + split_pos, CWMD_ITEM_TEXT - 1U);
        suffix->text[CWMD_ITEM_TEXT - 1U] = '\0';
        suffix->width = cwmd_text_width(canvas, item->style, suffix->text);
        for(j = 0U; j < item->soft_break_count; j++) {
            if(item->soft_breaks[j] > split_pos && item->soft_breaks[j] < len &&
               suffix->soft_break_count < CWMD_SOFT_BREAKS_MAX) {
                suffix->soft_breaks[suffix->soft_break_count++] =
                    (uint8_t)(item->soft_breaks[j] - split_pos);
            }
        }
        return true;
    }

    return false;
}

static bool cwmd_build_line(
    Canvas* canvas,
    const CwmdConfig* cfg,
    CwmdParser* parser,
    uint16_t avail_width,
    CwmdLine* line) {
    uint16_t line_width = avail_width;
    CwmdParser saved;
    CwmdItem item;
    CwmdItem prefix;
    CwmdItem suffix;
    bool have_item = false;
    bool split_line;

    /* Snapshot before each atom so overflow can hand the word to the next line intact. */
    memset(line, 0, sizeof(*line));
    cwmd_prepare_logical(parser, cfg);
    line->bullet = parser->bullet;
    line->bullet_mark = parser->bullet && parser->first_bullet_line;
    line->align = parser->bullet ? CwmdAlignLeft : parser->align;
    line->line_style = parser->line_style;
    if(line->bullet) {
        line_width = line_width > CWMD_BULLET_INDENT ?
                         (uint16_t)(line_width - CWMD_BULLET_INDENT) :
                         line_width;
    }

    if(!parser->has_pending_item && *parser->p == '\0') return false;
    if(!parser->has_pending_item && *parser->p == '\n') {
        parser->p++;
        cwmd_parser_reset(parser);
        line->ended_logical = true;
        line->hard_empty = true;
        return true;
    }

    while(parser->has_pending_item || (*parser->p != '\0' && *parser->p != '\n')) {
        saved = *parser;
        if(!cwmd_next_item(canvas, cfg, parser, &item)) break;
        if(item.type == CwmdItemGap && !have_item) continue;

        if(line->item_count >= CWMD_MAX_ITEMS) {
            *parser = saved;
            break;
        }

        split_line = false;
        if((uint16_t)(line->width + item.width) > line_width) {
            bool fits_after_microfit = have_item &&
                                       cwmd_microfit_line_for_item(line, &item, line_width);
            if(!fits_after_microfit) {
                if(cwmd_soft_split_item(canvas, line, &item, line_width, &prefix, &suffix)) {
                    item = prefix;
                    parser->pending_item = suffix;
                    parser->has_pending_item = true;
                    split_line = true;
                } else if(have_item) {
                    *parser = saved;
                    break;
                }
            }
        }

        if(have_item && (uint16_t)(line->width + item.width) > line_width && !split_line) {
            *parser = saved;
            break;
        }

        line->items[line->item_count++] = item;
        line->width += item.width;
        if(item.type == CwmdItemGap) line->gaps++;
        have_item = true;
        if(split_line) break;
    }

    cwmd_trim_trailing_gaps(line);
    line->align = parser->bullet ? CwmdAlignLeft : parser->align;
    line->line_style = parser->line_style;
    if(!parser->has_pending_item && *parser->p == '\n') {
        parser->p++;
        cwmd_parser_reset(parser);
        line->ended_logical = true;
    } else if(!parser->has_pending_item && *parser->p == '\0') {
        line->ended_logical = true;
    } else {
        parser->first_bullet_line = false;
    }

    return have_item;
}

static bool cwmd_line_should_justify(const CwmdLine* line, uint16_t avail_width, uint8_t* extra) {
    uint16_t left;
    uint16_t base_add;
    uint16_t gap_width;
    uint16_t rem_add;

    /* Justify only dense prose lines; short lines stretched wide look like broken teeth. */
    *extra = 0U;
    if(line->align != CwmdAlignJustify) return false;
    if(line->ended_logical) return false;
    if(line->line_style.mono) return false;
    if(line->gaps < CWMD_JUSTIFY_MIN_GAPS) return false;
    if(line->width >= avail_width) return false;
    if((uint32_t)line->width * CWMD_JUSTIFY_MIN_DEN < (uint32_t)avail_width * CWMD_JUSTIFY_MIN_NUM)
        return false;

    left = (uint16_t)(avail_width - line->width);
    base_add = (uint16_t)(left / line->gaps);
    rem_add = (left % line->gaps) != 0U ? 1U : 0U;
    for(uint8_t i = 0U; i < line->item_count; i++) {
        if(line->items[i].type == CwmdItemGap) {
            gap_width = (uint16_t)(line->items[i].width + base_add + rem_add);
            if(gap_width > CWMD_JUSTIFY_MAX_GAP) return false;
        }
    }
    *extra = (uint8_t)left;
    return left != 0U;
}

static void cwmd_draw_bullet(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_box(canvas, x + 1, y, 2U, 1U);
    canvas_draw_box(canvas, x, y + 1, CWMD_BULLET_SIZE, 2U);
    canvas_draw_box(canvas, x + 1, y + 3, 2U, 1U);
}

static void cwmd_draw_line(
    Canvas* canvas,
    const CwmdConfig* cfg,
    const CwmdLine* line,
    int32_t top,
    uint16_t avail_width) {
    int32_t x = cfg->x;
    uint8_t justify_extra = 0U;
    uint8_t gap_i = 0U;
    bool justify;

    if(line->bullet) x += CWMD_BULLET_INDENT;
    if(line->align == CwmdAlignCenter && line->width < avail_width) {
        x += (int32_t)((avail_width - line->width) / 2U);
    } else if(line->align == CwmdAlignRight && line->width < avail_width) {
        x += (int32_t)(avail_width - line->width);
    }

    justify = cwmd_line_should_justify(line, avail_width, &justify_extra);
    if(line->bullet_mark) {
        cwmd_draw_bullet(canvas, cfg->x + 1, top + 3);
    }

    for(uint8_t i = 0U; i < line->item_count; i++) {
        const CwmdItem* item = &line->items[i];
        uint16_t w = item->width;

        if(item->type == CwmdItemGap) {
            if(justify && line->gaps != 0U) {
                w = (uint16_t)(w + (justify_extra / line->gaps));
                if(gap_i < justify_extra % line->gaps) w++;
                gap_i++;
            }
            x += w;
            continue;
        }

        if(item->type == CwmdItemIcon) {
            int32_t ix = x + item->icon->left_bearing;
            int32_t iy =
                top + ((int32_t)cfg->line_height - item->icon->height) / 2 + item->icon->y_offset;
            if(ix + item->icon->width > cfg->x && ix < (int32_t)(cfg->x + avail_width) &&
               iy + item->icon->height > cfg->y && iy < (int32_t)(cfg->y + cfg->height)) {
                canvas_draw_xbm(
                    canvas, ix, iy, item->icon->width, item->icon->height, item->icon->xbm);
            }
            x += item->width;
            continue;
        }

        canvas_set_font(canvas, cwmd_font(item->style));
        canvas_draw_str(canvas, x, top + cfg->line_height - 1, item->text);
        x += item->width;
    }
}

static uint16_t cwmd_process(
    Canvas* canvas,
    const CwmdConfig* cfg,
    const CwmdState* state,
    const char* text,
    bool draw
#ifdef CWMD_HOST_TEST
    ,
    CwmdTestStats* stats
#endif
) {
    CwmdParser parser = {0};
    /* Keep the large line scratch buffer off the GUI thread stack. */
    static CwmdLine line;
    uint16_t avail_width;
    uint16_t line_no = 0U;
    int16_t scroll_px = state ? state->scroll_px : 0;

    /* Shared pass for measuring and drawing. Same layout, one less place to drift. */
    if(canvas == NULL || cfg == NULL || text == NULL || cfg->line_height == 0U) return 0U;
    if(scroll_px < 0) scroll_px = 0;
    avail_width = cwmd_effective_width(cfg);
    parser.p = text;
    cwmd_parser_reset(&parser);
#ifdef CWMD_HOST_TEST
    parser.stats = stats;
#endif

    while(cwmd_build_line(canvas, cfg, &parser, avail_width, &line)) {
        int32_t top = (int32_t)cfg->y + (int32_t)(line_no * cfg->line_height) - scroll_px;
#ifdef CWMD_HOST_TEST
        uint8_t extra = 0U;
#endif

#ifdef CWMD_HOST_TEST
        if(stats) {
            stats->lines++;
            stats->atoms += line.item_count;
            stats->bullets += line.bullet_mark ? 1U : 0U;
            stats->centered += line.align == CwmdAlignCenter ? 1U : 0U;
            stats->righted += line.align == CwmdAlignRight ? 1U : 0U;
            stats->last_line_width = (int16_t)line.width;
            if(line.microfit_shrink != 0U) {
                stats->microfit_lines++;
                stats->last_microfit_shrink_px = (int16_t)line.microfit_shrink;
            }
            if(cwmd_line_should_justify(&line, avail_width, &extra)) {
                stats->justified++;
                stats->last_extra_gap_px = extra / line.gaps;
            }
            for(uint8_t i = 0U; i < line.item_count; i++) {
                if(line.items[i].type == CwmdItemIcon) stats->icons++;
                if(line.items[i].type == CwmdItemText && line.items[i].style.bold)
                    stats->bold_atoms++;
                if(line.items[i].type == CwmdItemText && line.items[i].style.mono)
                    stats->mono_atoms++;
            }
        }
#endif

        if(draw && top >= cfg->y && top + cfg->line_height <= (int32_t)(cfg->y + cfg->height)) {
            cwmd_draw_line(canvas, cfg, &line, top, avail_width);
        }
        line_no++;
    }

    return (uint16_t)(line_no * cfg->line_height);
}

uint16_t cwmd_content_height(Canvas* canvas, const CwmdConfig* cfg, const char* text) {
    return cwmd_process(
        canvas,
        cfg,
        NULL,
        text,
        false
#ifdef CWMD_HOST_TEST
        ,
        NULL
#endif
    );
}

int16_t cwmd_max_scroll_px(Canvas* canvas, const CwmdConfig* cfg, const char* text) {
    uint16_t h = cwmd_content_height(canvas, cfg, text);
    if(cfg == NULL || h <= cfg->height) return 0;
    return (int16_t)(h - cfg->height);
}

static int16_t cwmd_clamp_scroll(int16_t v, int16_t max_scroll_px) {
    if(v < 0) return 0;
    if(v > max_scroll_px) return max_scroll_px;
    return v;
}

static int16_t cwmd_scroll_target_limit(int16_t max_scroll_px, uint8_t step_px) {
    int16_t remainder;

    if(max_scroll_px <= 0) return 0;
    if(step_px == 0U) step_px = 9U;
    remainder = (int16_t)(max_scroll_px % (int16_t)step_px);
    if(remainder > 0 && remainder < 9) {
        return (int16_t)(max_scroll_px + ((int16_t)step_px - remainder));
    }
    return max_scroll_px;
}

void cwmd_scroll_step(CwmdState* state, int8_t dir, int16_t max_scroll_px, uint8_t step_px) {
    int16_t step;
    int16_t target_limit;

    if(state == NULL) return;
    if(step_px == 0U) step_px = 9U;
    if(max_scroll_px < 0) max_scroll_px = 0;
    target_limit = cwmd_scroll_target_limit(max_scroll_px, step_px);
    state->max_scroll_px = target_limit;
    step = (int16_t)step_px * dir;
    state->target_scroll_px =
        cwmd_clamp_scroll((int16_t)(state->target_scroll_px + step), target_limit);
}

bool cwmd_scroll_tick(CwmdState* state) {
    int16_t delta;
    int16_t step = CWMD_SCROLL_PX_PER_TICK;

    if(state == NULL) return false;
    delta = state->target_scroll_px - state->scroll_px;
    if(delta == 0) return false;
    if(delta > step)
        state->scroll_px += step;
    else if(delta < -step)
        state->scroll_px -= step;
    else
        state->scroll_px = state->target_scroll_px;
    return true;
}

static void
    cwmd_draw_scrollbar(Canvas* canvas, const CwmdConfig* cfg, CwmdState* state, const char* text) {
    uint16_t content_h;
    int16_t max_scroll;
    int16_t scroll;
    uint8_t thumb_h;
    uint8_t thumb_y;
    uint8_t track_x;

    if(!cfg->scrollbar || cfg->height == 0U) return;
    content_h = cwmd_content_height(canvas, cfg, text);
    max_scroll = content_h > cfg->height ? (int16_t)(content_h - cfg->height) : 0;
    if(state) {
        int16_t target_limit = state->max_scroll_px > max_scroll ? state->max_scroll_px :
                                                                   max_scroll;
        state->max_scroll_px = target_limit;
        state->target_scroll_px = cwmd_clamp_scroll(state->target_scroll_px, target_limit);
        state->scroll_px = cwmd_clamp_scroll(state->scroll_px, target_limit);
    }
    if(max_scroll <= 0) return;
    scroll = state ? state->scroll_px : 0;
    scroll = cwmd_clamp_scroll(scroll, max_scroll);
    thumb_h = (uint8_t)(((uint32_t)cfg->height * cfg->height) / content_h);
    if(thumb_h < 4U) thumb_h = 4U;
    if(thumb_h > cfg->height) thumb_h = cfg->height;
    thumb_y = cfg->y;
    if(max_scroll > 0 && cfg->height > thumb_h) {
        thumb_y = (uint8_t)(cfg->y + (((uint32_t)scroll * (cfg->height - thumb_h)) / max_scroll));
    }
    track_x = (uint8_t)(cfg->x + cfg->width - 1U);
    canvas_draw_box(canvas, track_x, cfg->y, 1U, cfg->height);
    canvas_set_color(canvas, ColorWhite);
    if(thumb_y > cfg->y) canvas_draw_box(canvas, track_x, cfg->y, 1U, (uint8_t)(thumb_y - cfg->y));
    if(thumb_y + thumb_h < cfg->y + cfg->height) {
        canvas_draw_box(
            canvas,
            track_x,
            (int32_t)(thumb_y + thumb_h),
            1U,
            (uint8_t)(cfg->y + cfg->height - thumb_y - thumb_h));
    }
    canvas_set_color(canvas, ColorBlack);
}

static void cwmd_draw_center_chip(Canvas* canvas, const char* label) {
    const uint8_t button_w = 36U;
    const uint8_t button_h = 12U;
    const uint8_t x = (uint8_t)((CWMD_SCREEN_W - button_w) / 2U);
    const uint8_t y = CWMD_SCREEN_H;
    const uint8_t top = (uint8_t)(y - button_h);

    if(label == NULL) return;
    canvas_draw_box(canvas, x, top, button_w, button_h);
    canvas_draw_line(canvas, (int32_t)(x - 1U), y, (int32_t)(x - 1U), top);
    canvas_draw_line(canvas, (int32_t)(x - 2U), y, (int32_t)(x - 2U), (int32_t)(top + 1U));
    canvas_draw_line(canvas, (int32_t)(x - 3U), y, (int32_t)(x - 3U), (int32_t)(top + 2U));
    canvas_draw_line(canvas, (int32_t)(x + button_w), y, (int32_t)(x + button_w), top);
    canvas_draw_line(
        canvas, (int32_t)(x + button_w + 1U), y, (int32_t)(x + button_w + 1U), (int32_t)(top + 1U));
    canvas_draw_line(
        canvas, (int32_t)(x + button_w + 2U), y, (int32_t)(x + button_w + 2U), (int32_t)(top + 2U));
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, x + (button_w / 2U), top + (button_h / 2U), AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

static void cwmd_draw_chrome(Canvas* canvas, const CwmdConfig* cfg) {
    if((cfg->chrome & CwmdChromeLeft) && cfg->left_label) {
        elements_button_left(canvas, cfg->left_label);
    }
    if((cfg->chrome & CwmdChromeRight) && cfg->right_label) {
        elements_button_right(canvas, cfg->right_label);
    }
    if(cfg->chrome & CwmdChromeCenter) {
        cwmd_draw_center_chip(canvas, cfg->center_label);
    }
}

void cwmd_draw(Canvas* canvas, const CwmdConfig* cfg, CwmdState* state, const char* text) {
    if(canvas == NULL || cfg == NULL || text == NULL) return;
    cwmd_process(
        canvas,
        cfg,
        state,
        text,
        true
#ifdef CWMD_HOST_TEST
        ,
        NULL
#endif
    );
    cwmd_draw_scrollbar(canvas, cfg, state, text);
    cwmd_draw_chrome(canvas, cfg);
}

#ifdef CWMD_HOST_TEST
void cwmd_test_stats(Canvas* canvas, const CwmdConfig* cfg, const char* text, CwmdTestStats* out) {
    if(out == NULL) return;
    memset(out, 0, sizeof(*out));
    (void)cwmd_process(canvas, cfg, NULL, text, false, out);
}
#endif
