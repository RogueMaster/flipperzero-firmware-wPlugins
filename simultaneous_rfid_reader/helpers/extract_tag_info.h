#include <string.h>
#include <stdlib.h>
#include <furi.h>

//Function Declarations
char* extract_epc(const char* Input);

char* extract_res(const char* Input);

char* extract_tid(const char* Input);

char* extract_mem(const char* Input);

char* extract_name(const char* Input);

char* convertToHexString(uint8_t* array, size_t length);

char* extract_crc(const char* Input);

char* extract_pc(const char* Input);