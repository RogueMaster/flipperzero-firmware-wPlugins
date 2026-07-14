#pragma once

#include <stddef.h>
#include <stdint.h>

uint32_t time_hms_sec(uint8_t, uint8_t, uint8_t);
void time_sec_hms(uint32_t, uint8_t*, uint8_t*, uint8_t*);

#ifndef TIMED_REMOTE_TEST_BUILD
uint32_t time_until(uint8_t, uint8_t, uint8_t);
void time_name(char*, size_t);
#endif
