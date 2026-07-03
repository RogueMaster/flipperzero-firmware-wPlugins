#include "../flipcraft.h"

#include <limits>
#include <string.h>

namespace flipcraft {

static constexpr uint32_t FCW_MAGIC = 0x31574346;
static constexpr uint16_t FCW_VERSION = 2;
static constexpr uint32_t HEADER_SIZE = 64;
static constexpr uint8_t INVENTORY_MAGIC = 0xA5;

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

static void* fileOpen(const FileSystem& fs, const char* path, FileMode mode) {
    return fs.open ? fs.open(fs.ctx, path, mode) : nullptr;
}

static void fileClose(const FileSystem& fs, void* file) {
    if(fs.close) fs.close(fs.ctx, file);
}

static bool fileSeek(const FileSystem& fs, void* file, uint32_t offset) {
    return fs.seek && fs.seek(fs.ctx, file, offset);
}

static size_t fileRead(const FileSystem& fs, void* file, void* data, size_t size) {
    return fs.read ? fs.read(fs.ctx, file, data, size) : 0;
}

static size_t fileWrite(const FileSystem& fs, void* file, const void* data, size_t size) {
    return fs.write ? fs.write(fs.ctx, file, data, size) : 0;
}

static uint32_t fileSize(const FileSystem& fs, void* file) {
    return fs.size ? fs.size(fs.ctx, file) : 0;
}

static void fileSync(const FileSystem& fs, void* file) {
    if(fs.sync) fs.sync(fs.ctx, file);
}

uint32_t worldArrayBytes(const World& w) {
    return (uint32_t)w.chunksX * (uint32_t)w.chunksZ * CHUNK_BLOCKS;
}

uint32_t inventoryBase(const World& w) {
    return HEADER_SIZE + worldArrayBytes(w);
}

uint32_t storageBase(const World& w) {
    return inventoryBase(w) + INVENTORY_REGION_SIZE;
}

uint32_t regionEnd(const World& w) {
    return storageBase(w) + (uint32_t)STORAGE_CAPACITY * STORAGE_SLOT_SIZE;
}

bool World::tryOpenAndReadHeader(const char* path) {
    void* f = fileOpen(*fs, path, FileMode::ReadWriteExisting);
    if(!f) return false;

    uint8_t hdr[HEADER_SIZE];
    uint16_t ver = 0;
    uint16_t fileChunksX = 0, fileChunksZ = 0;
    bool valid = fileSeek(*fs, f, 0) && fileRead(*fs, f, hdr, HEADER_SIZE) == HEADER_SIZE &&
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
        valid = endSize <= std::numeric_limits<uint32_t>::max() && fileSize(*fs, f) >= minSize;
    }
    if(!valid) {
        fileClose(*fs, f);
        return false;
    }
    file = f;
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

bool World::openWorld(const FileSystem& files, const char* dataPath) {
    fs = &files;
    existed = false;
    if(!tryOpenAndReadHeader(dataPath)) {
        file = nullptr;
        return false;
    }
    existed = true;

    ensureRegion();

    for(int sx = 0; sx < WINDOW_CHUNKS; sx++)
        for(int sz = 0; sz < WINDOW_CHUNKS; sz++) {
            slotCX[sx][sz] = slotCZ[sx][sz] = -1;
            slotMaxY[sx][sz] = -1;
            slotDirty[sx][sz] = false;
        }
    centerCX = centerCZ = -2;
    opened = true;
    return true;
}

bool World::ensureRegion() {
    if(!file) return false;
    uint32_t sz = fileSize(*fs, file);
    uint32_t invBase = inventoryBase(*this);
    uint32_t end = regionEnd(*this);
    if(sz < end) {
        uint8_t zero[256];
        memset(zero, 0, sizeof(zero));
        uint32_t off = (sz < invBase) ? invBase : sz;
        if(!fileSeek(*fs, file, off)) return false;
        while(off < end) {
            uint32_t n = end - off;
            if(n > sizeof(zero)) n = sizeof(zero);
            if(fileWrite(*fs, file, zero, n) != n) return false;
            off += n;
        }
    }
    if(hdrVersion != FCW_VERSION) {
        uint8_t v[2];
        put_u16(v, FCW_VERSION);
        if(fileSeek(*fs, file, 4) && fileWrite(*fs, file, v, 2) == 2) hdrVersion = FCW_VERSION;
    }
    fileSync(*fs, file);
    return true;
}

bool World::readInventory(uint8_t* dst, uint32_t n) {
    if(!opened || n + 1 > INVENTORY_REGION_SIZE) return false;
    uint8_t buf[INVENTORY_REGION_SIZE];
    if(!fileSeek(*fs, file, inventoryBase(*this)) ||
       fileRead(*fs, file, buf, INVENTORY_REGION_SIZE) != INVENTORY_REGION_SIZE)
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
    if(fileSeek(*fs, file, inventoryBase(*this))) fileWrite(*fs, file, buf, INVENTORY_REGION_SIZE);
    fileSync(*fs, file);
}

bool World::readStorageBatch(int first, int count, uint8_t* dst) {
    if(!opened || first < 0 || count <= 0 || first + count > STORAGE_CAPACITY) return false;
    uint32_t off = storageBase(*this) + (uint32_t)first * STORAGE_SLOT_SIZE;
    size_t bytes = (size_t)count * STORAGE_SLOT_SIZE;
    return fileSeek(*fs, file, off) && fileRead(*fs, file, dst, bytes) == bytes;
}

bool World::readStorageSlot(int index, uint8_t* dst) {
    if(!opened || (unsigned)index >= STORAGE_CAPACITY) return false;
    uint32_t off = storageBase(*this) + (uint32_t)index * STORAGE_SLOT_SIZE;
    return fileSeek(*fs, file, off) &&
           fileRead(*fs, file, dst, STORAGE_SLOT_SIZE) == STORAGE_SLOT_SIZE;
}

void World::writeStorageSlot(int index, const uint8_t* src) {
    if(!opened || (unsigned)index >= STORAGE_CAPACITY) return;
    uint32_t off = storageBase(*this) + (uint32_t)index * STORAGE_SLOT_SIZE;
    if(fileSeek(*fs, file, off)) fileWrite(*fs, file, src, STORAGE_SLOT_SIZE);
    fileSync(*fs, file);
}

bool World::flushSlot(int sx, int sz) {
    if(!slotDirty[sx][sz] || slotCX[sx][sz] < 0) return true;
    uint32_t off =
        HEADER_SIZE + (uint32_t)(slotCZ[sx][sz] * chunksX + slotCX[sx][sz]) * CHUNK_BLOCKS;
    bool ok = fileSeek(*fs, file, off) &&
              fileWrite(*fs, file, &slot[sx][sz][0][0][0], CHUNK_BLOCKS) == CHUNK_BLOCKS;
    slotDirty[sx][sz] = false;
    return ok;
}

bool World::loadChunkDirect(int cx, int cz) {
    int sx = cx % 3, sz = cz % 3;
    flushSlot(sx, sz);
    uint32_t off = HEADER_SIZE + (uint32_t)(cz * chunksX + cx) * CHUNK_BLOCKS;
    bool ok = fileSeek(*fs, file, off) &&
              fileRead(*fs, file, &slot[sx][sz][0][0][0], CHUNK_BLOCKS) == CHUNK_BLOCKS;
    if(!ok) {
        slotCX[sx][sz] = slotCZ[sx][sz] = -1;
        slotMaxY[sx][sz] = -1;
        slotDirty[sx][sz] = false;
        return false;
    }
    slotMaxY[sx][sz] = chunkMaxY(&slot[sx][sz][0][0][0]);
    slotCX[sx][sz] = cx;
    slotCZ[sx][sz] = cz;
    slotDirty[sx][sz] = false;
    return ok;
}

bool World::loadRunStaged(int cx0, int cz, int count) {
    uint8_t staging[WINDOW_CHUNKS * CHUNK_BLOCKS];
    uint32_t off = HEADER_SIZE + (uint32_t)(cz * chunksX + cx0) * CHUNK_BLOCKS;
    size_t bytes = (size_t)count * CHUNK_BLOCKS;
    if(!fileSeek(*fs, file, off) || fileRead(*fs, file, staging, bytes) != bytes) return false;
    for(int i = 0; i < count; i++) {
        int cx = cx0 + i, sx = cx % 3, sz = cz % 3;
        flushSlot(sx, sz);
        memcpy(&slot[sx][sz][0][0][0], staging + (size_t)i * CHUNK_BLOCKS, CHUNK_BLOCKS);
        slotMaxY[sx][sz] = chunkMaxY(&slot[sx][sz][0][0][0]);
        slotCX[sx][sz] = cx;
        slotCZ[sx][sz] = cz;
        slotDirty[sx][sz] = false;
    }
    return true;
}

void World::updateWindow(int blockX, int blockZ) {
    if(!opened) return;
    int cx = blockX >> 3, cz = blockZ >> 3;
    if(cx < 0)
        cx = 0;
    else if(cx >= chunksX)
        cx = chunksX - 1;
    if(cz < 0)
        cz = 0;
    else if(cz >= chunksZ)
        cz = chunksZ - 1;
    if(cx == centerCX && cz == centerCZ) return;
    centerCX = cx;
    centerCZ = cz;

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
}

void World::save() {
    if(!opened) return;
    for(int sx = 0; sx < WINDOW_CHUNKS; sx++)
        for(int sz = 0; sz < WINDOW_CHUNKS; sz++)
            flushSlot(sx, sz);
    fileSync(*fs, file);
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
    if(fileSeek(*fs, file, 18)) fileWrite(*fs, file, buf, sizeof(buf));
    fileSync(*fs, file);
    fileClose(*fs, file);
    file = nullptr;
    fs = nullptr;
    opened = false;
}
}
