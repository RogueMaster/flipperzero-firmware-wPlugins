#pragma once
#include <stdint.h>

typedef struct {
    uint32_t CTRL;
    uint32_t CYCCNT; // the harness writes this to place each edge in time
} DWT_Type;

typedef struct {
    uint32_t DEMCR;
} CoreDebug_Type;

extern DWT_Type* const DWT;
extern CoreDebug_Type* const CoreDebug;

#define DWT_CTRL_CYCCNTENA_Msk (1u << 0)
#define CoreDebug_DEMCR_TRCENA_Msk (1u << 24)

#define __DMB() __sync_synchronize()
