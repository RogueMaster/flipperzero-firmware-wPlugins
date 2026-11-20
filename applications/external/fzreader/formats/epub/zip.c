/**
 * @file zip.c
 * @brief Minimal ZIP reader: central directory lookup and per-entry streams.
 *
 * The central directory is never cached: entries are read one at a time from
 * the file, so the memory footprint does not depend on the archive size.
 */
#include "zip.h"

#include <stdlib.h>
#include <string.h>

#include "inflate.h"

#define ZIP_SIG_LOCAL   0x04034b50u
#define ZIP_SIG_CENTRAL 0x02014b50u
#define ZIP_SIG_EOCD    0x06054b50u

#define ZIP_LOCAL_HEADER_SIZE   30
#define ZIP_CENTRAL_HEADER_SIZE 46
#define ZIP_EOCD_SIZE           22
#define ZIP_MAX_COMMENT_SIZE    65535
#define ZIP_CHUNK_SIZE          1024
#define ZIP_SEARCH_SIZE         (ZIP_EOCD_SIZE + ZIP_MAX_COMMENT_SIZE)
#define ZIP_SIZE_UNKNOWN        0xFFFFFFFFu

struct ReaderZip {
    ReaderIo io;
    ReaderInflate* inflate; /**< window shared by all the streams of the archive */
    uint32_t cd_offset;
    uint32_t cd_size;
    uint32_t entry_count;
    bool opened;
    bool stream_open;
};

struct ReaderZipStream {
    ReaderZip* zip;
    uint16_t method;
    uint64_t offset;
    uint32_t remaining;
    uint64_t pull_offset;
    uint32_t pull_remaining;
    ReaderInflate* inflate; /**< owned by the archive, NULL for stored entries */
    bool failed;
    bool ended;
};

/** Cursor over the central directory; only the current entry is buffered. */
typedef struct {
    ReaderZip* zip;
    uint64_t offset;
} ZipCursor;

static uint16_t zip_le16(const uint8_t* data) {
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static uint32_t zip_le32(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static bool zip_read_at(ReaderZip* zip, uint64_t offset, void* buffer, size_t size) {
    return zip->io.read_at(zip->io.context, offset, buffer, size);
}

static uint64_t zip_directory_end(const ReaderZip* zip) {
    return (uint64_t)zip->cd_offset + zip->cd_size;
}

static bool zip_read_eocd(ReaderZip* zip, uint64_t offset, uint64_t total) {
    uint8_t record[ZIP_EOCD_SIZE];

    if(offset + ZIP_EOCD_SIZE > total) {
        return false;
    }
    if(!zip_read_at(zip, offset, record, sizeof(record))) {
        return false;
    }
    if(zip_le32(record) != ZIP_SIG_EOCD) {
        return false;
    }

    uint32_t size = zip_le32(record + 12);
    uint32_t directory_offset = zip_le32(record + 16);
    uint16_t comment = zip_le16(record + 20);
    if(size == ZIP_SIZE_UNKNOWN || directory_offset == ZIP_SIZE_UNKNOWN) {
        return false; /* ZIP64 is not supported */
    }
    if(offset + ZIP_EOCD_SIZE + comment > total) {
        return false;
    }
    if((uint64_t)directory_offset + size > offset) {
        return false;
    }

    zip->cd_offset = directory_offset;
    zip->cd_size = size;
    zip->entry_count = zip_le16(record + 10);
    zip->opened = true;
    return true;
}

/**
 * Look for the EOCD signature inside one chunk. Positions near the chunk end
 * are verified with a separate small read, so a signature split by the chunk
 * boundary is not lost.
 */
static bool zip_scan_chunk(
    ReaderZip* zip,
    uint64_t start,
    const uint8_t* chunk,
    size_t size,
    uint64_t total) {
    for(size_t pos = size; pos-- > 0;) {
        uint32_t signature;
        if(pos + sizeof(signature) <= size) {
            signature = zip_le32(chunk + pos);
        } else {
            uint8_t tail[sizeof(signature)];
            if(!zip_read_at(zip, start + pos, tail, sizeof(tail))) {
                continue;
            }
            signature = zip_le32(tail);
        }
        if(signature == ZIP_SIG_EOCD && zip_read_eocd(zip, start + pos, total)) {
            return true;
        }
    }
    return false;
}

ReaderZip* reader_zip_alloc(ReaderIo io) {
    if(io.read_at == NULL) {
        return NULL;
    }

    ReaderZip* zip = malloc(sizeof(ReaderZip));
    if(zip == NULL) {
        return NULL;
    }

    zip->io = io;
    zip->inflate = NULL;
    zip->cd_offset = 0;
    zip->cd_size = 0;
    zip->entry_count = 0;
    zip->opened = false;
    zip->stream_open = false;
    return zip;
}

void reader_zip_free(ReaderZip* zip) {
    if(zip == NULL) {
        return;
    }

    reader_inflate_free(zip->inflate);
    free(zip);
}

bool reader_zip_open(ReaderZip* zip) {
    if(zip == NULL) {
        return false;
    }

    zip->opened = false;
    uint64_t total = zip->io.size != NULL ? zip->io.size(zip->io.context) : 0;
    if(total < ZIP_EOCD_SIZE) {
        return false;
    }

    uint64_t limit = total > ZIP_SEARCH_SIZE ? total - ZIP_SEARCH_SIZE : 0;
    uint8_t chunk[ZIP_CHUNK_SIZE];
    uint64_t end = total;
    while(end > limit) {
        uint64_t start = (end - limit > ZIP_CHUNK_SIZE) ? end - ZIP_CHUNK_SIZE : limit;
        size_t size = (size_t)(end - start);
        if(!zip_read_at(zip, start, chunk, size)) {
            return false;
        }
        if(zip_scan_chunk(zip, start, chunk, size, total)) {
            return true;
        }
        end = start;
    }

    return false;
}

static bool zip_cursor_next(ZipCursor* cursor, ReaderZipEntry* entry, uint16_t* name_size) {
    ReaderZip* zip = cursor->zip;
    uint64_t offset = cursor->offset;
    uint64_t directory_end = zip_directory_end(zip);

    if(offset < zip->cd_offset || offset + ZIP_CENTRAL_HEADER_SIZE > directory_end) {
        return false;
    }

    uint8_t record[ZIP_CENTRAL_HEADER_SIZE];
    if(!zip_read_at(zip, offset, record, sizeof(record))) {
        return false;
    }
    if(zip_le32(record) != ZIP_SIG_CENTRAL) {
        return false;
    }

    uint16_t central_name_size = zip_le16(record + 28);
    uint16_t extra_size = zip_le16(record + 30);
    uint16_t comment_size = zip_le16(record + 32);
    uint64_t next =
        offset + ZIP_CENTRAL_HEADER_SIZE + central_name_size + extra_size + comment_size;
    if(central_name_size == 0 || next > directory_end) {
        return false;
    }

    entry->method = zip_le16(record + 10);
    entry->compressed_size = zip_le32(record + 20);
    entry->uncompressed_size = zip_le32(record + 24);
    entry->local_header_offset = zip_le32(record + 42);
    if(entry->compressed_size == ZIP_SIZE_UNKNOWN ||
       entry->uncompressed_size == ZIP_SIZE_UNKNOWN ||
       entry->local_header_offset == ZIP_SIZE_UNKNOWN) {
        return false;
    }

    size_t copy = central_name_size;
    if(copy > READER_ZIP_NAME_MAX - 1) {
        copy = READER_ZIP_NAME_MAX - 1;
    }
    if(!zip_read_at(zip, offset + ZIP_CENTRAL_HEADER_SIZE, entry->name, copy)) {
        return false;
    }
    entry->name[copy] = '\0';

    cursor->offset = next;
    if(name_size != NULL) {
        *name_size = central_name_size;
    }
    return true;
}

static bool
    zip_entry_at(ReaderZip* zip, size_t index, ReaderZipEntry* entry, uint16_t* name_size) {
    if(!zip->opened || entry == NULL || index >= zip->entry_count) {
        return false;
    }

    ZipCursor cursor = {.zip = zip, .offset = zip->cd_offset};
    bool found = false;
    for(size_t current = 0; current <= index; current++) {
        found = zip_cursor_next(&cursor, entry, current == index ? name_size : NULL);
        if(!found) {
            break;
        }
    }
    return found;
}

size_t reader_zip_find(ReaderZip* zip, const char* name) {
    if(zip == NULL || !zip->opened || name == NULL) {
        return (size_t)-1;
    }

    ZipCursor cursor = {.zip = zip, .offset = zip->cd_offset};
    ReaderZipEntry entry;
    for(size_t index = 0; index < zip->entry_count; index++) {
        if(!zip_cursor_next(&cursor, &entry, NULL)) {
            return (size_t)-1;
        }
        if(strcmp(entry.name, name) == 0) {
            return index;
        }
    }

    return (size_t)-1;
}

static size_t zip_stream_pull(void* context, uint8_t* buffer, size_t size) {
    ReaderZipStream* stream = context;

    if(stream->pull_remaining == 0) {
        return 0;
    }
    if(size > (size_t)stream->pull_remaining) {
        size = stream->pull_remaining;
    }
    if(!zip_read_at(stream->zip, stream->pull_offset, buffer, size)) {
        stream->failed = true;
        return 0;
    }

    stream->pull_offset += size;
    stream->pull_remaining -= (uint32_t)size;
    return size;
}

ReaderZipStream* reader_zip_entry_open(ReaderZip* zip, size_t index) {
    if(zip == NULL || zip->stream_open) {
        return NULL;
    }

    ReaderZipEntry entry;
    uint16_t name_size;
    if(!zip_entry_at(zip, index, &entry, &name_size)) {
        return NULL;
    }
    if(entry.method != 0 && entry.method != 8) {
        return NULL;
    }

    uint8_t header[ZIP_LOCAL_HEADER_SIZE];
    if(!zip_read_at(zip, entry.local_header_offset, header, sizeof(header))) {
        return NULL;
    }
    if(zip_le32(header) != ZIP_SIG_LOCAL) {
        return NULL;
    }

    uint16_t local_name_size = zip_le16(header + 26);
    uint16_t local_extra_size = zip_le16(header + 28);
    if(local_name_size != name_size) {
        return NULL;
    }

    size_t compare = local_name_size;
    if(compare > READER_ZIP_NAME_MAX - 1) {
        compare = READER_ZIP_NAME_MAX - 1;
    }
    uint8_t local_name[READER_ZIP_NAME_MAX];
    if(!zip_read_at(zip, entry.local_header_offset + ZIP_LOCAL_HEADER_SIZE, local_name, compare)) {
        return NULL;
    }
    if(memcmp(local_name, entry.name, compare) != 0) {
        return NULL;
    }

    uint64_t data_offset = (uint64_t)entry.local_header_offset + ZIP_LOCAL_HEADER_SIZE +
                           local_name_size + local_extra_size;
    uint64_t total = zip->io.size != NULL ? zip->io.size(zip->io.context) : 0;
    if(data_offset > total || (uint64_t)entry.compressed_size > total - data_offset) {
        return NULL;
    }

    ReaderZipStream* stream = malloc(sizeof(ReaderZipStream));
    if(stream == NULL) {
        return NULL;
    }

    stream->zip = zip;
    stream->method = entry.method;
    stream->offset = data_offset;
    stream->remaining = entry.compressed_size;
    stream->pull_offset = data_offset;
    stream->pull_remaining = entry.compressed_size;
    stream->inflate = NULL;
    stream->failed = false;
    stream->ended = false;

    if(entry.method == 8) {
        /* One window per archive: allocate it on first use, then keep it. */
        if(zip->inflate == NULL) {
            zip->inflate = reader_inflate_alloc();
            if(zip->inflate == NULL) {
                free(stream);
                return NULL;
            }
        }
        stream->inflate = zip->inflate;
        reader_inflate_begin(stream->inflate, zip_stream_pull, stream);
    }

    zip->stream_open = true;
    return stream;
}

void reader_zip_stream_close(ReaderZipStream* stream) {
    if(stream == NULL) {
        return;
    }

    if(stream->inflate != NULL) {
        /* Park the window so it keeps no pointer to the stream being freed. */
        reader_inflate_begin(stream->inflate, NULL, NULL);
    }
    stream->zip->stream_open = false;
    free(stream);
}

size_t reader_zip_stream_read(ReaderZipStream* stream, void* buffer, size_t size) {
    if(stream == NULL || buffer == NULL || size == 0 || stream->failed || stream->ended) {
        return 0;
    }

    if(stream->method == 0) {
        if(stream->remaining == 0) {
            stream->ended = true;
            return 0;
        }
        if(size > (size_t)stream->remaining) {
            size = stream->remaining;
        }
        if(!zip_read_at(stream->zip, stream->offset, buffer, size)) {
            stream->failed = true;
            return 0;
        }
        stream->offset += size;
        stream->remaining -= (uint32_t)size;
        return size;
    }

    size_t read = reader_inflate_read(stream->inflate, buffer, size);
    if(read == 0) {
        if(reader_inflate_failed(stream->inflate)) {
            stream->failed = true;
        } else {
            stream->ended = true;
        }
    }
    return read;
}
