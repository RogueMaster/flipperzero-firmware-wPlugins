#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>

// furi storage handles (opaque here; world.cpp uses the real storage API).
struct Storage;
struct File;

namespace flipcraft {

struct GameConfig {
    const char* worldDataPath = nullptr;
};

constexpr int BLOCKSIZE            = 16;
constexpr int CHUNK_SIZE           = 8;
constexpr int CHUNK_SHIFT          = 3;
constexpr int CHUNK_MASK           = CHUNK_SIZE - 1;
constexpr int RENDER_RADIUS_BLOCKS = 8;
constexpr int WORLD_CHUNKS_X       = 16;
constexpr int WORLD_CHUNKS_Z       = 16;
constexpr int WORLD_SX             = WORLD_CHUNKS_X * CHUNK_SIZE;
constexpr int WORLD_SY             = 16;
constexpr int WORLD_SZ             = WORLD_CHUNKS_Z * CHUNK_SIZE;
constexpr int WINDOW_CHUNKS        = 3;
constexpr int CHUNK_BLOCKS         = CHUNK_SIZE * WORLD_SY * CHUNK_SIZE;
constexpr int STORAGE_CAPACITY       = 256;
constexpr int STORAGE_SLOT_SIZE      = 32;
constexpr int STORAGE_PAD_V2         = 4096; // old v2 slot region, kept as migration source
constexpr int INVENTORY_REGION_SIZE  = 32;
constexpr int MAX_STACK              = 99;
constexpr int PLAYERWIDTH          = 9;
constexpr int PLAYERHEIGHT         = 28;
constexpr int PLAYERHALFWIDTH      = 5;
constexpr int PLAYERCAMHEIGHT      = 24;
constexpr int PICKUPDOWN           = 12;
constexpr int PICKUPUP             = 37;
constexpr int PICKUPSIDEPOS        = 28;
constexpr int PLAYERCROUCHCAMHEIGHT= 22;
constexpr int GRAVITY              = 15;
constexpr int JUMPSTRENGTH         = 22;
constexpr int VERT_SUBPIXEL        = 16;
constexpr int JUMP_AIRTIME         = 2;
constexpr int SPEEDFACTOR          = 0x40;
constexpr int RAYCASTMAXLENGTH     = 0x40;
constexpr int MAXHEALTH            = 8;
constexpr int HUD_LABEL_TICKS      = 10; // hotbar tooltip, ~0.8 s at the 80 ms tick
constexpr int APPLEHEALTH          = 2;
constexpr int MINFALLDAMAGESPEED   = 32;
constexpr int FALLDAMAGESCALING    = 0x08;
constexpr int RANDOMTICKSPEED      = 10;
constexpr int SMELTTIME            = 0xC0;
constexpr int MAX_MOBS             = 6;
constexpr int MOBWIDTH             = 14;
constexpr int MOB_HURT_TICKS      = 12;  // ~1 s of damage flash at the 80 ms tick
constexpr int MOB_ATTACK_COOL     = 12;  // ticks between touch attacks
constexpr int MOB_FUSE_TICKS      = 26;  // ~2 s exploder wind-up, flashes and swells
constexpr int MOB_BLAST_RANGE     = 40;  // 2.5 blocks, Chebyshev, in sub-pixels
constexpr int MOB_BLAST_DMG       = 7;   // ~90% of MAXHEALTH
constexpr int MOB_DEADZONE        = 12;  // sub-px a chase target may stray before re-aim
constexpr int MOB_RETARGET_TICKS  = 6;   // ~0.5 s reaction delay between re-aims
constexpr int DYNAMITE_FUSE_TICKS = 38;  // ~3 s at the 80 ms tick
constexpr int LEAVES_SAPLING_PROBABILITY = 50;
constexpr int LEAVES_STICK_PROBABILITY   = 70;
constexpr int LEAVES_APPLE_PROBABILITY   = 80;
constexpr int LEAF_LOG_RADIUS      = 3;

static_assert(CHUNK_SIZE == 8, "block addressing assumes 8-block chunks");
static_assert((1 << CHUNK_SHIFT) == CHUNK_SIZE, "CHUNK_SHIFT must match CHUNK_SIZE");
static_assert(CHUNK_MASK == 7, "CHUNK_MASK must match CHUNK_SIZE");
static_assert(RENDER_RADIUS_BLOCKS <= ((WINDOW_CHUNKS - 1) / 2) * CHUNK_SIZE,
    "render radius exceeds the resident chunk ring; raise WINDOW_CHUNKS too");

struct ActiveWindow { int x0, x1, z0, z1; };
inline ActiveWindow activeWindowAround(int blockX, int blockZ, int worldSX = WORLD_SX, int worldSZ = WORLD_SZ) {
    ActiveWindow w;
    w.x0 = blockX - RENDER_RADIUS_BLOCKS; if (w.x0 < 0) w.x0 = 0;
    w.x1 = blockX + RENDER_RADIUS_BLOCKS; if (w.x1 > worldSX - 1) w.x1 = worldSX - 1;
    w.z0 = blockZ - RENDER_RADIUS_BLOCKS; if (w.z0 < 0) w.z0 = 0;
    w.z1 = blockZ + RENDER_RADIUS_BLOCKS; if (w.z1 > worldSZ - 1) w.z1 = worldSZ - 1;
    return w;
}

enum Block : uint8_t {
    BLOCK_AIR = 0x0, BLOCK_GRASS = 0x1, BLOCK_DIRT = 0x2, BLOCK_STONE = 0x3,
    BLOCK_COBBLE = 0x4, BLOCK_LOG = 0x5, BLOCK_LEAVES = 0x6, BLOCK_PLANK = 0x7,
    BLOCK_COALORE = 0x8, BLOCK_IRONORE = 0x9, BLOCK_SAND = 0xA, BLOCK_GLASS = 0xB,
    BLOCK_SAPLING = 0xC, BLOCK_TABLE = 0xD, BLOCK_FURNACE = 0xE, BLOCK_CHEST = 0xF,
    BLOCK_DYNAMITE = 0x10,
};

enum Item : uint8_t {
    ITEM_AIR = 0x00, ITEM_STICK = 0x10, ITEM_DIRT = 0x20, ITEM_STONE = 0x30,
    ITEM_COBBLE = 0x40, ITEM_LOG = 0x50, ITEM_LEAVES = 0x60, ITEM_PLANK = 0x70,
    ITEM_COAL = 0x80, ITEM_IRONORE = 0x90, ITEM_SAND = 0xA0, ITEM_GLASS = 0xB0,
    ITEM_SAPLING = 0xC0, ITEM_IRONINGOT = 0xD0, ITEM_APPLE = 0xE0,
    ITEM_NONSTACKABLE = 0xF0,

    ITEM_WOODPICKAXE = 0xF0, ITEM_WOODAXE = 0xF1, ITEM_WOODSHOVEL = 0xF2,
    ITEM_WOODSWORD = 0xF3, ITEM_STONEPICKAXE = 0xF4, ITEM_STONEAXE = 0xF5,
    ITEM_STONESHOVEL = 0xF6, ITEM_STONESWORD = 0xF7, ITEM_IRONPICKAXE = 0xF8,
    ITEM_IRONAXE = 0xF9, ITEM_IRONSHOVEL = 0xFA, ITEM_IRONSWORD = 0xFB,
    ITEM_SHEARS = 0xFC, ITEM_TABLE = 0xFD, ITEM_FURNACE = 0xFE, ITEM_CHEST = 0xFF,

    // Gunpowder shares glass's craft nibble (type>>4); dynamite's nibble is 0,
    // invisible to recipes.
    ITEM_GUNPOWDER = 0xB1,
    ITEM_DYNAMITE = 0x01,
};

struct ItemCell {
    uint8_t type = 0;  // Item enum value; materials keep the id in the high nibble
    uint8_t count = 0;
    bool empty() const { return type == 0; }
};

enum Entity : uint8_t {
    ENTITY_STICK = 0x1, ENTITY_DIRT = 0x2, ENTITY_APPLE = 0x3, ENTITY_COBBLE = 0x4,
    ENTITY_LOG = 0x5, ENTITY_LEAVES = 0x6, ENTITY_PLANK = 0x7, ENTITY_COAL = 0x8,
    ENTITY_IRONORE = 0x9, ENTITY_SAND = 0xA, ENTITY_FALLINGSAND = 0xB,
    ENTITY_SAPLING = 0xC, ENTITY_TABLE = 0xD, ENTITY_FURNACE = 0xE, ENTITY_CHEST = 0xF,
    ENTITY_DYNAMITE = 0x10, ENTITY_GUNPOWDER = 0x11, ENTITY_LITDYNAMITE = 0x12,
};

constexpr int TOOL_PICKAXE = 0, TOOL_AXE = 1, TOOL_SHOVEL = 2, TOOL_SWORD = 3;
constexpr int STRENGTHFORITEM = 3;
constexpr int STRENGTH_FIST = 4, STRENGTH_WOOD = 5, STRENGTH_STONE = 6, STRENGTH_IRON = 7;
// Block material types. Chosen so that a matching tool's low bits equal the
// type it is effective against: TOOL_PICKAXE==BLOCKTYPE_STONE, TOOL_AXE==
// BLOCKTYPE_WOOD, TOOL_SHOVEL==BLOCKTYPE_SOFT.
constexpr int BLOCKTYPE_STONE = 0, BLOCKTYPE_WOOD = 1, BLOCKTYPE_SOFT = 2,
              BLOCKTYPE_LEAVES = 3, BLOCKTYPE_GLASS = 4, BLOCKTYPE_SAPLING = 5;

// Per-block property bitmasks: bit N describes block id N (ids 0..31).
// "Transparent": a face of an adjacent full block is visible through it.
constexpr uint32_t BLOCKS_TRANSPARENT =
    (1u << BLOCK_AIR) | (1u << BLOCK_LEAVES) | (1u << BLOCK_SAPLING) |
    (1u << BLOCK_GLASS) | (1u << BLOCK_CHEST);
// "Full": renders as a full cube via face culling (everything except the
// mesh-quad blocks: air, sapling cross, small chest box).
constexpr uint32_t BLOCKS_NOT_FULL =
    (1u << BLOCK_AIR) | (1u << BLOCK_SAPLING) | (1u << BLOCK_CHEST);
// "Solid": collides with the player and stops falling items.
constexpr uint32_t BLOCKS_SOLID =
    ~((1u << BLOCK_AIR) | (1u << BLOCK_SAPLING));
// Entities that render as a small textured cube (the rest are cross sprites).
constexpr uint32_t ENTITIES_NOT_BLOCKITEM =
    (1u << ENTITY_STICK) | (1u << ENTITY_APPLE) | (1u << ENTITY_COAL) |
    (1u << ENTITY_FALLINGSAND) | (1u << ENTITY_SAPLING) | (1u << ENTITY_GUNPOWDER);

inline bool blockIsTransparent(uint8_t id) { return (BLOCKS_TRANSPARENT >> (id & 0x1F)) & 1u; }
inline bool blockIsFull(uint8_t id)        { return !((BLOCKS_NOT_FULL >> (id & 0x1F)) & 1u); }
inline bool blockIsSolid(uint8_t id)       { return (BLOCKS_SOLID >> (id & 0x1F)) & 1u; }
inline bool itemIsBlockItem(uint8_t id)    { return !((ENTITIES_NOT_BLOCKITEM >> (id & 0x1F)) & 1u); }

enum Texture : uint8_t {
    TEX_EMPTY = 0x00, TEX_COALITEMLIGHT = 0x01, TEX_GRASSSIDE = 0x02, TEX_DIRT = 0x03,
    TEX_STONE = 0x04, TEX_COBBLE = 0x05, TEX_LOGTOP = 0x06, TEX_LOGSIDE = 0x07,
    TEX_LEAVES = 0x08, TEX_PLANK = 0x09, TEX_COALORE = 0x0A, TEX_IRONORE = 0x0B,
    TEX_GLASS = 0x0C, TEX_SAPLINGLIGHT = 0x0D, TEX_SAPLINGDARK = 0x0E,
    TEX_TABLESIDE = 0x0F, TEX_TABLETOP = 0x10, TEX_FURNACESIDE = 0x11,
    TEX_FURNACETOP = 0x12, TEX_FURNACEFRONTOFF = 0x13, TEX_FURNACEFRONTON = 0x14,
    TEX_CHESTSIDE = 0x15, TEX_CHESTTOP = 0x16, TEX_CHESTFRONT = 0x17,
    TEX_COALITEMDARK = 0x18, TEX_STICKITEMLIGHT = 0x19, TEX_STICKITEMDARK = 0x1A,
    TEX_APPLEITEMLIGHT = 0x1B, TEX_APPLEITEMDARK = 0x1C, TEX_SHADOW = 0x1D,
    TEX_BREAK0 = 0x65,
    TEX_SHEEPFRONT = 0x90, TEX_SHEEPSIDE = 0x91, TEX_SHEEPTOP = 0x92,
    TEX_WOLFFRONT = 0x93, TEX_WOLFSIDE = 0x94, TEX_WOLFTOP = 0x95,
    TEX_CREEPERFRONT = 0x96, TEX_CREEPERSIDE = 0x97, TEX_CREEPERTOP = 0x98,
    TEX_DYNAMITE = 0x99, TEX_DYNAMITETOP = 0x9A,
    TEX_BEEFRONT = 0x9B, TEX_BEESIDE = 0x9C, TEX_BEETOP = 0x9D,
};

enum Quad : uint8_t {
    QUAD_FULL_NEGX = 0x00, QUAD_FULL_POSX = 0x01, QUAD_FULL_NEGZ = 0x02,
    QUAD_FULL_POSZ = 0x03, QUAD_FULL_NEGY = 0x04, QUAD_FULL_POSY = 0x05,
    QUAD_CROSS1 = 0x06, QUAD_CROSS2 = 0x07,
    QUAD_SMALL_NEGX = 0x08, QUAD_SMALL_POSX = 0x09, QUAD_SMALL_NEGZ = 0x0A,
    QUAD_SMALL_POSZ = 0x0B, QUAD_SMALL_NEGY = 0x0C, QUAD_SMALL_POSY = 0x0D,
    QUAD_ITEMSHADOW = 0x0E,
    QUAD_BLOCKITEM_NEGX = 0x0F, QUAD_BLOCKITEM_POSX = 0x10, QUAD_BLOCKITEM_NEGZ = 0x11,
    QUAD_BLOCKITEM_POSZ = 0x12, QUAD_BLOCKITEM_NEGY = 0x13, QUAD_BLOCKITEM_POSY = 0x14,
    QUAD_CROSSITEM1 = 0x15, QUAD_CROSSITEM2 = 0x16, QUAD_CROSSITEM3 = 0x17,
    QUAD_CROSSITEM4 = 0x18, QUAD_BEDROCK = 0x19,
    QUAD_COUNT = 0x1A,
};

// Texture settings nibble, shared by mesh tables and the rasterizer.
constexpr uint8_t TS_CULLBACK    = 0b1000;
constexpr uint8_t TS_TRANSPARENT = 0b0100;
constexpr uint8_t TS_INVERTED    = 0b0010;
constexpr uint8_t TS_OVERLAY     = 0b0001;

constexpr int SCREEN_WIDTH  = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr int UI_WIDTH      = 96;
constexpr int UI_X_OFFSET   = (SCREEN_WIDTH - UI_WIDTH) / 2;
constexpr int LENS          = 56;
constexpr int CLIP          = 3;

constexpr float BOB_SPEED        = 0.35f;
constexpr float BOB_EASE         = 0.20f;
constexpr float CAM_BOB_AMPLITUDE= 1.3f;

// floor(x) -> int without a libm call. vcvt truncates toward zero (1 cycle on
// M4F), so correct downward for negatives that have a fractional part.
inline int ifloor(float x) {
    int i = (int)x;
    return ((float)i > x) ? i - 1 : i;
}

inline uint8_t u8(int v) { return (uint8_t)(v & 0xFF); }
inline int8_t  s8(int v) { return (int8_t)(uint8_t)(v & 0xFF); }

struct World {
    uint8_t slot[WINDOW_CHUNKS][WINDOW_CHUNKS][WORLD_SY][CHUNK_SIZE][CHUNK_SIZE];
    int     slotCX[WINDOW_CHUNKS][WINDOW_CHUNKS];
    int     slotCZ[WINDOW_CHUNKS][WINDOW_CHUNKS];
    int     slotMaxY[WINDOW_CHUNKS][WINDOW_CHUNKS];
    bool    slotDirty[WINDOW_CHUNKS][WINDOW_CHUNKS];
    // Bumped whenever a slot's visible content may have changed (block edit,
    // chunk (re)load, or an edit on a shared face of a neighbouring chunk).
    // The renderer compares it against its cached mesh and rebuilds lazily.
    uint16_t slotGen[WINDOW_CHUNKS][WINDOW_CHUNKS];

    int     centerCX = -2, centerCZ = -2;
    bool    loadPending = false; // chunks of the current ring still on disk
    uint32_t revision = 0;

    ::Storage* storage = nullptr;
    ::File*    file = nullptr;
    bool    opened = false;
    int     chunksX = WORLD_CHUNKS_X, chunksZ = WORLD_CHUNKS_Z;

    int      hdrPX = 0, hdrPY = 0, hdrPZ = 0;
    uint8_t  hdrRot = 0x08;
    uint32_t hdrRng = 0x1234;
    bool     existed = false;
    uint16_t hdrVersion = 1;

    int worldSX() const { return chunksX * CHUNK_SIZE; }
    int worldSZ() const { return chunksZ * CHUNK_SIZE; }

    uint8_t getBlock(int x, int y, int z) const {
        if ((unsigned)x < (unsigned)worldSX() && (unsigned)y < (unsigned)WORLD_SY &&
            (unsigned)z < (unsigned)worldSZ()) {
            int cx = x >> CHUNK_SHIFT, cz = z >> CHUNK_SHIFT, sx = cx % 3, sz = cz % 3;
            if (slotCX[sx][sz] == cx && slotCZ[sx][sz] == cz)
                return slot[sx][sz][y][z & CHUNK_MASK][x & CHUNK_MASK];
        }
        return BLOCK_AIR;
    }

    // Invalidate the cached mesh of chunk (cx,cz) if it is resident.
    void bumpGen(int cx, int cz) {
        if ((unsigned)cx >= (unsigned)chunksX || (unsigned)cz >= (unsigned)chunksZ) return;
        int sx = cx % 3, sz = cz % 3;
        if (slotCX[sx][sz] == cx && slotCZ[sx][sz] == cz) slotGen[sx][sz]++;
    }

    void setBlock(int x, int y, int z, uint8_t id) {
        if ((unsigned)x >= (unsigned)worldSX() || (unsigned)y >= (unsigned)WORLD_SY ||
            (unsigned)z >= (unsigned)worldSZ())
            return;
        int cx = x >> CHUNK_SHIFT, cz = z >> CHUNK_SHIFT, sx = cx % 3, sz = cz % 3;
        if (slotCX[sx][sz] != cx || slotCZ[sx][sz] != cz) return;
        uint8_t& cell = slot[sx][sz][y][z & CHUNK_MASK][x & CHUNK_MASK];
        if (cell == id) return;
        cell = id;
        revision++;
        slotDirty[sx][sz] = true;
        slotGen[sx][sz]++;
        // Edits on a chunk border also change which faces the neighbour shows.
        int lx = x & CHUNK_MASK, lz = z & CHUNK_MASK;
        if (lx == 0) bumpGen(cx - 1, cz); else if (lx == CHUNK_MASK) bumpGen(cx + 1, cz);
        if (lz == 0) bumpGen(cx, cz - 1); else if (lz == CHUNK_MASK) bumpGen(cx, cz + 1);
        if (id != BLOCK_AIR) {
            if (y > slotMaxY[sx][sz]) slotMaxY[sx][sz] = y;
        } else if (y == slotMaxY[sx][sz]) {
            int maxY = -1;
            for (int yy = WORLD_SY - 1; yy >= 0 && maxY < 0; yy--)
                for (int zz = 0; zz < CHUNK_SIZE && maxY < 0; zz++)
                    for (int xx = 0; xx < CHUNK_SIZE; xx++)
                        if (slot[sx][sz][yy][zz][xx] != BLOCK_AIR) {
                            maxY = yy;
                            break;
                        }
            slotMaxY[sx][sz] = maxY;
        }
    }

    bool openWorld(const char* dataPath);
    // Keeps the 3x3 chunk ring around the player resident. In the normal
    // (streaming) mode it loads at most one chunk per call so SD latency is
    // spread across ticks instead of stalling one frame; `immediate` loads
    // everything synchronously (setup, respawn/teleport).
    void updateWindow(int blockX, int blockZ, bool immediate = false);
    void save();
    void closeWorld(int px, int py, int pz, uint8_t rot, uint32_t rng);
    bool readInventory(uint8_t* dst, uint32_t n);
    void writeInventory(const uint8_t* src, uint32_t n);
    bool readStorageBatch(int first, int count, uint8_t* dst);
    bool readStorageSlot(int index, uint8_t* dst);
    void writeStorageSlot(int index, const uint8_t* src);

private:
    bool tryOpenAndReadHeader(const char* path);
    void migrateV2();
    bool loadChunkDirect(int cx, int cz);
    bool loadRunStaged(int cx0, int cz, int count);
    void onSlotLoaded(int cx, int cz);
    bool flushSlot(int sx, int sz);
    bool ensureRegion();
};

// Bit 0 of each byte is the pixel colour; the 3D rasterizer keeps its z-depth
// in bits 1-7 of the same byte (see Renderer::zbuf), 2D UI writes plain 0/1.
struct Framebuffer {
    uint8_t px[SCREEN_HEIGHT][SCREEN_WIDTH];
    void clear() { for (auto& row : px) for (auto& p : row) p = 0; }
};

const char* itemName(uint8_t type);

// 8-byte row-packed 8x8 texture: bit `u` of byte `v` is texel (u, v).
const uint8_t* texturePacked(int texId);
const int (*quadTemplate(int quadId))[3];

struct MeshTex { uint8_t id; uint8_t settings; };
struct MeshQuadRef { uint8_t quadId; uint8_t texIndex; };

struct MeshEntry {
    MeshTex textures[4];
    MeshQuadRef quads[8];
    uint8_t texCount = 0;
    uint8_t quadCount = 0;
    bool exists = false;
};
const MeshEntry& meshBlock(uint8_t blockId);
const MeshEntry& meshItem(uint8_t blockOrItemHighNibbleId);

constexpr uint8_t MOB_SHEEP = 0, MOB_WOLF = 1, MOB_CREEPER = 2, MOB_BEE = 3,
                  MOB_SPECIES = 4;
constexpr uint8_t MOB_IDLE = 0, MOB_WANDER = 1, MOB_CHASE = 2, MOB_FLEE = 3;
constexpr uint8_t TEMPER_PASSIVE = 0, TEMPER_NEUTRAL = 1, TEMPER_HOSTILE = 2;

struct MobSpec {
    uint8_t texFront, texSide, texTop;
    uint8_t prey;    // bitmask of species this one hunts (1 << species)
    uint8_t info;    // bit0 exploder | temperament << 1 | bit3 flyer
    uint8_t hpDmg;   // max HP << 4 | touch damage (0 = never bites)
    uint8_t geom;    // (collision height / 2) << 4 | speed, px per tick
};
const MobSpec& mobSpec(uint8_t species);

// Box in mob-local px, origin at the feet centre, +Z forward; rotated to the
// facing at render time. flags bit0: front of this box wears texFront.
struct MobBox { int8_t ox, oy, oz; uint8_t sx, sy, sz, flags; };
const MobBox* mobBoxes(uint8_t species, int& count);

// Result packed as type << 8 | count, 0 = no recipe.
uint16_t craftTable(const ItemCell grid[9]);
uint16_t craftFurnace(uint8_t inputType);

}
