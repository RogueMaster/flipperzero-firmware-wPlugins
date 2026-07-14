#pragma once

#include <stddef.h>
#include <stdint.h>

// Forward declaration for Icon type
struct Icon;

// Card struct
typedef struct {
    const char* name;
    const struct Icon* icon;
} Card;

extern const Card card[];
extern const int card_number;
uint16_t unbiased_rand(uint16_t max);

// Returns the total number of cards in the deck (major + minor)
int get_total_card_count(void);
