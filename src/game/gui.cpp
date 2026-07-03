
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

// A shape pixel is "body" when all four orthogonal neighbours are also part of
// the heart; the rest form its 1px border. Full = black body / white border,
// empty = white body / black border. Every shape pixel is painted, so both are
// fully opaque and nothing of the scene shows through.
void Screen2D::heart(int x,int y,bool full) {
    auto on = [](int r,int c){ return r>=0&&r<7&&c>=0&&c<7 && (HEART[r]&(1<<(6-c))); };
    for (int r=0;r<7;r++) for (int c=0;c<7;c++) {
        if (!on(r,c)) continue;
        bool body = on(r-1,c) && on(r+1,c) && on(r,c-1) && on(r,c+1);
        setPixel(x+c,y+r, full ? (body?1:0) : (body?0:1));
    }
}

const char* itemName(uint8_t id) {
    if (id == 0) return nullptr;                     // empty cell
    if ((id & 0xF0) == 0x00) return "Dynamite";      // "air, count N" encoding
    if (id == ITEM_GUNPOWDER) return "Gunpowder";    // "glass, count 0" cell
    if ((id & 0xF0) == ITEM_NONSTACKABLE) {
        static const char* const tools[16] = {
            "Wood Pickaxe","Wood Axe","Wood Shovel","Wood Sword",
            "Stone Pickaxe","Stone Axe","Stone Shovel","Stone Sword",
            "Iron Pickaxe","Iron Axe","Iron Shovel","Iron Sword",
            "Shears","Crafting Table","Furnace","Chest" };
        return tools[id & 0x0F];
    }
    static const char* const mats[16] = {
        nullptr,"Stick","Dirt","Stone","Cobblestone","Wood Log","Leaves","Planks",
        "Coal","Iron Ore","Sand","Glass","Sapling","Iron Ingot","Apple",nullptr };
    return mats[(id >> 4) & 0x0F];
}

void Screen2D::itemIcon(int x,int y,int itemId) {
    int type = (itemId & 0xF0) >> 4;
    bool nonstack = (itemId & 0xF0) == 0xF0;
    int sub = itemId & 0x0F;
    int key = nonstack ? (0x10 | sub) : type;
    if(!nonstack && sub==0 && type) key = type ^ 0x08;   // count-0 single items
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

}
