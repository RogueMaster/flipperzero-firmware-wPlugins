#include "../flipcraft.h"
#include <initializer_list>

namespace flipcraft {

#include "../../assets/textures.inc"

const uint8_t* texturePacked(int texId) {
    return TEXTURES_PACKED[(uint8_t)texId];
}

static const int QUADS[][4][3] = {
    {{0, 0, 16}, {0, 16, 16}, {0, 16, 0}, {0, 0, 0}},
    {{16, 0, 0}, {16, 16, 0}, {16, 16, 16}, {16, 0, 16}},
    {{0, 0, 0}, {0, 16, 0}, {16, 16, 0}, {16, 0, 0}},
    {{16, 0, 16}, {16, 16, 16}, {0, 16, 16}, {0, 0, 16}},
    {{0, 0, 16}, {0, 0, 0}, {16, 0, 0}, {16, 0, 16}},
    {{0, 16, 0}, {0, 16, 16}, {16, 16, 16}, {16, 16, 0}},
    {{2, 0, 2}, {2, 16, 2}, {14, 16, 14}, {14, 0, 14}},
    {{2, 0, 14}, {2, 16, 14}, {14, 16, 2}, {14, 0, 2}},
    {{1, 0, 15}, {1, 14, 15}, {1, 14, 1}, {1, 0, 1}},
    {{15, 0, 1}, {15, 14, 1}, {15, 14, 15}, {15, 0, 15}},
    {{1, 0, 1}, {1, 14, 1}, {15, 14, 1}, {15, 0, 1}},
    {{15, 0, 15}, {15, 14, 15}, {1, 14, 15}, {1, 0, 15}},
    {{1, 0, 15}, {1, 0, 1}, {15, 0, 1}, {15, 0, 15}},
    {{1, 14, 1}, {1, 14, 15}, {15, 14, 15}, {15, 14, 1}},
    {{0, 0, 0}, {0, 0, 8}, {8, 0, 8}, {8, 0, 0}},
    {{1, 1, 7}, {1, 7, 7}, {1, 7, 1}, {1, 1, 1}},
    {{7, 1, 1}, {7, 7, 1}, {7, 7, 7}, {7, 1, 7}},
    {{1, 1, 1}, {1, 7, 1}, {7, 7, 1}, {7, 1, 1}},
    {{7, 1, 7}, {7, 7, 7}, {1, 7, 7}, {1, 1, 7}},
    {{1, 1, 7}, {1, 1, 1}, {7, 1, 1}, {7, 1, 7}},
    {{1, 7, 1}, {1, 7, 7}, {7, 7, 7}, {7, 7, 1}},
    {{2, 1, 2}, {2, 6, 2}, {6, 6, 6}, {6, 1, 6}},
    {{2, 1, 6}, {2, 6, 6}, {6, 6, 2}, {6, 1, 2}},
    {{6, 1, 2}, {6, 6, 2}, {2, 6, 6}, {2, 1, 6}},
    {{6, 1, 6}, {6, 6, 6}, {2, 6, 2}, {2, 1, 2}},
    {{0, 0, 0}, {0, 0, 16}, {16, 0, 16}, {16, 0, 0}},
};

const int (*quadTemplate(int quadId))[3] {
    if(quadId >= 0 && quadId < QUAD_COUNT) return QUADS[quadId];
    return QUADS[0];
}

static void setTextures(MeshEntry& e, std::initializer_list<MeshTex> list) {
    e.texCount = 0;
    for(const MeshTex& t : list)
        if(e.texCount < 4) e.textures[e.texCount++] = t;
}
static void setQuads(MeshEntry& e, std::initializer_list<MeshQuadRef> list) {
    e.quadCount = 0;
    for(const MeshQuadRef& q : list)
        if(e.quadCount < 8) e.quads[e.quadCount++] = q;
}

static MeshEntry makeCube(
    uint8_t top,
    uint8_t topS,
    uint8_t bot,
    uint8_t botS,
    uint8_t side,
    uint8_t sideS,
    bool withFront,
    uint8_t front = 0,
    uint8_t frontS = 0) {
    MeshEntry e;
    e.exists = true;
    setTextures(e, {{top, topS}, {bot, botS}, {side, sideS}});
    if(withFront) e.textures[e.texCount++] = {front, frontS};
    return e;
}

static MeshEntry g_blockMesh[32];
static MeshEntry g_itemMesh[32];
static const MeshEntry g_emptyMesh{};
static bool g_meshReady = false;

static void initMesh() {
    if(g_meshReady) return;
    g_meshReady = true;

    {
        MeshEntry e;
        e.exists = true;
        setTextures(e, {{TEX_EMPTY, 0b1010}, {TEX_DIRT, 0b1000}, {TEX_GRASSSIDE, 0b1000}});
        g_blockMesh[BLOCK_GRASS] = e;
    }

    g_blockMesh[BLOCK_DIRT] =
        makeCube(TEX_DIRT, 0b1000, TEX_DIRT, 0b1000, TEX_DIRT, 0b1000, true, TEX_DIRT, 0b1000);
    g_itemMesh[BLOCK_DIRT] = g_blockMesh[BLOCK_DIRT];

    {
        MeshEntry e;
        e.exists = true;
        setTextures(e, {{TEX_STICKITEMLIGHT, 0b1100}, {TEX_STICKITEMDARK, 0b1110}});
        setQuads(
            e,
            {{QUAD_CROSSITEM1, 0},
             {QUAD_CROSSITEM1, 1},
             {QUAD_CROSSITEM2, 0},
             {QUAD_CROSSITEM2, 1},
             {QUAD_CROSSITEM3, 0},
             {QUAD_CROSSITEM3, 1},
             {QUAD_CROSSITEM4, 0},
             {QUAD_CROSSITEM4, 1}});
        g_itemMesh[ENTITY_STICK] = e;
    }

    g_blockMesh[BLOCK_STONE] =
        makeCube(TEX_STONE, 0b1000, TEX_STONE, 0b1000, TEX_STONE, 0b1000, false);
    {
        MeshEntry e;
        e.exists = true;
        setTextures(e, {{TEX_APPLEITEMLIGHT, 0b1100}, {TEX_APPLEITEMDARK, 0b1110}});
        setQuads(
            e,
            {{QUAD_CROSSITEM1, 0},
             {QUAD_CROSSITEM1, 1},
             {QUAD_CROSSITEM2, 0},
             {QUAD_CROSSITEM2, 1},
             {QUAD_CROSSITEM3, 0},
             {QUAD_CROSSITEM3, 1},
             {QUAD_CROSSITEM4, 0},
             {QUAD_CROSSITEM4, 1}});
        g_itemMesh[ENTITY_APPLE] = e;
    }

    g_blockMesh[BLOCK_COBBLE] = makeCube(
        TEX_COBBLE, 0b1000, TEX_COBBLE, 0b1000, TEX_COBBLE, 0b1000, true, TEX_COBBLE, 0b1000);
    g_itemMesh[BLOCK_COBBLE] = g_blockMesh[BLOCK_COBBLE];

    g_blockMesh[BLOCK_LOG] = makeCube(
        TEX_LOGTOP, 0b1000, TEX_LOGTOP, 0b1000, TEX_LOGSIDE, 0b1000, true, TEX_LOGSIDE, 0b1000);
    g_itemMesh[BLOCK_LOG] = g_blockMesh[BLOCK_LOG];

    g_blockMesh[BLOCK_LEAVES] = makeCube(
        TEX_LEAVES, 0b1000, TEX_LEAVES, 0b1000, TEX_LEAVES, 0b1000, true, TEX_LEAVES, 0b1000);
    g_itemMesh[BLOCK_LEAVES] = g_blockMesh[BLOCK_LEAVES];

    g_blockMesh[BLOCK_PLANK] =
        makeCube(TEX_PLANK, 0b1000, TEX_PLANK, 0b1000, TEX_PLANK, 0b1000, true, TEX_PLANK, 0b1000);
    g_itemMesh[BLOCK_PLANK] = g_blockMesh[BLOCK_PLANK];

    g_blockMesh[BLOCK_COALORE] =
        makeCube(TEX_COALORE, 0b1000, TEX_COALORE, 0b1000, TEX_COALORE, 0b1000, false);

    {
        MeshEntry e;
        e.exists = true;
        setTextures(e, {{TEX_COALITEMLIGHT, 0b1100}, {TEX_COALITEMDARK, 0b1110}});
        setQuads(
            e,
            {{QUAD_CROSSITEM1, 0},
             {QUAD_CROSSITEM1, 1},
             {QUAD_CROSSITEM2, 0},
             {QUAD_CROSSITEM2, 1},
             {QUAD_CROSSITEM3, 0},
             {QUAD_CROSSITEM3, 1},
             {QUAD_CROSSITEM4, 0},
             {QUAD_CROSSITEM4, 1}});
        g_itemMesh[BLOCK_COALORE] = e;
    }

    g_blockMesh[BLOCK_IRONORE] = makeCube(
        TEX_IRONORE, 0b1000, TEX_IRONORE, 0b1000, TEX_IRONORE, 0b1000, true, TEX_IRONORE, 0b1000);
    g_itemMesh[BLOCK_IRONORE] = g_blockMesh[BLOCK_IRONORE];

    g_blockMesh[BLOCK_SAND] =
        makeCube(TEX_DIRT, 0b1010, TEX_DIRT, 0b1010, TEX_DIRT, 0b1010, true, TEX_DIRT, 0b1010);
    g_itemMesh[BLOCK_SAND] = g_blockMesh[BLOCK_SAND];

    g_blockMesh[BLOCK_GLASS] =
        makeCube(TEX_GLASS, 0b1100, TEX_GLASS, 0b1100, TEX_GLASS, 0b1100, false);

    {
        MeshEntry e;
        e.exists = true;
        setTextures(e, {{TEX_SAPLINGLIGHT, 0b0100}, {TEX_SAPLINGDARK, 0b0110}});
        setQuads(e, {{QUAD_CROSS1, 0}, {QUAD_CROSS1, 1}, {QUAD_CROSS2, 0}, {QUAD_CROSS2, 1}});
        g_blockMesh[BLOCK_SAPLING] = e;
        MeshEntry it;
        it.exists = true;
        setTextures(it, {{TEX_SAPLINGLIGHT, 0b0100}, {TEX_SAPLINGDARK, 0b0110}});
        setQuads(
            it,
            {{QUAD_CROSSITEM1, 0},
             {QUAD_CROSSITEM1, 1},
             {QUAD_CROSSITEM2, 0},
             {QUAD_CROSSITEM2, 1}});
        g_itemMesh[BLOCK_SAPLING] = it;
    }

    g_blockMesh[BLOCK_TABLE] = makeCube(
        TEX_TABLETOP, 0b1000, TEX_PLANK, 0b1000, TEX_TABLESIDE, 0b1000, true, TEX_TABLESIDE, 0b1000);
    g_itemMesh[BLOCK_TABLE] = g_blockMesh[BLOCK_TABLE];

    g_blockMesh[BLOCK_FURNACE] = makeCube(
        TEX_FURNACETOP,
        0b1000,
        TEX_FURNACETOP,
        0b1000,
        TEX_FURNACESIDE,
        0b1000,
        true,
        TEX_FURNACEFRONTOFF,
        0b1000);
    g_itemMesh[BLOCK_FURNACE] = g_blockMesh[BLOCK_FURNACE];

    {
        MeshEntry e;
        e.exists = true;
        setTextures(
            e,
            {{TEX_CHESTTOP, 0b1000},
             {TEX_CHESTTOP, 0b1000},
             {TEX_CHESTSIDE, 0b1000},
             {TEX_CHESTFRONT, 0b1000}});
        setQuads(
            e,
            {{QUAD_SMALL_NEGX, 2},
             {QUAD_SMALL_POSX, 2},
             {QUAD_SMALL_NEGZ, 2},
             {QUAD_SMALL_POSZ, 2},
             {QUAD_SMALL_NEGY, 0},
             {QUAD_SMALL_POSY, 0}});
        g_blockMesh[BLOCK_CHEST] = e;
        g_itemMesh[BLOCK_CHEST] = e;
    }

    g_blockMesh[BLOCK_DYNAMITE] = makeCube(
        TEX_DYNAMITETOP,
        0b1000,
        TEX_DYNAMITETOP,
        0b1000,
        TEX_DYNAMITE,
        0b1000,
        true,
        TEX_DYNAMITE,
        0b1000);
    g_itemMesh[ENTITY_DYNAMITE] = g_blockMesh[BLOCK_DYNAMITE];

    {
        MeshEntry e;
        e.exists = true;
        setTextures(e, {{TEX_COALITEMLIGHT, 0b1100}, {TEX_COALITEMDARK, 0b1110}});
        setQuads(
            e,
            {{QUAD_CROSSITEM1, 0},
             {QUAD_CROSSITEM1, 1},
             {QUAD_CROSSITEM2, 0},
             {QUAD_CROSSITEM2, 1},
             {QUAD_CROSSITEM3, 0},
             {QUAD_CROSSITEM3, 1},
             {QUAD_CROSSITEM4, 0},
             {QUAD_CROSSITEM4, 1}});
        g_itemMesh[ENTITY_GUNPOWDER] = e;
    }
}

const MeshEntry& meshBlock(uint8_t id) {
    initMesh();
    return (id < 32) ? g_blockMesh[id] : g_emptyMesh;
}
const MeshEntry& meshItem(uint8_t hi) {
    initMesh();
    return (hi < 32) ? g_itemMesh[hi] : g_emptyMesh;
}

static constexpr MobSpec MOB_SPECS[MOB_SPECIES] = {
    {TEX_SHEEPFRONT, TEX_SHEEPSIDE, TEX_SHEEPTOP, 0, TEMPER_PASSIVE << 1, 0x20, (9 << 4) | 3},
    {TEX_WOLFFRONT,
     TEX_WOLFSIDE,
     TEX_WOLFTOP,
     (1 << MOB_SHEEP) | (1 << MOB_CREEPER),
     TEMPER_NEUTRAL << 1,
     0x21,
     (7 << 4) | 5},
    {TEX_CREEPERFRONT,
     TEX_CREEPERSIDE,
     TEX_CREEPERTOP,
     0,
     (TEMPER_HOSTILE << 1) | 1,
     0x20,
     (13 << 4) | 4},
    {TEX_BEEFRONT, TEX_BEESIDE, TEX_BEETOP, 0, (TEMPER_NEUTRAL << 1) | 8, 0x32, (4 << 4) | 6},
};
const MobSpec& mobSpec(uint8_t species) {
    return MOB_SPECS[species % MOB_SPECIES];
}

static constexpr MobBox SHEEP_BOXES[] = {
    {-4, 0, 4, 8, 6, 3, 0},
    {-4, 0, -7, 8, 6, 3, 0},
    {-6, 6, -10, 12, 11, 20, 0},
    {-3, 12, 9, 6, 7, 6, 1},
};
static constexpr MobBox WOLF_BOXES[] = {
    {-3, 0, 5, 6, 5, 3, 0},
    {-3, 0, -8, 6, 5, 3, 0},
    {-4, 4, -9, 8, 8, 17, 0},
    {-3, 9, 8, 6, 6, 6, 1},
    {-1, 10, -12, 2, 3, 4, 0},
};
static constexpr MobBox CREEPER_BOXES[] = {
    {-4, 0, 2, 8, 5, 4, 0},
    {-4, 0, -6, 8, 5, 4, 0},
    {-3, 5, -3, 6, 12, 6, 0},
    {-4, 17, -4, 8, 9, 8, 1},
};
static constexpr MobBox BEE_BOXES[] = {
    {-3, 1, -4, 6, 5, 8, 1},
    {-5, 6, -2, 2, 1, 4, 0},
    {3, 6, -2, 2, 1, 4, 0},
};
static constexpr struct {
    const MobBox* b;
    uint8_t n;
} MOB_PLANS[MOB_SPECIES] = {
    {SHEEP_BOXES, 4},
    {WOLF_BOXES, 5},
    {CREEPER_BOXES, 4},
    {BEE_BOXES, 3},
};
const MobBox* mobBoxes(uint8_t species, int& count) {
    const auto& p = MOB_PLANS[species % MOB_SPECIES];
    count = p.n;
    return p.b;
}

// The 3x3 grid packs into 36 bits: one nibble of item type per cell, cell i
// (row-major) at bit 4*i. Normalisation and matching are then pure bit ops.
constexpr uint64_t GRID_COL0 = 0x00F00F00Full; // cells 0,3,6
constexpr uint64_t GRID_ROW0 = 0x0000000FFFull; // cells 0,1,2
constexpr uint64_t GRID_KEEP01 = 0x0FF0FF0FFull; // columns 0,1 of every row

// Compile-time recipe key from the same row-major digit strings as before
// ('0'-'9' and 'A'-'F' are item type nibbles).
static constexpr uint64_t craftKey(const char* s) {
    uint64_t v = 0;
    for(int i = 0; i < 9; i++) {
        uint64_t n = (s[i] >= 'A') ? (uint64_t)(s[i] - 'A' + 10) : (uint64_t)(s[i] - '0');
        v |= n << (4 * i);
    }
    return v;
}

// Result packed as type << 8 | count.
static constexpr uint16_t R(uint8_t type, uint8_t n) {
    return (uint16_t)((type << 8) | n);
}

struct CraftRecipe {
    uint64_t key;
    uint16_t result;
};
static constexpr CraftRecipe CRAFT_RECIPES[] = {
    {craftKey("770770000"), R(ITEM_TABLE, 1)},
    {craftKey("500000000"), R(ITEM_PLANK, 4)},
    {craftKey("444404444"), R(ITEM_FURNACE, 1)},
    {craftKey("777707777"), R(ITEM_CHEST, 1)},
    {craftKey("700700000"), R(ITEM_STICK, 4)},
    {craftKey("777010010"), R(ITEM_WOODPICKAXE, 1)},
    {craftKey("444010010"), R(ITEM_STONEPICKAXE, 1)},
    {craftKey("DDD010010"), R(ITEM_IRONPICKAXE, 1)},
    {craftKey("770710010"), R(ITEM_WOODAXE, 1)},
    {craftKey("440410010"), R(ITEM_STONEAXE, 1)},
    {craftKey("DD0D10010"), R(ITEM_IRONAXE, 1)},
    {craftKey("700100100"), R(ITEM_WOODSHOVEL, 1)},
    {craftKey("400100100"), R(ITEM_STONESHOVEL, 1)},
    {craftKey("D00100100"), R(ITEM_IRONSHOVEL, 1)},
    {craftKey("700700100"), R(ITEM_WOODSWORD, 1)},
    {craftKey("400400100"), R(ITEM_STONESWORD, 1)},
    {craftKey("D00D00100"), R(ITEM_IRONSWORD, 1)},
    {craftKey("0D0D00000"), R(ITEM_SHEARS, 1)},
    {craftKey("AB0000000"), R(ITEM_DYNAMITE, 1)},
    {craftKey("BA0000000"), R(ITEM_DYNAMITE, 1)},
    {craftKey("A00B00000"), R(ITEM_DYNAMITE, 1)},
    {craftKey("B00A00000"), R(ITEM_DYNAMITE, 1)},
};

uint16_t craftTable(const ItemCell grid[9]) {
    uint64_t v = 0;
    for(int i = 0; i < 9; i++)
        v |= (uint64_t)(grid[i].type >> 4) << (4 * i);
    if(!v) return 0;
    // Shift the pattern into the top-left corner: drop empty leading columns
    // (each row moves one nibble right, its last column cleared), then rows.
    while(!(v & GRID_COL0))
        v = (v >> 4) & GRID_KEEP01;
    while(!(v & GRID_ROW0))
        v >>= 12;
    for(const CraftRecipe& r : CRAFT_RECIPES)
        if(r.key == v) {
            // gunpowder shares the glass craft nibble; require the exact type
            if(r.result == R(ITEM_DYNAMITE, 1)) {
                bool ok = false;
                for(int i = 0; i < 9; i++)
                    ok |= grid[i].type == ITEM_GUNPOWDER;
                if(!ok) continue;
            }
            return r.result;
        }
    return 0;
}
uint16_t craftFurnace(uint8_t inputType) {
    switch(inputType >> 4) {
    case BLOCK_COBBLE:
        return R(ITEM_STONE, 1);
    case BLOCK_LOG:
        return R(ITEM_COAL, 1);
    case BLOCK_IRONORE:
        return R(ITEM_IRONINGOT, 1);
    case BLOCK_SAND:
        return R(ITEM_GLASS, 1);
    default:
        return 0;
    }
}

}
