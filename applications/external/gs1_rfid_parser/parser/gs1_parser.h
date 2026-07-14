#pragma once

#include <furi_hal.h>
#include <stdbool.h>
#include <stdint.h>

#include "uart/uhf_u107_tag.h"

#define GTIN_LENGTH     15
#define SERIAL_LENGTH   21
#define DATE_LENGTH     9
#define AI_VALUE_LENGTH 64

typedef enum {
    PARSING_SUCCESS,
    PARSING_NOT_GTIN,
    PARSING_MALFORMED_DATA,
} ParsingResult;

typedef struct {
    uint16_t ai_identifier;
    char ai_value[AI_VALUE_LENGTH];
} GS1_AI;

typedef struct {
    char* type;
    uint8_t ai_count;
    GS1_AI ai_list[AI_VALUE_LENGTH];
} ParsedTagInformation;

ParsingResult parse_epc(const UhfEpcTagData* epc_data, ParsedTagInformation* tag_info);
