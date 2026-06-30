#include "lib/ArduboyFX.h"
#include "src/utils/Arduboy2Ext.h"

static inline size_t fx_min_sz(size_t a, size_t b) {
    return a < b ? a : b;
}
static inline uint16_t fx_be16(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}
uint16_t FX::programDataPage = 0;
uint16_t FX::programSavePage = 0;

Storage* FX::storage_ = nullptr;
File* FX::data_ = nullptr;
File* FX::save_ = nullptr;

bool FX::data_opened_ = false;
bool FX::save_opened_ = false;

char FX::data_path_[FX::kPathMax] = APP_ASSETS_PATH("fxdata.bin");
char FX::save_path_[FX::kPathMax] = APP_DATA_PATH("fxsave.bin");

FX::Domain FX::domain_ = FX::Domain::Data;
uint32_t FX::cur_abs_ = 0;

uint32_t FX::page_size_ = 1024;
uint8_t FX::cache_pages_ = 30;

uint8_t* FX::cache_mem_ = nullptr;
uint32_t* FX::cache_base_ = nullptr;
uint16_t* FX::cache_len_ = nullptr;
uint32_t* FX::cache_age_ = nullptr;
uint8_t* FX::cache_valid_ = nullptr;
uint32_t FX::cache_age_ctr_ = 1;

uint8_t FX::last_hit_ = 0xFF;
uint32_t FX::last_base_ = 0;
uint8_t FX::seq_score_ = 0;

uint8_t FX::stream_page_i_ = 0xFF;
uint32_t FX::stream_base_ = 0;
uint16_t FX::stream_len_ = 0;
uint32_t FX::stream_abs_ = 0;
uint8_t* FX::stream_ptr_ = nullptr;
bool FX::stream_valid_ = false;

bool FX::pending_valid_ = false;
uint8_t FX::pending_byte_ = 0xFF;

uint24_t FX::frame_addr_ = 0;
uint24_t FX::frame_base_addr_ = 0;
uint8_t FX::frame_count_ = 0;
uint8_t FX::frame_idx_ = 0;

uint32_t FX::alignDown_(uint32_t v, uint32_t a) {
    return a ? (v & ~(a - 1u)) : v;
}
uint32_t FX::alignUp_(uint32_t v, uint32_t a) {
    if(!a) return v;
    if(v > (UINT32_MAX - (a - 1u))) return UINT32_MAX;
    return (v + (a - 1u)) & ~(a - 1u);
}

void FX::setCacheConfig(uint32_t page_size, uint8_t pages) {
    if(page_size < 512) page_size = 512;
    page_size = alignUp_(page_size, 512);
    if(pages < 2) pages = 2;
    page_size_ = page_size;
    cache_pages_ = pages;
}

void FX::setPaths(const char* data_bin_path, const char* save_path) {
    if(data_bin_path && *data_bin_path) {
        strncpy(data_path_, data_bin_path, sizeof(data_path_) - 1);
        data_path_[sizeof(data_path_) - 1] = 0;
    }
    if(save_path && *save_path) {
        strncpy(save_path_, save_path, sizeof(save_path_) - 1);
        save_path_[sizeof(save_path_) - 1] = 0;
    }
}

bool FX::ensureStorage_() {
    if(storage_) return true;
    storage_ = (Storage*)furi_record_open(RECORD_STORAGE);
    if(!storage_) return false;
    data_ = storage_file_alloc(storage_);
    save_ = storage_file_alloc(storage_);
    if(!data_ || !save_) {
        end();
        return false;
    }
    return true;
}

bool FX::openData_() {
    if(data_opened_) return true;
    if(!data_) return false;
    if(!storage_file_open(data_, data_path_, FSAM_READ, FSOM_OPEN_EXISTING)) return false;
    data_opened_ = true;
    return true;
}

bool FX::fileFill_(File* f, uint8_t value, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, 0, true)) return false;
    uint8_t buf[256];
    memset(buf, value, sizeof(buf));
    size_t remain = len;
    while(remain) {
        size_t chunk = remain > sizeof(buf) ? sizeof(buf) : remain;
        if(storage_file_write(f, buf, chunk) != chunk) return false;
        remain -= chunk;
    }
    return true;
}

bool FX::fileReadAt_(File* f, uint32_t off, void* out, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, off, true)) return false;
    return storage_file_read(f, out, len) == len;
}

bool FX::fileWriteAt_(File* f, uint32_t off, const void* in, size_t len) {
    if(!f) return false;
    if(!storage_file_seek(f, off, true)) return false;
    return storage_file_write(f, in, len) == len;
}

bool FX::openSave_() {
    if(save_opened_) return true;
    if(!save_) return false;

    if(!storage_file_open(save_, save_path_, FSAM_READ_WRITE, FSOM_OPEN_ALWAYS)) return false;
    save_opened_ = true;

    uint8_t tmp = 0;
    bool ok_tail = fileReadAt_(save_, (uint32_t)kSaveBlockSize - 1u, &tmp, 1);
    if(!ok_tail) {
        storage_file_close(save_);
        save_opened_ = false;

        if(!storage_file_open(save_, save_path_, FSAM_WRITE, FSOM_CREATE_ALWAYS)) return false;
        save_opened_ = true;
        if(!fileFill_(save_, 0xFF, kSaveBlockSize)) return false;

        storage_file_close(save_);
        save_opened_ = false;

        if(!storage_file_open(save_, save_path_, FSAM_READ_WRITE, FSOM_OPEN_EXISTING))
            return false;
        save_opened_ = true;
    }

    return true;
}

void FX::freeCaches_() {
    if(cache_mem_) {
        free(cache_mem_);
        cache_mem_ = nullptr;
    }
    if(cache_base_) {
        free(cache_base_);
        cache_base_ = nullptr;
    }
    if(cache_len_) {
        free(cache_len_);
        cache_len_ = nullptr;
    }
    if(cache_age_) {
        free(cache_age_);
        cache_age_ = nullptr;
    }
    if(cache_valid_) {
        free(cache_valid_);
        cache_valid_ = nullptr;
    }

    last_hit_ = 0xFF;
    last_base_ = 0;
    seq_score_ = 0;

    streamReset_();
    pending_valid_ = false;
    pending_byte_ = 0xFF;
}

bool FX::allocCaches_() {
    freeCaches_();

    cache_mem_ = (uint8_t*)malloc((size_t)page_size_ * (size_t)cache_pages_);
    cache_base_ = (uint32_t*)malloc(sizeof(uint32_t) * cache_pages_);
    cache_len_ = (uint16_t*)malloc(sizeof(uint16_t) * cache_pages_);
    cache_age_ = (uint32_t*)malloc(sizeof(uint32_t) * cache_pages_);
    cache_valid_ = (uint8_t*)malloc(sizeof(uint8_t) * cache_pages_);

    if(!cache_mem_ || !cache_base_ || !cache_len_ || !cache_age_ || !cache_valid_) {
        freeCaches_();
        return false;
    }

    for(uint8_t i = 0; i < cache_pages_; i++) {
        cache_valid_[i] = 0;
        cache_base_[i] = 0;
        cache_len_[i] = 0;
        cache_age_[i] = 0;
    }

    cache_age_ctr_ = 1;
    last_hit_ = 0xFF;
    last_base_ = 0;
    seq_score_ = 0;

    streamReset_();
    pending_valid_ = false;
    pending_byte_ = 0xFF;

    return true;
}

uint32_t FX::absDataOffset_(uint32_t address) {
    // File backend uses plain offsets inside fxdata.bin.
    // FX_DATA_PAGE is meaningful on Arduboy FX flashcart, not for local asset files.
    return address;
}

void FX::commit() {
}

bool FX::begin() {
    if(!ensureStorage_()) return false;
    if(!openData_()) return false;
    if(!openSave_()) return false;
    if(!allocCaches_()) return false;

    domain_ = Domain::Data;
    cur_abs_ = 0;

    streamReset_();
    pending_valid_ = false;
    pending_byte_ = 0xFF;

    return true;
}

bool FX::begin(uint16_t developmentDataPage) {
    programDataPage = developmentDataPage;
    return begin();
}

bool FX::begin(uint16_t developmentDataPage, uint16_t developmentSavePage) {
    programDataPage = developmentDataPage;
    programSavePage = developmentSavePage;
    return begin();
}

void FX::end() {
    if(data_) {
        storage_file_close(data_);
        storage_file_free(data_);
        data_ = nullptr;
    }
    if(save_) {
        storage_file_close(save_);
        storage_file_free(save_);
        save_ = nullptr;
    }
    data_opened_ = false;
    save_opened_ = false;

    if(storage_) {
        furi_record_close(RECORD_STORAGE);
        storage_ = nullptr;
    }

    freeCaches_();
}

bool FX::detect() {
    if(!data_opened_ && !openData_()) return false;
    uint8_t b = 0;
    return data_opened_ ? fileReadAt_(data_, 0, &b, 1) : false;
}

void FX::readJedecID(JedecID& id) {
    id.manufacturer = 0;
    id.device = 0;
    id.size = 0;
}
void FX::readJedecID(JedecID* id) {
    if(id) readJedecID(*id);
}

void FX::streamReset_() {
    stream_page_i_ = 0xFF;
    stream_base_ = 0;
    stream_len_ = 0;
    stream_abs_ = 0;
    stream_ptr_ = nullptr;
    stream_valid_ = false;
}

bool FX::dataPageHas_(uint32_t base) {
    if(last_hit_ != 0xFF && cache_valid_[last_hit_] && cache_base_[last_hit_] == base) return true;
    for(uint8_t i = 0; i < cache_pages_; i++) {
        if(cache_valid_[i] && cache_base_[i] == base) {
            last_hit_ = i;
            return true;
        }
    }
    return false;
}

uint8_t FX::dataPickVictim_() {
    uint8_t victim = 0;
    uint32_t best_age = 0xFFFFFFFFu;
    for(uint8_t i = 0; i < cache_pages_; i++) {
        if(!cache_valid_[i]) return i;
        uint32_t a = cache_age_[i];
        if(a < best_age) {
            best_age = a;
            victim = i;
        }
    }
    return victim;
}

bool FX::dataLoadPage_(uint32_t base, uint8_t page_i) {
    if(!data_opened_ && !openData_()) return false;
    if(!data_) return false;

    if(!storage_file_seek(data_, base, true)) return false;

    uint8_t* dst = cache_mem_ + ((size_t)page_i * (size_t)page_size_);
    size_t r = storage_file_read(data_, dst, page_size_);
    if(r == 0) return false;

    cache_valid_[page_i] = 1;
    cache_base_[page_i] = base;
    cache_len_[page_i] = (uint16_t)r;
    cache_age_[page_i] = cache_age_ctr_++;
    last_hit_ = page_i;

    return true;
}

void FX::dataMaybePrefetch_(uint32_t base) {
    if(cache_pages_ < 2) return;
    if(seq_score_ < 3) return;

    uint32_t next1 = base + page_size_;
    if(!dataPageHas_(next1)) {
        uint8_t v = dataPickVictim_();
        if(!(cache_valid_[v] && cache_base_[v] == base)) (void)dataLoadPage_(next1, v);
    }
}

bool FX::dataEnsurePageIndex_(uint32_t abs_off, uint8_t* out_index) {
    if(!cache_mem_ || !out_index) return false;

    uint32_t base = alignDown_(abs_off, page_size_);

    if(last_hit_ != 0xFF && cache_valid_[last_hit_] && cache_base_[last_hit_] == base) {
        cache_age_[last_hit_] = cache_age_ctr_++;
        *out_index = last_hit_;
        if(base == last_base_ + page_size_) {
            if(seq_score_ < 255) seq_score_++;
        } else
            seq_score_ = 0;
        last_base_ = base;
        dataMaybePrefetch_(base);
        return true;
    }

    for(uint8_t i = 0; i < cache_pages_; i++) {
        if(cache_valid_[i] && cache_base_[i] == base) {
            cache_age_[i] = cache_age_ctr_++;
            last_hit_ = i;
            *out_index = i;
            if(base == last_base_ + page_size_) {
                if(seq_score_ < 255) seq_score_++;
            } else
                seq_score_ = 0;
            last_base_ = base;
            dataMaybePrefetch_(base);
            return true;
        }
    }

    uint8_t victim = dataPickVictim_();
    if(!dataLoadPage_(base, victim)) return false;

    *out_index = victim;

    if(base == last_base_ + page_size_) {
        if(seq_score_ < 255) seq_score_++;
    } else
        seq_score_ = 0;
    last_base_ = base;
    dataMaybePrefetch_(base);

    return true;
}

bool FX::streamEnsureAbs_(uint32_t abs_off) {
    if(stream_valid_) {
        uint32_t end = stream_base_ + (uint32_t)stream_len_;
        if(abs_off >= stream_base_ && abs_off < end) {
            stream_abs_ = abs_off;
            return true;
        }
    }
    uint8_t page_i = 0xFF;
    if(!dataEnsurePageIndex_(abs_off, &page_i)) return false;
    stream_page_i_ = page_i;
    stream_base_ = cache_base_[page_i];
    stream_len_ = cache_len_[page_i];
    stream_ptr_ = cache_mem_ + ((size_t)page_i * (size_t)page_size_);
    stream_valid_ = true;
    stream_abs_ = abs_off;
    return true;
}

bool FX::streamEnsureAbsFast_(uint32_t abs_off) {
    if(stream_valid_) {
        uint32_t end = stream_base_ + (uint32_t)stream_len_;
        if(abs_off >= stream_base_ && abs_off < end) {
            stream_abs_ = abs_off;
            return true;
        }
    }
    return streamEnsureAbs_(abs_off);
}

uint8_t FX::streamReadU8Fast_() {
    uint32_t idx = stream_abs_ - stream_base_;
    if(idx < stream_len_) {
        uint8_t v = stream_ptr_[idx];
        stream_abs_++;
        return v;
    }
    stream_valid_ = false;
    if(!streamEnsureAbs_(stream_abs_)) return 0xFF;
    idx = stream_abs_ - stream_base_;
    if(idx >= stream_len_) return 0xFF;
    uint8_t v = stream_ptr_[idx];
    stream_abs_++;
    return v;
}

void FX::primePendingData_() {
    pending_valid_ = false;
    streamReset_();
    if(!streamEnsureAbs_(cur_abs_)) {
        pending_byte_ = 0xFF;
        pending_valid_ = true;
        cur_abs_++;
        return;
    }
    pending_byte_ = streamReadU8Fast_();
    pending_valid_ = true;
    cur_abs_++;
}

void FX::primePendingSave_() {
    pending_valid_ = false;
    uint8_t v = 0xFF;
    if(save_opened_ && save_ && cur_abs_ < (uint32_t)kSaveBlockSize)
        (void)fileReadAt_(save_, cur_abs_, &v, 1);
    pending_byte_ = v;
    pending_valid_ = true;
    cur_abs_++;
}

void FX::seekData(uint32_t address) {
    domain_ = Domain::Data;
    cur_abs_ = absDataOffset_(address);
    primePendingData_();
}

void FX::seekDataArray(uint32_t address, uint8_t index, uint8_t offset, uint8_t elementSize) {
    uint32_t add = (elementSize == 0) ? (uint32_t)index * 256u :
                                        (uint32_t)index * (uint32_t)elementSize;
    add += (uint32_t)offset;
    seekData(address + add);
}

void FX::seekSave(uint32_t address) {
    domain_ = Domain::Save;
    cur_abs_ = address;
    primePendingSave_();
}

uint8_t FX::readPendingUInt8() {
    if(!pending_valid_) {
        if(domain_ == Domain::Data)
            primePendingData_();
        else
            primePendingSave_();
    }

    uint8_t out = pending_byte_;

    if(domain_ == Domain::Save) {
        uint8_t v = 0xFF;
        if(save_opened_ && save_ && cur_abs_ < (uint32_t)kSaveBlockSize)
            (void)fileReadAt_(save_, cur_abs_, &v, 1);
        pending_byte_ = v;
        pending_valid_ = true;
        cur_abs_++;
        return out;
    }

    if(!streamEnsureAbsFast_(cur_abs_)) {
        pending_byte_ = 0xFF;
        pending_valid_ = true;
        cur_abs_++;
        return out;
    }

    pending_byte_ = streamReadU8Fast_();
    pending_valid_ = true;
    cur_abs_++;
    return out;
}

uint8_t FX::readEnd() {
    if(!pending_valid_) return 0xFF;
    uint8_t out = pending_byte_;
    pending_valid_ = false;
    return out;
}

uint8_t FX::readPendingLastUInt8() {
    return readEnd();
}

uint16_t FX::readPendingUInt16() {
    uint16_t hi = readPendingUInt8();
    uint16_t lo = readPendingUInt8();
    return (hi << 8) | lo;
}

uint16_t FX::readPendingLastUInt16() {
    uint16_t hi = readPendingUInt8();
    uint16_t lo = readEnd();
    return (hi << 8) | lo;
}

uint32_t FX::readPendingUInt24() {
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readPendingUInt8();
    return (b2 << 16) | (b1 << 8) | b0;
}

uint32_t FX::readPendingLastUInt24() {
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readEnd();
    return (b2 << 16) | (b1 << 8) | b0;
}

uint32_t FX::readPendingUInt32() {
    uint32_t b3 = readPendingUInt8();
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readPendingUInt8();
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

uint32_t FX::readPendingLastUInt32() {
    uint32_t b3 = readPendingUInt8();
    uint32_t b2 = readPendingUInt8();
    uint32_t b1 = readPendingUInt8();
    uint32_t b0 = readEnd();
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
}

void FX::readBytes(uint8_t* buffer, size_t length) {
    if(!buffer || length == 0) return;

    // Keep semantics identical to pending-byte stream operations:
    // after reading N bytes, pending points to the next byte.
    for(size_t i = 0; i < length; i++) {
        buffer[i] = readPendingUInt8();
    }
}

void FX::readBytesEnd(uint8_t* buffer, size_t length) {
    if(!buffer || length == 0) return;
    if(length == 1) {
        buffer[0] = readEnd();
        return;
    }

    for(size_t i = 0; i < length - 1; i++) {
        buffer[i] = readPendingUInt8();
    }
    buffer[length - 1] = readEnd();
}

void FX::readDataBytes(uint32_t address, uint8_t* buffer, size_t length) {
    seekData(address);
    readBytesEnd(buffer, length);
}

void FX::readSaveBytes(uint32_t address, uint8_t* buffer, size_t length) {
    seekSave(address);
    readBytesEnd(buffer, length);
}

void FX::readDataArray(
    uint32_t address,
    uint8_t index,
    uint8_t offset,
    uint8_t elementSize,
    uint8_t* buffer,
    size_t length) {
    seekDataArray(address, index, offset, elementSize);
    readBytesEnd(buffer, length);
}

uint8_t FX::readIndexedUInt8(uint32_t address, uint8_t index) {
    seekDataArray(address, index, 0, 1);
    return readEnd();
}

uint16_t FX::readIndexedUInt16(uint32_t address, uint8_t index) {
    seekDataArray(address, index, 0, 2);
    return readPendingLastUInt16();
}

uint32_t FX::readIndexedUInt24(uint32_t address, uint8_t index) {
    seekDataArray(address, index, 0, 3);
    return readPendingLastUInt24();
}

uint32_t FX::readIndexedUInt32(uint32_t address, uint8_t index) {
    seekDataArray(address, index, 0, 4);
    return readPendingLastUInt32();
}

uint16_t FX::readSaveU16BE_(uint16_t off) {
    if(!save_opened_ || !save_) return 0xFFFF;
    if((uint32_t)off + 1u >= (uint32_t)kSaveBlockSize) return 0xFFFF;
    uint8_t b[2] = {0xFF, 0xFF};
    if(!fileReadAt_(save_, off, b, 2)) return 0xFFFF;
    return ((uint16_t)b[0] << 8) | b[1];
}

void FX::writeSaveU16BE_(uint16_t off, uint16_t v) {
    if(!save_opened_ || !save_) return;
    if((uint32_t)off + 1u >= (uint32_t)kSaveBlockSize) return;
    uint8_t b[2] = {(uint8_t)(v >> 8), (uint8_t)(v & 0xFF)};
    (void)fileWriteAt_(save_, off, b, 2);
}

bool FX::readDataAt_(uint32_t address, uint8_t* buffer, size_t length) {
    if(!buffer || length == 0) return true;

    uint32_t abs = absDataOffset_(address);
    size_t done = 0;

    while(done < length) {
        uint8_t page_i = 0xFF;
        if(!dataEnsurePageIndex_(abs, &page_i)) return false;

        const uint32_t base = cache_base_[page_i];
        const uint16_t len = cache_len_[page_i];
        if(abs < base) return false;

        const uint32_t off = abs - base;
        if(off >= len) return false;

        const size_t available = (size_t)len - (size_t)off;
        const size_t chunk = fx_min_sz(length - done, available);

        const uint8_t* src = cache_mem_ + ((size_t)page_i * (size_t)page_size_) + (size_t)off;
        memcpy(buffer + done, src, chunk);

        done += chunk;
        abs += (uint32_t)chunk;
    }

    return true;
}

const uint8_t* FX::dataPtrAt_(uint32_t address, size_t length) {
    if(length == 0) return nullptr;

    const uint32_t abs = absDataOffset_(address);
    uint8_t page_i = 0xFF;
    if(!dataEnsurePageIndex_(abs, &page_i)) return nullptr;

    const uint32_t base = cache_base_[page_i];
    const uint16_t len = cache_len_[page_i];
    if(abs < base) return nullptr;

    const uint32_t off = abs - base;
    if(off >= len) return nullptr;
    if((size_t)len - (size_t)off < length) return nullptr;

    return cache_mem_ + ((size_t)page_i * (size_t)page_size_) + (size_t)off;
}

void FX::eraseSaveBlock(uint16_t) {
    if(!save_opened_ || !save_) return;
    (void)storage_file_seek(save_, 0, true);
    (void)fileFill_(save_, 0xFF, kSaveBlockSize);
}

uint8_t FX::loadGameState(uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return 0;
    if(!save_opened_ || !save_) return 0;
    if(size > 4094u) return 0;

    // AVR semantics: scan [size_be16][payload] chain, keep latest matching record.
    uint16_t addr = 0;
    uint8_t loaded = 0;
    for(;;) {
        if((uint32_t)addr + 2u > (uint32_t)kSaveBlockSize) break;
        const uint16_t rec_size = readSaveU16BE_(addr);
        if(rec_size != (uint16_t)size) break;

        const uint32_t payload_off = (uint32_t)addr + 2u;
        const uint32_t next = payload_off + (uint32_t)size;
        if(next > (uint32_t)kSaveBlockSize) break;

        if(!fileReadAt_(save_, payload_off, gameState, size)) break;
        loaded = 1;
        addr = (uint16_t)next;
    }
    return loaded;
}

void FX::saveGameState(const uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return;
    if(!save_opened_ || !save_) return;
    if(size > 4094u) return;

    // AVR semantics: locate end of previous same-sized records.
    uint16_t addr = 0;
    for(;;) {
        if((uint32_t)addr + 2u > (uint32_t)kSaveBlockSize) break;
        if(readSaveU16BE_(addr) != (uint16_t)size) break;

        const uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > (uint32_t)kSaveBlockSize) break;
        addr = (uint16_t)next;
    }

    // Keep last 2 bytes as erased (0xFF), same as original FX block rule.
    if(((uint32_t)addr + 2u + (uint32_t)size) > 4094u) {
        eraseSaveBlock(0);
        addr = 0;
    }

    // Write record in original order: size header, then payload.
    writeSaveU16BE_(addr, (uint16_t)size);
    (void)fileWriteAt_(save_, (uint32_t)addr + 2u, gameState, size);
}

void FX::warmUpData(uint32_t address, size_t length) {
    if(!data_opened_ && !openData_()) return;
    if(page_size_ == 0) return;

    uint32_t abs = absDataOffset_(address);
    uint32_t end = abs + (uint32_t)length;
    uint32_t p = alignDown_(abs, page_size_);

    while(p < end) {
        uint8_t page_i = 0xFF;
        (void)dataEnsurePageIndex_(p, &page_i);
        p += page_size_;
    }
}

void FX::waitWhileBusy() {
}

void FX::writeSavePage(uint16_t page, const uint8_t* buffer) {
    if(!save_opened_ || !save_ || !buffer) return;
    const uint32_t off = (uint32_t)page * 256u;
    if(off >= (uint32_t)kSaveBlockSize) return;
    (void)fileWriteAt_(save_, off, buffer, fx_min_sz(256u, (size_t)(kSaveBlockSize - off)));
}

void FX::setFrame(uint24_t frame_addr, uint8_t frame_count) {
    frame_base_addr_ = frame_addr;
    frame_addr_ = frame_addr;
    frame_count_ = frame_count;
    frame_idx_ = 0;
}

bool FX::drawFrame() {
    // Frame script record layout in fxdata:
    // int16_be x, int16_be y, uint24_be image, uint8 frame, uint8 mode
    static constexpr uint8_t kRecordSize = 9;
    static constexpr uint16_t kMaxRecordsPerFrame = 512;

    const uint32_t current_frame_addr = frame_addr_;
    uint32_t cursor = current_frame_addr;
    uint8_t rec[kRecordSize];
    bool last_frame = false;

    for(uint16_t n = 0; n < kMaxRecordsPerFrame; n++) {
        if(!readDataAt_(cursor, rec, sizeof(rec))) return false;

        const int16_t x = (int16_t)fx_be16(&rec[0]);
        const int16_t y = (int16_t)fx_be16(&rec[2]);
        const uint24_t image_addr = ((uint32_t)rec[4] << 16) | ((uint32_t)rec[5] << 8) | rec[6];
        const uint8_t frame = rec[7];
        const uint8_t mode = rec[8];

        drawBitmap(x, y, image_addr, frame, mode);
        cursor += kRecordSize;

        const bool end_of_frame = (mode & 0x40u) != 0;
        last_frame = (mode & 0x80u) != 0;
        if(end_of_frame || last_frame) {
            break;
        }
    }

    // `frame_count_` here is a frame hold/delay parameter from TitleFrameIndexTable.
    // Each logical frame is shown for (frame_count_ + 1) ticks.
    if(frame_idx_ >= frame_count_) {
        frame_idx_ = 0;
        frame_addr_ = last_frame ? frame_base_addr_ : (uint24_t)cursor;
    } else {
        frame_idx_++;
        frame_addr_ = (uint24_t)current_frame_addr;
    }

    // Signal completion when a *_last frame is reached.
    return !last_frame;
}

bool FX::drawFrame(uint24_t frame_addr) {
    setFrame(frame_addr, 0);
    return drawFrame();
}

void FX::drawBitmap(int16_t x, int16_t y, uint24_t bitmap_addr, uint8_t frame, uint8_t mode) {
    uint8_t wh[4] = {0, 0, 0, 0};
    if(!readDataAt_(bitmap_addr, wh, sizeof(wh))) return;

    int16_t width = (int16_t)fx_be16(&wh[0]);
    int16_t height = (int16_t)fx_be16(&wh[2]);
    if(width <= 0 || height <= 0) return;
    if(x + width <= 0 || x >= WIDTH || y + height <= 0 || y >= HEIGHT) return;

    int16_t skipleft = 0;
    uint8_t renderwidth = 0;
    if(x < 0) {
        skipleft = -x;
        renderwidth = (uint8_t)(((width - skipleft) < WIDTH) ? (width - skipleft) : WIDTH);
    } else {
        renderwidth = (uint8_t)(((x + width) > WIDTH) ? (WIDTH - x) : width);
    }
    if(renderwidth == 0) return;

    int16_t skiptop = 0;
    int16_t renderheight = 0;
    if(y < 0) {
        int16_t skiptop_px = (int16_t)((-y) & ~7);
        renderheight = (int16_t)(((height - skiptop_px) <= HEIGHT) ? (height - skiptop_px) :
                                                                     (HEIGHT + ((-y) & 7)));
        skiptop = (int16_t)(skiptop_px >> 3);
    } else {
        skiptop = 0;
        renderheight = (int16_t)(((y + height) > HEIGHT) ? (HEIGHT - y) : height);
    }
    if(renderheight <= 0) return;

    const uint16_t rows_per_frame = (uint16_t)((height + 7) >> 3);
    uint32_t offset = (uint32_t)((uint32_t)frame * (uint32_t)rows_per_frame + (uint32_t)skiptop) *
                          (uint32_t)width +
                      (uint32_t)skipleft;
    if(mode & dbmMasked) {
        offset += offset;
        width += width;
    }

    uint32_t address = bitmap_addr + 4u + offset;
    int16_t displayrow = (int16_t)((y >> 3) + skiptop);
    const int16_t base_x = (int16_t)(x + skipleft);

    const uint8_t shift = (uint8_t)(y & 7);
    const uint8_t yshift = (uint8_t)(1u << shift);
    const uint8_t lastmask = (uint8_t)((height & 7) ? ((1u << (height & 7)) - 1u) : 0xFFu);
    uint8_t* const screen = arduboy.getBuffer();
    if(!screen) return;

    uint8_t rowbuf_local[256];
    uint8_t* rowbuf = rowbuf_local;
    size_t rowbuf_cap = sizeof(rowbuf_local);

    while(renderheight > 0) {
        const uint8_t rowmask = (renderheight < 8) ? lastmask : 0xFFu;
        const bool extra_row = (yshift != 1u) && (displayrow < ((HEIGHT / 8) - 1));
        const uint32_t row_bytes = (uint32_t)width;

        if(row_bytes == 0) break;
        if(row_bytes > rowbuf_cap) {
            if(rowbuf != rowbuf_local) free(rowbuf);
            rowbuf = (uint8_t*)malloc((size_t)row_bytes);
            if(!rowbuf) return;
            rowbuf_cap = (size_t)row_bytes;
        }
        if(!readDataAt_(address, rowbuf, (size_t)row_bytes)) {
            if(rowbuf != rowbuf_local) free(rowbuf);
            return;
        }
        address += row_bytes;

        uint16_t src = 0;
        for(uint8_t c = 0; c < renderwidth; c++) {
            uint8_t bitmapbyte = rowbuf[src++];
            if(mode & (1u << dbfReverseBlack)) bitmapbyte ^= rowmask;

            uint8_t maskbyte = rowmask;
            if(mode & (1u << dbfWhiteBlack)) maskbyte = bitmapbyte;
            if(mode & (1u << dbfBlack)) bitmapbyte = 0;

            const uint16_t bitmap = (uint16_t)bitmapbyte * (uint16_t)yshift;

            if(mode & dbmMasked) {
                uint8_t tmp = rowbuf[src++];
                if((mode & (1u << dbfWhiteBlack)) == 0u) maskbyte = tmp;
            }

            const uint16_t mask = (uint16_t)maskbyte * (uint16_t)yshift;
            const int16_t sx = (mode & (1u << dbfFlip)) ? (int16_t)(base_x + renderwidth - 1 - c) :
                                                          (int16_t)(base_x + c);
            if((uint16_t)sx >= WIDTH) continue;

            if((uint16_t)displayrow < (HEIGHT / 8)) {
                const uint16_t idx = (uint16_t)(displayrow * WIDTH + sx);
                uint8_t display = screen[idx];
                uint8_t pixels = (uint8_t)(bitmap & 0xFFu);
                if((mode & (1u << dbfInvert)) == 0u) pixels ^= display;
                pixels &= (uint8_t)(mask & 0xFFu);
                pixels ^= display;
                screen[idx] = pixels;
            }

            if(extra_row) {
                const int16_t row2 = (int16_t)(displayrow + 1);
                if((uint16_t)row2 >= (HEIGHT / 8)) continue;
                const uint16_t idx2 = (uint16_t)(row2 * WIDTH + sx);
                uint8_t display = screen[idx2];
                uint8_t pixels = (uint8_t)(bitmap >> 8);
                if((mode & (1u << dbfInvert)) == 0u) pixels ^= display;
                pixels &= (uint8_t)(mask >> 8);
                pixels ^= display;
                screen[idx2] = pixels;
            }
        }

        displayrow++;
        renderheight -= 8;
    }

    if(rowbuf != rowbuf_local) free(rowbuf);
}

void FX::display(bool clear) {
    arduboy.display(clear);
}

void FX::enableOLED() {
}

void FX::disableOLED() {
}
