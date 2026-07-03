#include "../flipcraft.h"

#include <furi.h>
#include <storage/storage.h>

#include <limits>
#include <string.h>

namespace flipcraft {

static constexpr uint32_t FCW_MAGIC = 0x31574346;
static constexpr uint16_t FCW_VERSION = 3;
static constexpr uint32_t HEADER_SIZE = 64;
static constexpr uint8_t INVENTORY_MAGIC = 0xA6;    // v3; v2 used 0xA5
static constexpr uint8_t INVENTORY_MAGIC_V2 = 0xA5;

static inline void put_u16(uint8_t* p, uint16_t v) {
    p[0] = v;
    p[1] = v >> 8;
}
static inline void put_u32(uint8_t* p, uint32_t v) {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}
static inline uint16_t get_u16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}
static inline uint32_t get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int chunkMaxY(const uint8_t* chunk) {
    for(int y = WORLD_SY - 1; y >= 0; y--)
        for(int z = 0; z < CHUNK_SIZE; z++)
            for(int x = 0; x < CHUNK_SIZE; x++)
                if(chunk[(y * CHUNK_SIZE + z) * CHUNK_SIZE + x] != BLOCK_AIR) return y;
    return -1;
}

uint32_t worldArrayBytes(const World& w) {
    return (uint32_t)w.chunksX * (uint32_t)w.chunksZ * CHUNK_BLOCKS;
}

uint32_t inventoryBase(const World& w) {
    return HEADER_SIZE + worldArrayBytes(w);
}

uint32_t storageBase(const World& w) {
    return inventoryBase(w) + INVENTORY_REGION_SIZE + STORAGE_PAD_V2;
}

uint32_t regionEnd(const World& w) {
    return storageBase(w) + (uint32_t)STORAGE_CAPACITY * STORAGE_SLOT_SIZE;
}

bool World::tryOpenAndReadHeader(const char* path) {
    if(!storage_file_open(file, path, FSAM_READ_WRITE, FSOM_OPEN_EXISTING)) return false;

    uint8_t hdr[HEADER_SIZE];
    uint16_t ver = 0;
    uint16_t fileChunksX = 0, fileChunksZ = 0;
    bool valid = storage_file_seek(file, 0, true) &&
                 storage_file_read(file, hdr, HEADER_SIZE) == HEADER_SIZE &&
                 get_u32(hdr + 0) == FCW_MAGIC && (ver = get_u16(hdr + 4)) >= 1 &&
                 ver <= FCW_VERSION && (fileChunksX = get_u16(hdr + 6)) > 0 &&
                 (fileChunksZ = get_u16(hdr + 8)) > 0 && hdr[10] == CHUNK_SIZE &&
                 hdr[11] == WORLD_SY && hdr[12] == CHUNK_SIZE && hdr[13] == 1 &&
                 get_u32(hdr + 14) == HEADER_SIZE;
    if(valid) {
        uint64_t minSize =
            (uint64_t)HEADER_SIZE + (uint64_t)fileChunksX * (uint64_t)fileChunksZ * CHUNK_BLOCKS;
        uint64_t endSize =
            minSize + INVENTORY_REGION_SIZE + (uint64_t)STORAGE_CAPACITY * STORAGE_SLOT_SIZE;
        valid = endSize <= std::numeric_limits<uint32_t>::max() &&
                storage_file_size(file) >= minSize;
    }
    if(!valid) {
        storage_file_close(file);
        return false;
    }
    chunksX = fileChunksX;
    chunksZ = fileChunksZ;
    hdrVersion = ver;
    hdrPX = (int)get_u32(hdr + 18);
    hdrPY = (int)get_u32(hdr + 22);
    hdrPZ = (int)get_u32(hdr + 26);
    hdrRot = hdr[30];
    hdrRng = get_u32(hdr + 32);
    return true;
}

bool World::openWorld(const char* dataPath) {
    storage = static_cast<::Storage*>(furi_record_open(RECORD_STORAGE));
    file = storage_file_alloc(storage);
    existed = false;
    if(!tryOpenAndReadHeader(dataPath)) {
        storage_file_free(file);
        file = nullptr;
        furi_record_close(RECORD_STORAGE);
        storage = nullptr;
        return false;
    }
    existed = true;

    uint32_t szBefore = storage_file_size(file);
    ensureRegion();
    // v2 regions occupied exactly [inventoryBase, storageBase); the old slot
    // area stays untouched as STORAGE_PAD_V2, so a crashed migration reruns.
    if(hdrVersion == 2 && szBefore >= storageBase(*this)) migrateV2();
    if(hdrVersion != FCW_VERSION) {
        uint8_t v[2];
        put_u16(v, FCW_VERSION);
        if(storage_file_seek(file, 4, true) && storage_file_write(file, v, 2) == 2)
            hdrVersion = FCW_VERSION;
    }
    storage_file_sync(file);

    for(int sx = 0; sx < WINDOW_CHUNKS; sx++)
        for(int sz = 0; sz < WINDOW_CHUNKS; sz++) {
            slotCX[sx][sz] = slotCZ[sx][sz] = -1;
            slotMaxY[sx][sz] = -1;
            slotDirty[sx][sz] = false;
            slotGen[sx][sz] = 0;
        }
    centerCX = centerCZ = -2;
    loadPending = false;
    opened = true;
    return true;
}

bool World::ensureRegion() {
    if(!file) return false;
    uint32_t sz = storage_file_size(file);
    uint32_t invBase = inventoryBase(*this);
    uint32_t end = regionEnd(*this);
    if(sz < end) {
        uint8_t zero[256];
        memset(zero, 0, sizeof(zero));
        uint32_t off = (sz < invBase) ? invBase : sz;
        if(!storage_file_seek(file, off, true)) return false;
        while(off < end) {
            uint32_t n = end - off;
            if(n > sizeof(zero)) n = sizeof(zero);
            if(storage_file_write(file, zero, n) != n) return false;
            off += n;
        }
    }
    return true;
}

// v2 cell byte -> v3 {type, count}: high nibble type, low nibble count;
// 0x0N = dynamite, 0xB0 = one gunpowder, 0xFx = tool.
static void cellFromV2(uint8_t c, uint8_t* out) {
    uint8_t t = c & 0xF0, n = c & 0x0F;
    out[0] = 0;
    out[1] = 0;
    if(c == 0) return;
    if(t == 0xF0) {
        out[0] = c;
        out[1] = 1;
    } else if(t == 0) {
        out[0] = ITEM_DYNAMITE;
        out[1] = n;
    } else if(n == 0) {
        if(c == 0xB0) {
            out[0] = ITEM_GUNPOWDER;
            out[1] = 1;
        }
    } else {
        out[0] = t;
        out[1] = n;
    }
}

// v2 layout: inventory [magic A5][15 cells][invSlot][health] at inventoryBase,
// 256x16 storage slots right after it (= today's pad). Slots copy old->new
// region; both stay intact during the copy, so the pass is idempotent.
void World::migrateV2() {
    uint32_t invBase = inventoryBase(*this);
    uint8_t inv[INVENTORY_REGION_SIZE];
    if(storage_file_seek(file, invBase, true) &&
       storage_file_read(file, inv, sizeof(inv)) == sizeof(inv) &&
       inv[0] == INVENTORY_MAGIC_V2) {
        uint8_t out[INVENTORY_REGION_SIZE];
        memset(out, 0, sizeof(out));
        out[0] = INVENTORY_MAGIC;
        for(int i = 0; i < 15; i++) cellFromV2(inv[1 + i], out + 1 + 2 * i);
        out[31] = (uint8_t)(((inv[17] & 0x0F) << 4) | (inv[16] & 0x0F));
        if(storage_file_seek(file, invBase, true)) storage_file_write(file, out, sizeof(out));
    }

    uint32_t oldBase = invBase + INVENTORY_REGION_SIZE;
    uint32_t newBase = storageBase(*this);
    for(int first = 0; first < STORAGE_CAPACITY; first += 16) {
        uint8_t oldB[16 * 16], newB[16 * STORAGE_SLOT_SIZE];
        if(!storage_file_seek(file, oldBase + (uint32_t)first * 16, true) ||
           storage_file_read(file, oldB, sizeof(oldB)) != sizeof(oldB))
            return;
        memset(newB, 0, sizeof(newB));
        for(int i = 0; i < 16; i++) {
            const uint8_t* o = oldB + i * 16;
            uint8_t* n = newB + i * STORAGE_SLOT_SIZE;
            if(!(o[0] & 0x01)) continue;
            n[0] = o[0];
            n[1] = o[1];
            n[2] = o[2];
            n[3] = o[3];
            n[4] = o[4];
            n[5] = o[15]; // furnace fuel|timer moved from [15] to [5]
            for(int k = 0; k < 10; k++) cellFromV2(o[5 + k], n + 6 + 2 * k);
        }
        if(!storage_file_seek(file, newBase + (uint32_t)first * STORAGE_SLOT_SIZE, true) ||
           storage_file_write(file, newB, sizeof(newB)) != sizeof(newB))
            return;
    }
}

bool World::readInventory(uint8_t* dst, uint32_t n) {
    if(!opened || n + 1 > INVENTORY_REGION_SIZE) return false;
    uint8_t buf[INVENTORY_REGION_SIZE];
    if(!storage_file_seek(file, inventoryBase(*this), true) ||
       storage_file_read(file, buf, INVENTORY_REGION_SIZE) != INVENTORY_REGION_SIZE)
        return false;
    if(buf[0] != INVENTORY_MAGIC) return false; // never written -> use defaults
    memcpy(dst, buf + 1, n);
    return true;
}

void World::writeInventory(const uint8_t* src, uint32_t n) {
    if(!opened || n + 1 > INVENTORY_REGION_SIZE) return;
    uint8_t buf[INVENTORY_REGION_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = INVENTORY_MAGIC;
    memcpy(buf + 1, src, n);
    if(storage_file_seek(file, inventoryBase(*this), true))
        storage_file_write(file, buf, INVENTORY_REGION_SIZE);
    storage_file_sync(file);
}

bool World::readStorageBatch(int first, int count, uint8_t* dst) {
    if(!opened || first < 0 || count <= 0 || first + count > STORAGE_CAPACITY) return false;
    uint32_t off = storageBase(*this) + (uint32_t)first * STORAGE_SLOT_SIZE;
    size_t bytes = (size_t)count * STORAGE_SLOT_SIZE;
    return storage_file_seek(file, off, true) && storage_file_read(file, dst, bytes) == bytes;
}

bool World::readStorageSlot(int index, uint8_t* dst) {
    if(!opened || (unsigned)index >= STORAGE_CAPACITY) return false;
    uint32_t off = storageBase(*this) + (uint32_t)index * STORAGE_SLOT_SIZE;
    return storage_file_seek(file, off, true) &&
           storage_file_read(file, dst, STORAGE_SLOT_SIZE) == STORAGE_SLOT_SIZE;
}

void World::writeStorageSlot(int index, const uint8_t* src) {
    if(!opened || (unsigned)index >= STORAGE_CAPACITY) return;
    uint32_t off = storageBase(*this) + (uint32_t)index * STORAGE_SLOT_SIZE;
    if(storage_file_seek(file, off, true)) storage_file_write(file, src, STORAGE_SLOT_SIZE);
    // No per-write sync: storage slots are flushed while streaming chunks and a
    // FAT sync here stalls the tick. save()/closeWorld() sync the file.
}

bool World::flushSlot(int sx, int sz) {
    if(!slotDirty[sx][sz] || slotCX[sx][sz] < 0) return true;
    uint32_t off =
        HEADER_SIZE + (uint32_t)(slotCZ[sx][sz] * chunksX + slotCX[sx][sz]) * CHUNK_BLOCKS;
    bool ok = storage_file_seek(file, off, true) &&
              storage_file_write(file, &slot[sx][sz][0][0][0], CHUNK_BLOCKS) == CHUNK_BLOCKS;
    slotDirty[sx][sz] = false;
    return ok;
}

// A slot's content changed: invalidate its cached mesh and the meshes of the
// four adjacent chunks, whose boundary faces depend on this chunk's blocks.
void World::onSlotLoaded(int cx, int cz) {
    revision++; // a freshly streamed chunk must reach the next rendered frame
    bumpGen(cx, cz);
    bumpGen(cx - 1, cz);
    bumpGen(cx + 1, cz);
    bumpGen(cx, cz - 1);
    bumpGen(cx, cz + 1);
}

bool World::loadChunkDirect(int cx, int cz) {
    int sx = cx % 3, sz = cz % 3;
    flushSlot(sx, sz);
    uint32_t off = HEADER_SIZE + (uint32_t)(cz * chunksX + cx) * CHUNK_BLOCKS;
    bool ok = storage_file_seek(file, off, true) &&
              storage_file_read(file, &slot[sx][sz][0][0][0], CHUNK_BLOCKS) == CHUNK_BLOCKS;
    if(!ok) {
        slotCX[sx][sz] = slotCZ[sx][sz] = -1;
        slotMaxY[sx][sz] = -1;
        slotDirty[sx][sz] = false;
        onSlotLoaded(cx, cz); // neighbours may have meshed against the old occupant
        return false;
    }
    slotMaxY[sx][sz] = chunkMaxY(&slot[sx][sz][0][0][0]);
    slotCX[sx][sz] = cx;
    slotCZ[sx][sz] = cz;
    slotDirty[sx][sz] = false;
    onSlotLoaded(cx, cz);
    return ok;
}

bool World::loadRunStaged(int cx0, int cz, int count) {
    uint8_t staging[WINDOW_CHUNKS * CHUNK_BLOCKS];
    uint32_t off = HEADER_SIZE + (uint32_t)(cz * chunksX + cx0) * CHUNK_BLOCKS;
    size_t bytes = (size_t)count * CHUNK_BLOCKS;
    if(!storage_file_seek(file, off, true) || storage_file_read(file, staging, bytes) != bytes)
        return false;
    for(int i = 0; i < count; i++) {
        int cx = cx0 + i, sx = cx % 3, sz = cz % 3;
        flushSlot(sx, sz);
        memcpy(&slot[sx][sz][0][0][0], staging + (size_t)i * CHUNK_BLOCKS, CHUNK_BLOCKS);
        slotMaxY[sx][sz] = chunkMaxY(&slot[sx][sz][0][0][0]);
        slotCX[sx][sz] = cx;
        slotCZ[sx][sz] = cz;
        slotDirty[sx][sz] = false;
        onSlotLoaded(cx, cz);
    }
    return true;
}

void World::updateWindow(int blockX, int blockZ, bool immediate) {
    if(!opened) return;
    int cx = blockX >> CHUNK_SHIFT, cz = blockZ >> CHUNK_SHIFT;
    if(cx < 0)
        cx = 0;
    else if(cx >= chunksX)
        cx = chunksX - 1;
    if(cz < 0)
        cz = 0;
    else if(cz >= chunksZ)
        cz = chunksZ - 1;
    if(cx == centerCX && cz == centerCZ && !loadPending) return;
    centerCX = cx;
    centerCZ = cz;

    if(immediate) {
        // Load every missing chunk of the ring now, coalescing horizontal runs
        // into one sequential read per row.
        for(int ncz = cz - 1; ncz <= cz + 1; ncz++) {
            if(ncz < 0 || ncz >= chunksZ) continue;
            int run0 = -1, run1 = -1;
            for(int ncx = cx - 1; ncx <= cx + 1; ncx++) {
                bool valid = (ncx >= 0 && ncx < chunksX);
                bool resident = valid && slotCX[ncx % 3][ncz % 3] == ncx &&
                                slotCZ[ncx % 3][ncz % 3] == ncz;
                if(valid && !resident) {
                    if(run0 < 0) run0 = ncx;
                    run1 = ncx;
                } else if(run0 >= 0) {
                    int n = run1 - run0 + 1;
                    if(n == 1)
                        loadChunkDirect(run0, ncz);
                    else
                        loadRunStaged(run0, ncz, n);
                    run0 = -1;
                }
            }
            if(run0 >= 0) {
                int n = run1 - run0 + 1;
                if(n == 1)
                    loadChunkDirect(run0, ncz);
                else
                    loadRunStaged(run0, ncz, n);
            }
        }
        loadPending = false;
        return;
    }

    // Streaming mode: one SD read per tick, nearest chunk first. The player
    // covers at most half a block per tick while the freshly-entered ring is
    // still RENDER_RADIUS_BLOCKS away, so spreading the loads over a few ticks
    // is invisible but removes the multi-chunk stall from a single frame.
    static const int8_t kOrder[9][2] = {
        {0,0}, {-1,0}, {1,0}, {0,-1}, {0,1}, {-1,-1}, {1,-1}, {-1,1}, {1,1}};
    int missing = 0, firstCX = 0, firstCZ = 0;
    for(const auto& o : kOrder) {
        int ncx = cx + o[0], ncz = cz + o[1];
        if(ncx < 0 || ncx >= chunksX || ncz < 0 || ncz >= chunksZ) continue;
        if(slotCX[ncx % 3][ncz % 3] == ncx && slotCZ[ncx % 3][ncz % 3] == ncz) continue;
        if(missing == 0) {
            firstCX = ncx;
            firstCZ = ncz;
        }
        missing++;
    }
    if(missing) loadChunkDirect(firstCX, firstCZ);
    loadPending = missing > 1;
}

void World::save() {
    if(!opened) return;
    for(int sx = 0; sx < WINDOW_CHUNKS; sx++)
        for(int sz = 0; sz < WINDOW_CHUNKS; sz++)
            flushSlot(sx, sz);
    storage_file_sync(file);
}

void World::closeWorld(int px, int py, int pz, uint8_t rot, uint32_t rng) {
    if(!opened) return;
    save();

    uint8_t buf[20];
    memset(buf, 0, sizeof(buf));
    put_u32(buf + 0, (uint32_t)px);
    put_u32(buf + 4, (uint32_t)py);
    put_u32(buf + 8, (uint32_t)pz);
    buf[12] = rot;
    buf[13] = 0;
    put_u32(buf + 14, rng);
    if(storage_file_seek(file, 18, true)) storage_file_write(file, buf, sizeof(buf));
    storage_file_sync(file);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    file = nullptr;
    storage = nullptr;
    opened = false;
}
}
