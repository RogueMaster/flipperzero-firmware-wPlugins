#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Storage Storage;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t bitmap[];
} Pbm;

// Supports only P4 PBM files for now
Pbm* pbm_load_file(Storage* storage, const char* path);

void pbm_free(Pbm* pbm);

uint16_t pbm_get_pitch(Pbm* pbm);

#ifdef __cplusplus
}
#endif
