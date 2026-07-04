#pragma once
//
// Flipcraft world generator core (from tools/worldgen.py). Self-contained
// (stdint + math), all `static`; include from one TU. Also builds on host
// via tools/hostgen.cpp.
//
// Worlds up to 1024x1024 blocks are generated in 128x128-block tiles so RAM
// stays bounded: per tile a 1-byte-per-column height/biome map (16KB) and a
// few small feature lists, then each chunk is filled and written at its file
// offset. Terrain is a pure function of global coords (noise scaled by the
// fixed tile size, not the world size), so tiles join seamlessly and a given
// seed looks identical at any world size.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

namespace fcgen {

constexpr int CHUNK = 8, HEIGHT = 16;
constexpr int TILE_CHUNKS = 16, TILE_W = TILE_CHUNKS * CHUNK; // 128 blocks
constexpr int MAX_CHUNKS = 128;                               // 1024 blocks
constexpr int CHUNK_BYTES = CHUNK * HEIGHT * CHUNK;           // 1024
constexpr int BLOCKSIZE = 16;
constexpr uint32_t HEADER_SIZE = 64;

enum : uint8_t {
    AIR = 0, GRASS = 1, DIRT = 2, STONE = 3, COBBLE = 4, LOG = 5, LEAVES = 6,
    PLANK = 7, COALORE = 8, IRONORE = 9, SAND = 10,
    TABLE = 13, FURNACE = 14, CHEST = 15, // engine Block ids (flipcraft.h)
};

// v3 tile-entity region layout, must match world.cpp/game.cpp packStorage()
constexpr uint32_t INV_REGION = 32, PAD_V2 = 4096;
constexpr int SLOT_CAP = 256, SLOT_SIZE = 32;

// Random-access sink: tiles are generated depth-first, so chunk payloads are
// written at explicit file offsets.
struct Writer {
    bool (*writeAt)(void* ctx, uint32_t offset, const void* data, size_t n);
    void* ctx;
};
typedef void (*Progress)(void* ctx, uint8_t percent);

// world-hash / noise: hashes match worldgen.py bit-for-bit.

static uint32_t g_seed;
static int g_worldW;         // world size in blocks
static int g_tileX0, g_tileZ0, g_tileW; // current tile origin/size in blocks

static uint32_t whash(int32_t x, int32_t z, uint32_t salt) {
    salt = salt + g_seed * 1013904223u;
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u + salt * 362437u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return h;
}

// Python derives the gradient angle from 16 hash bits; a 256-entry unit-vector
// table (top 8 of those bits) is visually identical and avoids sinf/cosf in
// the inner loop.
static float g_grad[256][2];
static bool g_gradReady = false;

static void gradInit() {
    if(g_gradReady) return;
    for(int i = 0; i < 256; i++) {
        float a = (float)i * (6.28318530718f / 256.0f);
        g_grad[i][0] = cosf(a);
        g_grad[i][1] = sinf(a);
    }
    g_gradReady = true;
}

static float fadef(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float smoothstepf(float e0, float e1, float x) {
    x = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static int ifloorf(float x) {
    int i = (int)x;
    return ((float)i > x) ? i - 1 : i;
}

static float perlin(float x, float z, uint32_t salt) {
    int x0 = ifloorf(x), z0 = ifloorf(z);
    float fx = x - x0, fz = z - z0;
    float sx = fadef(fx), sz = fadef(fz);

    const float* g00 = g_grad[(whash(x0, z0, salt) >> 8) & 0xFF];
    const float* g10 = g_grad[(whash(x0 + 1, z0, salt) >> 8) & 0xFF];
    const float* g01 = g_grad[(whash(x0, z0 + 1, salt) >> 8) & 0xFF];
    const float* g11 = g_grad[(whash(x0 + 1, z0 + 1, salt) >> 8) & 0xFF];

    float n00 = g00[0] * fx + g00[1] * fz;
    float n10 = g10[0] * (fx - 1) + g10[1] * fz;
    float n01 = g01[0] * fx + g01[1] * (fz - 1);
    float n11 = g11[0] * (fx - 1) + g11[1] * (fz - 1);
    return lerpf(lerpf(n00, n10, sx), lerpf(n01, n11, sx), sz) * 1.41421356237f;
}

static float fbm(float x, float z, uint32_t salt, int octaves) {
    float total = 0, amp = 1, norm = 0;
    for(int i = 0; i < octaves; i++) {
        total += perlin(x, z, salt + (uint32_t)i * 1013u) * amp;
        norm += amp;
        x *= 2.0f;
        z *= 2.0f;
        amp *= 0.5f;
    }
    return total / norm;
}

static float ridged(float x, float z, uint32_t salt, int octaves) {
    float a = fabsf(fbm(x, z, salt, octaves)) * 2.4f;
    return 1.0f - (a > 1.0f ? 1.0f : a);
}

// One byte per column: bits 0-3 terrain top y (1..10), then biome flags.
// A pure function of global (x, z), cached per tile.

constexpr uint8_t COL_TOP = 0x0F, COL_DESERT = 0x10, COL_RAVINE = 0x20, COL_FOREST = 0x40;

static uint8_t g_col[TILE_W * TILE_W];

static uint8_t colAt(int x, int z) { return g_col[(z - g_tileZ0) * g_tileW + (x - g_tileX0)]; }
static int topAt(int x, int z) { return colAt(x, z) & COL_TOP; }
static bool sandColumn(uint8_t c) { return (c & (COL_DESERT | COL_RAVINE)) || (c & COL_TOP) <= 2; }

static uint8_t computeColumn(int x, int z) {
    constexpr float S = 1.0f / (float)TILE_W; // fixed landscape scale
    float u = x * S, v = z * S;

    float warpX = fbm(u * 3.0f + 17.0f, v * 3.0f - 11.0f, 20, 3) * 18.0f;
    float warpZ = fbm(u * 3.0f - 7.0f, v * 3.0f + 23.0f, 21, 3) * 18.0f;
    float px = (x + warpX) * S, pz = (z + warpZ) * S;

    float continent = fbm(px * 2.2f, pz * 2.2f, 1, 5);
    float detail = fbm(px * 7.5f + 9.0f, pz * 7.5f - 4.0f, 2, 3) * 0.55f;
    float hillField = fbm(px * 3.2f - 13.0f, pz * 3.2f + 5.0f, 3, 4);
    float hillMask = smoothstepf(0.04f, 0.31f, hillField);
    float hill = hillMask * smoothstepf(-0.08f, 0.32f, continent + detail) * 6.2f;

    float valley = ridged(px * 3.0f + 2.5f, pz * 3.0f - 1.5f, 4, 4);
    float valleyGate = smoothstepf(0.04f, 0.38f, fbm(px * 1.5f + 4.0f, pz * 1.5f - 6.0f, 5, 3));
    int ravineDepth = (int)(smoothstepf(0.72f, 0.92f, valley) * valleyGate * 4.0f + 0.5f);

    float moisture = fbm(px * 2.4f - 8.0f, pz * 2.4f + 19.0f, 6, 5);
    // West-east gradient spans the whole world, whatever its size.
    float temperature = fbm(px * 1.8f + 31.0f, pz * 1.8f - 17.0f, 7, 4) +
                        ((float)x / (float)g_worldW - 0.5f) * 0.35f;
    float desertScore =
        temperature * 0.68f - moisture * 0.72f + fbm(px * 5.0f, pz * 5.0f, 8, 3) * 0.24f;
    bool desert = desertScore > 0.18f && ravineDepth < 3;

    float forestScore = moisture * 0.78f - fabsf(temperature) * 0.22f +
                        fbm(px * 6.0f + 5.0f, pz * 6.0f, 9, 3) * 0.18f;
    bool forest = forestScore > 0.02f && !desert && ravineDepth == 0;

    float topF = 4.4f + continent * 1.6f + detail + hill - (float)ravineDepth;
    int top = (int)(topF + 0.5f);
    if(top < 1) top = 1;
    if(top > 10) top = 10;

    return (uint8_t)(top | (desert ? COL_DESERT : 0) | (ravineDepth > 0 ? COL_RAVINE : 0) |
                     (forest ? COL_FOREST : 0));
}

// Callers stay >= 1 column inside the tile (feature margins), so the 3x3
// neighbourhood never leaves the cached map.
static int localSlope(int x, int z) {
    int h = topAt(x, z), s = 0;
    for(int dz = -1; dz <= 1; dz++)
        for(int dx = -1; dx <= 1; dx++) {
            int d = h - topAt(x + dx, z + dz);
            if(d < 0) d = -d;
            if(d > s) s = d;
        }
    return s;
}

// The Python script scores every eligible column, sorts them all and picks
// greedily with a minimum spacing. Storing every candidate does not fit, so we
// keep the best candidate of each small map cell (the score fields are smooth,
// so cell maxima are the columns a global sort would visit first) and run the
// same greedy spacing pass over those. Coordinates are tile-local (0..127).

struct Cand { float score; uint8_t x, z; };
constexpr int MAX_CANDS = 1024; // 4x4 cells on a 128x128 tile
static Cand g_cand[MAX_CANDS];

static int collectCellMaxima(
    int cellSize,
    int lo,
    int hi, // inclusive tile-local range in both axes
    float minScore,
    bool (*eligible)(int x, int z), // global coords
    float (*score)(int x, int z)) {
    int cells = g_tileW / cellSize;
    int n = cells * cells;
    for(int i = 0; i < n; i++) g_cand[i].score = -1e30f;
    for(int lz = lo; lz <= hi; lz++)
        for(int lx = lo; lx <= hi; lx++) {
            int x = g_tileX0 + lx, z = g_tileZ0 + lz;
            if(!eligible(x, z)) continue;
            float s = score(x, z);
            if(s <= minScore) continue;
            Cand& c = g_cand[(lz / cellSize) * cells + (lx / cellSize)];
            if(s > c.score) c = {s, (uint8_t)lx, (uint8_t)lz};
        }
    // compact + insertion sort by score, descending (n <= 1024)
    int m = 0;
    for(int i = 0; i < n; i++)
        if(g_cand[i].score > -1e29f) g_cand[m++] = g_cand[i];
    for(int i = 1; i < m; i++) {
        Cand key = g_cand[i];
        int j = i - 1;
        while(j >= 0 && g_cand[j].score < key.score) {
            g_cand[j + 1] = g_cand[j];
            j--;
        }
        g_cand[j + 1] = key;
    }
    return m;
}

struct Placed { uint8_t x, z, aux; }; // tile-local

static int greedyPlace(
    int candCount,
    int cap,
    int minDist2,
    bool (*fits)(int x, int z), // global coords
    Placed* out) {
    int placed = 0;
    for(int i = 0; i < candCount && placed < cap; i++) {
        int lx = g_cand[i].x, lz = g_cand[i].z;
        bool near = false;
        for(int p = 0; p < placed && !near; p++) {
            int dx = lx - out[p].x, dz = lz - out[p].z;
            near = dx * dx + dz * dz < minDist2;
        }
        if(near) continue;
        if(fits && !fits(g_tileX0 + lx, g_tileZ0 + lz)) continue;
        out[placed++] = {(uint8_t)lx, (uint8_t)lz, 0};
    }
    return placed;
}

static Placed g_trees[96];
static int g_treeCount;

static bool treeEligible(int x, int z) {
    return (colAt(x, z) & COL_FOREST) && localSlope(x, z) <= 1;
}
static float treeScore(int x, int z) {
    return fbm(x * 0.19f + 4.0f, z * 0.19f - 9.0f, 62, 3) +
           (float)(whash(x, z, 63) & 255) / 512.0f;
}
// place_tree succeeds only on a grass top with enough sky for the crown
static bool treeFits(int x, int z) {
    uint8_t c = colAt(x, z);
    if(sandColumn(c)) return false; // top block is SAND, not GRASS
    int top = c & COL_TOP;
    if(whash(x, z, 64) & 1) { // classic: trunk 3..4, crown+2 must fit
        int trunk = 3 + (int)(whash(x, z, 61) % 2);
        return top + trunk + 2 <= HEIGHT - 1;
    }
    int lower = top + 3 + (int)(whash(x, z, 61) & 1); // sapling-shaped
    return lower + 2 <= HEIGHT - 1;
}

constexpr int FALLEN_LEN = 4;
static Placed g_fallen[8];
static int g_fallenCount;

static bool clearGroundEligible(int x, int z) {
    uint8_t c = colAt(x, z);
    return !(c & (COL_DESERT | COL_RAVINE)) && localSlope(x, z) <= 1;
}
static float fallenScore(int x, int z) {
    return fbm(x * 0.11f, z * 0.11f, 70, 3) + (float)(whash(x, z, 71) & 255) / 700.0f;
}
static bool fallenFits(int x, int z) {
    bool alongX = (whash(x, z, 72) & 1) == 0;
    for(int i = 0; i < FALLEN_LEN; i++) {
        int tx = alongX ? x + i : x, tz = alongX ? z : z + i;
        if(tx - g_tileX0 < 1 || tx - g_tileX0 >= g_tileW - 1) return false;
        if(tz - g_tileZ0 < 1 || tz - g_tileZ0 >= g_tileW - 1) return false;
        if(colAt(tx, tz) & (COL_DESERT | COL_RAVINE)) return false;
    }
    return true;
}

static Placed g_piles[8];
static int g_pileCount;

static float pileScore(int x, int z) {
    return fbm(x * 0.13f + 11.0f, z * 0.13f - 3.0f, 80, 3);
}

static Placed g_houses[4]; // aux = floor y
static int g_houseCount;

static bool houseEligible(int x, int z) {
    int base = topAt(x + 2, z + 2);
    for(int dz = 0; dz < 5; dz++)
        for(int dx = 0; dx < 5; dx++) {
            uint8_t c = colAt(x + dx, z + dz);
            if(c & (COL_DESERT | COL_RAVINE)) return false;
            int d = (c & COL_TOP) - base;
            if(d < -1 || d > 1) return false;
        }
    return true;
}
static float houseScore(int x, int z) {
    return fbm(x * 0.08f + 19.0f, z * 0.08f - 7.0f, 90, 3) +
           (float)(whash(x, z, 91) & 255) / 900.0f;
}
// Houses are stamped after trees and would otherwise carve a placed trunk out
// with their interior, stranding the crown; keep them clear of tree canopies.
static bool houseFits(int x, int z) {
    int lx = x - g_tileX0, lz = z - g_tileZ0;
    for(int i = 0; i < g_treeCount; i++) {
        int tx = g_trees[i].x, tz = g_trees[i].z;
        if(tx >= lx - 2 && tx <= lx + 6 && tz >= lz - 2 && tz <= lz + 6) return false;
    }
    return true;
}

static void placeFeatures(bool allowHouse) {
    int w = g_tileW;
    int n;

    n = collectCellMaxima(4, 3, w - 4, 0.23f, treeEligible, treeScore);
    int treeCap = w * w / 190;
    if(treeCap < 24) treeCap = 24;
    if(treeCap > (int)(sizeof(g_trees) / sizeof(g_trees[0])))
        treeCap = sizeof(g_trees) / sizeof(g_trees[0]);
    g_treeCount = greedyPlace(n, treeCap, 36, treeFits, g_trees);

    n = collectCellMaxima(8, 4, w - 5, -1e29f, clearGroundEligible, fallenScore);
    g_fallenCount = greedyPlace(n, 5, 225, fallenFits, g_fallen);

    n = collectCellMaxima(8, 3, w - 4, -1e29f, clearGroundEligible, pileScore);
    g_pileCount = greedyPlace(n, 6, 196, nullptr, g_piles);

    g_houseCount = 0;
    if(allowHouse) { // the world gets a single house (see generate)
        n = collectCellMaxima(8, 5, w - 9, -1e29f, houseEligible, houseScore);
        g_houseCount = greedyPlace(n, 1, 625, houseFits, g_houses);
        if(g_houseCount)
            g_houses[0].aux =
                (uint8_t)topAt(g_tileX0 + g_houses[0].x + 2, g_tileZ0 + g_houses[0].z + 2);
    }
}

// Clipped block write into the chunk buffer. `allow` is a bitmask of block ids
// the cell may currently hold (the Python get_block()==AIR guards); pass
// ALLOW_ANY for unconditional writes.
constexpr uint16_t ALLOW_ANY = 0xFFFF;
constexpr uint16_t ALLOW_AIR = 1u << AIR;
constexpr uint16_t ALLOW_AIR_LEAVES = (1u << AIR) | (1u << LEAVES);

static void chSet(uint8_t* ch, int bx0, int bz0, int x, int y, int z, uint8_t id, uint16_t allow) {
    unsigned lx = (unsigned)(x - bx0), lz = (unsigned)(z - bz0);
    if(lx >= CHUNK || lz >= CHUNK || (unsigned)y >= HEIGHT) return;
    uint8_t* cell = &ch[(y * CHUNK + (int)lz) * CHUNK + (int)lx];
    if(allow != ALLOW_ANY && !((allow >> *cell) & 1)) return;
    *cell = id;
}

static void fillTerrain(uint8_t* ch, int bx0, int bz0) {
    memset(ch, AIR, CHUNK_BYTES);
    for(int lz = 0; lz < CHUNK; lz++)
        for(int lx = 0; lx < CHUNK; lx++) {
            int x = bx0 + lx, z = bz0 + lz;
            uint8_t c = colAt(x, z);
            int top = c & COL_TOP;
            bool sand = sandColumn(c);
            for(int y = 0; y <= top; y++) {
                uint8_t id;
                if(y == top)
                    id = sand ? SAND : GRASS;
                else if(y >= top - 2)
                    id = sand ? SAND : DIRT;
                else {
                    uint32_t r = whash(x * 17 + y, z, 40);
                    id = (r % 47 == 0) ? COALORE : (r % 67 == 0) ? IRONORE : STONE;
                }
                ch[(y * CHUNK + lz) * CHUNK + lx] = id;
            }
        }
}

static void stampTree(uint8_t* ch, int bx0, int bz0, int x, int z) {
    int top = topAt(x, z);
    if(whash(x, z, 64) & 1) { // classic: diamond crown
        int trunk = 3 + (int)(whash(x, z, 61) % 2);
        int crown = top + trunk;
        for(int dz = -2; dz <= 2; dz++)
            for(int dx = -2; dx <= 2; dx++) {
                int d = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
                if(d <= 3) chSet(ch, bx0, bz0, x + dx, crown, z + dz, LEAVES, ALLOW_AIR);
                if(d <= 2) chSet(ch, bx0, bz0, x + dx, crown + 1, z + dz, LEAVES, ALLOW_AIR);
            }
        chSet(ch, bx0, bz0, x, crown + 2, z, LEAVES, ALLOW_AIR);
        for(int ty = top + 1; ty < crown + 2; ty++)
            chSet(ch, bx0, bz0, x, ty, z, LOG, ALLOW_AIR_LEAVES);
    } else { // sapling-shaped: two square layers
        int y0 = top + 1;
        int lower = y0 + 2 + (int)(whash(x, z, 61) & 1);
        int upper = lower + 2;
        for(int ly = lower - 1; ly <= lower; ly++)
            for(int dx = -2; dx <= 2; dx++)
                for(int dz = -2; dz <= 2; dz++)
                    chSet(ch, bx0, bz0, x + dx, ly, z + dz, LEAVES, ALLOW_AIR);
        for(int ly = upper - 1; ly <= upper; ly++)
            for(int dx = -1; dx <= 1; dx++)
                for(int dz = -1; dz <= 1; dz++)
                    chSet(ch, bx0, bz0, x + dx, ly, z + dz, LEAVES, ALLOW_AIR);
        for(int ty = y0; ty < upper; ty++)
            chSet(ch, bx0, bz0, x, ty, z, LOG, ALLOW_AIR_LEAVES);
    }
}

static void stampFallen(uint8_t* ch, int bx0, int bz0, int x, int z) {
    bool alongX = (whash(x, z, 72) & 1) == 0;
    for(int i = 0; i < FALLEN_LEN; i++) {
        int tx = alongX ? x + i : x, tz = alongX ? z : z + i;
        chSet(ch, bx0, bz0, tx, topAt(tx, tz) + 1, tz, LOG, ALLOW_ANY);
    }
}

static void stampPile(uint8_t* ch, int bx0, int bz0, int x, int z) {
    static const int8_t P[5][3] = {
        {0, 0, STONE}, {1, 0, COBBLE}, {-1, 0, COBBLE}, {0, 1, COBBLE}, {0, -1, STONE}};
    for(const auto& p : P) {
        int tx = x + p[0], tz = z + p[1];
        chSet(ch, bx0, bz0, tx, topAt(tx, tz) + 1, tz, (uint8_t)p[2], ALLOW_ANY);
    }
    chSet(ch, bx0, bz0, x, topAt(x, z) + 2, z, COBBLE, ALLOW_ANY);
}

static void stampHouse(uint8_t* ch, int bx0, int bz0, int x, int z, int floorY) {
    for(int dz = 0; dz < 5; dz++)
        for(int dx = 0; dx < 5; dx++) {
            int tx = x + dx, tz = z + dz;
            chSet(ch, bx0, bz0, tx, floorY, tz, COBBLE, ALLOW_ANY);
            bool wall = dx == 0 || dx == 4 || dz == 0 || dz == 4;
            for(int y = floorY + 1; y <= floorY + 2; y++)
                chSet(ch, bx0, bz0, tx, y, tz, wall ? PLANK : AIR, ALLOW_ANY);
            chSet(ch, bx0, bz0, tx, floorY + 3, tz, PLANK, ALLOW_ANY);
        }
    chSet(ch, bx0, bz0, x + 2, floorY + 1, z, AIR, ALLOW_ANY); // doorway
    chSet(ch, bx0, bz0, x + 2, floorY + 2, z, AIR, ALLOW_ANY);
    chSet(ch, bx0, bz0, x + 1, floorY + 1, z + 3, TABLE, ALLOW_ANY);
    chSet(ch, bx0, bz0, x + 2, floorY + 1, z + 3, FURNACE, ALLOW_ANY);
    chSet(ch, bx0, bz0, x + 3, floorY + 1, z + 3, CHEST, ALLOW_ANY);
}

static bool bboxHitsChunk(int bx0, int bz0, int x0, int z0, int x1, int z1) {
    return x1 >= bx0 && x0 <= bx0 + CHUNK - 1 && z1 >= bz0 && z0 <= bz0 + CHUNK - 1;
}

// Stamp order matches the Python build order (trees, trunks, piles, houses),
// so replayed AIR checks see the same intermediate state per cell.
static void stampFeatures(uint8_t* ch, int bx0, int bz0) {
    const int ox = g_tileX0, oz = g_tileZ0;
    for(int i = 0; i < g_treeCount; i++) {
        int x = ox + g_trees[i].x, z = oz + g_trees[i].z;
        if(bboxHitsChunk(bx0, bz0, x - 2, z - 2, x + 2, z + 2)) stampTree(ch, bx0, bz0, x, z);
    }
    for(int i = 0; i < g_fallenCount; i++) {
        int x = ox + g_fallen[i].x, z = oz + g_fallen[i].z;
        if(bboxHitsChunk(bx0, bz0, x, z, x + FALLEN_LEN - 1, z + FALLEN_LEN - 1))
            stampFallen(ch, bx0, bz0, x, z);
    }
    for(int i = 0; i < g_pileCount; i++) {
        int x = ox + g_piles[i].x, z = oz + g_piles[i].z;
        if(bboxHitsChunk(bx0, bz0, x - 1, z - 1, x + 1, z + 1)) stampPile(ch, bx0, bz0, x, z);
    }
    for(int i = 0; i < g_houseCount; i++) {
        int x = ox + g_houses[i].x, z = oz + g_houses[i].z;
        if(bboxHitsChunk(bx0, bz0, x, z, x + 4, z + 4))
            stampHouse(ch, bx0, bz0, x, z, g_houses[i].aux);
    }
}

// Furnace + chest tile-entity slots for one house, both facing the doorway
// (dir 2 = -Z face); the chest holds one seed-random item.
static bool writeHouseSlots(const Writer& out, uint32_t slot0, int& used, int hx, int hz, int by) {
    uint8_t s[2][SLOT_SIZE];
    memset(s, 0, sizeof(s));
    for(int c = 0; c < 2; c++) { // 0 furnace, 1 chest
        int bx = hx + 2 + c, bz = hz + 3;
        uint8_t* b = s[c];
        b[0] = (uint8_t)(0x01 | (c << 1)); // in-use | isChest
        b[1] = (uint8_t)(2 | (((bx >> 8) & 3) << 4) | (((bz >> 8) & 3) << 6));
        b[2] = (uint8_t)bx;
        b[3] = (uint8_t)by;
        b[4] = (uint8_t)bz;
    }
    // apple coal ingot gunpowder dynamite glass stick sapling (flipcraft.h Item)
    static const uint8_t kLoot[8] = {0xE0, 0x80, 0xD0, 0xB1, 0x01, 0xB0, 0x10, 0xC0};
    s[1][6] = kLoot[whash(hx, hz, 100) & 7];
    s[1][7] = 1;
    if(!out.writeAt(out.ctx, slot0 + (uint32_t)used * SLOT_SIZE, s, sizeof(s))) return false;
    used += 2;
    return true;
}

static void putU16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void putU32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static bool generate(int chunks, uint32_t seed, Writer out, Progress progress, void* pctx) {
    if(chunks < 1 || chunks > MAX_CHUNKS) return false;
    if(chunks > TILE_CHUNKS && chunks % TILE_CHUNKS != 0) return false;
    g_seed = seed;
    g_worldW = chunks * CHUNK;
    gradInit();

    // Header. Spawn sits on the terrain surface at the world centre; the
    // engine lifts the player out of the ground if a feature landed there.
    uint8_t hdr[HEADER_SIZE];
    memset(hdr, 0, sizeof(hdr));
    putU32(hdr + 0, 0x31574346); // 'FCW1'
    putU16(hdr + 4, 3);          // v3: the tile-entity region is written below
    putU16(hdr + 6, (uint16_t)chunks);
    putU16(hdr + 8, (uint16_t)chunks);
    hdr[10] = CHUNK;
    hdr[11] = HEIGHT;
    hdr[12] = CHUNK;
    hdr[13] = 1;
    putU32(hdr + 14, HEADER_SIZE);
    int sbx = g_worldW / 2, sbz = g_worldW / 2;
    g_tileX0 = g_tileZ0 = 0; // computeColumn is tile-independent
    putU32(hdr + 18, (uint32_t)(sbx * BLOCKSIZE));
    putU32(hdr + 22, (uint32_t)(((computeColumn(sbx, sbz) & COL_TOP) + 1) * BLOCKSIZE));
    putU32(hdr + 26, (uint32_t)(sbz * BLOCKSIZE));
    hdr[30] = 0x08;
    putU32(hdr + 32, seed);
    if(!out.writeAt(out.ctx, 0, hdr, sizeof(hdr))) return false;

    // Zero the whole tile-entity region so unwritten slots read as free.
    const uint32_t invBase = HEADER_SIZE + (uint32_t)chunks * (uint32_t)chunks * CHUNK_BYTES;
    const uint32_t slot0 = invBase + INV_REGION + PAD_V2;
    {
        uint8_t zero[256];
        memset(zero, 0, sizeof(zero));
        uint32_t end = slot0 + (uint32_t)SLOT_CAP * SLOT_SIZE;
        for(uint32_t off = invBase; off < end; off += sizeof(zero)) {
            uint32_t n = end - off;
            if(n > sizeof(zero)) n = sizeof(zero);
            if(!out.writeAt(out.ctx, off, zero, n)) return false;
        }
    }
    int slotsUsed = 0;

    const int tiles = (chunks + TILE_CHUNKS - 1) / TILE_CHUNKS;
    const int tileChunks = chunks < TILE_CHUNKS ? chunks : TILE_CHUNKS;
    uint8_t chunk[CHUNK_BYTES];
    int tileIdx = 0, tileTotal = tiles * tiles;

    // The world's single house goes to the tile holding the global maximum of
    // the house score field, biome-checked on an 8-block grid (pure functions
    // of coords, so the pick is a seed-deterministic property of the world).
    int houseTx = 0, houseTz = 0;
    {
        float best = -1e30f;
        for(int z = 0; z + 4 < g_worldW; z += 8)
            for(int x = 0; x + 4 < g_worldW; x += 8) {
                if(computeColumn(x + 2, z + 2) & (COL_DESERT | COL_RAVINE)) continue;
                float s = houseScore(x, z);
                if(s > best) { best = s; houseTx = x / TILE_W; houseTz = z / TILE_W; }
            }
    }

    for(int tz = 0; tz < tiles; tz++)
        for(int tx = 0; tx < tiles; tx++, tileIdx++) {
            g_tileX0 = tx * TILE_W;
            g_tileZ0 = tz * TILE_W;
            g_tileW = tileChunks * CHUNK;

            const int base = tileIdx * 100;
            for(int lz = 0; lz < g_tileW; lz++) {
                for(int lx = 0; lx < g_tileW; lx++)
                    g_col[lz * g_tileW + lx] = computeColumn(g_tileX0 + lx, g_tileZ0 + lz);
                if(progress)
                    progress(pctx, (uint8_t)((base + lz * 60 / g_tileW) / tileTotal));
            }

            placeFeatures(tx == houseTx && tz == houseTz);

            for(int i = 0; i < g_houseCount; i++)
                if(!writeHouseSlots(
                       out,
                       slot0,
                       slotsUsed,
                       g_tileX0 + g_houses[i].x,
                       g_tileZ0 + g_houses[i].z,
                       g_houses[i].aux + 1))
                    return false;

            for(int lcz = 0; lcz < tileChunks; lcz++) {
                for(int lcx = 0; lcx < tileChunks; lcx++) {
                    int ccx = tx * TILE_CHUNKS + lcx, ccz = tz * TILE_CHUNKS + lcz;
                    int bx0 = ccx * CHUNK, bz0 = ccz * CHUNK;
                    fillTerrain(chunk, bx0, bz0);
                    stampFeatures(chunk, bx0, bz0);
                    uint32_t off = HEADER_SIZE + (uint32_t)(ccz * chunks + ccx) * CHUNK_BYTES;
                    if(!out.writeAt(out.ctx, off, chunk, CHUNK_BYTES)) return false;
                }
                if(progress)
                    progress(
                        pctx,
                        (uint8_t)((base + 60 + (lcz + 1) * 40 / tileChunks) / tileTotal));
            }
        }
    if(progress) progress(pctx, 100);
    return true;
}

} // namespace fcgen
