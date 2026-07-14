#pragma once

#include "gs1_parser.h"

ParsingResult parse_epc_ais(
    ParsedTagInformation* tag_info,
    const uint8_t* data_stream,
    uint32_t starting_bit,
    uint32_t data_bit_count);

uint64_t read_next_n_bits(const uint8_t* data_stream, uint32_t starting_bit, uint32_t num_bits);
void snprintf_with_padding(char* data, size_t data_length, uint64_t num, uint32_t num_length);
int32_t parse_variable_length_alphanumeric(
    const uint8_t* data,
    uint32_t starting_bit,
    uint8_t length_bit_count,
    char* parsed_value);
int32_t parse_fixed_length_numeric(
    const uint8_t* data,
    uint32_t starting_bit,
    uint8_t length,
    char* parsed_value);
int32_t parse_variable_length_numeric(
    const uint8_t* data,
    uint32_t starting_bit,
    uint8_t length_bit_count,
    char* parsed_value);
int32_t parse_country_code(const uint8_t* data, uint32_t starting_bit, char* parsed_value);
