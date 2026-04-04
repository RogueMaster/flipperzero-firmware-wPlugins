#include "xml_parser.h"
#include "memzero.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XML_TAG_BUFFER_SIZE    1024U
#define XML_TEXT_BUFFER_SIZE   256U
#define XML_ENTITY_BUFFER_SIZE 16U
#define XML_MAX_ATTRIBUTES     16U

typedef enum {
    XmlParserStateText = 0,
    XmlParserStateTag,
    XmlParserStateComment,
    XmlParserStateProcessingInstruction,
    XmlParserStateDeclaration,
} XmlParserState;

struct XmlParser {
    void* context;
    XmlParserStartElementCallback start_element_handler;
    XmlParserEndElementCallback end_element_handler;
    XmlParserCharacterDataCallback character_data_handler;
    XmlParserState state;
    bool tag_in_quote;
    char tag_quote_char;
    bool in_entity;
    char entity_buffer[XML_ENTITY_BUFFER_SIZE];
    size_t entity_len;
    char tag_buffer[XML_TAG_BUFFER_SIZE];
    size_t tag_len;
    char text_buffer[XML_TEXT_BUFFER_SIZE];
    size_t text_len;
    char special_tail[3];
    char error[128];
};

static void xml_parser_set_error(XmlParser* parser, const char* message) {
    if(parser->error[0] == '\0') {
        snprintf(parser->error, sizeof(parser->error), "%s", message);
    }
}

static bool xml_parser_emit_text(XmlParser* parser) {
    if(parser->text_len == 0U) {
        return true;
    }

    if(parser->character_data_handler != NULL) {
        parser->character_data_handler(parser->context, parser->text_buffer, (int)parser->text_len);
    }
    parser->text_len = 0U;
    return true;
}

static bool xml_parser_emit_codepoint(XmlParser* parser, uint32_t codepoint) {
    char encoded[4];
    size_t encoded_len = 0U;

    if(codepoint <= 0x7FU) {
        encoded[0] = (char)codepoint;
        encoded_len = 1U;
    } else if(codepoint <= 0x7FFU) {
        encoded[0] = (char)(0xC0U | ((codepoint >> 6) & 0x1FU));
        encoded[1] = (char)(0x80U | (codepoint & 0x3FU));
        encoded_len = 2U;
    } else if(codepoint <= 0xFFFFU) {
        encoded[0] = (char)(0xE0U | ((codepoint >> 12) & 0x0FU));
        encoded[1] = (char)(0x80U | ((codepoint >> 6) & 0x3FU));
        encoded[2] = (char)(0x80U | (codepoint & 0x3FU));
        encoded_len = 3U;
    } else if(codepoint <= 0x10FFFFU) {
        encoded[0] = (char)(0xF0U | ((codepoint >> 18) & 0x07U));
        encoded[1] = (char)(0x80U | ((codepoint >> 12) & 0x3FU));
        encoded[2] = (char)(0x80U | ((codepoint >> 6) & 0x3FU));
        encoded[3] = (char)(0x80U | (codepoint & 0x3FU));
        encoded_len = 4U;
    } else {
        xml_parser_set_error(parser, "The XML entity contains an invalid codepoint.");
        return false;
    }

    if(parser->text_len + encoded_len > sizeof(parser->text_buffer) && !xml_parser_emit_text(parser)) {
        return false;
    }

    memcpy(&parser->text_buffer[parser->text_len], encoded, encoded_len);
    parser->text_len += encoded_len;
    return true;
}

static bool xml_parser_emit_plain_char(XmlParser* parser, char c) {
    if(parser->text_len >= sizeof(parser->text_buffer) && !xml_parser_emit_text(parser)) {
        return false;
    }

    parser->text_buffer[parser->text_len++] = c;
    return true;
}

static bool xml_parser_finish_entity(XmlParser* parser) {
    uint32_t codepoint = 0U;

    parser->entity_buffer[parser->entity_len] = '\0';
    if(strcmp(parser->entity_buffer, "amp") == 0) {
        codepoint = '&';
    } else if(strcmp(parser->entity_buffer, "lt") == 0) {
        codepoint = '<';
    } else if(strcmp(parser->entity_buffer, "gt") == 0) {
        codepoint = '>';
    } else if(strcmp(parser->entity_buffer, "quot") == 0) {
        codepoint = '"';
    } else if(strcmp(parser->entity_buffer, "apos") == 0) {
        codepoint = '\'';
    } else if(parser->entity_buffer[0] == '#') {
        char* end = NULL;
        if(parser->entity_buffer[1] == 'x' || parser->entity_buffer[1] == 'X') {
            codepoint = (uint32_t)strtoul(&parser->entity_buffer[2], &end, 16);
        } else {
            codepoint = (uint32_t)strtoul(&parser->entity_buffer[1], &end, 10);
        }
        if(end == NULL || *end != '\0') {
            xml_parser_set_error(parser, "The XML contains an invalid numeric entity.");
            return false;
        }
    } else {
        xml_parser_set_error(parser, "The XML contains an unsupported entity.");
        return false;
    }

    parser->in_entity = false;
    parser->entity_len = 0U;
    return xml_parser_emit_codepoint(parser, codepoint);
}

static bool xml_parser_text_char(XmlParser* parser, char c) {
    if(parser->in_entity) {
        if(c == ';') {
            return xml_parser_finish_entity(parser);
        }

        if(parser->entity_len + 1U >= sizeof(parser->entity_buffer)) {
            xml_parser_set_error(parser, "The XML entity is too large.");
            return false;
        }

        parser->entity_buffer[parser->entity_len++] = c;
        return true;
    }

    if(c == '&') {
        parser->in_entity = true;
        parser->entity_len = 0U;
        return true;
    }

    return xml_parser_emit_plain_char(parser, c);
}

static char* xml_parser_trim_left(char* text) {
    while(*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    return text;
}

static void xml_parser_trim_right(char* text) {
    size_t len = strlen(text);
    while(len > 0U &&
          (text[len - 1U] == ' ' || text[len - 1U] == '\t' || text[len - 1U] == '\r' ||
           text[len - 1U] == '\n')) {
        text[--len] = '\0';
    }
}

static bool xml_parser_parse_tag(XmlParser* parser) {
    char* cursor;
    const char* attrs[XML_MAX_ATTRIBUTES * 2U + 2U];
    size_t attr_count = 0U;
    bool self_closing = false;

    parser->tag_buffer[parser->tag_len] = '\0';
    cursor = xml_parser_trim_left(parser->tag_buffer);
    xml_parser_trim_right(cursor);

    if(cursor[0] == '\0') {
        return true;
    }

    if(cursor[0] == '!') {
        return true;
    }

    if(cursor[0] == '/') {
        char* name = xml_parser_trim_left(cursor + 1U);
        char* end = name;
        while(*end != '\0' && *end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') {
            end++;
        }
        *end = '\0';
        if(parser->end_element_handler != NULL) {
            parser->end_element_handler(parser->context, name);
        }
        return true;
    }

    xml_parser_trim_right(cursor);
    size_t len = strlen(cursor);
    if(len > 0U && cursor[len - 1U] == '/') {
        cursor[len - 1U] = '\0';
        xml_parser_trim_right(cursor);
        self_closing = true;
    }

    char* name = cursor;
    while(*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' &&
          *cursor != '\n') {
        cursor++;
    }

    if(*cursor != '\0') {
        *cursor++ = '\0';
    }

    while(*cursor != '\0') {
        char* attr_name = xml_parser_trim_left(cursor);
        char* attr_value = NULL;
        char quote = '\0';

        if(*attr_name == '\0') {
            break;
        }

        cursor = attr_name;
        while(*cursor != '\0' && *cursor != '=' && *cursor != ' ' && *cursor != '\t' &&
              *cursor != '\r' && *cursor != '\n') {
            cursor++;
        }

        if(*cursor == '\0') {
            xml_parser_set_error(parser, "The XML tag contains an incomplete attribute.");
            return false;
        }

        if(*cursor != '=') {
            *cursor++ = '\0';
            cursor = xml_parser_trim_left(cursor);
            if(*cursor != '=') {
                xml_parser_set_error(parser, "The XML attribute is missing '='.");
                return false;
            }
        }

        *cursor++ = '\0';
        cursor = xml_parser_trim_left(cursor);
        if(*cursor != '"' && *cursor != '\'') {
            xml_parser_set_error(parser, "The XML attribute is missing quotes.");
            return false;
        }

        quote = *cursor++;
        attr_value = cursor;
        while(*cursor != '\0' && *cursor != quote) {
            cursor++;
        }

        if(*cursor != quote) {
            xml_parser_set_error(parser, "The XML attribute string is not terminated.");
            return false;
        }

        *cursor++ = '\0';
        if(attr_count + 2U >= COUNT_OF(attrs)) {
            xml_parser_set_error(parser, "The XML tag contains too many attributes.");
            return false;
        }

        attrs[attr_count++] = attr_name;
        attrs[attr_count++] = attr_value;
    }

    attrs[attr_count] = NULL;
    attrs[attr_count + 1U] = NULL;

    if(parser->start_element_handler != NULL) {
        parser->start_element_handler(parser->context, name, attrs);
    }
    if(self_closing && parser->end_element_handler != NULL) {
        parser->end_element_handler(parser->context, name);
    }

    return true;
}

static bool xml_parser_special_complete(
    XmlParser* parser,
    XmlParserState state,
    char c,
    bool* consumed_special_char) {
    parser->special_tail[0] = parser->special_tail[1];
    parser->special_tail[1] = parser->special_tail[2];
    parser->special_tail[2] = c;
    *consumed_special_char = false;

    if(state == XmlParserStateProcessingInstruction) {
        if(parser->special_tail[1] == '?' && parser->special_tail[2] == '>') {
            parser->state = XmlParserStateText;
            parser->special_tail[0] = '\0';
            parser->special_tail[1] = '\0';
            parser->special_tail[2] = '\0';
            return true;
        }
        return true;
    }

    if(state == XmlParserStateComment) {
        if(parser->special_tail[0] == '-' && parser->special_tail[1] == '-' &&
           parser->special_tail[2] == '>') {
            parser->state = XmlParserStateText;
            parser->special_tail[0] = '\0';
            parser->special_tail[1] = '\0';
            parser->special_tail[2] = '\0';
            return true;
        }
        return true;
    }

    if(state == XmlParserStateDeclaration) {
        if(parser->special_tail[2] == '>') {
            parser->state = XmlParserStateText;
            parser->special_tail[0] = '\0';
            parser->special_tail[1] = '\0';
            parser->special_tail[2] = '\0';
            return true;
        }
        return true;
    }

    return false;
}

XmlParser* xml_parser_alloc(void) {
    XmlParser* parser = malloc(sizeof(XmlParser));
    if(parser == NULL) {
        return NULL;
    }

    memset(parser, 0, sizeof(*parser));
    parser->state = XmlParserStateText;
    return parser;
}

void xml_parser_free(XmlParser* parser) {
    if(parser != NULL) {
        memzero(parser, sizeof(*parser));
        free(parser);
    }
}

void xml_parser_reset(XmlParser* parser) {
    if(parser == NULL) {
        return;
    }

    parser->state = XmlParserStateText;
    parser->tag_in_quote = false;
    parser->tag_quote_char = '\0';
    parser->in_entity = false;
    parser->entity_len = 0U;
    parser->tag_len = 0U;
    parser->text_len = 0U;
    memset(parser->special_tail, 0, sizeof(parser->special_tail));
    parser->error[0] = '\0';
}

const char* xml_parser_get_last_error(const XmlParser* parser) {
    return parser != NULL ? parser->error : "";
}

void xml_parser_set_callback_context(XmlParser* parser, void* context) {
    if(parser != NULL) {
        parser->context = context;
    }
}

void xml_parser_set_element_handlers(
    XmlParser* parser,
    XmlParserStartElementCallback start_element_handler,
    XmlParserEndElementCallback end_element_handler) {
    if(parser != NULL) {
        parser->start_element_handler = start_element_handler;
        parser->end_element_handler = end_element_handler;
    }
}

void xml_parser_set_character_data_handler(
    XmlParser* parser,
    XmlParserCharacterDataCallback character_data_handler) {
    if(parser != NULL) {
        parser->character_data_handler = character_data_handler;
    }
}

bool xml_parser_feed(XmlParser* parser, const char* xml, size_t xml_len, bool final) {
    furi_assert(parser);

    if(xml == NULL && xml_len > 0U) {
        xml_parser_set_error(parser, "The XML parser received invalid input.");
        return false;
    }

    for(size_t index = 0U; index < xml_len; index++) {
        const char c = xml[index];
        bool ignored = false;

        if(parser->state == XmlParserStateComment ||
           parser->state == XmlParserStateProcessingInstruction ||
           parser->state == XmlParserStateDeclaration) {
            if(!xml_parser_special_complete(parser, parser->state, c, &ignored)) {
                return false;
            }
            continue;
        }

        if(parser->state == XmlParserStateText) {
            if(c == '<') {
                if(parser->in_entity) {
                    xml_parser_set_error(parser, "The XML contains an unterminated entity.");
                    return false;
                }
                if(!xml_parser_emit_text(parser)) {
                    return false;
                }
                parser->state = XmlParserStateTag;
                parser->tag_len = 0U;
                parser->tag_in_quote = false;
                parser->tag_quote_char = '\0';
                continue;
            }

            if(!xml_parser_text_char(parser, c)) {
                return false;
            }
            continue;
        }

        if(parser->tag_len >= (sizeof(parser->tag_buffer) - 1U)) {
            xml_parser_set_error(parser, "The XML tag exceeds the parser buffer.");
            return false;
        }

        parser->tag_buffer[parser->tag_len++] = c;
        if(parser->tag_len == 1U && c == '?') {
            parser->state = XmlParserStateProcessingInstruction;
            parser->tag_len = 0U;
            memset(parser->special_tail, 0, sizeof(parser->special_tail));
            continue;
        }

        if(parser->tag_len == 1U && c == '!') {
            continue;
        }

        if(parser->tag_len == 3U && memcmp(parser->tag_buffer, "!--", 3U) == 0) {
            parser->state = XmlParserStateComment;
            parser->tag_len = 0U;
            memset(parser->special_tail, 0, sizeof(parser->special_tail));
            continue;
        }

        if(parser->tag_len == 8U && memcmp(parser->tag_buffer, "![CDATA[", 8U) == 0) {
            xml_parser_set_error(parser, "CDATA sections are not supported.");
            return false;
        }

        if(parser->tag_buffer[0] == '!' && parser->tag_len == 2U &&
           parser->tag_buffer[1] != '-' && parser->tag_buffer[1] != '[') {
            parser->state = XmlParserStateDeclaration;
            parser->tag_len = 0U;
            memset(parser->special_tail, 0, sizeof(parser->special_tail));
            continue;
        }

        if(c == '"' || c == '\'') {
            if(parser->tag_in_quote && parser->tag_quote_char == c) {
                parser->tag_in_quote = false;
                parser->tag_quote_char = '\0';
            } else if(!parser->tag_in_quote) {
                parser->tag_in_quote = true;
                parser->tag_quote_char = c;
            }
            continue;
        }

        if(c == '>' && !parser->tag_in_quote) {
            parser->tag_len--;
            if(!xml_parser_parse_tag(parser)) {
                return false;
            }
            parser->state = XmlParserStateText;
            parser->tag_len = 0U;
        }
    }

    if(parser->error[0] != '\0') {
        return false;
    }

    if(final) {
        if(parser->state != XmlParserStateText || parser->tag_len != 0U || parser->in_entity) {
            xml_parser_set_error(parser, "The XML document ended with an incomplete token.");
            return false;
        }
        return xml_parser_emit_text(parser);
    }

    return true;
}

bool xml_parser_parse(XmlParser* parser, const char* xml, size_t xml_len) {
    xml_parser_reset(parser);
    return xml_parser_feed(parser, xml, xml_len, true);
}
