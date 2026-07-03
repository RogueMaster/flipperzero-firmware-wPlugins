#include <furi.h>
#include <math.h>
#include "draw.h"

void ie_draw_pixel(uint8_t* buffer, int x, int y, int bw, int bh) {
    if(x < 0 || x >= bw) return;
    if(y < 0 || y >= bh) return;
    buffer[y * bw + x] = 1;
}

void ie_draw_line(uint8_t* buffer, int x1, int y1, int x2, int y2, int bw, int bh) {
    uint8_t tmp;
    uint8_t x, y;
    uint8_t dx, dy;
    int8_t err;
    int8_t ystep;

    uint8_t swapxy = 0;

    /* no intersection check at the moment, should be added... */
    if(x1 > x2)
        dx = x1 - x2;
    else
        dx = x2 - x1;
    if(y1 > y2)
        dy = y1 - y2;
    else
        dy = y2 - y1;

    if(dy > dx) {
        swapxy = 1;
        tmp = dx;
        dx = dy;
        dy = tmp;
        tmp = x1;
        x1 = y1;
        y1 = tmp;
        tmp = x2;
        x2 = y2;
        y2 = tmp;
    }
    if(x1 > x2) {
        tmp = x1;
        x1 = x2;
        x2 = tmp;
        tmp = y1;
        y1 = y2;
        y2 = tmp;
    }
    err = dx >> 1;
    if(y2 > y1)
        ystep = 1;
    else
        ystep = -1;
    y = y1;

#ifndef U8G2_16BIT
    if(x2 == 255) x2--;
#else
    if(x2 == 0xffff) x2--;
#endif

    for(x = x1; x <= x2; x++) {
        if(swapxy == 0) {
            ie_draw_pixel(buffer, x, y, bw, bh);
        } else {
            ie_draw_pixel(buffer, y, x, bw, bh);
        }
        err -= (uint8_t)dy;
        if(err < 0) {
            y += (uint8_t)ystep;
            err += (uint8_t)dx;
        }
    }
}

#define DRAW_UPPER_RIGHT 0x01
#define DRAW_UPPER_LEFT  0x02
#define DRAW_LOWER_LEFT  0x04
#define DRAW_LOWER_RIGHT 0x08
#define DRAW_ALL         (DRAW_UPPER_RIGHT | DRAW_UPPER_LEFT | DRAW_LOWER_LEFT | DRAW_LOWER_RIGHT)

void ie_draw_circle_section(
    uint8_t* buffer,
    uint8_t x,
    uint8_t y,
    uint8_t x0,
    uint8_t y0,
    uint8_t option,
    int bw,
    int bh) {
    /* upper right */
    if(option & DRAW_UPPER_RIGHT) {
        ie_draw_pixel(buffer, x0 + x, y0 - y, bw, bh);
        ie_draw_pixel(buffer, x0 + y, y0 - x, bw, bh);
    }

    /* upper left */
    if(option & DRAW_UPPER_LEFT) {
        ie_draw_pixel(buffer, x0 - x, y0 - y, bw, bh);
        ie_draw_pixel(buffer, x0 - y, y0 - x, bw, bh);
    }

    /* lower right */
    if(option & DRAW_LOWER_RIGHT) {
        ie_draw_pixel(buffer, x0 + x, y0 + y, bw, bh);
        ie_draw_pixel(buffer, x0 + y, y0 + x, bw, bh);
    }

    /* lower left */
    if(option & DRAW_LOWER_LEFT) {
        ie_draw_pixel(buffer, x0 - x, y0 + y, bw, bh);
        ie_draw_pixel(buffer, x0 - y, y0 + x, bw, bh);
    }
}

void ie_draw_circle(uint8_t* buffer, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, int bw, int bh) {
    int8_t f;
    int8_t ddF_x;
    int8_t ddF_y;
    uint8_t x;
    uint8_t y;

    int8_t dx = x1 - x2;
    int8_t dy = y1 - y2;
    uint8_t rad = (uint8_t)sqrt(dx * dx + dy * dy);

    f = 1;
    f -= rad;
    ddF_x = 1;
    ddF_y = 0;
    ddF_y -= rad;
    ddF_y *= 2;
    x = 0;
    y = rad;
    ie_draw_circle_section(buffer, x, y, x1, y1, DRAW_ALL, bw, bh);

    while(x < y) {
        if(f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        ie_draw_circle_section(buffer, x, y, x1, y1, DRAW_ALL, bw, bh);
    }
}

/*                                                                                                                                                                                                                                                            
  Fontname: micro                                                                                                                                                                                                                                             
  Copyright: Public domain font.  Share and enjoy.                                                                                                                                                                                                            
  Glyphs: 96/128                                                                                                                                                                                                                                              
  BBX Build Mode: 0                                                                                                                                                                                                                                           
*/
const uint8_t u8g2_font_micro_tr[702] =
    "`\0\2\2\2\3\2\4\4\3\5\0\0\5\0\5\0\0\336\1\306\2\241 \4\300f!\6VdN"
    "\1\42\6\313e\222\12#\10Wd\322PC\5$\7W\344\322\220C%\10\323dR\32\251\0&\10"
    "W\344b\32\62\1'\5\352\345\6(\6v\344T\31)\7VdbJ\12*\7WdR\265\32+"
    "\7\317\344\322J\0,\5J\344\6-\5Ge\6.\6JdF\0/\10Wd\253\230\42\0\60\7"
    "W\344*\253\2\61\6V\344V\3\62\7Wdt*\7\63\7Wdt\222#\64\10Wd\222\65b"
    "\0\65\7WdF\324\13\66\7W\344\346H#\67\10Wdf\25\23\0\70\10WdF\32j\4\71"
    "\10WdF\32q\1:\7VdF\34\1;\6VdF\36<\6WdS]=\6\317d\366\0"
    ">\7WdrU\2\77\10Wdf\322(\1@\6W\344\256,A\10WdF\32J\5B\10W"
    "dFZi\4C\7WdF,\7D\10WdT\262\26\0E\7WdF\34qF\10WdF"
    "\34\61\2G\10WdFLj\4H\10Wd\222\32J\5I\7WdV\254\6J\7WdK\65"
    "\2K\10Wd\222ZI\5L\6WdbsM\10Wd\322PV\0N\7WdFr\5O\10"
    "WdF\262F\0P\10WdF\32\62\2Q\7WdFRSR\10WdFZI\5S\7W"
    "dF<\2T\7WdVl\1U\7Wd\222k\4V\7Wd\222\253\2W\10Wd\222\65T"
    "\0X\11Wd\322Hi\244\0Y\11Wd\222\32)&\0Z\7Wdf*\7[\6vdV%"
    "\134\7Wdb\216\71]\6VdT\65^\5\313\345\32_\5Gd\6`\6\352e\222\0a\6S"
    "d\326\21b\10Wd\342Hj\4c\7SdF\214\3d\7Wd\207R#e\6SdF:f"
    "\7W\344\246\212\21g\7SdFJCh\10Wd\342HV\0i\6qdF\0j\7Sd\243"
    "\32\1k\10WdbZ\252\0l\6VdT\7m\7Sd\322P*n\7SdF\262\2o\7"
    "SdFR#p\7SdF\32\22q\7SdF\32\61r\7SdF\222\21s\7Sd\206\34"
    "\2t\7W\344\322\212Qu\7Sd\222\65\2v\7Sd\222U\1w\7Sd\222\32*x\7S"
    "dR\231\12y\10Sd\222J#\1z\7Sd\326H\3{\7W\344T\222Q|\6ud\206\0"
    "}\10WddTI\1~\6Ke\244\0\177\10W\344T\254\24\0\0\0\0\4\377\377\0";

/*                                                                                                                                                                                            
  Fontname: -Misc-Fixed-Medium-R-Normal--7-70-75-75-C-50-ISO10646-1                                                                                                                           
  Copyright: Public domain font.  Share and enjoy.                                                                                                                                            
  Glyphs: 95/1848                                                                                                                                                                             
  BBX Build Mode: 0                                                                                                                                                                           
*/
const uint8_t u8g2_font_5x7_tr[804] =
    "_\0\2\2\3\3\3\4\4\5\7\0\377\6\377\6\0\1\12\2\26\3\7 \5\0\275\1!\6\261\261"
    "\31)\42\7[\267IV\0#\12-\261\253\206\252\206\252\0$\11-\261[\365Ni\1%\10\64\261"
    "\311\261w\0&\11,\261\213)V\61\5'\5\231\267\31(\7r\261S\315\0)\10r\261\211\251R"
    "\0*\7k\261I\325j+\12-\261\315(\16\231Q\4,\7[\257S%\0-\6\14\265\31\1."
    "\6R\261\31\1/\7$\263\217m\0\60\10s\261\253\134\25\0\61\7s\261K\262\65\62\11\64\261S"
    "\61\307r\4\63\12\64\261\31\71i$\223\2\64\12\64\261\215\252\32\61'\0\65\12\64\261\31z#\231"
    "\24\0\66\12\64\261SyE\231\24\0\67\12\64\261\31\71\346\230#\0\70\12\64\261S\61\251(\223\2"
    "\71\12\64\261SQ\246\235\24\0:\7j\261\31q\4;\10\63\257\263\221*\1<\7k\261Mu\1"
    "=\10\34\263\31\31\215\0>\7k\261\311U\11\77\11s\261k\246\14\23\0@\11\64\261SQ\335H"
    "\1A\11\64\261SQ\216)\3B\12\64\261Yq\244(G\2C\11\64\261SQ\227I\1D\11\64"
    "\261Y\321\71\22\0E\11\64\261\31z\345<\2F\10\64\261\31z\345\32G\11\64\261SQ\247\231\6"
    "H\10\64\261\211rL\63I\7s\261Y\261\65J\10\64\261o\313\244\0K\12\64\261\211*I\231\312"
    "\0L\7\64\261\311\335#M\11\64\261\211\343\210f\0N\10\64\261\211k\251\63O\11\64\261S\321\231"
    "\24\0P\12\64\261YQ\216\224\63\0Q\12<\257S\321\134I\243\0R\11\64\261YQ\216\324\14S"
    "\12\64\261S\61eT&\5T\7s\261Y\261\13U\10\64\261\211\236I\1V\11\64\261\211\316$\25"
    "\0W\11\64\261\211\346\70b\0X\12\64\261\211\62I\25e\0Y\10s\261IVY\1Z\11\64\261"
    "\31\71\266G\0[\7s\261\31\261\71\134\11$\263\311(\243\214\2]\7s\261\231\315\21^\5S\271"
    "k_\6\14\261\31\1`\6R\271\211\1a\10$\261\33Q\251\2b\12\64\261\311yE\71\22\0c"
    "\6#\261\233Yd\10\64\261\257F\224ie\10$\261Sid\5f\11\64\261\255\312\231#\0g\11"
    ",\257\33\61\251\214\6h\10\64\261\311yE\63i\10s\261\313HV\3j\11{\257\315\260T\25\0"
    "k\11\64\261\311U\222\251\14l\7s\261\221]\3m\10$\261IiH\31n\7$\261Y\321\14o"
    "\10$\261SQ&\5p\11,\257YQ\216\224\1q\10,\257\33Q\246\35r\10$\261YQg\0"
    "s\10$\261\33\32\15\5t\11\64\261\313q\346\214\4u\7$\261\211f\32v\7c\261IV\5w"
    "\7$\261\211r\34x\10$\261\211I\252\30y\11,\257\211\62\225%\0z\10$\261\31\261\34\1{"
    "\10s\261MI\326\1|\5\261\261\71}\11s\261\311Q\305\24\1~\7\24\271K*\1\0\0\0\4"
    "\377\377\0";

uint16_t
    ie_draw_str(Canvas* canvas, int x, int y, Align h, Align v, IEFont font, const char* str) {
    canvas_set_custom_u8g2_font(canvas, font == FontMicro ? u8g2_font_micro_tr : u8g2_font_5x7_tr);
    canvas_draw_str_aligned(canvas, x, y, h, v, str);
    uint16_t w = canvas_string_width(canvas, str);
    canvas_set_font(canvas, FontSecondary);
    return w;
}

uint16_t ie_draw_get_str_width(Canvas* canvas, IEFont font, const char* str) {
    canvas_set_custom_u8g2_font(canvas, font == FontMicro ? u8g2_font_micro_tr : u8g2_font_5x7_tr);
    return canvas_string_width(canvas, str);
}

void ie_draw_modal_panel_frame(Canvas* canvas, int x, int y, int w, int h) {
    int pad = 2;
    int shadow_x = 1;
    int shadow_y = 1;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(
        canvas,
        x - pad - 1,
        y - pad - 1,
        w + pad * 2 + 2 + 1 + shadow_x,
        h + pad * 2 + 2 + 1 + shadow_y);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x - pad, y - pad, w + pad * 2, h + pad * 2);
    // draw shadow lines
    canvas_draw_line(
        canvas,
        x - pad + shadow_x,
        y + h + pad + shadow_y,
        x + w + pad + shadow_x,
        y + h + pad + shadow_y);
    canvas_draw_line(
        canvas,
        x + w + pad + shadow_x,
        y - pad + shadow_y,
        x + w + pad + shadow_x,
        y + h + pad + shadow_y);
}
