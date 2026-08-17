#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool sha1_digest_short(const uint8_t* input, size_t length, uint8_t output[20]);
bool sha1_digest_self_test(void);
