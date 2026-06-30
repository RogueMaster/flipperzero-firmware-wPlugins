#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "base64.h"

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static int is_base64(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           (c == '+') || (c == '/');
}

char* base64_encode(const unsigned char* input, size_t len) {
    char* output;
    int i = 0, j = 0;
    unsigned char array3[3], array4[4];

    output = (char*)malloc(((len + 2) / 3) * 4 + 1);
    if (!output) return "memory allocation failed, try again";

    size_t out_index = 0;
    while (len--) {
        array3[i++] = *(input++);
        if (i == 3) {
            array4[0] = (array3[0] & 0xfc) >> 2;
            array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
            array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
            array4[3] = array3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                output[out_index++] = base64_chars[array4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            array3[j] = '\0';

        array4[0] = (array3[0] & 0xfc) >> 2;
        array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
        array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
        array4[3] = array3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            output[out_index++] = base64_chars[array4[j]];

        while (i++ < 3)
            output[out_index++] = '=';
    }

    output[out_index] = '\0';
    return output;
}

unsigned char* base64_decode(const char* input, size_t* out_len) {
    int len = strlen(input);
    int i = 0, j = 0, in = 0;
    unsigned char array4[4], array3[3];
    unsigned char* output = (unsigned char*)malloc(len * 3 / 4 + 1);
    if (!output) return (uint8_t *)"memory allocation failed, try again";

    size_t out_index = 0;
    while (len-- && (input[in] != '=') && is_base64(input[in])) {
        array4[i++] = input[in++];
        if (i == 4) {
            for (i = 0; i < 4; i++)
                array4[i] = strchr(base64_chars, array4[i]) - base64_chars;

            array3[0] = (array4[0] << 2) + ((array4[1] & 0x30) >> 4);
            array3[1] = ((array4[1] & 0xf) << 4) + ((array4[2] & 0x3c) >> 2);
            array3[2] = ((array4[2] & 0x3) << 6) + array4[3];

            for (i = 0; i < 3; i++)
                output[out_index++] = array3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 4; j++)
            array4[j] = 0;

        for (j = 0; j < 4; j++)
            array4[j] = strchr(base64_chars, array4[j]) - base64_chars;

        array3[0] = (array4[0] << 2) + ((array4[1] & 0x30) >> 4);
        array3[1] = ((array4[1] & 0xf) << 4) + ((array4[2] & 0x3c) >> 2);
        array3[2] = ((array4[2] & 0x3) << 6) + array4[3];

        for (j = 0; j < i - 1; j++)
            output[out_index++] = array3[j];
    }

    output[out_index] = '\0';
    if (out_len)
        *out_len = out_index;
    return output;
}