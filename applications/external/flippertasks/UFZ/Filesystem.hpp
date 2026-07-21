#pragma once
#include "Common.hpp"
#include <vector>

namespace UFZ {
class Filesystem;

class File {
public:
    File() = default;
    explicit File(const Filesystem& store) noexcept;
    File(
        const UFZ::Filesystem& store,
        const char* path,
        FS_AccessMode accessMode,
        FS_OpenMode openMode) noexcept;

    // Owns a raw ::File* freed in the destructor; copying would double-close it.
    // Declaring the copy operations deleted also suppresses the implicit moves,
    // so a File cannot be copied or moved (the app only uses scoped locals).
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    bool open(
        const UFZ::Filesystem& store,
        const char* path,
        FS_AccessMode accessMode,
        FS_OpenMode openMode) noexcept;

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] bool isDirectory() const noexcept;

    size_t read(void* buffer, size_t bytesToRead) const noexcept;

    // Appends the file contents to buffer, growing it chunkSize elements at a time.
    // chunkSize is a count of T elements; each iteration reads up to that many
    // elements and stops on the first short read (end of file). Returns bytes read.
    template <typename T>
    size_t read(std::vector<T>& buffer, size_t chunkSize = 128) const noexcept {
        // A zero chunk never advances the read cursor; the loop below would spin forever.
        if(chunkSize == 0) return 0;

        const size_t start = buffer.size();
        const size_t chunkBytes = chunkSize * sizeof(T);
        size_t elementsRead = 0;
        size_t bytesRead = 0;
        size_t bytes;

        do {
            buffer.resize(start + elementsRead + chunkSize);
            bytes = read(buffer.data() + start + elementsRead, chunkBytes);
            bytesRead += bytes;
            elementsRead += bytes / sizeof(T);
        } while(bytes == chunkBytes);

        // Only whole elements fit in a vector<T>; a trailing partial element (file size
        // not a multiple of sizeof(T)) is dropped, but the returned byte count reflects
        // everything read, so the caller can detect it (bytesRead % sizeof(T) != 0).
        buffer.resize(start + elementsRead);
        return bytesRead;
    }

    size_t write(const void* buffer, size_t bytesToWrite) const noexcept;

    template <typename T>
    size_t write(const std::vector<T>& buffer) const noexcept {
        return write(buffer.data(), buffer.size() * sizeof(T));
    }

    [[nodiscard]] bool seek(uint32_t offset, bool bFromStart) const noexcept;
    [[nodiscard]] uint64_t tell() const noexcept;
    [[nodiscard]] bool truncate() const noexcept;
    [[nodiscard]] uint64_t size() const noexcept;

    [[nodiscard]] bool sync() const noexcept;
    [[nodiscard]] bool eof() const noexcept;

    static bool copyToFile(const File& source, const File& destination, size_t size) noexcept;

    void close() noexcept;

    ~File() noexcept;

private:
    friend class Filesystem;
    friend class Directory;

    ::File* file = nullptr;
    Filesystem* storage = nullptr;

    // True while `file` is open as a directory (via Directory::open): selects
    // storage_dir_close over storage_file_close at teardown so the handle is not
    // closed as the wrong stream type. Reset by free().
    bool bDirectory = false;

    void init() noexcept;
    void free() noexcept;
};

class Directory {
public:
    Directory() = default;
    explicit Directory(File& f) noexcept;

    bool open(File& f, const char* path) noexcept;
    [[nodiscard]] bool close() const noexcept;

    bool read(FileInfo* info, char* name, uint16_t nameLength) const noexcept;
    [[nodiscard]] bool rewind() const noexcept;

private:
    friend class Filesystem;

    File* file{};
};
}
