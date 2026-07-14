#pragma once

#include <furi.h>

typedef void (
    *XmlParserStartElementCallback)(void* context, const char* name, const char** attributes);
typedef void (*XmlParserEndElementCallback)(void* context, const char* name);
typedef void (*XmlParserCharacterDataCallback)(void* context, const char* data, int len);

typedef struct XmlParser XmlParser;

XmlParser* xml_parser_alloc(void);
void xml_parser_free(XmlParser* parser);
void xml_parser_reset(XmlParser* parser);
const char* xml_parser_get_last_error(const XmlParser* parser);
void xml_parser_set_callback_context(XmlParser* parser, void* context);
void xml_parser_set_element_handlers(
    XmlParser* parser,
    XmlParserStartElementCallback start_element_handler,
    XmlParserEndElementCallback end_element_handler);
void xml_parser_set_character_data_handler(
    XmlParser* parser,
    XmlParserCharacterDataCallback character_data_handler);
bool xml_parser_feed(XmlParser* parser, const char* xml, size_t xml_len, bool final);
bool xml_parser_parse(XmlParser* parser, const char* xml, size_t xml_len);
