/**
 * @file xhtml.c
 * @brief Streaming XHTML to text spans: tags are dropped, entities decoded.
 *
 * The document is never loaded as a whole: a fixed input buffer is refilled
 * from a pull callback and the parser is a resumable state machine, so it may
 * stop at any byte and continue on the next call. The output span is built in
 * a fixed buffer as well; a span never splits a UTF-8 sequence.
 *
 * Whitespace in the markup is collapsed to single spaces and does not start a
 * new line: line breaks come from block elements only.
 */
#include "xhtml.h"

#include <stdlib.h>
#include <string.h>

#include "../../text/utf8.h"

/** Bytes pulled from the source at a time. */
#define XHTML_INPUT_SIZE   512
/** Largest span handed out in one call. */
#define XHTML_SPAN_SIZE    256
/** Longest element name kept, longer names never match a keyword. */
#define XHTML_NAME_SIZE    16
/** Longest entity name kept, longer ones are printed literally. */
#define XHTML_ENTITY_SIZE  16
/** Room reserved for pending line breaks and a space before text. */
#define XHTML_LEAD_SIZE    3
/** Most line breaks in a row, that is at most one empty line. */
#define XHTML_MAX_NEWLINES 2

typedef enum {
    XhtmlStateText,
    XhtmlStateTagOpen,
    XhtmlStateTagName,
    XhtmlStateTagAttributes,
    XhtmlStateComment,
    XhtmlStateDeclarationStart,
    XhtmlStateDeclaration,
    XhtmlStateInstruction,
    XhtmlStateSkipElement,
    XhtmlStateSkipSlash,
    XhtmlStateSkipName,
    XhtmlStateSkipBoundary,
    XhtmlStateSkipEnd,
} XhtmlState;

typedef enum {
    XhtmlSkipScan,
    XhtmlSkipAfterOpen,
    XhtmlSkipAfterSlash,
    XhtmlSkipInName,
    XhtmlSkipBoundary,
    XhtmlSkipEnd,
} XhtmlSkipStage;

struct XhtmlParser {
    XhtmlPull pull;
    void* context;
    char input[XHTML_INPUT_SIZE];
    size_t input_pos;
    size_t input_size;
    bool input_eof;
    int pushback;

    XhtmlState state;
    char tag_name[XHTML_NAME_SIZE];
    size_t tag_name_size;
    bool tag_name_overflow;
    bool tag_closing;
    bool tag_self_closing;
    char tag_quote;

    unsigned declaration_dashes;
    unsigned declaration_brackets;
    char declaration_quote;

    char skip_name[XHTML_NAME_SIZE];
    size_t skip_name_size;
    size_t skip_match_size;
    XhtmlSkipStage skip_stage;

    unsigned bold_depth;
    bool bold;

    char span[XHTML_SPAN_SIZE];
    size_t span_size;
    bool span_bold;

    unsigned pending_newlines;
    unsigned trailing_newlines;
    bool pending_space;
    bool started;
    bool finished;
    bool flush;
};

/* ---------------------------------------------------------------- input --- */

static bool xhtml_fill(XhtmlParser* parser, size_t need) {
    if(parser->input_size - parser->input_pos >= need) {
        return true;
    }

    if(parser->input_pos > 0) {
        memmove(
            parser->input,
            parser->input + parser->input_pos,
            parser->input_size - parser->input_pos);
        parser->input_size -= parser->input_pos;
        parser->input_pos = 0;
    }

    while(parser->input_size < need && !parser->input_eof) {
        size_t room = sizeof(parser->input) - parser->input_size;
        size_t read = parser->pull(parser->context, parser->input + parser->input_size, room);
        if(read == 0) {
            parser->input_eof = true;
            break;
        }
        if(read > room) {
            read = room;
        }
        parser->input_size += read;
    }

    return parser->input_size - parser->input_pos >= need;
}

static int xhtml_get(XhtmlParser* parser) {
    if(parser->pushback >= 0) {
        int byte = parser->pushback;
        parser->pushback = -1;
        return byte;
    }
    if(!xhtml_fill(parser, 1)) {
        return -1;
    }
    return (unsigned char)parser->input[parser->input_pos++];
}

static void xhtml_unget(XhtmlParser* parser, int byte) {
    if(byte >= 0) {
        parser->pushback = byte;
    }
}

/* ----------------------------------------------------------------- text --- */

static bool xhtml_is_space(int byte) {
    return byte == ' ' || byte == '\t' || byte == '\r' || byte == '\n' || byte == '\v' ||
           byte == '\f';
}

static char xhtml_lower(int byte) {
    if(byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return (char)byte;
}

static bool xhtml_is_name_start(int byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || byte == '_' ||
           byte == ':';
}

static bool xhtml_is_name_char(int byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == ':' ||
           byte == '.';
}

/** Note that a block element boundary asks for at least count line breaks. */
static void xhtml_break(XhtmlParser* parser, unsigned count) {
    if(count > XHTML_MAX_NEWLINES) {
        count = XHTML_MAX_NEWLINES;
    }
    if(count > parser->pending_newlines) {
        parser->pending_newlines = count;
    }
    parser->pending_space = false;
}

/** Turn deferred line breaks into bytes just before the next glyph. */
static void xhtml_flush_breaks(XhtmlParser* parser) {
    if(!parser->started) {
        parser->pending_newlines = 0;
        parser->pending_space = false;
        return;
    }

    unsigned want = parser->pending_newlines;
    if(want > XHTML_MAX_NEWLINES) {
        want = XHTML_MAX_NEWLINES;
    }
    while(parser->trailing_newlines < want) {
        parser->span[parser->span_size++] = '\n';
        parser->trailing_newlines++;
    }
    parser->pending_newlines = 0;
}

/** Append UTF-8 bytes as text. The caller has checked that they fit. */
static void xhtml_append(XhtmlParser* parser, const char* bytes, size_t size) {
    if(parser->span_size == 0) {
        parser->span_bold = parser->bold;
    }
    xhtml_flush_breaks(parser);
    if(parser->pending_space && parser->started && parser->trailing_newlines == 0) {
        parser->span[parser->span_size++] = ' ';
    }
    parser->pending_space = false;
    memcpy(parser->span + parser->span_size, bytes, size);
    parser->span_size += size;
    parser->trailing_newlines = 0;
    parser->started = true;
}

static bool xhtml_decode_entity(const char* name, size_t size, char* out, size_t* out_size) {
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

/** Decode an entity after '&'; unknown entities are printed as they are. */
static void xhtml_entity(XhtmlParser* parser) {
    char name[XHTML_ENTITY_SIZE];
    size_t size = 0;
    bool closed = false;

    while(size < sizeof(name) - 1) {
        int byte = xhtml_get(parser);
        if(byte < 0) {
            break;
        }
        if(byte == ';') {
            closed = true;
            break;
        }
        if(byte == '<' || byte == '&' || byte == '>' || byte == '"' || byte == '\'' ||
           xhtml_is_space(byte)) {
            xhtml_unget(parser, byte);
            break;
        }
        name[size++] = (char)byte;
    }

    if(!closed && size == sizeof(name) - 1) {
        /* The name filled the buffer: the terminator may still be the next byte. */
        int byte = xhtml_get(parser);
        if(byte == ';') {
            closed = true;
        } else {
            xhtml_unget(parser, byte);
        }
    }

    char out[XHTML_ENTITY_SIZE + 3];
    size_t out_size = 0;

    if(closed && xhtml_decode_entity(name, size, out, &out_size)) {
        xhtml_append(parser, out, out_size);
        return;
    }

    out[out_size++] = '&';
    memcpy(out + out_size, name, size);
    out_size += size;
    if(closed) {
        out[out_size++] = ';';
    }
    xhtml_append(parser, out, out_size);
}

/** Append one byte of text, pulling the rest of its UTF-8 sequence. */
static void xhtml_text_byte(XhtmlParser* parser, int byte) {
    if(parser->span_size > 0 && parser->span_bold != parser->bold) {
        xhtml_unget(parser, byte);
        parser->flush = true;
        return;
    }

    unsigned char lead = (unsigned char)byte;
    size_t length = 1;
    if((lead & 0xE0u) == 0xC0u) {
        length = 2;
    } else if((lead & 0xF0u) == 0xE0u) {
        length = 3;
    } else if((lead & 0xF8u) == 0xF0u) {
        length = 4;
    }

    if(parser->span_size + length + XHTML_LEAD_SIZE >= sizeof(parser->span)) {
        xhtml_unget(parser, byte);
        parser->flush = true;
        return;
    }

    char bytes[4];
    bytes[0] = (char)lead;
    size_t got = 1;
    while(got < length) {
        int next = xhtml_get(parser);
        if(next < 0 || ((unsigned char)next & 0xC0u) != 0x80u) {
            xhtml_unget(parser, next);
            break;
        }
        bytes[got++] = (char)next;
    }

    xhtml_append(parser, bytes, got);
}

/* ----------------------------------------------------------------- tags --- */

static bool xhtml_is_bold_tag(const char* name) {
    return strcmp(name, "b") == 0 || strcmp(name, "strong") == 0 || strcmp(name, "h1") == 0 ||
           strcmp(name, "h2") == 0 || strcmp(name, "h3") == 0 || strcmp(name, "h4") == 0 ||
           strcmp(name, "h5") == 0 || strcmp(name, "h6") == 0;
}

static bool xhtml_is_header_tag(const char* name) {
    return name[0] == 'h' && name[1] >= '1' && name[1] <= '6' && name[2] == '\0';
}

static bool xhtml_is_paragraph_tag(const char* name) {
    return strcmp(name, "p") == 0;
}

static bool xhtml_is_block_tag(const char* name) {
    return strcmp(name, "div") == 0 || strcmp(name, "br") == 0 || strcmp(name, "li") == 0 ||
           strcmp(name, "tr") == 0 || strcmp(name, "blockquote") == 0 || strcmp(name, "pre") == 0;
}

/** Elements whose contents are never reading text. */
static bool xhtml_is_skipped_tag(const char* name) {
    return strcmp(name, "script") == 0 || strcmp(name, "style") == 0 ||
           strcmp(name, "head") == 0 || strcmp(name, "svg") == 0;
}

static void xhtml_enter_skip(XhtmlParser* parser) {
    memcpy(parser->skip_name, parser->tag_name, parser->tag_name_size + 1);
    parser->skip_name_size = parser->tag_name_size;
    parser->skip_match_size = 0;
    parser->skip_stage = XhtmlSkipScan;
    parser->state = XhtmlStateSkipElement;
}

static void xhtml_tag_done(XhtmlParser* parser) {
    parser->tag_name[parser->tag_name_size] = '\0';
    const char* name = parser->tag_name;
    bool known = !parser->tag_name_overflow;

    if(known && parser->tag_closing) {
        if(xhtml_is_bold_tag(name)) {
            if(parser->bold_depth > 0) {
                parser->bold_depth--;
            }
        }
        if(xhtml_is_header_tag(name) || xhtml_is_paragraph_tag(name)) {
            xhtml_break(parser, 1);
        } else if(xhtml_is_block_tag(name)) {
            xhtml_break(parser, 1);
        }
    } else if(known) {
        if(!parser->tag_self_closing && xhtml_is_bold_tag(name)) {
            parser->bold_depth++;
        }
        if(xhtml_is_header_tag(name) || xhtml_is_paragraph_tag(name)) {
            xhtml_break(parser, 2);
        } else if(xhtml_is_block_tag(name)) {
            xhtml_break(parser, 1);
        }
        if(!parser->tag_self_closing && xhtml_is_skipped_tag(name)) {
            xhtml_enter_skip(parser);
        }
    }

    parser->bold = parser->bold_depth > 0;
}

/* ------------------------------------------------------------ skip mode --- */

static void xhtml_skip_byte(XhtmlParser* parser, int byte) {
    switch(parser->skip_stage) {
    case XhtmlSkipScan:
        if(byte == '<') {
            parser->skip_stage = XhtmlSkipAfterOpen;
        }
        break;
    case XhtmlSkipAfterOpen:
        if(byte == '/') {
            parser->skip_stage = XhtmlSkipAfterSlash;
            parser->skip_match_size = 0;
        } else {
            parser->skip_stage = XhtmlSkipScan;
        }
        break;
    case XhtmlSkipAfterSlash:
        parser->skip_stage = XhtmlSkipInName;
        /* fall through */
    case XhtmlSkipInName:
        if(xhtml_is_name_char(byte)) {
            if(parser->skip_match_size < parser->skip_name_size) {
                if(xhtml_lower(byte) == parser->skip_name[parser->skip_match_size]) {
                    parser->skip_match_size++;
                } else {
                    parser->skip_stage = XhtmlSkipScan;
                }
            } else {
                parser->skip_stage = XhtmlSkipScan;
            }
            if(parser->skip_match_size == parser->skip_name_size) {
                parser->skip_stage = XhtmlSkipBoundary;
            }
        } else if(byte == '>' && parser->skip_match_size == parser->skip_name_size) {
            parser->state = XhtmlStateText;
        } else {
            parser->skip_stage = XhtmlSkipScan;
        }
        break;
    case XhtmlSkipBoundary:
        if(byte == '>') {
            parser->state = XhtmlStateText;
        } else if(xhtml_is_space(byte) || byte == '/') {
            parser->skip_stage = XhtmlSkipEnd;
        } else {
            parser->skip_stage = XhtmlSkipScan;
        }
        break;
    case XhtmlSkipEnd:
        if(byte == '>') {
            parser->state = XhtmlStateText;
        }
        break;
    }
}

/* ----------------------------------------------------------------- main --- */

static void xhtml_tag_name_byte(XhtmlParser* parser, int byte) {
    if(parser->tag_name_size < sizeof(parser->tag_name) - 1) {
        parser->tag_name[parser->tag_name_size++] = xhtml_lower(byte);
    } else {
        parser->tag_name_overflow = true;
    }
}

static void xhtml_state_byte(XhtmlParser* parser, int byte) {
    switch(parser->state) {
    case XhtmlStateText:
        if(byte == '<') {
            if(parser->span_size + XHTML_LEAD_SIZE + 1 >= sizeof(parser->span)) {
                xhtml_unget(parser, byte);
                parser->flush = true;
                break;
            }
            parser->state = XhtmlStateTagOpen;
        } else if(byte == '&') {
            if(parser->span_size + XHTML_ENTITY_SIZE + XHTML_LEAD_SIZE >= sizeof(parser->span)) {
                xhtml_unget(parser, byte);
                parser->flush = true;
                break;
            }
            xhtml_entity(parser);
        } else if(xhtml_is_space(byte)) {
            parser->pending_space = true;
        } else {
            xhtml_text_byte(parser, byte);
        }
        break;

    case XhtmlStateTagOpen:
        if(byte == '!') {
            parser->state = XhtmlStateDeclarationStart;
            parser->declaration_dashes = 0;
        } else if(byte == '?') {
            parser->state = XhtmlStateInstruction;
        } else if(byte == '/') {
            parser->tag_closing = true;
            parser->tag_name_size = 0;
            parser->tag_name_overflow = false;
            parser->tag_self_closing = false;
            parser->tag_quote = 0;
            parser->state = XhtmlStateTagName;
        } else if(xhtml_is_name_start(byte)) {
            parser->tag_closing = false;
            parser->tag_name_size = 0;
            parser->tag_name_overflow = false;
            parser->tag_self_closing = false;
            parser->tag_quote = 0;
            xhtml_tag_name_byte(parser, byte);
            parser->state = XhtmlStateTagName;
        } else {
            /* Not a tag after all: keep the '<' as text and reprocess. */
            xhtml_append(parser, "<", 1);
            parser->state = XhtmlStateText;
            xhtml_unget(parser, byte);
        }
        break;

    case XhtmlStateTagName:
        if(xhtml_is_name_char(byte)) {
            xhtml_tag_name_byte(parser, byte);
        } else if(byte == '>') {
            parser->tag_self_closing = false;
            xhtml_tag_done(parser);
            if(parser->state != XhtmlStateSkipElement) {
                parser->state = XhtmlStateText;
            }
        } else if(byte == '/') {
            parser->tag_self_closing = true;
            parser->state = XhtmlStateTagAttributes;
        } else {
            parser->state = XhtmlStateTagAttributes;
            xhtml_state_byte(parser, byte);
        }
        break;

    case XhtmlStateTagAttributes:
        if(parser->tag_quote != 0) {
            if(byte == (int)parser->tag_quote) {
                parser->tag_quote = 0;
            }
        } else if(byte == '"' || byte == '\'') {
            parser->tag_quote = (char)byte;
        } else if(byte == '>') {
            xhtml_tag_done(parser);
            if(parser->state != XhtmlStateSkipElement) {
                parser->state = XhtmlStateText;
            }
        } else if(byte == '/') {
            parser->tag_self_closing = true;
        } else if(!xhtml_is_space(byte)) {
            parser->tag_self_closing = false;
        }
        break;

    case XhtmlStateComment:
        if(byte == '-') {
            parser->declaration_dashes++;
        } else if(byte == '>' && parser->declaration_dashes >= 2) {
            parser->declaration_dashes = 0;
            parser->state = XhtmlStateText;
        } else {
            parser->declaration_dashes = 0;
        }
        break;

    case XhtmlStateDeclarationStart:
        if(byte == '-' && parser->declaration_dashes == 0) {
            parser->declaration_dashes = 1;
        } else if(byte == '-' && parser->declaration_dashes == 1) {
            parser->declaration_dashes = 0;
            parser->state = XhtmlStateComment;
        } else {
            parser->declaration_dashes = 0;
            parser->declaration_brackets = 0;
            parser->declaration_quote = 0;
            parser->state = XhtmlStateDeclaration;
            xhtml_state_byte(parser, byte);
        }
        break;

    case XhtmlStateDeclaration:
        if(parser->declaration_quote != 0) {
            if(byte == (int)parser->declaration_quote) {
                parser->declaration_quote = 0;
            }
        } else if(byte == '"' || byte == '\'') {
            parser->declaration_quote = (char)byte;
        } else if(byte == '[') {
            parser->declaration_brackets++;
        } else if(byte == ']') {
            if(parser->declaration_brackets > 0) {
                parser->declaration_brackets--;
            }
        } else if(byte == '>' && parser->declaration_brackets == 0) {
            parser->state = XhtmlStateText;
        }
        break;

    case XhtmlStateInstruction:
        if(byte == '>') {
            parser->state = XhtmlStateText;
        }
        break;

    case XhtmlStateSkipElement:
    case XhtmlStateSkipSlash:
    case XhtmlStateSkipName:
    case XhtmlStateSkipBoundary:
    case XhtmlStateSkipEnd:
        xhtml_skip_byte(parser, byte);
        break;
    }
}

/* ------------------------------------------------------------ interface --- */

XhtmlParser* xhtml_alloc(void) {
    XhtmlParser* parser = malloc(sizeof(XhtmlParser));
    if(parser == NULL) {
        return NULL;
    }
    memset(parser, 0, sizeof(XhtmlParser));
    parser->pushback = -1;
    return parser;
}

void xhtml_free(XhtmlParser* parser) {
    free(parser);
}

void xhtml_begin(XhtmlParser* parser, XhtmlPull pull, void* context) {
    if(parser == NULL) {
        return;
    }

    memset(parser, 0, sizeof(XhtmlParser));
    parser->pull = pull;
    parser->context = context;
    parser->pushback = -1;
    parser->state = XhtmlStateText;

    if(pull == NULL) {
        parser->input_eof = true;
        return;
    }

    /* Skip a UTF-8 byte order mark, also when the file is shorter than one. */
    static const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    size_t available = 0;
    if(xhtml_fill(parser, 3)) {
        available = 3;
    } else {
        available = parser->input_size - parser->input_pos;
    }
    size_t matched = 0;
    while(matched < available &&
          (unsigned char)parser->input[parser->input_pos + matched] == bom[matched]) {
        matched++;
    }
    if(matched == 3 || (parser->input_eof && matched == available && available > 0)) {
        parser->input_pos += matched;
    }
}

bool xhtml_next_span(XhtmlParser* parser, ReaderSpan* span) {
    if(parser == NULL || span == NULL || parser->pull == NULL) {
        return false;
    }

    for(;;) {
        parser->span_size = 0;
        parser->span_bold = parser->bold;
        parser->flush = false;
        bool eof = false;

        while(parser->span_size < sizeof(parser->span)) {
            int byte = xhtml_get(parser);
            if(byte < 0) {
                eof = true;
                break;
            }

            xhtml_state_byte(parser, byte);

            if(parser->span_size > 0 && parser->span_bold != parser->bold) {
                /* A space before a style change belongs to the text before it. */
                if(parser->pending_space && parser->trailing_newlines == 0 &&
                   parser->span_size < sizeof(parser->span)) {
                    parser->span[parser->span_size++] = ' ';
                    parser->pending_space = false;
                }
                parser->flush = true;
            }
            if(parser->flush) {
                break;
            }
        }

        if(parser->span_size > 0) {
            span->text = parser->span;
            span->size = parser->span_size;
            span->bold = parser->span_bold;
            return true;
        }
        if(eof || parser->finished) {
            parser->finished = true;
            return false;
        }
        parser->flush = false;
    }
}
