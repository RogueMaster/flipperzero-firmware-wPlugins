#include "Filesystem.hpp"

// =====================================================================================================================
// ====================================================== Storage ======================================================
// =====================================================================================================================

void UFZ::Filesystem::init() noexcept
{
    storage = static_cast<Storage*>(furi_record_open(RECORD_STORAGE));
}

FS_Error UFZ::Filesystem::timestamp(const char* path, uint32_t* timestamp) const noexcept
{
    return storage_common_timestamp(storage, path, timestamp);
}

FS_Error UFZ::Filesystem::stat(const char* path, FileInfo* fileInfo) const noexcept
{
    return storage_common_stat(storage, path, fileInfo);
}

bool UFZ::Filesystem::exists(const char* path) const noexcept
{
    return storage_common_exists(storage, path);
}

FS_Error UFZ::Filesystem::remove(const char* path) const noexcept
{
    return storage_common_remove(storage, path);
}

FS_Error UFZ::Filesystem::rename(const char* oldPath, const char* newPath) const noexcept
{
    return storage_common_rename(storage, oldPath, newPath);
}

FS_Error UFZ::Filesystem::copy(const char* oldPath, const char* newPath) const noexcept
{
    return storage_common_copy(storage, oldPath, newPath);
}

FS_Error UFZ::Filesystem::merge(const char* oldPath, const char* newPath) const noexcept
{
    return storage_common_merge(storage, oldPath, newPath);
}

FS_Error UFZ::Filesystem::migrate(const char* source, const char* destination) const noexcept
{
    return storage_common_migrate(storage, source, destination);
}

FS_Error UFZ::Filesystem::mkdir(const char* path) const noexcept
{
    return storage_common_mkdir(storage, path);
}

FS_Error UFZ::Filesystem::filesystemInfo(const char* path, uint64_t* totalSpace, uint64_t* freeSpace) const noexcept
{
    return storage_common_fs_info(storage, path, totalSpace, freeSpace);
}

void UFZ::Filesystem::resolvePathAndEnsureAppDirectory(FuriString* path) const noexcept
{
    storage_common_resolve_path_and_ensure_app_directory(storage, path);
}

bool UFZ::Filesystem::areEquivalent(const char* path1, const char* path2) const noexcept
{
    return storage_common_equivalent_path(storage, path1, path2);
}

const char* UFZ::Filesystem::getErrorDescription(const FS_Error error) noexcept
{
    return storage_error_get_desc(error);
}

FS_Error UFZ::Filesystem::SDCardInfo(SDInfo* info) const noexcept
{
    return storage_sd_info(storage, info);
}

FS_Error UFZ::Filesystem::SDCardStatus() const noexcept
{
    return storage_sd_status(storage);
}

bool UFZ::Filesystem::removeSimple(const char* path) const noexcept
{
    return storage_simply_remove(storage, path);
}

bool UFZ::Filesystem::removeRecursiveSimple(const char* path) const noexcept
{
    return storage_simply_remove_recursive(storage, path);
}

bool UFZ::Filesystem::mkdirSimple(const char* path) const noexcept
{
    return storage_simply_mkdir(storage, path);
}

void UFZ::Filesystem::getNextFilename(const char* dirname, const char* filename, const char* fileExtension, FuriString* nextFilename, const uint8_t maxLength) const noexcept
{
    storage_get_next_filename(storage, dirname, filename, fileExtension, nextFilename, maxLength);
}

void UFZ::Filesystem::destroy() noexcept
{
    if (storage != nullptr)
    {
        furi_record_close(RECORD_STORAGE);
        storage = nullptr;
    }
}

// =====================================================================================================================
// ======================================================= Files =======================================================
// =====================================================================================================================

UFZ::File::File(const UFZ::Filesystem& store) noexcept
{
    storage = const_cast<Filesystem*>(&store);
}

UFZ::File::File(const UFZ::Filesystem& store, const char* path, const FS_AccessMode accessMode, const FS_OpenMode openMode) noexcept
{
    open(store, path, accessMode, openMode);
}

bool UFZ::File::open(const UFZ::Filesystem& store, const char* path, const FS_AccessMode accessMode, const FS_OpenMode openMode) noexcept
{

    storage = const_cast<Filesystem*>(&store);
    init();
    return storage_file_open(file, path, accessMode, openMode);
}

bool UFZ::File::isOpen() const noexcept
{
    return storage_file_is_open(file);
}

bool UFZ::File::isDirectory() const noexcept
{
    return storage_file_is_dir(file);
}

size_t UFZ::File::read(void* buffer, const size_t bytesToRead) const noexcept
{
    return storage_file_read(file, buffer, bytesToRead);
}

size_t UFZ::File::write(const void* buffer, const size_t bytesToWrite) const noexcept
{
    return storage_file_write(file, buffer, bytesToWrite);
}

uint64_t UFZ::File::tell() const noexcept
{
    return storage_file_tell(file);
}

bool UFZ::File::seek(const uint32_t offset, const bool bFromStart) const noexcept
{
    return storage_file_seek(file, offset, bFromStart);
}

bool UFZ::File::truncate() const noexcept
{
    return storage_file_truncate(file);
}

uint64_t UFZ::File::size() const noexcept
{
    return storage_file_size(file);
}

bool UFZ::File::sync() const noexcept
{
    return storage_file_sync(file);
}

bool UFZ::File::eof() const noexcept
{
    return storage_file_eof(file);
}

bool UFZ::File::copyToFile(const File& source, const File& destination, const size_t size) noexcept
{
    return storage_file_copy_to_file(source.file, destination.file, size);
}

void UFZ::File::close() noexcept
{
    storage_file_close(file);
    free();
}

UFZ::File::~File() noexcept
{
    close();
}

void UFZ::File::init() noexcept
{
    file = storage_file_alloc(storage->storage);
}

void UFZ::File::free() noexcept
{
    FREE_GUARD(storage_file_free, file);
}

// =====================================================================================================================
// ==================================================== Directories ====================================================
// =====================================================================================================================

UFZ::Directory::Directory(File& f) noexcept
{
    file = &f;
}

bool UFZ::Directory::open(UFZ::File& f, const char* path) noexcept
{
    file = &f;
    return storage_dir_open(file->file, path);
}

bool UFZ::Directory::close() const noexcept
{
    return storage_dir_close(file->file);
}

bool UFZ::Directory::read(FileInfo* info, char* name, const uint16_t nameLength) const noexcept
{
    return storage_dir_read(file->file, info, name, nameLength);
}

bool UFZ::Directory::rewind() const noexcept
{
    return storage_dir_rewind(file->file);
}
