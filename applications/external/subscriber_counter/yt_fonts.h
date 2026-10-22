#pragma once

#include <gui/canvas.h>
#include <stdint.h>

/* Inter Display Bold for the hero number (BIG, MID when it would not fit),
 * Inter SemiBold for the header (TITLE) and Inter Medium for labels (LABEL). */
extern const uint8_t yt_inter_big[];
extern const uint8_t yt_inter_mid[];
extern const uint8_t yt_inter_title[];
extern const uint8_t yt_inter_label[];

typedef enum {
    YtFontBig,
    YtFontMid,
    YtFontTitle,
    YtFontLabel,
    YtFontRoleCount,
} YtFontRole;

typedef enum {
    YtStyleInter, /* Inter everywhere */
    YtStyleFlipper, /* Flipper's built-in fonts only (default) */
    YtStyleMix, /* Inter numbers, built-in text */
    YtStyleCount,
} YtFontStyle;

/* One type system: per role either a custom u8g2 font or a built-in Font,
 * plus the vertical metrics the layout needs (y positions in pixels). */
typedef struct {
    const char* name;
    const char* config_value;
    const uint8_t* custom[YtFontRoleCount]; /* NULL: use builtin[role] */
    Font builtin[YtFontRoleCount];
    uint8_t title_baseline;
    uint8_t label_baseline;
    uint8_t label_cap; /* Cap/digit height of the label font, sizes the badge */
    uint8_t big_baseline;
    uint8_t mid_baseline;
} YtStyle;

static const YtStyle yt_styles[YtStyleCount] = {
    [YtStyleInter] =
        {
            .name = "Inter",
            .config_value = "inter",
            .custom = {yt_inter_big, yt_inter_mid, yt_inter_title, yt_inter_label},
            .title_baseline = 10,
            .label_baseline = 49,
            .label_cap = 8,
            .big_baseline = 37,
            .mid_baseline = 35,
        },
    [YtStyleFlipper] =
        {
            .name = "Flipper",
            .config_value = "flipper",
            .builtin = {FontBigNumbers, FontPrimary, FontPrimary, FontSecondary},
            .title_baseline = 10,
            .label_baseline = 48,
            .label_cap = 7,
            .big_baseline = 36,
            .mid_baseline = 34,
        },
    [YtStyleMix] =
        {
            .name = "Mix",
            .config_value = "mix",
            .custom = {yt_inter_big, yt_inter_mid, NULL, NULL},
            .builtin = {FontBigNumbers, FontPrimary, FontPrimary, FontSecondary},
            .title_baseline = 10,
            .label_baseline = 48,
            .label_cap = 7,
            .big_baseline = 37,
            .mid_baseline = 35,
        },
};
