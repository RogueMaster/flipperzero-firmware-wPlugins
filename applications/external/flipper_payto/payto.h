#pragma once

#include <stdbool.h>
#include <stddef.h>

#define PAYTO_MAX_URI_LEN    192
#define PAYTO_MAX_NAME_LEN   16
#define PAYTO_SHORT_NAME_LEN 18

typedef struct {
    char uri[PAYTO_MAX_URI_LEN];
    char currency[8];
    char address[128];
    char short_name[PAYTO_SHORT_NAME_LEN];
    char name[PAYTO_MAX_NAME_LEN + 1];
} PaytoWallet;

bool payto_parse_uri(const char* uri, PaytoWallet* wallet);
bool payto_parse_ndef(const unsigned char* data, size_t length, PaytoWallet* wallet);
void payto_format_name(const PaytoWallet* wallet, char* output, size_t output_size);
