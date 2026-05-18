#include "infos_view.h"

static void draw_header(Canvas* canvas, const char* title, uint8_t page) {
    char buf[8];
    canvas_draw_str_aligned(canvas, 3, 3, AlignLeft, AlignTop, title);
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)(page + 1), (unsigned)INFOS_PAGES_COUNT);
    canvas_draw_str_aligned(canvas, 124, 3, AlignRight, AlignTop, buf);
}

static void draw_nav(Canvas* canvas) {
    canvas_draw_icon(canvas, 4, 55, &I_ButtonLeft_4x7);
    canvas_draw_icon(canvas, 12, 55, &I_ButtonRight_4x7);
    canvas_draw_str_aligned(canvas, 22, 54, AlignLeft, AlignTop, "page");
    canvas_draw_str_aligned(canvas, 60, 54, AlignLeft, AlignTop, "Back: menu");
}

static void draw_wiring(Canvas* canvas) {
    draw_header(canvas, "Wiring", 0);
    canvas_draw_str_aligned(canvas, 3, 14, AlignLeft, AlignTop, "SCL -> C0");
    canvas_draw_str_aligned(canvas, 3, 22, AlignLeft, AlignTop, "SDA -> C1");
    canvas_draw_str_aligned(canvas, 3, 30, AlignLeft, AlignTop, "GND -> GND");
    canvas_draw_icon(canvas, 3, 38, &I_Voltage_16x16);
    canvas_draw_str_aligned(canvas, 22, 42, AlignLeft, AlignTop, "Use 3.3V signals only");
}

static void draw_main_scan(Canvas* canvas) {
    draw_header(canvas, "Main & Scanner", 1);
    canvas_draw_str_aligned(canvas, 3, 14, AlignLeft, AlignTop, "Main menu:");
    canvas_draw_str_aligned(canvas, 8, 22, AlignLeft, AlignTop, "Up/Dn select  OK enter");
    canvas_draw_str_aligned(canvas, 3, 32, AlignLeft, AlignTop, "Scanner:");
    canvas_draw_str_aligned(canvas, 8, 40, AlignLeft, AlignTop, "OK run scan");
    canvas_draw_str_aligned(canvas, 8, 47, AlignLeft, AlignTop, "Up/Dn scroll  Lng jump 5");
}

static void draw_sniff(Canvas* canvas) {
    draw_header(canvas, "Sniffer", 2);
    canvas_draw_str_aligned(canvas, 3, 14, AlignLeft, AlignTop, "OK    start / stop capture");
    canvas_draw_str_aligned(canvas, 3, 22, AlignLeft, AlignTop, "L/R   prev / next frame");
    canvas_draw_str_aligned(canvas, 3, 30, AlignLeft, AlignTop, "Up/Dn scroll bytes");
    canvas_draw_str_aligned(canvas, 3, 38, AlignLeft, AlignTop, "Long Up/Dn jump 5");
    canvas_draw_str_aligned(canvas, 3, 46, AlignLeft, AlignTop, "Back  back to menu");
}

static void draw_send_read(Canvas* canvas) {
    draw_header(canvas, "Sender - READ mode", 3);
    canvas_draw_str_aligned(canvas, 3, 14, AlignLeft, AlignTop, "L/R       device address");
    canvas_draw_str_aligned(canvas, 3, 22, AlignLeft, AlignTop, "Long L/R  Len 1..32");
    canvas_draw_str_aligned(canvas, 3, 30, AlignLeft, AlignTop, "Up/Dn     register byte");
    canvas_draw_str_aligned(canvas, 3, 38, AlignLeft, AlignTop, "OK        execute READ");
    canvas_draw_str_aligned(canvas, 3, 46, AlignLeft, AlignTop, "after read: Lng U/D scroll");
}

static void draw_send_write(Canvas* canvas) {
    draw_header(canvas, "Sender - WRITE mode", 4);
    canvas_draw_str_aligned(canvas, 3, 14, AlignLeft, AlignTop, "L/R       device address");
    canvas_draw_str_aligned(canvas, 3, 22, AlignLeft, AlignTop, "Long L/R  byte to write");
    canvas_draw_str_aligned(canvas, 3, 30, AlignLeft, AlignTop, "Up/Dn     register byte");
    canvas_draw_str_aligned(canvas, 3, 38, AlignLeft, AlignTop, "OK        execute WRITE");
    canvas_draw_str_aligned(canvas, 3, 46, AlignLeft, AlignTop, "Long OK  toggle READ/WRITE");
}

static void draw_cli(Canvas* canvas) {
    draw_header(canvas, "USB Serial CLI", 5);
    canvas_draw_str_aligned(canvas, 3, 14, AlignLeft, AlignTop, "Open: 'ufbt cli'");
    canvas_draw_str_aligned(canvas, 3, 22, AlignLeft, AlignTop, "i2c scan");
    canvas_draw_str_aligned(canvas, 3, 30, AlignLeft, AlignTop, "i2c probe <addr>");
    canvas_draw_str_aligned(canvas, 3, 38, AlignLeft, AlignTop, "i2c read <a> <r> <n> [fmt]");
    canvas_draw_str_aligned(canvas, 3, 46, AlignLeft, AlignTop, "i2c write <a> <r> <b>...");
}

void draw_infos_view(Canvas* canvas, i2cInfos* infos) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 0, 0, 128, 64, 3);
    canvas_set_font(canvas, FontSecondary);

    switch(infos->page) {
    case 0:
        draw_wiring(canvas);
        break;
    case 1:
        draw_main_scan(canvas);
        break;
    case 2:
        draw_sniff(canvas);
        break;
    case 3:
        draw_send_read(canvas);
        break;
    case 4:
        draw_send_write(canvas);
        break;
    case 5:
        draw_cli(canvas);
        break;
    default:
        break;
    }
    draw_nav(canvas);
}

i2cInfos* i2c_infos_alloc(void) {
    i2cInfos* infos = malloc(sizeof(i2cInfos));
    infos->page = 0;
    return infos;
}

void i2c_infos_free(i2cInfos* infos) {
    furi_assert(infos);
    free(infos);
}
