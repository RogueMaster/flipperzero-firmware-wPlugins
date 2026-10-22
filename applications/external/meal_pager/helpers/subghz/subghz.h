#pragma once

#include "subghz_i.h"

typedef struct SubGhz SubGhz;

SubGhz* subghz_alloc();
void subghz_free(SubGhz* subghz);
bool subghz_send(void* context);
