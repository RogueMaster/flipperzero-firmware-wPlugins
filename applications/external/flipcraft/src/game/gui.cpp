
#include "gui.h"

namespace flipcraft {

static const uint8_t DIGITS[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111},
    {0b010, 0b110, 0b010, 0b010, 0b111},
    {0b111, 0b001, 0b111, 0b100, 0b111},
    {0b111, 0b001, 0b111, 0b001, 0b111},
    {0b101, 0b101, 0b111, 0b001, 0b001},
    {0b111, 0b100, 0b111, 0b001, 0b111},
    {0b111, 0b100, 0b111, 0b101, 0b111},
    {0b111, 0b001, 0b010, 0b010, 0b010},
    {0b111, 0b101, 0b111, 0b101, 0b111},
    {0b111, 0b101, 0b111, 0b001, 0b111},
};

void Screen2D::number(int x, int y, int d) {
    if(d < 0 || d > 9) return;
    for(int r = 0; r < 5; r++)
        for(int c = 0; c < 3; c++)
            if(DIGITS[d][r] & (1 << (2 - c))) setPixel(x + c, y + r, 1);
}

static const uint8_t HEART[7] =
    {0b0110110, 0b1111111, 0b1111111, 0b1111111, 0b0111110, 0b0011100, 0b0001000};

// full = black body / white 1px border, empty = inverted; both opaque
void Screen2D::heart(int x, int y, bool full) {
    auto on = [](int r, int c) {
        return r >= 0 && r < 7 && c >= 0 && c < 7 && (HEART[r] & (1 << (6 - c)));
    };
    for(int r = 0; r < 7; r++)
        for(int c = 0; c < 7; c++) {
            if(!on(r, c)) continue;
            bool body = on(r - 1, c) && on(r + 1, c) && on(r, c - 1) && on(r, c + 1);
            setPixel(x + c, y + r, full ? (body ? 1 : 0) : (body ? 0 : 1));
        }
}

// reference arrow (2px shaft, 4-column even-taper head), shaft cut to cell width
void Screen2D::arrow(int x, int y) {
    static const uint16_t A[8] = {0x008, 0x00C, 0x00E, 0x1FF, 0x1FF, 0x00E, 0x00C, 0x008};
    for(int r = 0; r < 8; r++)
        for(int c = 0; c < 9; c++)
            if((A[r] >> (8 - c)) & 1) setPixel(x + c, y + r, 1);
}

// fire, extracted pixel-for-pixel from the reference furnace screenshot (8x6)
void Screen2D::flame(int x, int y) {
    static const uint8_t F[6] = {0x42, 0x49, 0x99, 0xD2, 0x4B, 0x9B};
    for(int r = 0; r < 6; r++)
        for(int c = 0; c < 8; c++)
            if((F[r] >> (7 - c)) & 1) setPixel(x + c, y + r, 1);
}

// light corner brackets marking an empty craft-target cell; flush with the
// cell edge so neighbouring marks are split by the 1px seam only
void Screen2D::ticks(int x, int y, int w) {
    int a = w / 4;
    for(int d = 0; d < a; d++) {
        setPixel(x + d, y, 0);
        setPixel(x + w - 1 - d, y, 0);
        setPixel(x + d, y + w - 1, 0);
        setPixel(x + w - 1 - d, y + w - 1, 0);
        setPixel(x, y + d, 0);
        setPixel(x, y + w - 1 - d, 0);
        setPixel(x + w - 1, y + d, 0);
        setPixel(x + w - 1, y + w - 1 - d, 0);
    }
}

const char* itemName(uint8_t type) {
    if(type == 0) return nullptr;
    if(type == ITEM_DYNAMITE) return "Dynamite";
    if(type == ITEM_GUNPOWDER) return "Gunpowder";
    if(type >= ITEM_NONSTACKABLE) {
        static const char* const tools[16] = {
            "Wood Pickaxe",
            "Wood Axe",
            "Wood Shovel",
            "Wood Sword",
            "Stone Pickaxe",
            "Stone Axe",
            "Stone Shovel",
            "Stone Sword",
            "Iron Pickaxe",
            "Iron Axe",
            "Iron Shovel",
            "Iron Sword",
            "Shears",
            "Crafting Table",
            "Furnace",
            "Chest"};
        return tools[type & 0x0F];
    }
    static const char* const mats[16] = {
        nullptr,
        "Stick",
        "Dirt",
        "Stone",
        "Cobblestone",
        "Wood Log",
        "Leaves",
        "Planks",
        "Coal",
        "Iron Ore",
        "Sand",
        "Glass",
        "Sapling",
        "Iron Ingot",
        "Apple",
        nullptr};
    return mats[type >> 4];
}

static void sprite8(Screen2D& s, int x, int y, const uint8_t* t, bool texInv, int ink) {
    for(int r = 0; r < 8; r++)
        for(int c = 0; c < 8; c++) {
            bool px = (((t[7 - r] >> c) & 1) != 0) != texInv;
            if(px == (ink == 1)) s.setPixel(x + c, y + r, ink);
        }
}

// MSB = left column; pickaxe extracted pixel-for-pixel from the reference
// screenshot (all tiers share it, no fill overlay)
static const uint8_t kToolShape[4][8] = {
    {0x78, 0x86, 0x72, 0x19, 0x2D, 0x55, 0xA5, 0xC2}, // pickaxe
    {0xF8, 0x88, 0xF8, 0x18, 0x18, 0x18, 0x18, 0x18}, // axe
    {0x3C, 0x24, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18}, // shovel
    {0x03, 0x07, 0x0E, 0x1C, 0xB8, 0x70, 0xD0, 0x00}, // sword
};
static const uint8_t kToolFill[4][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00, 0x00},
};
static const uint8_t kShears[8] = {0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0xC6, 0x00};
static const uint8_t kIngot[8] = {0x00, 0x00, 0x3C, 0x7E, 0xFF, 0xFF, 0x00, 0x00};
// sapling extracted pixel-for-pixel from the reference screenshot
static const uint8_t kSapling[8] = {0x56, 0x2B, 0xDA, 0x75, 0xAF, 0x3C, 0x18, 0x18};

static void bitmap8(Screen2D& s, int x, int y, const uint8_t* b, int ink) {
    for(int r = 0; r < 8; r++)
        for(int c = 0; c < 8; c++)
            if((b[r] >> (7 - c)) & 1) s.setPixel(x + c, y + r, ink);
}

// tier 0 wood: outline; 1 stone: dithered head; 2 iron: solid head
static void toolIcon(Screen2D& s, int x, int y, int idx, int ink) {
    int tier = idx >> 2;
    for(int r = 0; r < 8; r++) {
        uint8_t bits = kToolShape[idx & 3][r];
        if(tier == 2)
            bits |= kToolFill[idx & 3][r];
        else if(tier == 1)
            bits |= kToolFill[idx & 3][r] & ((r & 1) ? 0x55 : 0xAA);
        for(int c = 0; c < 8; c++)
            if((bits >> (7 - c)) & 1) s.setPixel(x + c, y + r, ink);
    }
}

// front face where the mesh has one, side face for plain cubes; flat cross
// items show the union of both planes — the silhouette seen in the world
static void meshIcon(Screen2D& s, int x, int y, const MeshEntry& e, int ink) {
    if(!e.exists) return;
    if(e.texCount > 3) {
        const MeshTex& mt = e.textures[3];
        sprite8(s, x, y, texturePacked(mt.id), (mt.settings & TS_INVERTED) != 0, ink);
        return;
    }
    if(e.quadCount) {
        const uint8_t* a = texturePacked(e.textures[0].id);
        const uint8_t* b = texturePacked(e.textures[e.texCount > 1 ? 1 : 0].id);
        for(int r = 0; r < 8; r++)
            for(int c = 0; c < 8; c++)
                if(((a[7 - r] | b[7 - r]) >> c) & 1) s.setPixel(x + c, y + r, ink);
        return;
    }
    const MeshTex& mt = e.textures[2];
    sprite8(s, x, y, texturePacked(mt.id), (mt.settings & TS_INVERTED) != 0, ink);
}

void Screen2D::itemIcon(int x, int y, int type, bool onDark) {
    int ink = onDark ? 0 : 1;
    if(type >= ITEM_NONSTACKABLE) {
        int t = type & 0x0F;
        if(t < 12)
            toolIcon(*this, x, y, t, ink);
        else if(t == 12)
            bitmap8(*this, x, y, kShears, ink);
        else
            meshIcon(*this, x, y, meshBlock((uint8_t)t), ink); // 0xD/0xE/0xF == block ids
        return;
    }
    if(type == ITEM_DYNAMITE) {
        meshIcon(*this, x, y, meshItem(ENTITY_DYNAMITE), ink);
        return;
    }
    if(type == ITEM_GUNPOWDER) {
        meshIcon(*this, x, y, meshItem(ENTITY_GUNPOWDER), ink);
        return;
    }
    int hi = type >> 4;
    if(hi == 0x0D) {
        bitmap8(*this, x, y, kIngot, ink);
        return;
    }
    if(hi == 0x0C) {
        bitmap8(*this, x, y, kSapling, ink);
        return;
    }
    // stone/glass never drop, no item mesh; apple's entity id is 3, not 0xE
    const MeshEntry& e = (hi == 0x03) ? meshBlock(BLOCK_STONE) :
                         (hi == 0x0B) ? meshBlock(BLOCK_GLASS) :
                                        meshItem((uint8_t)(hi == 0x0E ? ENTITY_APPLE : hi));
    meshIcon(*this, x, y, e, ink);
}

// icon + count over a white halo; w: 9 GUI cells, 10 output/hotbar cells
void Screen2D::slotItem(int x, int y, int w, const ItemCell& it, bool onDark) {
    if(!it.type) return;
    int o = (w - 8) / 2;
    itemIcon(x + o, y + o, it.type, onDark);
    if(it.type >= ITEM_NONSTACKABLE || it.count < 2) return;
    int n = it.count;
    int ux = x + w - 3, uy = y + w - 5; // units digit, flush to the cell corner
    fillRect((n > 9 ? ux - 4 : ux) - 1, uy - 1, x + w - 1, y + w - 1, 0);
    if(n > 9) number(ux - 4, uy, n / 10);
    number(ux, uy, n % 10);
}

}
