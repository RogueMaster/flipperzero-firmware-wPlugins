#pragma once
#ifdef ARDULIB_USE_FX

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include <furi.h>
#include <storage/storage.h>
using uint24_t = uint32_t;

struct JedecID {
    uint8_t manufacturer;
    uint8_t device;
    uint8_t size;
};

class FX {
public:
    enum DrawMode : uint8_t {
        dbfWhiteBlack = 0,
        dbfInvert = 1,
        dbfBlack = 2,
        dbfReverseBlack = 3,
        dbfMasked = 4,
        dbfFlip = 5,
        dbfEndFrame = 6,
        dbfLastFrame = 7,

        dbmNormal = 0x00,
        dbmMasked = (1u << dbfMasked),
        dbmWhite = (1u << dbfWhiteBlack),

        dbmNormal_end = dbmNormal | (1u << dbfEndFrame),
        dbmNormal_last = dbmNormal | (1u << dbfLastFrame),
        dbmMasked_end = dbmMasked | (1u << dbfEndFrame),
        dbmMasked_last = dbmMasked | (1u << dbfLastFrame),
        dbmWhite_end = dbmWhite | (1u << dbfEndFrame),
        dbmWhite_last = dbmWhite | (1u << dbfLastFrame),
    };

    static uint16_t programDataPage;
    static uint16_t programSavePage;
    static void setCacheConfig(uint32_t page_size, uint8_t pages);
    static void setPaths(const char* data_bin_path, const char* save_path);

    static bool begin();
    static bool begin(uint16_t developmentDataPage);
    static bool begin(uint16_t developmentDataPage, uint16_t developmentSavePage);
    static void end();

    static bool detect();
    static void readJedecID(JedecID& id);
    static void readJedecID(JedecID* id);

    static void seekData(uint32_t address);
    static void seekDataArray(uint32_t address, uint8_t index, uint8_t offset, uint8_t elementSize);
    static void seekSave(uint32_t address);

    static uint8_t  readPendingUInt8();
    static uint8_t  readEnd();
    static uint8_t  readPendingLastUInt8();

    static uint16_t readPendingUInt16();
    static uint16_t readPendingLastUInt16();

    static uint32_t readPendingUInt24();
    static uint32_t readPendingLastUInt24();

    static uint32_t readPendingUInt32();
    static uint32_t readPendingLastUInt32();

    static void readBytes(uint8_t* buffer, size_t length);
    static void readBytesEnd(uint8_t* buffer, size_t length);

    static void readDataBytes(uint32_t address, uint8_t* buffer, size_t length);
    static void readSaveBytes(uint32_t address, uint8_t* buffer, size_t length);

    static void readDataArray(uint32_t address, uint8_t index, uint8_t offset, uint8_t elementSize,
                              uint8_t* buffer, size_t length);

    static uint8_t  readIndexedUInt8(uint32_t address, uint8_t index);
    static uint16_t readIndexedUInt16(uint32_t address, uint8_t index);
    static uint32_t readIndexedUInt24(uint32_t address, uint8_t index);
    static uint32_t readIndexedUInt32(uint32_t address, uint8_t index);

    static void commit();
    static void eraseSaveBlock(uint16_t block);
    static uint8_t loadGameState(uint8_t* gameState, size_t size);
    static void saveGameState(const uint8_t* gameState, size_t size);
    template <typename T>
    static uint8_t loadGameState(T& gameState) {
        return loadGameState((uint8_t*)&gameState, sizeof(T));
    }
    template <typename T>
    static void saveGameState(const T& gameState) {
        saveGameState((const uint8_t*)&gameState, sizeof(T));
    }

    static void setFrame(uint24_t frame_addr, uint8_t frame_count);
    static bool drawFrame();
    static bool drawFrame(uint24_t frame_addr);
    static void drawBitmap(int16_t x, int16_t y, uint24_t bitmap_addr, uint8_t frame, uint8_t mode);
    static void display(bool clear);
    static void enableOLED();
    static void disableOLED();

    static void warmUpData(uint32_t address, size_t length);
    static void waitWhileBusy();
    static void writeSavePage(uint16_t page, const uint8_t* buffer);


private:
    enum class Domain : uint8_t { Data, Save };

    static constexpr uint16_t kSaveBlockSize = 4096;
    static constexpr size_t kPathMax = 128;
    static constexpr const char* kDataPath = APP_ASSETS_PATH("fxdata.bin");
    static constexpr const char* kSavePath = APP_DATA_PATH("fxsave.bin");

    static Storage* storage_;
    static File*    data_;
    static File*    save_;
    static bool     data_opened_;
    static bool     save_opened_;
    static char     data_path_[kPathMax];
    static char     save_path_[kPathMax];

    static Domain   domain_;
    static uint32_t cur_abs_;
    static uint32_t page_size_;
    static uint8_t  cache_pages_;
    static uint8_t* cache_mem_;
    static uint32_t* cache_base_;
    static uint16_t* cache_len_;
    static uint32_t* cache_age_;
    static uint8_t* cache_valid_;
    static uint32_t cache_age_ctr_;
    static uint8_t  last_hit_;
    static uint32_t last_base_;
    static uint8_t  seq_score_;
    static uint8_t  stream_page_i_;
    static uint32_t stream_base_;
    static uint16_t stream_len_;
    static uint32_t stream_abs_;
    static uint8_t* stream_ptr_;
    static bool     stream_valid_;

    static uint32_t data_file_pos_;
    static bool     data_file_pos_valid_;

    static bool    pending_valid_;
    static uint8_t pending_byte_;
    static uint8_t data_read_buf_[256];
    static uint32_t data_read_buf_base_;
    static uint16_t data_read_buf_len_;
    static bool data_read_buf_valid_;

    static uint24_t frame_addr_;
    static uint24_t frame_base_addr_;
    static uint8_t frame_count_;
    static uint8_t frame_idx_;

    struct BitmapMetaCacheEntry {
        uint24_t addr;
        uint16_t w;
        uint16_t h;
        uint16_t pages;
        uint8_t valid;
    };
    static constexpr uint8_t kBitmapMetaCacheSize = 16;
    static BitmapMetaCacheEntry bitmap_meta_cache_[kBitmapMetaCacheSize];
    static uint8_t bitmap_meta_rr_;

    static bool ensureStorage_();
    static bool openData_();
    static bool openSave_();

    static bool fileFill_(File* f, uint8_t value, size_t len);
    static bool fileReadAt_(File* f, uint32_t off, void* out, size_t len);
    static size_t fileReadSomeAt_(File* f, uint32_t off, void* out, size_t len);
    static bool fileWriteAt_(File* f, uint32_t off, const void* in, size_t len);

    static void freeCaches_();
    static bool allocCaches_();
    static uint32_t alignDown_(uint32_t v, uint32_t a);
    static uint32_t alignUp_(uint32_t v, uint32_t a);
    static void streamReset_();
    static bool dataPageHas_(uint32_t base);
    static uint8_t dataPickVictim_();
    static bool dataLoadPage_(uint32_t base, uint8_t page_i);
    static void dataMaybePrefetch_(uint32_t base);
    static bool dataEnsurePageIndex_(uint32_t abs_off, uint8_t* out_index);
    static bool streamEnsureAbs_(uint32_t abs_off);
    static bool streamEnsureAbsFast_(uint32_t abs_off);
    static uint8_t streamReadU8Fast_();

    static uint32_t absDataOffset_(uint32_t address);

    static void primePendingData_();
    static void primePendingSave_();

    static uint16_t readSaveU16BE_(uint16_t off);
    static void writeSaveU16BE_(uint16_t off, uint16_t v);
    static bool readDataAt_(uint32_t address, uint8_t* buffer, size_t length);
    static const uint8_t* dataPtrAt_(uint32_t address, size_t length);
    static bool getBitmapMeta_(uint24_t bitmap_addr, uint16_t* out_w, uint16_t* out_h, uint16_t* out_pages);
    static void dataReadBufInvalidate_();
    static size_t dataReadSpanAt_(uint32_t abs, uint8_t* out, size_t len);
    static bool dataReadByteAt_(uint32_t abs, uint8_t* out);
};

constexpr uint8_t dbmNormal = FX::dbmNormal;
constexpr uint8_t dbmMasked = FX::dbmMasked;
constexpr uint8_t dbmNormal_end = FX::dbmNormal_end;
constexpr uint8_t dbmNormal_last = FX::dbmNormal_last;
constexpr uint8_t dbmMasked_end = FX::dbmMasked_end;
constexpr uint8_t dbmMasked_last = FX::dbmMasked_last;

#endif // ARDULIB_USE_FX
