/**
 * @file epub.c
 * @brief EPUB driver: OPF metadata, spine and XHTML chapters inside a ZIP.
 *
 * The package document is scanned three times through a streaming tag reader:
 * the first pass counts the spine, the second one copies the idrefs of the
 * spine, the third one walks the manifest and resolves the href of every
 * idref. Only the chapters of the spine are kept, so a book costs about 200
 * bytes per chapter instead of a table of all manifest entries.
 *
 * This file depends on libc, reader_io.h, zip.h and xhtml.h only: the storage
 * service lives behind EpubFile, which a host test replaces with stdio.
 */
#include "epub.h"

#include <stdlib.h>
#include <string.h>

#include "epub_file.h"
#include "reader_io.h"
#include "xhtml.h"
#include "zip.h"

#include "../../text/utf8.h"

/** Spine entries kept at most. */
#define EPUB_SPINE_MAX     512
/** Bytes for an entry id, archive path or attribute value. */
#define EPUB_NAME_MAX      128
/** Bytes for one spine idref. */
#define EPUB_IDREF_MAX     64
/** Bytes for the book title taken from the package document. */
#define EPUB_TITLE_MAX     96
/** Bytes for a media type. */
#define EPUB_MEDIA_MAX     48
/** Bytes read from the package document at a time. */
#define EPUB_XML_BUFFER    256
/** Bytes accumulated for one span handed to the reader. */
#define EPUB_SPAN_SIZE     256
/** Length of the longest element name handled by the tag reader. */
#define EPUB_TAG_NAME_MAX  24
/** Attribute name buffer in the tag reader. */
#define EPUB_ATTR_NAME_MAX 24

typedef enum {
    EpubMediaOther,
    EpubMediaXhtml,
    EpubMediaHtml,
} EpubMedia;

/** One chapter of the spine: the only thing kept from the package document. */
typedef struct {
    char href[EPUB_NAME_MAX]; /**< archive path, relative to the archive root */
    char* title; /**< optional title attribute of the manifest item, NULL if none */
    int32_t zip_index; /**< cached central directory index, -1 if unknown */
    bool xhtml; /**< the media type is an XHTML/HTML document */
    bool readable; /**< href is resolved and the entry can be read */
} EpubSpineItem;

typedef struct {
    ReaderZipStream* stream;
    bool active;
    ReaderSpan pending;
    bool has_pending;
    char buffer[EPUB_SPAN_SIZE];
    size_t size;
    bool bold;
} EpubChapter;

typedef struct {
    ReaderBook base;
    EpubFile* file;
    ReaderIo io;
    ReaderZip* zip; /**< archive; owns the DEFLATE window shared by all the streams */
    XhtmlParser* xhtml;
    EpubSpineItem* spine;
    size_t spine_count;
    char title[EPUB_TITLE_MAX];
    EpubChapter chapter;
} EpubBook;

/* ------------------------------------------------------------ XML input --- */

typedef struct {
    ReaderZipStream* stream;
    char buffer[EPUB_XML_BUFFER];
    size_t pos;
    size_t size;
    bool eof;
} EpubXml;

typedef struct {
    char name[EPUB_TAG_NAME_MAX];
    bool closing;
    bool self_closing;
    char value1[EPUB_NAME_MAX]; /**< id, full-path or idref */
    char value2[EPUB_NAME_MAX]; /**< href */
    char value3[EPUB_MEDIA_MAX]; /**< media-type */
    char value4[EPUB_NAME_MAX]; /**< title */
    bool has1;
    bool has2;
    bool has3;
    bool has4;
    bool overflow1; /**< value1 did not fit */
    bool overflow2; /**< value2 did not fit */
    bool overflow3; /**< value3 did not fit */
    bool overflow4; /**< value4 did not fit */
} EpubTag;

static int epub_xml_get(EpubXml* xml) {
    if(xml->pos >= xml->size) {
        if(xml->eof) {
            return -1;
        }
        size_t read = reader_zip_stream_read(xml->stream, xml->buffer, sizeof(xml->buffer));
        if(read == 0) {
            xml->eof = true;
            return -1;
        }
        xml->pos = 0;
        xml->size = read;
    }
    return (unsigned char)xml->buffer[xml->pos++];
}

static int epub_xml_peek(EpubXml* xml) {
    int byte = epub_xml_get(xml);
    if(byte >= 0) {
        xml->pos--;
    }
    return byte;
}

static void epub_xml_skip_to(EpubXml* xml, char end) {
    int byte;
    while((byte = epub_xml_get(xml)) >= 0) {
        if(byte == (int)(unsigned char)end) {
            return;
        }
    }
}

static void epub_xml_skip_comment(EpubXml* xml) {
    unsigned dashes = 0;
    int byte;
    while((byte = epub_xml_get(xml)) >= 0) {
        if(byte == '-') {
            dashes++;
        } else if(byte == '>' && dashes >= 2) {
            return;
        } else {
            dashes = 0;
        }
    }
}

static bool epub_xml_is_space(int byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n' || byte == '\v' ||
           byte == '\f';
}

static char epub_xml_lower(int byte) {
    if(byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return (char)byte;
}

static bool epub_xml_is_name_char(int byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == ':' ||
           byte == '.';
}

/**
 * Read the next element start or end tag.
 *
 * @returns false at the end of the stream. Attributes are kept only for the
 *          elements the driver uses: id/href/media-type of item, full-path of
 *          rootfile and idref of itemref.
 */
static bool epub_xml_read_tag(EpubXml* xml, EpubTag* tag) {
    for(;;) {
        int byte;
        do {
            byte = epub_xml_get(xml);
        } while(byte >= 0 && byte != '<');
        if(byte < 0) {
            return false;
        }

        byte = epub_xml_get(xml);
        if(byte < 0) {
            return false;
        }
        if(byte == '?') {
            epub_xml_skip_to(xml, '>');
            continue;
        }
        if(byte == '!') {
            int first = epub_xml_get(xml);
            if(first == '-') {
                int second = epub_xml_get(xml);
                if(second == '-') {
                    epub_xml_skip_comment(xml);
                } else {
                    epub_xml_skip_to(xml, '>');
                }
            } else if(first >= 0 && first != '>') {
                epub_xml_skip_to(xml, '>');
            }
            continue;
        }

        tag->closing = false;
        tag->self_closing = false;
        tag->has1 = false;
        tag->has2 = false;
        tag->has3 = false;
        tag->has4 = false;
        tag->overflow1 = false;
        tag->overflow2 = false;
        tag->overflow3 = false;
        tag->overflow4 = false;
        tag->value1[0] = '\0';
        tag->value2[0] = '\0';
        tag->value3[0] = '\0';
        tag->value4[0] = '\0';

        if(byte == '/') {
            tag->closing = true;
            byte = epub_xml_get(xml);
            if(byte < 0) {
                return false;
            }
        }

        size_t name_size = 0;
        while(byte >= 0 && epub_xml_is_name_char(byte)) {
            if(name_size < sizeof(tag->name) - 1) {
                tag->name[name_size++] = epub_xml_lower(byte);
            }
            byte = epub_xml_get(xml);
        }
        tag->name[name_size] = '\0';

        for(;;) {
            while(byte >= 0 && epub_xml_is_space(byte)) {
                byte = epub_xml_get(xml);
            }
            if(byte < 0) {
                return true;
            }
            if(byte == '>') {
                return true;
            }
            if(byte == '/') {
                tag->self_closing = true;
                byte = epub_xml_get(xml);
                continue;
            }

            char attribute[EPUB_ATTR_NAME_MAX];
            size_t attribute_size = 0;
            while(byte >= 0 && byte != '=' && byte != '>' && byte != '/' &&
                  !epub_xml_is_space(byte)) {
                if(attribute_size < sizeof(attribute) - 1) {
                    attribute[attribute_size++] = epub_xml_lower(byte);
                }
                byte = epub_xml_get(xml);
            }
            attribute[attribute_size] = '\0';

            while(byte >= 0 && epub_xml_is_space(byte)) {
                byte = epub_xml_get(xml);
            }
            if(byte != '=') {
                continue;
            }
            byte = epub_xml_get(xml);
            while(byte >= 0 && epub_xml_is_space(byte)) {
                byte = epub_xml_get(xml);
            }

            char quote = 0;
            if(byte == '"' || byte == '\'') {
                quote = (char)byte;
                byte = epub_xml_get(xml);
            }

            char* destination = NULL;
            size_t capacity = 0;
            bool* present = NULL;
            bool* overflow = NULL;
            if(strcmp(tag->name, "item") == 0) {
                if(strcmp(attribute, "id") == 0) {
                    destination = tag->value1;
                    capacity = sizeof(tag->value1);
                    present = &tag->has1;
                    overflow = &tag->overflow1;
                } else if(strcmp(attribute, "href") == 0) {
                    destination = tag->value2;
                    capacity = sizeof(tag->value2);
                    present = &tag->has2;
                    overflow = &tag->overflow2;
                } else if(strcmp(attribute, "media-type") == 0) {
                    destination = tag->value3;
                    capacity = sizeof(tag->value3);
                    present = &tag->has3;
                    overflow = &tag->overflow3;
                } else if(strcmp(attribute, "title") == 0) {
                    destination = tag->value4;
                    capacity = sizeof(tag->value4);
                    present = &tag->has4;
                    overflow = &tag->overflow4;
                }
            } else if(strcmp(tag->name, "rootfile") == 0 && strcmp(attribute, "full-path") == 0) {
                destination = tag->value1;
                capacity = sizeof(tag->value1);
                present = &tag->has1;
                overflow = &tag->overflow1;
            } else if(strcmp(tag->name, "itemref") == 0 && strcmp(attribute, "idref") == 0) {
                destination = tag->value1;
                capacity = sizeof(tag->value1);
                present = &tag->has1;
                overflow = &tag->overflow1;
            }

            size_t value_size = 0;
            while(byte >= 0) {
                if(quote != 0) {
                    if(byte == (int)(unsigned char)quote) {
                        byte = epub_xml_get(xml);
                        break;
                    }
                } else if(byte == '>' || epub_xml_is_space(byte)) {
                    break;
                }
                if(destination != NULL) {
                    if(value_size + 1 < capacity) {
                        destination[value_size] = (char)byte;
                    } else if(overflow != NULL) {
                        *overflow = true;
                    }
                }
                value_size++;
                byte = epub_xml_get(xml);
            }
            if(destination != NULL) {
                size_t stored = value_size < capacity - 1 ? value_size : capacity - 1;
                destination[stored] = '\0';
                if(present != NULL) {
                    *present = true;
                }
            }
        }
    }
}

/* --------------------------------------------------------------- strings --- */

static bool epub_is_space(unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n' || byte == '\v' ||
           byte == '\f';
}

static void epub_trim(char* text) {
    size_t length = strlen(text);
    size_t start = 0;
    while(start < length && epub_is_space((unsigned char)text[start])) {
        start++;
    }
    size_t end = length;
    while(end > start && epub_is_space((unsigned char)text[end - 1])) {
        end--;
    }
    if(start > 0) {
        memmove(text, text + start, end - start);
    }
    text[end - start] = '\0';
}

static bool epub_decode_entity(const char* name, size_t size, char* out, size_t* out_size) {
    static const struct {
        const char* name;
        char value;
    } named[] = {
        {"amp", '&'},
        {"lt", '<'},
        {"gt", '>'},
        {"quot", '"'},
        {"apos", '\''},
    };

    if(size == 0) {
        return false;
    }

    if(name[0] == '#') {
        if(size < 2) {
            return false;
        }
        size_t i = 1;
        unsigned base = 10;
        if(name[1] == 'x' || name[1] == 'X') {
            base = 16;
            i = 2;
            if(size <= i) {
                return false;
            }
        }
        uint32_t value = 0;
        for(; i < size; i++) {
            unsigned char byte = (unsigned char)name[i];
            uint32_t digit;
            if(byte >= '0' && byte <= '9') {
                digit = (uint32_t)(byte - '0');
            } else if(base == 16 && byte >= 'a' && byte <= 'f') {
                digit = (uint32_t)(byte - 'a' + 10);
            } else if(base == 16 && byte >= 'A' && byte <= 'F') {
                digit = (uint32_t)(byte - 'A' + 10);
            } else {
                return false;
            }
            value = value * base + digit;
            if(value > 0x10FFFFu) {
                return false;
            }
        }
        if(value == 0 || (value >= 0xD800u && value <= 0xDFFFu)) {
            return false;
        }
        size_t written = utf8_encode(value, out);
        if(written == 0) {
            return false;
        }
        *out_size = written;
        return true;
    }

    if(size == 4 && memcmp(name, "nbsp", 4) == 0) {
        *out_size = utf8_encode(0x00A0u, out);
        return *out_size > 0;
    }

    for(size_t i = 0; i < sizeof(named) / sizeof(named[0]); i++) {
        size_t length = strlen(named[i].name);
        if(size == length && memcmp(name, named[i].name, length) == 0) {
            out[0] = named[i].value;
            *out_size = 1;
            return true;
        }
    }

    return false;
}

/** Decode XML entities in place; unknown ones are left untouched. */
static void epub_unescape(char* text) {
    char* in = text;
    char* out = text;

    while(*in != '\0') {
        if(*in != '&') {
            *out++ = *in++;
            continue;
        }

        char* semi = strchr(in + 1, ';');
        size_t size = semi != NULL ? (size_t)(semi - in - 1) : 0;
        if(semi == NULL || size == 0 || size > 16) {
            *out++ = *in++;
            continue;
        }

        char decoded[4];
        size_t decoded_size = 0;
        if(epub_decode_entity(in + 1, size, decoded, &decoded_size)) {
            memcpy(out, decoded, decoded_size);
            out += decoded_size;
            in = semi + 1;
        } else {
            *out++ = *in++;
        }
    }

    *out = '\0';
}

/**
 * Resolve href against the directory of the package document.
 *
 * Handles "./", "../" and absolute archive paths. @returns false if the result
 * does not fit, in which case the chapter is treated as unreadable.
 */
static bool epub_resolve_path(const char* opf_path, const char* href, char* out, size_t capacity) {
    char joined[EPUB_NAME_MAX * 2];
    size_t length = 0;

    const char* slash = strrchr(opf_path, '/');
    if(slash != NULL) {
        size_t directory = (size_t)(slash - opf_path) + 1;
        if(directory >= sizeof(joined)) {
            return false;
        }
        memcpy(joined, opf_path, directory);
        length = directory;
    }

    while(*href == '/') {
        href++;
    }
    for(const char* cursor = href; *cursor != '\0' && *cursor != '#' && *cursor != '?'; cursor++) {
        if(length + 1 >= sizeof(joined)) {
            return false;
        }
        joined[length++] = *cursor;
    }
    joined[length] = '\0';
    if(length == 0) {
        return false;
    }

    size_t out_size = 0;
    size_t position = 0;
    while(position < length) {
        while(position < length && joined[position] == '/') {
            position++;
        }
        size_t start = position;
        while(position < length && joined[position] != '/') {
            position++;
        }
        size_t segment = position - start;
        if(segment == 0) {
            continue;
        }
        if(segment == 1 && joined[start] == '.') {
            continue;
        }
        if(segment == 2 && joined[start] == '.' && joined[start + 1] == '.') {
            if(out_size == 0) {
                return false;
            }
            while(out_size > 0 && out[out_size - 1] != '/') {
                out_size--;
            }
            if(out_size > 0) {
                out_size--;
            }
            continue;
        }
        if(out_size > 0) {
            if(out_size + 1 >= capacity) {
                return false;
            }
            out[out_size++] = '/';
        }
        if(out_size + segment >= capacity) {
            return false;
        }
        memcpy(out + out_size, joined + start, segment);
        out_size += segment;
    }
    out[out_size] = '\0';
    return out_size > 0;
}

static bool epub_has_extension(const char* path, const char* extension) {
    size_t length = strlen(path);
    size_t extension_length = strlen(extension);
    if(length < extension_length) {
        return false;
    }
    const char* tail = path + length - extension_length;
    for(size_t i = 0; i < extension_length; i++) {
        if(epub_xml_lower((unsigned char)tail[i]) != extension[i]) {
            return false;
        }
    }
    return true;
}

static EpubMedia epub_media_type(const EpubTag* tag, const char* href) {
    if(tag->has3 && !tag->overflow3) {
        if(strcmp(tag->value3, "application/xhtml+xml") == 0) {
            return EpubMediaXhtml;
        }
        if(strcmp(tag->value3, "text/html") == 0) {
            return EpubMediaHtml;
        }
        return EpubMediaOther;
    }
    if(epub_has_extension(href, ".xhtml") || epub_has_extension(href, ".html")) {
        return EpubMediaXhtml;
    }
    return EpubMediaOther;
}

/** Read the character data of an element up to the next tag. */
static void epub_xml_read_text(EpubXml* xml, char* out, size_t capacity) {
    size_t size = 0;
    int byte;
    while((byte = epub_xml_peek(xml)) >= 0 && byte != '<') {
        epub_xml_get(xml);
        if(size + 1 < capacity) {
            out[size++] = (char)byte;
        }
    }
    out[size] = '\0';
    epub_unescape(out);
    epub_trim(out);
}

/* ---------------------------------------------------------------- container --- */

static bool epub_read_container(EpubBook* book, char* opf_path, size_t capacity) {
    size_t index = reader_zip_find(book->zip, "META-INF/container.xml");
    if(index == (size_t)-1) {
        return false;
    }

    ReaderZipStream* stream = reader_zip_entry_open(book->zip, index);
    if(stream == NULL) {
        return false;
    }

    EpubXml xml = {.stream = stream, .pos = 0, .size = 0, .eof = false};
    EpubTag tag;
    bool found = false;

    while(epub_xml_read_tag(&xml, &tag)) {
        if(!tag.closing && strcmp(tag.name, "rootfile") == 0 && tag.has1 && !tag.overflow1) {
            size_t length = strlen(tag.value1);
            if(length > 0 && length < capacity) {
                memcpy(opf_path, tag.value1, length + 1);
                epub_unescape(opf_path);
                found = true;
            }
            break;
        }
    }

    reader_zip_stream_close(stream);
    return found;
}

/* -------------------------------------------------------------- package --- */

/** Which part of the package document a pass collects. */
typedef enum {
    EpubOpfCount, /**< count the spine entries */
    EpubOpfSpine, /**< copy the idrefs of the spine */
    EpubOpfManifest, /**< resolve the href of every spine idref */
} EpubOpfPass;

typedef struct {
    bool spine_seen;
    size_t spine_count;
} EpubOpfResult;

/**
 * Read the package document once.
 *
 * The pass selects what is collected: the spine count, the spine idrefs or the
 * manifest items that resolve those idrefs. Nothing but the spine is kept.
 */
static bool epub_opf_parse(
    EpubBook* book,
    size_t opf_index,
    const char* opf_path,
    EpubOpfPass pass,
    char (*idrefs)[EPUB_IDREF_MAX],
    size_t idref_capacity,
    EpubOpfResult* result) {
    ReaderZipStream* stream = reader_zip_entry_open(book->zip, opf_index);
    if(stream == NULL) {
        return false;
    }

    EpubXml xml = {.stream = stream, .pos = 0, .size = 0, .eof = false};
    EpubTag tag;
    bool in_manifest = false;
    bool in_spine = false;
    bool spine_seen = false;
    size_t spine_count = 0;

    while(epub_xml_read_tag(&xml, &tag)) {
        if(tag.closing) {
            if(strcmp(tag.name, "manifest") == 0) {
                in_manifest = false;
            } else if(strcmp(tag.name, "spine") == 0) {
                in_spine = false;
            }
            continue;
        }

        if(strcmp(tag.name, "manifest") == 0) {
            in_manifest = true;
            continue;
        }
        if(strcmp(tag.name, "spine") == 0) {
            in_spine = true;
            spine_seen = true;
            continue;
        }

        if(in_spine && strcmp(tag.name, "itemref") == 0) {
            if(!tag.has1) {
                continue;
            }
            size_t index = spine_count;
            if(index < EPUB_SPINE_MAX) {
                spine_count++;
            }
            if(pass == EpubOpfSpine && index < idref_capacity) {
                if(tag.overflow1) {
                    /* A truncated idref would match the wrong item. */
                    idrefs[index][0] = '\0';
                } else {
                    memcpy(idrefs[index], tag.value1, strlen(tag.value1) + 1);
                }
            }
        } else if(in_manifest && strcmp(tag.name, "item") == 0) {
            if(pass != EpubOpfManifest || !tag.has1 || !tag.has2 || tag.overflow1) {
                continue;
            }
            epub_unescape(tag.value2);
            EpubMedia media = epub_media_type(&tag, tag.value2);
            for(size_t index = 0; index < book->spine_count; index++) {
                if(strcmp(idrefs[index], tag.value1) != 0) {
                    continue;
                }
                EpubSpineItem* entry = &book->spine[index];
                entry->xhtml = media == EpubMediaXhtml || media == EpubMediaHtml;
                entry->readable =
                    !tag.overflow2 &&
                    epub_resolve_path(opf_path, tag.value2, entry->href, sizeof(entry->href));

                /*
                 * The package document usually has no title on a manifest item;
                 * when it does, it names the chapter. Keep it only for chapters
                 * of the spine, so the table stays small.
                 */
                if(tag.has4 && !tag.overflow4 && entry->title == NULL) {
                    epub_unescape(tag.value4);
                    size_t title_size = strlen(tag.value4);
                    if(title_size > 0) {
                        entry->title = malloc(title_size + 1);
                        if(entry->title != NULL) {
                            memcpy(entry->title, tag.value4, title_size + 1);
                        }
                    }
                }
            }
        } else if(strcmp(tag.name, "dc:title") == 0 || (strcmp(tag.name, "title") == 0 && !in_spine)) {
            if(book->title[0] == '\0' && !tag.self_closing) {
                epub_xml_read_text(&xml, book->title, sizeof(book->title));
            }
        }
    }

    reader_zip_stream_close(stream);

    result->spine_seen = spine_seen;
    result->spine_count = spine_count;
    return true;
}

/* ---------------------------------------------------------------- chapter --- */

static size_t epub_chapter_pull(void* context, char* buffer, size_t size) {
    ReaderZipStream* stream = context;
    return reader_zip_stream_read(stream, buffer, size);
}

/**
 * Length of the prefix that ends on a UTF-8 sequence boundary.
 *
 * Only bytes of [text, text + size) are read: the caller owns `size` bytes and
 * nothing beyond them.
 */
static size_t epub_utf8_prefix(const char* text, size_t size) {
    if(size == 0) {
        return 0;
    }

    /* Walk back over the continuation bytes of the last, possibly cut, unit. */
    size_t start = size;
    while(start > 0 && ((unsigned char)text[start - 1] & 0xC0u) == 0x80u) {
        start--;
    }
    if(start == 0) {
        return size;
    }

    unsigned char lead = (unsigned char)text[start - 1];
    size_t length = 1;
    if((lead & 0xE0u) == 0xC0u) {
        length = 2;
    } else if((lead & 0xF0u) == 0xE0u) {
        length = 3;
    } else if((lead & 0xF8u) == 0xF0u) {
        length = 4;
    }

    return start - 1 + length > size ? start - 1 : size;
}

static void epub_chapter_release(EpubBook* book) {
    if(book->chapter.stream != NULL) {
        reader_zip_stream_close(book->chapter.stream);
        book->chapter.stream = NULL;
    }
    book->chapter.active = false;
    book->chapter.has_pending = false;
    book->chapter.size = 0;
}

/* ------------------------------------------------------------ interface --- */

static const char* epub_title(const ReaderBook* base) {
    const EpubBook* book = (const EpubBook*)base;
    return book->title;
}

static const char* epub_chapter_title(const ReaderBook* base, size_t chapter) {
    const EpubBook* book = (const EpubBook*)base;
    if(chapter >= book->spine_count) {
        return NULL;
    }
    const char* title = book->spine[chapter].title;
    return title != NULL && title[0] != '\0' ? title : NULL;
}

static size_t epub_chapter_count(const ReaderBook* base) {
    const EpubBook* book = (const EpubBook*)base;
    return book->spine_count;
}

static bool epub_chapter_open(ReaderBook* base, size_t chapter) {
    EpubBook* book = (EpubBook*)base;
    /* The archive serves one stream at a time: drop the previous chapter first. */
    epub_chapter_release(book);

    if(chapter >= book->spine_count) {
        return false;
    }
    EpubSpineItem* entry = &book->spine[chapter];
    if(!entry->readable || !entry->xhtml) {
        return false;
    }

    if(entry->zip_index < 0) {
        size_t index = reader_zip_find(book->zip, entry->href);
        if(index == (size_t)-1) {
            return false;
        }
        entry->zip_index = (int32_t)index;
    }

    ReaderZipStream* stream = reader_zip_entry_open(book->zip, (size_t)entry->zip_index);
    if(stream == NULL) {
        return false;
    }

    book->chapter.stream = stream;
    book->chapter.active = true;
    book->chapter.has_pending = false;
    book->chapter.size = 0;
    xhtml_begin(book->xhtml, epub_chapter_pull, stream);
    return true;
}

static void epub_chapter_close(ReaderBook* base) {
    epub_chapter_release((EpubBook*)base);
}

static bool epub_read_span(ReaderBook* base, ReaderSpan* span) {
    EpubBook* book = (EpubBook*)base;
    if(span == NULL || !book->chapter.active) {
        return false;
    }

    book->chapter.size = 0;

    for(;;) {
        ReaderSpan next;
        bool available;
        if(book->chapter.has_pending) {
            next = book->chapter.pending;
            book->chapter.has_pending = false;
            available = true;
        } else {
            available = xhtml_next_span(book->xhtml, &next);
        }
        if(!available) {
            break;
        }
        if(next.size == 0) {
            continue;
        }

        if(book->chapter.size == 0) {
            book->chapter.bold = next.bold;
        } else if(next.bold != book->chapter.bold) {
            book->chapter.pending = next;
            book->chapter.has_pending = true;
            break;
        }

        size_t room = sizeof(book->chapter.buffer) - book->chapter.size;
        size_t copy = next.size;
        if(copy > room) {
            copy = epub_utf8_prefix(next.text, room);
        }
        if(copy == 0) {
            book->chapter.pending = next;
            book->chapter.has_pending = true;
            break;
        }

        memcpy(book->chapter.buffer + book->chapter.size, next.text, copy);
        book->chapter.size += copy;

        if(copy < next.size) {
            book->chapter.pending.text = next.text + copy;
            book->chapter.pending.size = next.size - copy;
            book->chapter.pending.bold = next.bold;
            book->chapter.has_pending = true;
            break;
        }
        if(book->chapter.size == sizeof(book->chapter.buffer)) {
            break;
        }
    }

    if(book->chapter.size == 0) {
        return false;
    }

    span->text = book->chapter.buffer;
    span->size = book->chapter.size;
    span->bold = book->chapter.bold;
    return true;
}

static void epub_title_from_path(EpubBook* book, const char* path) {
    const char* name = path;
    for(const char* cursor = path; *cursor != '\0'; cursor++) {
        if(*cursor == '/') {
            name = cursor + 1;
        }
    }
    const char* dot = strrchr(name, '.');
    size_t length = (dot != NULL && dot != name) ? (size_t)(dot - name) : strlen(name);
    if(length >= sizeof(book->title)) {
        length = sizeof(book->title) - 1;
    }
    memcpy(book->title, name, length);
    book->title[length] = '\0';
}

static void epub_free(EpubBook* book) {
    if(book == NULL) {
        return;
    }
    epub_chapter_release(book);
    xhtml_free(book->xhtml);
    /* Frees the shared DEFLATE window along with the archive. */
    reader_zip_free(book->zip);
    for(size_t index = 0; index < book->spine_count; index++) {
        free(book->spine[index].title);
    }
    free(book->spine);
    epub_file_close(book->file);
    free(book);
}

static void epub_close(ReaderBook* base) {
    epub_free((EpubBook*)base);
}

static ReaderBook* epub_open(const char* path) {
    if(path == NULL) {
        return NULL;
    }

    EpubBook* book = calloc(1, sizeof(EpubBook));
    if(book == NULL) {
        return NULL;
    }
    book->base.format = &reader_format_epub;

    book->file = epub_file_open(path);
    if(book->file == NULL) {
        epub_free(book);
        return NULL;
    }
    book->io = epub_file_io(book->file);
    book->zip = reader_zip_alloc(book->io);
    if(book->zip == NULL || !reader_zip_open(book->zip)) {
        epub_free(book);
        return NULL;
    }
    book->xhtml = xhtml_alloc();
    if(book->xhtml == NULL) {
        epub_free(book);
        return NULL;
    }

    char opf_path[EPUB_NAME_MAX];
    if(!epub_read_container(book, opf_path, sizeof(opf_path))) {
        epub_free(book);
        return NULL;
    }
    size_t opf_index = reader_zip_find(book->zip, opf_path);
    if(opf_index == (size_t)-1) {
        epub_free(book);
        return NULL;
    }

    EpubOpfResult counts;
    if(!epub_opf_parse(book, opf_index, opf_path, EpubOpfCount, NULL, 0, &counts) ||
       !counts.spine_seen) {
        epub_free(book);
        return NULL;
    }

    size_t spine_capacity = counts.spine_count < EPUB_SPINE_MAX ? counts.spine_count :
                                                                  EPUB_SPINE_MAX;
    if(spine_capacity == 0) {
        /* A book without chapters still opens, it just reads as finished. */
        spine_capacity = 1;
    }

    book->spine = malloc(spine_capacity * sizeof(EpubSpineItem));
    if(book->spine == NULL) {
        epub_free(book);
        return NULL;
    }
    for(size_t index = 0; index < spine_capacity; index++) {
        book->spine[index].href[0] = '\0';
        book->spine[index].title = NULL;
        book->spine[index].zip_index = -1;
        book->spine[index].xhtml = false;
        book->spine[index].readable = false;
    }

    /* The idrefs are only needed to join the manifest with the spine. */
    char(*idrefs)[EPUB_IDREF_MAX] = calloc(spine_capacity, EPUB_IDREF_MAX);
    if(idrefs == NULL) {
        epub_free(book);
        return NULL;
    }

    EpubOpfResult spine;
    if(!epub_opf_parse(book, opf_index, opf_path, EpubOpfSpine, idrefs, spine_capacity, &spine)) {
        free(idrefs);
        epub_free(book);
        return NULL;
    }
    book->spine_count = spine.spine_count < spine_capacity ? spine.spine_count : spine_capacity;

    if(!epub_opf_parse(
           book, opf_index, opf_path, EpubOpfManifest, idrefs, spine_capacity, &spine)) {
        free(idrefs);
        epub_free(book);
        return NULL;
    }
    free(idrefs);

    if(book->title[0] == '\0') {
        epub_title_from_path(book, path);
    }

    return &book->base;
}

/** Every extension the EPUB container is recognised by. */
static const char* const epub_extensions[] = {".epub", NULL};

const ReaderFormat reader_format_epub = {
    .name = "epub",
    .extensions = epub_extensions,
    .open = epub_open,
    .close = epub_close,
    .title = epub_title,
    .chapter_title = epub_chapter_title,
    .chapter_count = epub_chapter_count,
    .chapter_open = epub_chapter_open,
    .chapter_close = epub_chapter_close,
    .read_span = epub_read_span,
};
