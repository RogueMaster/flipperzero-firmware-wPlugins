#pragma once

#include <furi.h>

char byte_to_hex(uint8_t byte);
void print_bytes(char* tag, uint8_t* bytes, size_t len);
void merge_path(char* target, char* base, char* name);
bool CheckMTPStringHasUnicode(uint8_t* buffer);
char* ReadMTPString(uint8_t* buffer);
void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length);
