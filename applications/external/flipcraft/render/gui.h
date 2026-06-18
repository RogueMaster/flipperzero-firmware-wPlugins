

#pragma once
#include "../flipcraft.h"

namespace flipcraft {

struct Screen2D {
    Framebuffer* fb = nullptr;
    int x1=0, y1=0, x2=0, y2=0;

    void clearScreen() { fb->clear(); }
    void setPixel(int x,int y,int v) {
        if (x>=0&&x<UI_WIDTH&&y>=0&&y<SCREEN_HEIGHT)
            fb->px[y][x+UI_X_OFFSET]=(uint8_t)v;
    }
    void fillRect(int ax,int ay,int bx,int by,int v) {
        for (int y=ay;y<=by;y++) for (int x=ax;x<=bx;x++)
            setPixel(x,y,v);
    }
    void drawRect()  { fillRect(x1,y1,x2,y2,1); }
    void clearRect() { fillRect(x1,y1,x2,y2,0); }
    void invertRect(int ax,int ay,int bx,int by) {
        for (int y=ay;y<=by;y++) for (int x=ax;x<=bx;x++)
            if (x>=0&&x<UI_WIDTH&&y>=0&&y<SCREEN_HEIGHT)
                fb->px[y][x+UI_X_OFFSET]^=1;
    }

    void drawTex(int texId, bool inv=false);

    void number(int x,int y,int d);
    void itemIcon(int x,int y,int itemId);
    void heart(int x,int y,bool full);
};

}
