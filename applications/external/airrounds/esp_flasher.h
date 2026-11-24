#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace airrounds {
namespace flipper {

// ESP32-S2 ROM serial bootloader loader. Clean-room from the public ROM loader protocol, for which
// esptool is the reference specification; it copies no GPL code. The logic is pure and the I/O is
// injected, so the host tests drive it with fakes, without furi_hal or hardware. The Flipper app
// wires the callbacks to the UART, the two boot GPIO lines, and the microSD image.

// The I/O the flasher needs. Every call takes the opaque context the app set.
struct FlasherIo {
    void* context = nullptr;
    // Write a byte run to the board UART.
    void (*write)(void*, const uint8_t*, size_t) = nullptr;
    // Read one byte; return false on a timeout before the deadline.
    bool (*readByte)(void*, uint8_t*, uint32_t timeoutMs) = nullptr;
    // Drive the boot lines: bootDown pulls IO0 low to enter the ROM loader, then pulses EN; false
    // drives IO0 high and pulses EN to boot the application.
    void (*boot)(void*, bool bootDown) = nullptr;
    // Read size bytes of the image at offset into out; return the count read, 0 on an error.
    size_t (*readImage)(void*, uint32_t offset, uint8_t* out, size_t size) = nullptr;
};

// The manifest fields the flasher needs. valid is true only when offset, size, and a 32-hex md5 are
// all present; active marks the transmit image, which the app gates behind the attestation.
struct FlashManifest {
    uint32_t offset = 0;
    uint32_t size = 0;
    char md5[33] = {};
    bool active = false;
    bool valid = false;
};

enum class FlashStage {
    idle,
    enter,
    sync,
    begin,
    write,
    end,
    verify,
    reset,
    done
};
enum class FlashError {
    none,
    io,
    sync,
    begin,
    write,
    end,
    mismatch,
    image,
    manifest,
    overflow
};

namespace detail {

inline uint32_t readU32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8U) |
           (static_cast<uint32_t>(bytes[2]) << 16U) | (static_cast<uint32_t>(bytes[3]) << 24U);
}

inline void writeU32(uint8_t* bytes, uint32_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8U);
    bytes[2] = static_cast<uint8_t>(value >> 16U);
    bytes[3] = static_cast<uint8_t>(value >> 24U);
}

inline bool isHex(char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F');
}

inline char lowerHex(char character) {
    return (character >= 'A' && character <= 'F') ? static_cast<char>(character - 'A' + 'a') :
                                                    character;
}

// Find the value of "<key>" in the manifest JSON. The scan is global, so a key nested under an
// object is found the same as a top-level one. Returns the pointer just past the colon, or nullptr.
inline const char* findValue(const char* json, const char* key) {
    const size_t keyLength = strlen(key);
    for(const char* cursor = strchr(json, '"'); cursor != nullptr;
        cursor = strchr(cursor + 1, '"')) {
        if(strncmp(cursor + 1, key, keyLength) == 0 && cursor[1 + keyLength] == '"') {
            const char* colon = strchr(cursor + 1 + keyLength, ':');
            if(colon == nullptr) {
                return nullptr;
            }
            ++colon;
            while(*colon == ' ' || *colon == '\t') {
                ++colon;
            }
            return colon;
        }
    }
    return nullptr;
}

inline bool readNumber(const char* json, const char* key, uint32_t& value) {
    const char* cursor = findValue(json, key);
    if(cursor == nullptr || *cursor < '0' || *cursor > '9') {
        return false;
    }
    uint32_t parsed = 0;
    for(; *cursor >= '0' && *cursor <= '9'; ++cursor) {
        parsed = parsed * 10U + static_cast<uint32_t>(*cursor - '0');
    }
    value = parsed;
    return true;
}

// Copy a quoted string value into out, up to cap - 1 characters; out is always terminated.
inline bool readString(const char* json, const char* key, char* out, size_t cap) {
    const char* cursor = findValue(json, key);
    if(cursor == nullptr || *cursor != '"') {
        return false;
    }
    ++cursor;
    size_t length = 0;
    for(; *cursor != '"' && *cursor != '\0'; ++cursor) {
        if(length + 1 >= cap) {
            return false;
        }
        out[length++] = *cursor;
    }
    out[length] = '\0';
    return *cursor == '"';
}

} // namespace detail

// Parse the manifest the release ships beside the image. The CI writes nested JSON, with the image
// fields under an "image" object; the global key scan reads them without walking the hierarchy.
inline FlashManifest parseManifest(const char* json) {
    FlashManifest manifest;
    char variant[8] = {};
    const bool numbers = detail::readNumber(json, "offset", manifest.offset) &&
                         detail::readNumber(json, "size", manifest.size);
    const bool digest = detail::readString(json, "md5", manifest.md5, sizeof(manifest.md5));
    if(!numbers || !digest || manifest.size == 0 || strlen(manifest.md5) != 32) {
        return manifest;
    }
    for(char& character : manifest.md5) {
        if(character != '\0' && !detail::isHex(character)) {
            return manifest;
        }
        character = detail::lowerHex(character);
    }
    if(detail::readString(json, "variant", variant, sizeof(variant))) {
        manifest.active = strcmp(variant, "active") == 0;
    }
    manifest.valid = true;
    return manifest;
}

class EspFlasher {
public:
    static constexpr size_t blockSize = 1024;

    explicit EspFlasher(const FlasherIo& io)
        : io_(io) {
    }

    using Progress = void (*)(void*, FlashStage, uint32_t done, uint32_t total);

    // Flash the image the manifest names. On success the board holds the new firmware, verified by
    // the ROM MD5, and the flow has reset it into the application. On any failure the board stays
    // in the ROM loader, reflashable on a retry, so a bad write never bricks it.
    FlashError flash(
        const FlashManifest& manifest,
        Progress progress = nullptr,
        void* progressContext = nullptr) {
        if(!manifest.valid) {
            return fail(FlashError::manifest);
        }
        progress_ = progress;
        progressContext_ = progressContext;
        const uint32_t blocks = static_cast<uint32_t>((manifest.size + blockSize - 1) / blockSize);
        enter();
        if(!sync()) {
            return fail(FlashError::sync);
        }
        if(!flashBegin(manifest.size, blocks, manifest.offset)) {
            return fail(FlashError::begin);
        }
        const FlashError written = writeBlocks(blocks, manifest.size);
        if(written != FlashError::none) {
            return fail(written);
        }
        if(!flashEnd()) {
            return fail(FlashError::end);
        }
        if(!verifyMd5(manifest.offset, manifest.size, manifest.md5)) {
            return fail(FlashError::mismatch);
        }
        reset();
        return FlashError::none;
    }

    FlashStage stage() const {
        return stage_;
    }

private:
    enum : uint8_t {
        cmdFlashBegin = 0x02,
        cmdFlashData = 0x03,
        cmdFlashEnd = 0x04,
        cmdSync = 0x08,
        cmdMd5 = 0x13
    };
    enum : uint8_t {
        slipEnd = 0xC0,
        slipEsc = 0xDB,
        slipEscEnd = 0xDC,
        slipEscEsc = 0xDD
    };
    static constexpr size_t dataHeader = 16;
    static constexpr size_t maxData = dataHeader + blockSize;
    static constexpr size_t packetCapacity = 8 + maxData;
    static constexpr uint32_t shortTimeoutMs = 500;
    static constexpr uint32_t longTimeoutMs = 3000;

    void report(FlashStage reached, uint32_t done, uint32_t total) {
        stage_ = reached;
        if(progress_ != nullptr) {
            progress_(progressContext_, reached, done, total);
        }
    }

    FlashError fail(FlashError error) {
        error_ = error;
        return error;
    }

    void enter() {
        report(FlashStage::enter, 0, 0);
        io_.boot(io_.context, true);
    }

    void reset() {
        report(FlashStage::reset, 0, 0);
        io_.boot(io_.context, false);
        report(FlashStage::done, 0, 0);
    }

    bool sync() {
        report(FlashStage::sync, 0, 0);
        // The sync frame: two magic bytes, then 0x55 padding. The ROM answers after a few tries.
        uint8_t data[36];
        data[0] = 0x07;
        data[1] = 0x07;
        data[2] = 0x12;
        data[3] = 0x20;
        memset(data + 4, 0x55, sizeof(data) - 4);
        for(int attempt = 0; attempt < 8; ++attempt) {
            if(command(cmdSync, data, sizeof(data), 0, shortTimeoutMs)) {
                return true;
            }
        }
        return false;
    }

    bool flashBegin(uint32_t eraseSize, uint32_t blocks, uint32_t offset) {
        report(FlashStage::begin, 0, blocks);
        uint8_t data[16];
        detail::writeU32(data, eraseSize);
        detail::writeU32(data + 4, blocks);
        detail::writeU32(data + 8, blockSize);
        detail::writeU32(data + 12, offset);
        return command(cmdFlashBegin, data, sizeof(data), 0, longTimeoutMs);
    }

    FlashError writeBlocks(uint32_t blocks, uint32_t size) {
        report(FlashStage::write, 0, blocks);
        for(uint32_t sequence = 0; sequence < blocks; ++sequence) {
            const uint32_t offset = sequence * blockSize;
            const size_t want = size - offset < blockSize ? size - offset : blockSize;
            if(io_.readImage(io_.context, offset, payload_ + dataHeader, want) != want) {
                return FlashError::image;
            }
            // The ROM writes whole blocks, so pad the last block to the block size with 0xFF.
            memset(payload_ + dataHeader + want, 0xFF, blockSize - want);
            if(!flashBlock(sequence)) {
                return FlashError::write;
            }
            report(FlashStage::write, sequence + 1, blocks);
        }
        return FlashError::none;
    }

    bool flashBlock(uint32_t sequence) {
        detail::writeU32(payload_, blockSize);
        detail::writeU32(payload_ + 4, sequence);
        detail::writeU32(payload_ + 8, 0);
        detail::writeU32(payload_ + 12, 0);
        uint8_t checksum = 0xEF;
        for(size_t index = 0; index < blockSize; ++index) {
            checksum ^= payload_[dataHeader + index];
        }
        return command(cmdFlashData, payload_, maxData, checksum, longTimeoutMs);
    }

    bool flashEnd() {
        report(FlashStage::end, 0, 0);
        uint8_t data[4];
        // Flag 1 keeps the ROM loader running, so the MD5 verify can follow before the reset.
        detail::writeU32(data, 1);
        return command(cmdFlashEnd, data, sizeof(data), 0, shortTimeoutMs);
    }

    bool verifyMd5(uint32_t offset, uint32_t size, const char* expected) {
        report(FlashStage::verify, 0, 0);
        uint8_t data[16];
        detail::writeU32(data, offset);
        detail::writeU32(data + 4, size);
        detail::writeU32(data + 8, 0);
        detail::writeU32(data + 12, 0);
        uint8_t digest[40];
        size_t digestLength = 0;
        if(!command(
               cmdMd5,
               data,
               sizeof(data),
               0,
               longTimeoutMs,
               nullptr,
               digest,
               sizeof(digest),
               &digestLength) ||
           digestLength < 32) {
            return false;
        }
        for(size_t index = 0; index < 32; ++index) {
            if(detail::lowerHex(static_cast<char>(digest[index])) != expected[index]) {
                return false;
            }
        }
        return true;
    }

    // Send a command packet and read the matching response. value, respData, and respLength are
    // optional; respData holds the response payload without its two trailing status bytes.
    bool command(
        uint8_t cmd,
        const uint8_t* data,
        size_t size,
        uint8_t checksum,
        uint32_t timeoutMs,
        uint32_t* value = nullptr,
        uint8_t* respData = nullptr,
        size_t respCap = 0,
        size_t* respLength = nullptr) {
        if(8 + size > packetCapacity) {
            fail(FlashError::overflow);
            return false;
        }
        packet_[0] = 0x00;
        packet_[1] = cmd;
        packet_[2] = static_cast<uint8_t>(size);
        packet_[3] = static_cast<uint8_t>(size >> 8U);
        detail::writeU32(packet_ + 4, checksum);
        memcpy(packet_ + 8, data, size);
        slipWrite(packet_, 8 + size);
        return readResponse(cmd, timeoutMs, value, respData, respCap, respLength);
    }

    bool readResponse(
        uint8_t cmd,
        uint32_t timeoutMs,
        uint32_t* value,
        uint8_t* respData,
        size_t respCap,
        size_t* respLength) {
        for(int frame = 0; frame < 64; ++frame) {
            size_t length = 0;
            if(!slipRead(timeoutMs, length)) {
                return false;
            }
            if(length < 10 || response_[0] != 0x01 || response_[1] != cmd) {
                continue;
            }
            const size_t payload = static_cast<size_t>(response_[2]) |
                                   (static_cast<size_t>(response_[3]) << 8U);
            if(payload < 2 || 8 + payload > length) {
                continue;
            }
            // The last two payload bytes are the status and the error; status 0 is success.
            if(response_[8 + payload - 2] != 0) {
                return false;
            }
            if(value != nullptr) {
                *value = detail::readU32(response_ + 4);
            }
            if(respData != nullptr) {
                // Clamp to the caller's buffer: payload is bounded only by the 64-byte frame, so a
                // malformed or noisy response could otherwise copy past a smaller respData.
                const size_t bytes = payload - 2 < respCap ? payload - 2 : respCap;
                memcpy(respData, response_ + 8, bytes);
                if(respLength != nullptr) {
                    *respLength = bytes;
                }
            }
            return true;
        }
        return false;
    }

    void slipWrite(const uint8_t* data, size_t size) {
        size_t length = 0;
        frame_[length++] = slipEnd;
        for(size_t index = 0; index < size; ++index) {
            const uint8_t byte = data[index];
            if(byte == slipEnd) {
                frame_[length++] = slipEsc;
                frame_[length++] = slipEscEnd;
            } else if(byte == slipEsc) {
                frame_[length++] = slipEsc;
                frame_[length++] = slipEscEsc;
            } else {
                frame_[length++] = byte;
            }
        }
        frame_[length++] = slipEnd;
        io_.write(io_.context, frame_, length);
    }

    bool slipRead(uint32_t timeoutMs, size_t& length) {
        uint8_t byte = 0;
        do {
            if(!io_.readByte(io_.context, &byte, timeoutMs)) {
                return false;
            }
        } while(byte != slipEnd);
        length = 0;
        for(;;) {
            if(!io_.readByte(io_.context, &byte, timeoutMs)) {
                return false;
            }
            if(byte == slipEnd) {
                if(length == 0) {
                    continue; // A leading delimiter or an empty frame: keep reading.
                }
                return true;
            }
            if(byte == slipEsc) {
                if(!io_.readByte(io_.context, &byte, timeoutMs)) {
                    return false;
                }
                byte = byte == slipEscEnd ? static_cast<uint8_t>(slipEnd) :
                       byte == slipEscEsc ? static_cast<uint8_t>(slipEsc) :
                                            byte;
            }
            if(length >= sizeof(response_)) {
                return false;
            }
            response_[length++] = byte;
        }
    }

    FlasherIo io_;
    FlashStage stage_ = FlashStage::idle;
    FlashError error_ = FlashError::none;
    Progress progress_ = nullptr;
    void* progressContext_ = nullptr;
    uint8_t payload_[maxData]{};
    uint8_t packet_[packetCapacity]{};
    uint8_t frame_[2 + packetCapacity * 2]{};
    uint8_t response_[64]{};
};

} // namespace flipper
} // namespace airrounds
