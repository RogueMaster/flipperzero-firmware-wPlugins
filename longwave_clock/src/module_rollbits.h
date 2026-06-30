
#ifndef MODULE_ROLLBITS_HEADERS
#define MODULE_ROLLBITS_HEADERS

#include "flipper.h"
#include "src/protocols.h"
#include "logic_general.h"

void draw_decoded_bits(
    Canvas* canvas,
    LWCType type,
    uint16_t top,
    Bit buffer[],
    uint8_t length_buffer,
    uint8_t count_signals,
    uint8_t last_index,
    bool waiting_for_interrupt,
    bool waiting_for_sync);

#endif
