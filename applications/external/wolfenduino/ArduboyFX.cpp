#include "lib/ArduboyFX.h"

static inline size_t fx_min_sz(size_t a, size_t b) {
    return a < b ? a : b;
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

uint32_t FX::page_size_ = 4096;
uint8_t FX::cache_pages_ = 10;

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
    return address + ((uint32_t)programDataPage << 8);
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
    uint16_t lo = readPendingUInt8();
    uint16_t hi = readPendingUInt8();
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

    if(domain_ == Domain::Save) {
        for(size_t i = 0; i < length; i++)
            buffer[i] = readPendingUInt8();
        return;
    }

    if(!pending_valid_) {
        primePendingData_();
    }

    size_t remain = length;
    size_t offset = 0;

    while(remain > 0) {
        if(!streamEnsureAbsFast_(cur_abs_ - 1)) {
            for(size_t i = offset; i < length; i++)
                buffer[i] = readPendingUInt8();
            return;
        }

        uint32_t page_offset = (cur_abs_ - 1) - stream_base_;
        size_t available = stream_len_ - page_offset;
        size_t chunk = (remain < available) ? remain : available;

        memcpy(buffer + offset, stream_ptr_ + page_offset, chunk);

        offset += chunk;
        remain -= chunk;
        cur_abs_ += chunk;
        stream_abs_ = cur_abs_;
    }

    if(remain == 0 && cur_abs_ < 0xFFFFFFFFu) {
        if(streamEnsureAbsFast_(cur_abs_)) {
            pending_byte_ = streamReadU8Fast_();
            pending_valid_ = true;
            cur_abs_++;
        } else {
            pending_byte_ = 0xFF;
            pending_valid_ = true;
            cur_abs_++;
        }
    }
}

void FX::readBytesEnd(uint8_t* buffer, size_t length) {
    if(!buffer || length == 0) return;
    if(length == 1) {
        buffer[0] = readEnd();
        return;
    }

    if(domain_ == Domain::Save) {
        for(size_t i = 0; i < length - 1; i++)
            buffer[i] = readPendingUInt8();
        buffer[length - 1] = readEnd();
        return;
    }

    if(!pending_valid_) {
        primePendingData_();
    }

    size_t remain = length;
    size_t offset = 0;

    while(remain > 0) {
        if(!streamEnsureAbsFast_(cur_abs_ - 1)) {
            for(size_t i = offset; i < length - 1; i++)
                buffer[i] = readPendingUInt8();
            buffer[length - 1] = readEnd();
            return;
        }

        uint32_t page_offset = (cur_abs_ - 1) - stream_base_;
        size_t available = stream_len_ - page_offset;
        size_t chunk = (remain < available) ? remain : available;

        memcpy(buffer + offset, stream_ptr_ + page_offset, chunk);

        offset += chunk;
        remain -= chunk;
        cur_abs_ += chunk;
        stream_abs_ = cur_abs_;
    }

    pending_valid_ = false;
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

void FX::eraseSaveBlock(uint16_t) {
    if(!save_opened_ || !save_) return;
    (void)storage_file_seek(save_, 0, true);
    (void)fileFill_(save_, 0xFF, kSaveBlockSize);
}

uint8_t FX::loadGameState(uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return 0;
    if(!save_opened_ || !save_) return 0;

    uint16_t addr = 0;
    uint8_t loaded = 0;

    for(;;) {
        if((uint32_t)addr + 2u > (uint32_t)kSaveBlockSize) break;
        uint16_t s = readSaveU16BE_(addr);
        if(s != (uint16_t)size) break;

        uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > (uint32_t)kSaveBlockSize) break;

        if(!fileReadAt_(save_, (uint32_t)addr + 2u, gameState, size)) break;
        loaded = 1;
        addr = (uint16_t)next;
    }

    return loaded;
}

void FX::saveGameState(const uint8_t* gameState, size_t size) {
    if(!gameState || size == 0) return;
    if(!save_opened_ || !save_) return;

    uint16_t addr = 0;

    for(;;) {
        if((uint32_t)addr + 2u > (uint32_t)kSaveBlockSize) {
            addr = 0;
            break;
        }
        uint16_t s = readSaveU16BE_(addr);
        if(s != (uint16_t)size) break;

        uint32_t next = (uint32_t)addr + 2u + (uint32_t)size;
        if(next > (uint32_t)kSaveBlockSize) {
            addr = 0;
            break;
        }
        addr = (uint16_t)next;
    }

    if(((uint32_t)addr + 2u + (uint32_t)size) > 4094u) {
        eraseSaveBlock(0);
        addr = 0;
    }

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
