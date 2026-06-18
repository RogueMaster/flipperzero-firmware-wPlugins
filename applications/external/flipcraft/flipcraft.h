#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <array>

namespace flipcraft {

enum class FileMode {
    Read,
    WriteTruncate,
    ReadWriteExisting,
};

struct FileSystem {
    void* ctx = nullptr;
    void* (*open)(void* ctx, const char* path, FileMode mode) = nullptr;
    void (*close)(void* ctx, void* file) = nullptr;
    bool (*seek)(void* ctx, void* file, uint32_t offset) = nullptr;
    size_t (*read)(void* ctx, void* file, void* data, size_t size) = nullptr;
    size_t (*write)(void* ctx, void* file, const void* data, size_t size) = nullptr;
    uint32_t (*size)(void* ctx, void* file) = nullptr;
    void (*sync)(void* ctx, void* file) = nullptr;
};

struct GameConfig {
    const FileSystem* files = nullptr;
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

// Persisted storage region (after the world chunk array). Each chest/furnace
// owns one fixed-size slot addressed by an explicit index: contents are loaded
// on demand and written on close. STORAGE_CAPACITY caps the number of storages.
constexpr int STORAGE_CAPACITY       = 256;
constexpr int STORAGE_SLOT_SIZE      = 16;  // bytes per storage slot on disk
constexpr int INVENTORY_REGION_SIZE  = 32;  // [0]=valid magic, [1..]=payload
constexpr int PLAYERWIDTH          = 9;
constexpr int PLAYERHEIGHT         = 28;
constexpr int PLAYERHALFWIDTH      = 5;
constexpr int PLAYERCAMHEIGHT      = 24;
constexpr int PICKUPDOWN           = 12;
constexpr int PICKUPUP             = 37;
constexpr int PICKUPSIDENEG        = 22;
constexpr int PICKUPSIDEPOS        = 28;
constexpr int PLAYERCROUCHCAMHEIGHT= 22;
constexpr int MIDDLEOFVOID         = 0xA0;
constexpr int BLOCKMIDDLEOFVOID    = 0xC;
constexpr int GRAVITY              = 15;
constexpr int JUMPSTRENGTH         = 17;
constexpr int SPEEDFACTOR          = 0x40;
constexpr int RAYCASTMAXLENGTH     = 0x40;
constexpr int BREAKTIME            = 24;  
constexpr int MAXHEALTH            = 8;
constexpr int APPLEHEALTH          = 2;
constexpr int MINFALLDAMAGESPEED   = 32;
constexpr int FALLDAMAGESCALING    = 0x08;
constexpr int RANDOMTICKSPEED      = 10;
constexpr int SMELTTIME            = 0xC0;
constexpr int LEAVES_SAPLING_PROBABILITY = 50; 
constexpr int LEAVES_STICK_PROBABILITY   = 70; 
constexpr int LEAVES_APPLE_PROBABILITY   = 80; 

static_assert(CHUNK_SIZE == 8, "getBlock fast path assumes 8-block chunks");
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
};

enum Entity : uint8_t {
    ENTITY_STICK = 0x1, ENTITY_DIRT = 0x2, ENTITY_APPLE = 0x3, ENTITY_COBBLE = 0x4,
    ENTITY_LOG = 0x5, ENTITY_LEAVES = 0x6, ENTITY_PLANK = 0x7, ENTITY_COAL = 0x8,
    ENTITY_IRONORE = 0x9, ENTITY_SAND = 0xA, ENTITY_FALLINGSAND = 0xB,
    ENTITY_SAPLING = 0xC, ENTITY_TABLE = 0xD, ENTITY_FURNACE = 0xE, ENTITY_CHEST = 0xF,
};

constexpr int TOOL_PICKAXE = 0, TOOL_AXE = 1, TOOL_SHOVEL = 2, TOOL_SWORD = 3;
constexpr int STRENGTHFORITEM = 3;
constexpr int STRENGTH_FIST = 4, STRENGTH_WOOD = 5, STRENGTH_STONE = 6, STRENGTH_IRON = 7;
constexpr int BLOCKTYPE_STONE = 0, BLOCKTYPE_WOOD = 1, BLOCKTYPE_SOFT = 2,
              BLOCKTYPE_LEAVES = 3, BLOCKTYPE_GLASS = 4, BLOCKTYPE_SAPLING = 5;

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
    TEX_CHECKER = 0x30,
    TEX_BREAK0 = 0x65, TEX_BREAK5 = 0x6A, TEX_BREAK7 = 0x6C,
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
    QUAD_NONE = 0x1F, 
};

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

// Quantize to a fixed-point grid (bits wide, `precision` fractional bits) and
// return as float. All inputs use bits <= 17, precision <= 16, so int32 is
// enough -- no int64, no floorf. Semantics match the previous int64 version.
inline float FixedPoint(float value, int bits, int precision, bool sgned = false) {
    int32_t shiftamount = int32_t(1) << precision;
    int32_t bitmask = (int32_t(1) << bits) - 1;
    int32_t v = ifloor(value * (float)shiftamount) & bitmask;
    if (sgned && v > (int32_t(1) << (bits - 1)))
        v -= (int32_t(1) << bits);
    return (float)v / (float)shiftamount;
}

inline uint8_t u8(int v) { return (uint8_t)(v & 0xFF); }
inline int8_t  s8(int v) { return (int8_t)(uint8_t)(v & 0xFF); }

inline uint8_t addv(uint8_t a, uint8_t b) {
    int lo = ((a & 0xF) + (b & 0xF)) & 0xF;
    int hi = ((a >> 4) + (b >> 4)) & 0xF;
    return (uint8_t)((hi << 4) | lo);
}

struct World {
    uint8_t slot[WINDOW_CHUNKS][WINDOW_CHUNKS][WORLD_SY][CHUNK_SIZE][CHUNK_SIZE];
    int     slotCX[WINDOW_CHUNKS][WINDOW_CHUNKS];
    int     slotCZ[WINDOW_CHUNKS][WINDOW_CHUNKS];
    int     slotMaxY[WINDOW_CHUNKS][WINDOW_CHUNKS];
    bool    slotDirty[WINDOW_CHUNKS][WINDOW_CHUNKS];

    int     centerCX = -2, centerCZ = -2;

    const FileSystem* fs = nullptr;
    void*   file = nullptr;
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
    void setBlock(int x, int y, int z, uint8_t id) {
        if ((unsigned)x < (unsigned)worldSX() && (unsigned)y < (unsigned)WORLD_SY &&
            (unsigned)z < (unsigned)worldSZ()) {
            int cx = x >> CHUNK_SHIFT, cz = z >> CHUNK_SHIFT, sx = cx % 3, sz = cz % 3;
            if (slotCX[sx][sz] == cx && slotCZ[sx][sz] == cz) {
                slot[sx][sz][y][z & CHUNK_MASK][x & CHUNK_MASK] = id;
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
                slotDirty[sx][sz] = true;
            }
        }
    }
    int activeMaxY(const ActiveWindow& win) const {
        int x0 = win.x0 >> CHUNK_SHIFT, x1 = win.x1 >> CHUNK_SHIFT;
        int z0 = win.z0 >> CHUNK_SHIFT, z1 = win.z1 >> CHUNK_SHIFT;
        int maxY = 0;
        for (int cz = z0; cz <= z1; cz++)
            for (int cx = x0; cx <= x1; cx++) {
                int sx = cx % 3, sz = cz % 3;
                if (slotCX[sx][sz] == cx && slotCZ[sx][sz] == cz && slotMaxY[sx][sz] > maxY)
                    maxY = slotMaxY[sx][sz];
            }
        return maxY;
    }

    bool openWorld(const FileSystem& files, const char* dataPath);
    void updateWindow(int blockX, int blockZ);
    void save();
    void closeWorld(int px, int py, int pz, uint8_t rot, uint32_t rng);
    bool readInventory(uint8_t* dst, uint32_t n);
    void writeInventory(const uint8_t* src, uint32_t n);
    bool readStorageBatch(int first, int count, uint8_t* dst);
    bool readStorageSlot(int index, uint8_t* dst);
    void writeStorageSlot(int index, const uint8_t* src);

private:
    bool tryOpenAndReadHeader(const char* path);
    bool loadChunkDirect(int cx, int cz);
    bool loadRunStaged(int cx0, int cz, int count);
    bool flushSlot(int sx, int sz);
    bool ensureRegion();
};

struct Framebuffer {
    uint8_t px[SCREEN_HEIGHT][SCREEN_WIDTH];
    void clear() { for (auto& row : px) for (auto& p : row) p = 0; }
};

const uint8_t* textureBitmap(int texId);
const int (*quadTemplate(int quadId))[3];
bool blockIsTransparent(uint8_t id);
bool blockIsFull(uint8_t id);
bool itemIsBlockItem(uint8_t entityId);

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

uint16_t craftTable(const uint8_t grid[9]);
uint16_t craftFurnace(uint8_t input);

}
