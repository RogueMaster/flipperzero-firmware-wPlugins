
#include "gui.h"

namespace flipcraft {

static const uint8_t DIGITS[10][5] = {
    {0b111,0b101,0b101,0b101,0b111}, {0b010,0b110,0b010,0b010,0b111},
    {0b111,0b001,0b111,0b100,0b111}, {0b111,0b001,0b111,0b001,0b111},
    {0b101,0b101,0b111,0b001,0b001}, {0b111,0b100,0b111,0b001,0b111},
    {0b111,0b100,0b111,0b101,0b111}, {0b111,0b001,0b010,0b010,0b010},
    {0b111,0b101,0b111,0b101,0b111}, {0b111,0b101,0b111,0b001,0b111},
};

void Screen2D::number(int x,int y,int d) {
    if (d<0||d>9) return;
    for (int r=0;r<5;r++) for (int c=0;c<3;c++)
        if (DIGITS[d][r] & (1<<(2-c)))
            setPixel(x+c,y+r,1);
}

static const uint8_t HEART[7] = {
    0b0110110, 0b1111111, 0b1111111, 0b1111111, 0b0111110, 0b0011100, 0b0001000 };
void Screen2D::heart(int x,int y,bool full) {
    for (int r=0;r<7;r++) for (int c=0;c<7;c++) {
        bool on = HEART[r] & (1<<(6-c));

        bool outline = on && (r==0||r==6|| !( (HEART[r]&(1<<(7-c))) && (HEART[r]&(1<<(5-c))) && (r>0&&(HEART[r-1]&(1<<(6-c)))) && (r<6&&(HEART[r+1]&(1<<(6-c)))) ));
        bool v = full ? on : outline;
        if (v) setPixel(x+c,y+r,1);
    }
}

void Screen2D::itemIcon(int x,int y,int itemId) {
    int type = (itemId & 0xF0) >> 4;
    bool nonstack = (itemId & 0xF0) == 0xF0;
    int sub = itemId & 0x0F;
    int key = nonstack ? (0x10 | sub) : type;
    for (int r=0;r<6;r++) for (int c=0;c<6;c++) {
        bool border = (r==0||r==5||c==0||c==5);
        bool inside = false;

        if (!border) {
            int rr=r-1, cc=c-1;
            switch (key & 0x07) {
                case 0: inside = ((rr+cc)&1); break;
                case 1: inside = (rr&1)==0; break;
                case 2: inside = (cc&1)==0; break;
                case 3: inside = (rr==cc||rr+cc==3); break;
                case 4: inside = (rr==1||rr==2)&&(cc==1||cc==2); break;
                case 5: inside = (rr==0||rr==3||cc==0||cc==3); break;
                case 6: inside = ((rr*cc)&1); break;
                default: inside = (rr>=cc); break;
            }
            if (key & 0x08) inside = !inside;
        }
        bool v = border || inside;
        if (v) setPixel(x+c,y+r,1);
    }
}

void Screen2D::drawTex(int texId, bool inv) {
    static const uint8_t ARROW[8] = {0,0b00001000,0b00001100,0b11111110,0b11111110,0b00001100,0b00001000,0};
    static const uint8_t FLAME[8] = {0b00010000,0b00011000,0b00111100,0b01111110,0b01111110,0b01011010,0b00111100,0};
    static const uint8_t EMPTY8[8] = {0};
    const uint8_t* g = EMPTY8;
    if (texId==0x63||texId==0x64) g = ARROW;
    else if (texId==0x65) g = FLAME;
    for (int r=0;r<8;r++) for (int c=0;c<8;c++) {
        int bit = (g[r]>>(7-c))&1;
        if (inv) bit ^= 1;
        if (bit) setPixel(x1+c,y1+r,1);
    }
}

}
