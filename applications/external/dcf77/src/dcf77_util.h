#ifndef __arha_dcf77util_h
#define __arha_dcf77util_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

bool dcf77_message_get_bit(const uint8_t* message, uint8_t bit);
void set_dcf_message(
    uint8_t* dest,
    uint8_t minute,
    uint8_t hour,
    uint8_t day,
    uint8_t month,
    uint8_t year,
    uint8_t dow,
    bool dst,
    bool predst,
    bool abnormal,
    bool leap,
    uint16_t civbits);

#endif
