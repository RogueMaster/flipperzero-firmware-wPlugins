#pragma once

#include "subghz_i.h"

typedef struct SubGhz SubGhz;

SubGhz* subghz_alloc();
void subghz_free(SubGhz* subghz);
void subghz_send(void* context, const char* path);
void subghz_send_tick(void* context);
void subghz_send_stop(void* context);
