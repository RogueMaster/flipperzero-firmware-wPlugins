/**
 * @file about_view.c
 * @brief About 屏 View 实现 —— 三页:品牌+QR / GitHub+QR / Legal.
 *
 * 两块 QR(都离线生成,大写以塞 QR alphanumeric 模式;RFC 3986 URL 大小写不
 * 敏感,浏览器扫到自动转小写):
 *   - PINGEQUA.COM: HTTPS://PINGEQUA.COM (20 字符,version 1, 21x21, ECC-M)
 *   - GitHub repo:  HTTPS://GITHUB.COM/PINGEQUALAB/RF-LAB (37 字符,version 2, 25x25, ECC-M)
 * 必须带 HTTPS:// scheme —— 否则手机扫描器当纯文本不当网址跳转(开发早期踩过).
 *
 * 模型 page 字段只被 GUI 线程读写 → ViewModelTypeLockFree.
 * 右边缘 1px 轨道 + 3px thumb 进度条替代原 "N/3" 文字指示(避免与 QR 重叠).
 */
#include "about_view.h"

#include <furi.h>
#include <gui/canvas.h>
#include <stdlib.h>

/* ---------------------------------------------------------------------------
 * QR 数据 —— 离线 python qrcode 生成,行优先,每行 bpr 字节,MSB 先.
 * 改 URL 需重新跑生成器并替换整块.
 * --------------------------------------------------------------------------*/

typedef struct {
    uint8_t size; /* 模块数(每边) */
    uint8_t bpr;  /* 每行字节数 = ceil(size/8) */
    const uint8_t* data;
} PqQrCode;

/* HTTPS://PINGEQUA.COM: version 1, 21x21, ECC-M, 63 字节. */
static const uint8_t pq_qr_pingequa_data[21 * 3] = {
    0xFE, 0xF3, 0xF8, 0x82, 0x32, 0x08, 0xBA, 0xF2, 0xE8, 0xBA, 0x52, 0xE8,
    0xBA, 0x5A, 0xE8, 0x82, 0xC2, 0x08, 0xFE, 0xAB, 0xF8, 0x00, 0x00, 0x00,
    0xA3, 0x21, 0x28, 0x70, 0xF0, 0x28, 0x46, 0xD1, 0x50, 0x99, 0x25, 0xD0,
    0xD3, 0x34, 0xD8, 0x00, 0x8B, 0xD8, 0xFE, 0xC4, 0xB8, 0x82, 0x4C, 0x48,
    0xBA, 0x4E, 0xF0, 0xBA, 0x39, 0x40, 0xBA, 0xAC, 0x58, 0x82, 0x03, 0x08,
    0xFE, 0xB4, 0x18,
};
static const PqQrCode pq_qr_pingequa = {21, 3, pq_qr_pingequa_data};

/* HTTPS://GITHUB.COM/PINGEQUALAB/RF-LAB: version 2, 25x25, ECC-M, 100 字节. */
static const uint8_t pq_qr_github_data[25 * 4] = {
    0xFE, 0x74, 0xBF, 0x80, 0x82, 0xC9, 0x20, 0x80, 0xBA, 0xB9, 0xAE, 0x80,
    0xBA, 0xCE, 0x2E, 0x80, 0xBA, 0x2E, 0xAE, 0x80, 0x82, 0x0D, 0x20, 0x80,
    0xFE, 0xAA, 0xBF, 0x80, 0x00, 0xCD, 0x80, 0x00, 0x82, 0xDF, 0xE7, 0x00,
    0xF9, 0x0A, 0x3E, 0x00, 0x8A, 0x4F, 0x71, 0x80, 0xD1, 0x02, 0x54, 0x00,
    0x2A, 0x1B, 0x4F, 0x00, 0xD5, 0xAA, 0xD4, 0x00, 0xBA, 0x2E, 0x76, 0x00,
    0x99, 0x8C, 0xDB, 0x80, 0x8A, 0x33, 0xFA, 0x80, 0x00, 0xA3, 0x8F, 0x00,
    0xFE, 0x1D, 0xAB, 0x00, 0x82, 0x33, 0x8E, 0x00, 0xBA, 0x45, 0xF8, 0x80,
    0xBA, 0x77, 0x63, 0x80, 0xBA, 0x42, 0xAE, 0x80, 0x82, 0x34, 0xEE, 0x00,
    0xFE, 0xDC, 0x7D, 0x80,
};
static const PqQrCode pq_qr_github = {25, 4, pq_qr_github_data};

typedef struct {
    uint8_t page; /* 0..ABOUT_PAGE_COUNT-1 */
} AboutViewModel;

struct AboutView {
    View* view;
};

/* 通用 QR 绘制 —— 逐模块放大 scale 倍.背景框架已清成白,只画黑模块. */
static void draw_qr(Canvas* canvas, const PqQrCode* qr, int ox, int oy, int scale) {
    canvas_set_color(canvas, ColorBlack);
    for(int r = 0; r < qr->size; r++) {
        const uint8_t* row = qr->data + r * qr->bpr;
        for(int c = 0; c < qr->size; c++) {
            if(row[c >> 3] & (0x80 >> (c & 7))) {
                canvas_draw_box(canvas, ox + c * scale, oy + r * scale, scale, scale);
            }
        }
    }
}

/* 右边缘进度条:1px 轨道竖线 + 3px 宽 thumb 表示当前页位置.
 * 三页时 thumb 高 21px,thumb_y = page*64/3 → 0/21/42(page 2 ends at y62). */
static void draw_scrollbar(Canvas* canvas, uint8_t page) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, 127, 0, 127, 63);
    int h_thumb = 64 / ABOUT_PAGE_COUNT;
    int y_thumb = (page * 64) / ABOUT_PAGE_COUNT;
    canvas_draw_box(canvas, 125, y_thumb, 3, h_thumb);
}

/* Page 0:品牌 + pingequa.com QR.
 * QR 从 x84 左移到 x80 给右侧 scrollbar 让位,顺带稍微远离左侧 14 字符文本. */
static void draw_page_brand(Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "PINGEQUA RF Lab");
    canvas_draw_line(canvas, 0, 12, 123, 12);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "v0.5.2");
    canvas_draw_str(canvas, 2, 34, "nRF24 + CC1101");
    canvas_draw_str(canvas, 2, 44, "Precision Gear");
    canvas_draw_str(canvas, 2, 53, "for Hackers");
    canvas_draw_str(canvas, 2, 63, "pingequa.com");

    draw_qr(canvas, &pq_qr_pingequa, 80, 16, 2); /* 21x21 * 2 = 42x42,x80..121 / y16..57 */
}

/* Page 1:GitHub 链接 + GitHub QR(version 2, 25x25 * 2 = 50x50px). */
static void draw_page_code(Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "GitHub");
    canvas_draw_line(canvas, 0, 12, 123, 12);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "Repo:");
    canvas_draw_str(canvas, 2, 34, " pingequalab");
    canvas_draw_str(canvas, 2, 44, " /rf-lab");
    canvas_draw_str(canvas, 2, 61, "scan to open");

    draw_qr(canvas, &pq_qr_github, 73, 13, 2); /* x73..122 / y13..62,贴近右侧 scrollbar */
}

/* Page 2:Legal —— 简洁:核心警告 + license,4 行. */
static void draw_page_legal(Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Legal");
    canvas_draw_line(canvas, 0, 12, 123, 12);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "Authorized testing");
    canvas_draw_str(canvas, 2, 34, "only. Mind local");
    canvas_draw_str(canvas, 2, 44, "RF laws.");
    canvas_draw_str(canvas, 2, 61, "MIT (c) PINGEQUA");
}

static void about_view_draw_callback(Canvas* canvas, void* model) {
    AboutViewModel* m = model;
    switch(m->page) {
    case 0: draw_page_brand(canvas); break;
    case 1: draw_page_code(canvas); break;
    case 2: draw_page_legal(canvas); break;
    default: draw_page_brand(canvas); break;
    }
    /* scrollbar 最后画,确保覆盖在 divider 等元素之上. */
    draw_scrollbar(canvas, m->page);
}

AboutView* about_view_alloc(void) {
    AboutView* av = malloc(sizeof(*av));
    furi_check(av);
    av->view = view_alloc();
    view_allocate_model(av->view, ViewModelTypeLockFree, sizeof(AboutViewModel));
    view_set_context(av->view, av);
    view_set_draw_callback(av->view, about_view_draw_callback);
    return av;
}

void about_view_free(AboutView* av) {
    if(av == NULL) return;
    view_free(av->view);
    free(av);
}

View* about_view_get_view(AboutView* av) {
    furi_check(av);
    return av->view;
}

void about_view_set_page(AboutView* av, uint8_t page) {
    furi_check(av);
    if(page >= ABOUT_PAGE_COUNT) page = ABOUT_PAGE_COUNT - 1;
    with_view_model(
        av->view, AboutViewModel * m, { m->page = page; }, true);
}

uint8_t about_view_get_page(AboutView* av) {
    furi_check(av);
    uint8_t page = 0;
    with_view_model(
        av->view, AboutViewModel * m, { page = m->page; }, false);
    return page;
}
