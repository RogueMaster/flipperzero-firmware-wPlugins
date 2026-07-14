#include "graphics.h"

#define SCALE 10

namespace {

// Another algo - https://www.research-collection.ethz.ch/handle/20.500.11850/68976

/*
 * Thick line methods courtesy
 * https://github.com/ArminJo/Arduino-BlueDisplay/blob/master/src/LocalGUI/ThickLine.hpp 
 */
const int LOCAL_DISPLAY_WIDTH = 64;
const int LOCAL_DISPLAY_HEIGHT = 128;
/*
 * Overlap means drawing additional pixel when changing minor direction
 * Needed for drawThickLine, otherwise some pixels will be missing in the thick line
 */
const int LINE_OVERLAP_NONE = 0; // No line overlap, like in standard Bresenham
const int LINE_OVERLAP_MAJOR =
    0x01; // Overlap - first go major then minor direction. Pixel is drawn as extension after actual line
const int LINE_OVERLAP_MINOR =
    0x02; // Overlap - first go minor then major direction. Pixel is drawn as extension before next line
const int LINE_OVERLAP_BOTH = 0x03; // Overlap - both

const int LINE_THICKNESS_MIDDLE = 0; // Start point is on the line at center of the thick line
const int LINE_THICKNESS_DRAW_CLOCKWISE = 1; // Start point is on the counter clockwise border line
const int LINE_THICKNESS_DRAW_COUNTERCLOCKWISE = 2; // Start point is on the clockwise border line

/**
 * Draws a line from aXStart/aYStart to aXEnd/aYEnd including both ends
 * @param aOverlap One of LINE_OVERLAP_NONE, LINE_OVERLAP_MAJOR, LINE_OVERLAP_MINOR, LINE_OVERLAP_BOTH
 */
void drawLineOverlap(
    Canvas* canvas,
    unsigned int aXStart,
    unsigned int aYStart,
    unsigned int aXEnd,
    unsigned int aYEnd,
    uint8_t aOverlap) {
    int16_t tDeltaX, tDeltaY, tDeltaXTimes2, tDeltaYTimes2, tError, tStepX, tStepY;

    /*
     * Clip to display size
     */
    if(aXStart >= LOCAL_DISPLAY_WIDTH) {
        aXStart = LOCAL_DISPLAY_WIDTH - 1;
    }

    if(aXEnd >= LOCAL_DISPLAY_WIDTH) {
        aXEnd = LOCAL_DISPLAY_WIDTH - 1;
    }

    if(aYStart >= LOCAL_DISPLAY_HEIGHT) {
        aYStart = LOCAL_DISPLAY_HEIGHT - 1;
    }

    if(aYEnd >= LOCAL_DISPLAY_HEIGHT) {
        aYEnd = LOCAL_DISPLAY_HEIGHT - 1;
    }

    if((aXStart == aXEnd) || (aYStart == aYEnd)) {
        // horizontal or vertical line -> fillRect() is faster than drawLine()
        // fillRect(
        //     aXStart,
        //     aYStart,
        //     aXEnd,
        //     aYEnd,
        //     aColor); // you can remove the check and this line if you have no fillRect() or drawLine() available.
        canvas_draw_box(canvas, aXStart, aYStart, aXEnd - aXStart, aYEnd - aYStart);
    } else {
        // calculate direction
        tDeltaX = aXEnd - aXStart;
        tDeltaY = aYEnd - aYStart;
        if(tDeltaX < 0) {
            tDeltaX = -tDeltaX;
            tStepX = -1;
        } else {
            tStepX = +1;
        }
        if(tDeltaY < 0) {
            tDeltaY = -tDeltaY;
            tStepY = -1;
        } else {
            tStepY = +1;
        }
        tDeltaXTimes2 = tDeltaX << 1;
        tDeltaYTimes2 = tDeltaY << 1;
        // draw start pixel
        // drawPixel(aXStart, aYStart, aColor);
        canvas_draw_dot(canvas, aXStart, aYStart);
        if(tDeltaX > tDeltaY) {
            // start value represents a half step in Y direction
            tError = tDeltaYTimes2 - tDeltaX;
            while(aXStart != aXEnd) {
                // step in main direction
                aXStart += tStepX;
                if(tError >= 0) {
                    if(aOverlap & LINE_OVERLAP_MAJOR) {
                        // draw pixel in main direction before changing
                        // drawPixel(aXStart, aYStart, aColor);
                        canvas_draw_dot(canvas, aXStart, aYStart);
                    }
                    // change Y
                    aYStart += tStepY;
                    if(aOverlap & LINE_OVERLAP_MINOR) {
                        // draw pixel in minor direction before changing
                        // drawPixel(aXStart - tStepX, aYStart, aColor);
                        canvas_draw_dot(canvas, aXStart - tStepX, aYStart);
                    }
                    tError -= tDeltaXTimes2;
                }
                tError += tDeltaYTimes2;
                // drawPixel(aXStart, aYStart, aColor);
                canvas_draw_dot(canvas, aXStart, aYStart);
            }
        } else {
            tError = tDeltaXTimes2 - tDeltaY;
            while(aYStart != aYEnd) {
                aYStart += tStepY;
                if(tError >= 0) {
                    if(aOverlap & LINE_OVERLAP_MAJOR) {
                        // draw pixel in main direction before changing
                        // drawPixel(aXStart, aYStart, aColor);
                        canvas_draw_dot(canvas, aXStart, aYStart);
                    }
                    aXStart += tStepX;
                    if(aOverlap & LINE_OVERLAP_MINOR) {
                        // draw pixel in minor direction before changing
                        // drawPixel(aXStart, aYStart - tStepY, aColor);
                        canvas_draw_dot(canvas, aXStart, aYStart - tStepY);
                    }
                    tError -= tDeltaYTimes2;
                }
                tError += tDeltaXTimes2;
                // drawPixel(aXStart, aYStart, aColor);
                canvas_draw_dot(canvas, aXStart, aYStart);
            }
        }
    }
}

/**
 * Bresenham with thickness
 * No pixel missed and every pixel only drawn once!
 * The code is bigger and more complicated than drawThickLineSimple() but it tends to be faster, since drawing a pixel is often a slow operation.
 * aThicknessMode can be one of LINE_THICKNESS_MIDDLE, LINE_THICKNESS_DRAW_CLOCKWISE, LINE_THICKNESS_DRAW_COUNTERCLOCKWISE
 */
void drawThickLine(
    Canvas* canvas,
    unsigned int aXStart,
    unsigned int aYStart,
    unsigned int aXEnd,
    unsigned int aYEnd,
    unsigned int aThickness,
    uint8_t aThicknessMode) {
    int16_t i, tDeltaX, tDeltaY, tDeltaXTimes2, tDeltaYTimes2, tError, tStepX, tStepY;

    if(aThickness <= 1) {
        drawLineOverlap(canvas, aXStart, aYStart, aXEnd, aYEnd, LINE_OVERLAP_NONE);
    }
    /*
     * Clip to display size
     */
    if(aXStart >= LOCAL_DISPLAY_WIDTH) {
        aXStart = LOCAL_DISPLAY_WIDTH - 1;
    }

    if(aXEnd >= LOCAL_DISPLAY_WIDTH) {
        aXEnd = LOCAL_DISPLAY_WIDTH - 1;
    }

    if(aYStart >= LOCAL_DISPLAY_HEIGHT) {
        aYStart = LOCAL_DISPLAY_HEIGHT - 1;
    }

    if(aYEnd >= LOCAL_DISPLAY_HEIGHT) {
        aYEnd = LOCAL_DISPLAY_HEIGHT - 1;
    }

    /**
     * For coordinate system with 0.0 top left
     * Swap X and Y delta and calculate clockwise (new delta X inverted)
     * or counterclockwise (new delta Y inverted) rectangular direction.
     * The right rectangular direction for LINE_OVERLAP_MAJOR toggles with each octant
     */
    tDeltaY = aXEnd - aXStart;
    tDeltaX = aYEnd - aYStart;
    // mirror 4 quadrants to one and adjust deltas and stepping direction
    bool tSwap = true; // count effective mirroring
    if(tDeltaX < 0) {
        tDeltaX = -tDeltaX;
        tStepX = -1;
        tSwap = !tSwap;
    } else {
        tStepX = +1;
    }
    if(tDeltaY < 0) {
        tDeltaY = -tDeltaY;
        tStepY = -1;
        tSwap = !tSwap;
    } else {
        tStepY = +1;
    }
    tDeltaXTimes2 = tDeltaX << 1;
    tDeltaYTimes2 = tDeltaY << 1;
    bool tOverlap;
    // adjust for right direction of thickness from line origin
    int tDrawStartAdjustCount = aThickness / 2;
    if(aThicknessMode == LINE_THICKNESS_DRAW_COUNTERCLOCKWISE) {
        tDrawStartAdjustCount = aThickness - 1;
    } else if(aThicknessMode == LINE_THICKNESS_DRAW_CLOCKWISE) {
        tDrawStartAdjustCount = 0;
    }

    /*
     * Now tDelta* are positive and tStep* define the direction
     * tSwap is false if we mirrored only once
     */
    // which octant are we now
    if(tDeltaX >= tDeltaY) {
        // Octant 1, 3, 5, 7 (between 0 and 45, 90 and 135, ... degree)
        if(tSwap) {
            tDrawStartAdjustCount = (aThickness - 1) - tDrawStartAdjustCount;
            tStepY = -tStepY;
        } else {
            tStepX = -tStepX;
        }
        /*
         * Vector for draw direction of the starting points of lines is rectangular and counterclockwise to main line direction
         * Therefore no pixel will be missed if LINE_OVERLAP_MAJOR is used on change in minor rectangular direction
         */
        // adjust draw start point
        tError = tDeltaYTimes2 - tDeltaX;
        for(i = tDrawStartAdjustCount; i > 0; i--) {
            // change X (main direction here)
            aXStart -= tStepX;
            aXEnd -= tStepX;
            if(tError >= 0) {
                // change Y
                aYStart -= tStepY;
                aYEnd -= tStepY;
                tError -= tDeltaXTimes2;
            }
            tError += tDeltaYTimes2;
        }
        // draw start line. We can alternatively use drawLineOverlap(aXStart, aYStart, aXEnd, aYEnd, LINE_OVERLAP_NONE, aColor) here.
        // drawLine(aXStart, aYStart, aXEnd, aYEnd);
        // canvas_draw_line(canvas, aXStart, aYStart, aXEnd, aYEnd);
        drawLineOverlap(canvas, aXStart, aYStart, aXEnd, aYEnd, LINE_OVERLAP_NONE);
        // draw aThickness number of lines
        tError = tDeltaYTimes2 - tDeltaX;
        for(i = aThickness; i > 1; i--) {
            // change X (main direction here)
            aXStart += tStepX;
            aXEnd += tStepX;
            tOverlap = LINE_OVERLAP_NONE;
            if(tError >= 0) {
                // change Y
                aYStart += tStepY;
                aYEnd += tStepY;
                tError -= tDeltaXTimes2;
                /*
                 * Change minor direction reverse to line (main) direction
                 * because of choosing the right (counter)clockwise draw vector
                 * Use LINE_OVERLAP_MAJOR to fill all pixel
                 *
                 * EXAMPLE:
                 * 1,2 = Pixel of first 2 lines
                 * 3 = Pixel of third line in normal line mode
                 * - = Pixel which will additionally be drawn in LINE_OVERLAP_MAJOR mode
                 *           33
                 *       3333-22
                 *   3333-222211
                 * 33-22221111
                 *  221111                     ^
                 *  11                          Main direction of start of lines draw vector
                 *  -> Line main direction
                 *  <- Minor direction of counterclockwise of start of lines draw vector
                 */
                tOverlap = LINE_OVERLAP_MAJOR;
            }
            tError += tDeltaYTimes2;
            drawLineOverlap(canvas, aXStart, aYStart, aXEnd, aYEnd, tOverlap);
        }
    } else {
        // the other octant 2, 4, 6, 8 (between 45 and 90, 135 and 180, ... degree)
        if(tSwap) {
            tStepX = -tStepX;
        } else {
            tDrawStartAdjustCount = (aThickness - 1) - tDrawStartAdjustCount;
            tStepY = -tStepY;
        }
        // adjust draw start point
        tError = tDeltaXTimes2 - tDeltaY;
        for(i = tDrawStartAdjustCount; i > 0; i--) {
            aYStart -= tStepY;
            aYEnd -= tStepY;
            if(tError >= 0) {
                aXStart -= tStepX;
                aXEnd -= tStepX;
                tError -= tDeltaYTimes2;
            }
            tError += tDeltaXTimes2;
        }
        //draw start line
        // drawLine(aXStart, aYStart, aXEnd, aYEnd);
        canvas_draw_line(canvas, aXStart, aYStart, aXEnd, aYEnd);
        // draw aThickness number of lines
        tError = tDeltaXTimes2 - tDeltaY;
        for(i = aThickness; i > 1; i--) {
            aYStart += tStepY;
            aYEnd += tStepY;
            tOverlap = LINE_OVERLAP_NONE;
            if(tError >= 0) {
                aXStart += tStepX;
                aXEnd += tStepX;
                tError -= tDeltaYTimes2;
                tOverlap = LINE_OVERLAP_MAJOR;
            }
            tError += tDeltaXTimes2;
            drawLineOverlap(canvas, aXStart, aYStart, aXEnd, aYEnd, tOverlap);
        }
    }
}
/**
 * The same as before, but no clipping to display range, some pixel are drawn twice (because of using LINE_OVERLAP_BOTH)
 * and direction of thickness changes for each octant (except for LINE_THICKNESS_MIDDLE and aThickness value is odd)
 * aThicknessMode can be LINE_THICKNESS_MIDDLE or any other value
 *
 */
/*
void drawThickLineSimple(
    Canvas* canvas,
    unsigned int aXStart,
    unsigned int aYStart,
    unsigned int aXEnd,
    unsigned int aYEnd,
    unsigned int aThickness,
    uint8_t aThicknessMode) {
    int16_t i, tDeltaX, tDeltaY, tDeltaXTimes2, tDeltaYTimes2, tError, tStepX, tStepY;

    tDeltaY = aXStart - aXEnd;
    tDeltaX = aYEnd - aYStart;
    // mirror 4 quadrants to one and adjust deltas and stepping direction
    if(tDeltaX < 0) {
        tDeltaX = -tDeltaX;
        tStepX = -1;
    } else {
        tStepX = +1;
    }
    if(tDeltaY < 0) {
        tDeltaY = -tDeltaY;
        tStepY = -1;
    } else {
        tStepY = +1;
    }
    tDeltaXTimes2 = tDeltaX << 1;
    tDeltaYTimes2 = tDeltaY << 1;
    bool tOverlap;
    // which octant are we now
    if(tDeltaX > tDeltaY) {
        if(aThicknessMode == LINE_THICKNESS_MIDDLE) {
            // adjust draw start point
            tError = tDeltaYTimes2 - tDeltaX;
            for(i = aThickness / 2; i > 0; i--) {
                // change X (main direction here)
                aXStart -= tStepX;
                aXEnd -= tStepX;
                if(tError >= 0) {
                    // change Y
                    aYStart -= tStepY;
                    aYEnd -= tStepY;
                    tError -= tDeltaXTimes2;
                }
                tError += tDeltaYTimes2;
            }
        }
        // draw start line. We can alternatively use drawLineOverlap(aXStart, aYStart, aXEnd, aYEnd, LINE_OVERLAP_NONE, aColor) here.
        // drawLine(aXStart, aYStart, aXEnd, aYEnd, aColor);
        canvas_draw_line(canvas, aXStart, aYStart, aXEnd, aYEnd);
        // draw aThickness lines
        tError = tDeltaYTimes2 - tDeltaX;
        for(i = aThickness; i > 1; i--) {
            // change X (main direction here)
            aXStart += tStepX;
            aXEnd += tStepX;
            tOverlap = LINE_OVERLAP_NONE;
            if(tError >= 0) {
                // change Y
                aYStart += tStepY;
                aYEnd += tStepY;
                tError -= tDeltaXTimes2;
                tOverlap = LINE_OVERLAP_BOTH;
            }
            tError += tDeltaYTimes2;
            drawLineOverlap(canvas, aXStart, aYStart, aXEnd, aYEnd, tOverlap);
        }
    } else {
        // adjust draw start point
        if(aThicknessMode == LINE_THICKNESS_MIDDLE) {
            tError = tDeltaXTimes2 - tDeltaY;
            for(i = aThickness / 2; i > 0; i--) {
                aYStart -= tStepY;
                aYEnd -= tStepY;
                if(tError >= 0) {
                    aXStart -= tStepX;
                    aXEnd -= tStepX;
                    tError -= tDeltaYTimes2;
                }
                tError += tDeltaXTimes2;
            }
        }
        // draw start line. We can alternatively use drawLineOverlap(aXStart, aYStart, aXEnd, aYEnd, LINE_OVERLAP_NONE, aColor) here.
        // drawLine(aXStart, aYStart, aXEnd, aYEnd, aColor);
        canvas_draw_line(canvas, aXStart, aYStart, aXEnd, aYEnd);
        tError = tDeltaXTimes2 - tDeltaY;
        for(i = aThickness; i > 1; i--) {
            aYStart += tStepY;
            aYEnd += tStepY;
            tOverlap = LINE_OVERLAP_NONE;
            if(tError >= 0) {
                aXStart += tStepX;
                aXEnd += tStepX;
                tError -= tDeltaYTimes2;
                tOverlap = LINE_OVERLAP_BOTH;
            }
            tError += tDeltaXTimes2;
            drawLineOverlap(canvas, aXStart, aYStart, aXEnd, aYEnd, tOverlap);
        }
    }
}
*/

}; // namespace

/*
  Fontname: micro
  Copyright: Public domain font.  Share and enjoy.
  Glyphs: 18/128
  BBX Build Mode: 0
*/
const uint8_t u8g2_font_micro_tn[148] =
    "\22\0\2\3\2\3\1\4\4\3\5\0\0\5\0\5\0\0\0\0\0\0w \4`\63*\10\67\62Q"
    "j\312\0+\7or\321\24\1,\5*r\3-\5\247\62\3.\5*\62\4/\10\67\262\251\60\12"
    "\1\60\10\67r)U\12\0\61\6\66rS\6\62\7\67\62r\224\34\63\7\67\62r$\22\64\7\67"
    "\62\221\212\14\65\7\67\62\244<\1\66\6\67r#E\67\10\67\62c*\214\0\70\6\67\62TE\71"
    "\7\67\62\24\71\1:\6\66\62$\1\0\0\0\4\377\377\0";

// TODO: allow points to be located outside the canvas. currently, the canvas_* methods
// choke on this in some cases, resulting in large vertical/horizontal lines
void gfx_draw_line(Canvas* canvas, float x1, float y1, float x2, float y2) {
    canvas_draw_line(
        canvas, roundf(x1 / SCALE), roundf(y1 / SCALE), roundf(x2 / SCALE), roundf(y2 / SCALE));
}

void gfx_draw_line(Canvas* canvas, const Vec2& p1, const Vec2& p2) {
    gfx_draw_line(canvas, p1.x, p1.y, p2.x, p2.y);
}

void gfx_draw_line_thick(Canvas* canvas, float x1, float y1, float x2, float y2, int thickness) {
    x1 = roundf(x1 / SCALE);
    y1 = roundf(y1 / SCALE);
    x2 = roundf(x2 / SCALE);
    y2 = roundf(y2 / SCALE);

    drawThickLine(canvas, x1, y1, x2, y2, thickness, LINE_THICKNESS_MIDDLE);
}

void gfx_draw_line_thick(Canvas* canvas, const Vec2& p1, const Vec2& p2, int thickness) {
    gfx_draw_line_thick(canvas, p1.x, p1.y, p2.x, p2.y, thickness);
}

void gfx_draw_disc(Canvas* canvas, float x, float y, float r) {
    canvas_draw_disc(canvas, roundf(x / SCALE), roundf(y / SCALE), roundf(r / SCALE));
}
void gfx_draw_disc(Canvas* canvas, const Vec2& p, float r) {
    gfx_draw_disc(canvas, p.x, p.y, r);
}

void gfx_draw_circle(Canvas* canvas, float x, float y, float r) {
    canvas_draw_circle(canvas, roundf(x / SCALE), roundf(y / SCALE), roundf(r / SCALE));
}
void gfx_draw_circle(Canvas* canvas, const Vec2& p, float r) {
    gfx_draw_circle(canvas, p.x, p.y, r);
}

void gfx_draw_dot(Canvas* canvas, float x, float y) {
    canvas_draw_dot(canvas, roundf(x / SCALE), roundf(y / SCALE));
}
void gfx_draw_dot(Canvas* canvas, const Vec2& p) {
    gfx_draw_dot(canvas, p.x, p.y);
}

void gfx_draw_arc(Canvas* canvas, const Vec2& p, float r, float start, float end) {
    float adj_end = end;
    if(end < start) {
        adj_end += (float)M_PI * 2;
    }
    // initialize to start of arc
    float sx = p.x + r * cosf(start);
    float sy = p.y - r * sinf(start);
    size_t segments = r / 8;
    for(size_t i = 1; i <= segments; i++) { // for now, use r to determin number of segments
        float nx = p.x + r * cosf(start + i / (segments / (adj_end - start)));
        float ny = p.y - r * sinf(start + i / (segments / (adj_end - start)));
        gfx_draw_line(canvas, sx, sy, nx, ny);
        sx = nx;
        sy = ny;
    }
}

void gfx_draw_str(Canvas* canvas, int x, int y, Align h, Align v, const char* str) {
    canvas_set_custom_u8g2_font(canvas, u8g2_font_micro_tn);

    canvas_set_color(canvas, ColorWhite);
    int w = canvas_string_width(canvas, str);
    if(h == AlignRight) {
        canvas_draw_box(canvas, x - 1 - w, y, w + 2, 6);
    } else if(h == AlignLeft) {
        canvas_draw_box(canvas, x - 1, y, w + 2, 6);
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str_aligned(canvas, x, y, h, v, str);

    canvas_set_font(canvas, FontSecondary); // reset?
}
