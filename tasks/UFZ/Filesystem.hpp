#pragma once
#include "Common.hpp"
#include <vector>

namespace UFZ
{
    class Filesystem;

    class File
    {
    public:
        File() = default;
        explicit File(const Filesystem& store) noexcept;
        File(const UFZ::Filesystem& store, const char* path, FS_AccessMode accessMode, FS_OpenMode openMode) noexcept;
        bool open(const UFZ::Filesystem& store, const char* path, FS_AccessMode accessMode, FS_OpenMode openMode) noexcept;

        [[nodiscard]] bool isOpen() const noexcept;
        [[nodiscard]] bool isDirectory() const noexcept;

        size_t read(void* buffer, size_t bytesToRead) const noexcept;

        template<typename T>
        size_t read(std::vector<T>& buffer, size_t chunkSize = 128) noexcept
        {
            size_t sz = 0;
            buffer.resize(buffer.size() + chunkSize);

            while (const size_t a = read(buffer.data() + sz, chunkSize) != 0)
            {
                sz += a;
                buffer.resize(buffer.size() + chunkSize);
            }
            buffer.shrink_to_fit();
            return sz;
        }

        size_t write(const void* buffer, size_t bytesToWrite) const noexcept;

        template<typename T>
        size_t write(const std::vector<T>& buffer) noexcept
        {
            return write(buffer.data(), buffer.size());
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

        void init() noexcept;
        void free() noexcept;
    };

    class Directory
    {
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