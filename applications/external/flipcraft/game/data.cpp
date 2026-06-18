#include "../flipcraft.h"
#include <string.h>
#include <initializer_list>

namespace flipcraft {

#include "../assets/textures.inc"

const uint8_t* textureBitmap(int texId) {
    return SOURCE_TEXTURES[(uint8_t)texId];
}

static const int QUADS[][4][3] = {
    {{0,0,16},{0,16,16},{0,16,0},{0,0,0}},
    {{16,0,0},{16,16,0},{16,16,16},{16,0,16}},
    {{0,0,0},{0,16,0},{16,16,0},{16,0,0}},
    {{16,0,16},{16,16,16},{0,16,16},{0,0,16}},
    {{0,0,16},{0,0,0},{16,0,0},{16,0,16}},
    {{0,16,0},{0,16,16},{16,16,16},{16,16,0}},
    {{2,0,2},{2,16,2},{14,16,14},{14,0,14}},
    {{2,0,14},{2,16,14},{14,16,2},{14,0,2}},
    {{1,0,15},{1,14,15},{1,14,1},{1,0,1}},
    {{15,0,1},{15,14,1},{15,14,15},{15,0,15}},
    {{1,0,1},{1,14,1},{15,14,1},{15,0,1}},
    {{15,0,15},{15,14,15},{1,14,15},{1,0,15}},
    {{1,0,15},{1,0,1},{15,0,1},{15,0,15}},
    {{1,14,1},{1,14,15},{15,14,15},{15,14,1}},
    {{0,0,0},{0,0,8},{8,0,8},{8,0,0}},
    {{1,1,7},{1,7,7},{1,7,1},{1,1,1}},
    {{7,1,1},{7,7,1},{7,7,7},{7,1,7}},
    {{1,1,1},{1,7,1},{7,7,1},{7,1,1}},
    {{7,1,7},{7,7,7},{1,7,7},{1,1,7}},
    {{1,1,7},{1,1,1},{7,1,1},{7,1,7}},
    {{1,7,1},{1,7,7},{7,7,7},{7,7,1}},
    {{2,1,2},{2,6,2},{6,6,6},{6,1,6}},
    {{2,1,6},{2,6,6},{6,6,2},{6,1,2}},
    {{6,1,2},{6,6,2},{2,6,6},{2,1,6}},
    {{6,1,6},{6,6,6},{2,6,2},{2,1,2}},
    {{0,0,0},{0,0,16},{16,0,16},{16,0,0}},
};

const int (*quadTemplate(int quadId))[3] {
    if (quadId >= 0 && quadId <= 0x19) return QUADS[quadId];
    return QUADS[0];
}

bool blockIsTransparent(uint8_t id) {
    return id == BLOCK_AIR || id == BLOCK_LEAVES || id == BLOCK_SAPLING ||
           id == BLOCK_GLASS || id == BLOCK_CHEST;
}
bool blockIsFull(uint8_t id) {
    return !(id == BLOCK_AIR || id == BLOCK_SAPLING || id == BLOCK_CHEST);
}
bool itemIsBlockItem(uint8_t entityId) {
    return !(entityId == ENTITY_STICK || entityId == ENTITY_APPLE ||
             entityId == ENTITY_COAL || entityId == ENTITY_FALLINGSAND ||
             entityId == ENTITY_SAPLING);
}

static void setTextures(MeshEntry& e, std::initializer_list<MeshTex> list) {
    e.texCount = 0;
    for (const MeshTex& t : list)
        if (e.texCount < 4) e.textures[e.texCount++] = t;
}
// Stops at the QUAD_NONE sentinel so old terminated lists drop in unchanged.
static void setQuads(MeshEntry& e, std::initializer_list<MeshQuadRef> list) {
    e.quadCount = 0;
    for (const MeshQuadRef& q : list) {
        if (q.quadId == QUAD_NONE) break;
        if (e.quadCount < 8) e.quads[e.quadCount++] = q;
    }
}

static MeshEntry makeCube(uint8_t top, uint8_t topS, uint8_t bot, uint8_t botS,
                          uint8_t side, uint8_t sideS, bool withFront,
                          uint8_t front = 0, uint8_t frontS = 0) {
    MeshEntry e; e.exists = true;
    setTextures(e, {{top, topS}, {bot, botS}, {side, sideS}});
    if (withFront) e.textures[e.texCount++] = {front, frontS};
    return e;
}

static MeshEntry* g_blockMesh = nullptr;
static MeshEntry* g_itemMesh  = nullptr;
static MeshEntry* g_emptyMesh = nullptr;

static void initMesh() {
    if (g_blockMesh) return;
    g_blockMesh = new MeshEntry[16]();
    g_itemMesh  = new MeshEntry[16]();
    g_emptyMesh = new MeshEntry();

    { MeshEntry e; e.exists = true;
      setTextures(e, {{TEX_EMPTY,0b1010},{TEX_DIRT,0b1000},{TEX_GRASSSIDE,0b1000}});
      g_blockMesh[BLOCK_GRASS] = e; }

    g_blockMesh[BLOCK_DIRT] = makeCube(TEX_DIRT,0b1000,TEX_DIRT,0b1000,TEX_DIRT,0b1000,true,TEX_DIRT,0b1000);
    g_itemMesh[BLOCK_DIRT]  = g_blockMesh[BLOCK_DIRT];

    { MeshEntry e; e.exists = true;
      setTextures(e, {{TEX_STICKITEMLIGHT,0b1100},{TEX_STICKITEMDARK,0b1110}});
      setQuads(e, {{QUAD_CROSSITEM1,0},{QUAD_CROSSITEM1,1},{QUAD_CROSSITEM2,0},{QUAD_CROSSITEM2,1},
                   {QUAD_CROSSITEM3,0},{QUAD_CROSSITEM3,1},{QUAD_CROSSITEM4,0},{QUAD_CROSSITEM4,1}});
      g_itemMesh[ENTITY_STICK] = e; }

    g_blockMesh[BLOCK_STONE] = makeCube(TEX_STONE,0b1000,TEX_STONE,0b1000,TEX_STONE,0b1000,false);
    { MeshEntry e; e.exists = true;
      setTextures(e, {{TEX_APPLEITEMLIGHT,0b1100},{TEX_APPLEITEMDARK,0b1110}});
      setQuads(e, {{QUAD_CROSSITEM1,0},{QUAD_CROSSITEM1,1},{QUAD_CROSSITEM2,0},{QUAD_CROSSITEM2,1},
                   {QUAD_CROSSITEM3,0},{QUAD_CROSSITEM3,1},{QUAD_CROSSITEM4,0},{QUAD_CROSSITEM4,1}});
      g_itemMesh[ENTITY_APPLE] = e; }

    g_blockMesh[BLOCK_COBBLE] = makeCube(TEX_COBBLE,0b1000,TEX_COBBLE,0b1000,TEX_COBBLE,0b1000,true,TEX_COBBLE,0b1000);
    g_itemMesh[BLOCK_COBBLE]  = g_blockMesh[BLOCK_COBBLE];

    g_blockMesh[BLOCK_LOG] = makeCube(TEX_LOGTOP,0b1000,TEX_LOGTOP,0b1000,TEX_LOGSIDE,0b1000,true,TEX_LOGSIDE,0b1000);
    g_itemMesh[BLOCK_LOG]  = g_blockMesh[BLOCK_LOG];

    g_blockMesh[BLOCK_LEAVES] = makeCube(TEX_LEAVES,0b1000,TEX_LEAVES,0b1000,TEX_LEAVES,0b1000,true,TEX_LEAVES,0b1000);
    g_itemMesh[BLOCK_LEAVES]  = g_blockMesh[BLOCK_LEAVES];

    g_blockMesh[BLOCK_PLANK] = makeCube(TEX_PLANK,0b1000,TEX_PLANK,0b1000,TEX_PLANK,0b1000,true,TEX_PLANK,0b1000);
    g_itemMesh[BLOCK_PLANK]  = g_blockMesh[BLOCK_PLANK];

    g_blockMesh[BLOCK_COALORE] = makeCube(TEX_COALORE,0b1000,TEX_COALORE,0b1000,TEX_COALORE,0b1000,false);

    { MeshEntry e; e.exists = true;
      setTextures(e, {{TEX_COALITEMLIGHT,0b1100},{TEX_COALITEMDARK,0b1110}});
      setQuads(e, {{QUAD_CROSSITEM1,0},{QUAD_CROSSITEM1,1},{QUAD_CROSSITEM2,0},{QUAD_CROSSITEM2,1},
                   {QUAD_CROSSITEM3,0},{QUAD_CROSSITEM3,1},{QUAD_CROSSITEM4,0},{QUAD_CROSSITEM4,1}});
      g_itemMesh[BLOCK_COALORE] = e; }

    g_blockMesh[BLOCK_IRONORE] = makeCube(TEX_IRONORE,0b1000,TEX_IRONORE,0b1000,TEX_IRONORE,0b1000,true,TEX_IRONORE,0b1000);
    g_itemMesh[BLOCK_IRONORE]  = g_blockMesh[BLOCK_IRONORE];

    g_blockMesh[BLOCK_SAND] = makeCube(TEX_DIRT,0b1010,TEX_DIRT,0b1010,TEX_DIRT,0b1010,true,TEX_DIRT,0b1010);
    g_itemMesh[BLOCK_SAND]  = g_blockMesh[BLOCK_SAND];

    g_blockMesh[BLOCK_GLASS] = makeCube(TEX_GLASS,0b1100,TEX_GLASS,0b1100,TEX_GLASS,0b1100,false);

    { MeshEntry e; e.exists = true;
      setTextures(e, {{TEX_SAPLINGLIGHT,0b0100},{TEX_SAPLINGDARK,0b0110}});
      setQuads(e, {{QUAD_CROSS1,0},{QUAD_CROSS1,1},{QUAD_CROSS2,0},{QUAD_CROSS2,1},{QUAD_NONE,0}});
      g_blockMesh[BLOCK_SAPLING] = e;
      MeshEntry it; it.exists = true;
      setTextures(it, {{TEX_SAPLINGLIGHT,0b0100},{TEX_SAPLINGDARK,0b0110}});
      setQuads(it, {{QUAD_CROSSITEM1,0},{QUAD_CROSSITEM1,1},{QUAD_CROSSITEM2,0},{QUAD_CROSSITEM2,1},{QUAD_NONE,0}});
      g_itemMesh[BLOCK_SAPLING] = it; }

    g_blockMesh[BLOCK_TABLE] = makeCube(TEX_TABLETOP,0b1000,TEX_PLANK,0b1000,TEX_TABLESIDE,0b1000,true,TEX_TABLESIDE,0b1000);
    g_itemMesh[BLOCK_TABLE]  = g_blockMesh[BLOCK_TABLE];

    g_blockMesh[BLOCK_FURNACE] = makeCube(TEX_FURNACETOP,0b1000,TEX_FURNACETOP,0b1000,TEX_FURNACESIDE,0b1000,true,TEX_FURNACEFRONTOFF,0b1000);
    g_itemMesh[BLOCK_FURNACE]  = g_blockMesh[BLOCK_FURNACE];

    { MeshEntry e; e.exists = true;
      setTextures(e, {{TEX_CHESTTOP,0b1000},{TEX_CHESTTOP,0b1000},{TEX_CHESTSIDE,0b1000},{TEX_CHESTFRONT,0b1000}});
      setQuads(e, {{QUAD_SMALL_NEGX,2},{QUAD_SMALL_POSX,2},{QUAD_SMALL_NEGZ,2},{QUAD_SMALL_POSZ,2},
                   {QUAD_SMALL_NEGY,0},{QUAD_SMALL_POSY,0},{QUAD_NONE,0}});
      g_blockMesh[BLOCK_CHEST] = e; g_itemMesh[BLOCK_CHEST] = e; }
}

const MeshEntry& meshBlock(uint8_t id) { initMesh(); return (id < 16) ? g_blockMesh[id] : *g_emptyMesh; }
const MeshEntry& meshItem(uint8_t hi)  { initMesh(); return (hi < 16) ? g_itemMesh[hi] : *g_emptyMesh; }

struct CraftRecipe { char key[10]; uint16_t result; };
static const CraftRecipe CRAFT_RECIPES[] = {
    {"770770000", ITEM_TABLE},        {"500000000", uint16_t(ITEM_PLANK | 0x4)},
    {"444404444", ITEM_FURNACE},      {"777707777", ITEM_CHEST},
    {"700700000", uint16_t(ITEM_STICK | 0x4)},
    {"777010010", ITEM_WOODPICKAXE},  {"444010010", ITEM_STONEPICKAXE}, {"DDD010010", ITEM_IRONPICKAXE},
    {"770710010", ITEM_WOODAXE},      {"440410010", ITEM_STONEAXE},     {"DD0D10010", ITEM_IRONAXE},
    {"700100100", ITEM_WOODSHOVEL},   {"400100100", ITEM_STONESHOVEL},  {"D00100100", ITEM_IRONSHOVEL},
    {"700700100", ITEM_WOODSWORD},    {"400400100", ITEM_STONESWORD},   {"D00D00100", ITEM_IRONSWORD},
    {"0D0D00000", ITEM_SHEARS},
};
static const int CRAFT_COUNT = (int)(sizeof(CRAFT_RECIPES) / sizeof(CRAFT_RECIPES[0]));

static bool colEmpty(const char* s) { return s[0]=='0' && s[3]=='0' && s[6]=='0'; }
static void shiftLeft(char* s) {
    s[0]=s[1]; s[3]=s[4]; s[6]=s[7];
    s[1]=s[2]; s[4]=s[5]; s[7]=s[8];
    s[2]='0';  s[5]='0';  s[8]='0';
}
static bool rowEmpty0(const char* s) { return s[0]=='0' && s[1]=='0' && s[2]=='0'; }
static void shiftUp(char* s) {
    s[0]=s[3]; s[1]=s[4]; s[2]=s[5];
    s[3]=s[6]; s[4]=s[7]; s[5]=s[8];
    s[6]='0';  s[7]='0';  s[8]='0';
}

uint16_t craftTable(const uint8_t grid[9]) {
    char s[10]; s[9] = 0;
    bool anything = false;
    for (int i = 0; i < 9; i++) {
        uint8_t v = (grid[i] >> 4) & 0xF;
        s[i] = (char)((v < 10) ? ('0' + v) : ('A' + v - 10));
        if (s[i] != '0') anything = true;
    }

    if (!anything) return 0;
    while (colEmpty(s))  shiftLeft(s);
    while (rowEmpty0(s)) shiftUp(s);
    for (int i = 0; i < CRAFT_COUNT; i++)
        if (strcmp(s, CRAFT_RECIPES[i].key) == 0) return CRAFT_RECIPES[i].result;
    return 0;
}
uint16_t craftFurnace(uint8_t input) {
    switch (input >> 4) {
        case BLOCK_COBBLE: return ITEM_STONE | 0x1;
        case BLOCK_LOG:    return ITEM_COAL | 0x1;
        case BLOCK_IRONORE:return ITEM_IRONINGOT | 0x1;
        case BLOCK_SAND:   return ITEM_GLASS | 0x1;
        default: return 0;
    }
}

}
