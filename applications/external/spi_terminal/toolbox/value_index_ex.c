#include "value_index_ex.h"

#define SPI_TERMINAL_VALUE_INDEX_IMPL(func_name, type)                             \
    size_t func_name(const type value, const type values[], size_t values_count) { \
        size_t index = 0;                                                          \
        for(size_t i = 0; i < values_count; i++) {                                 \
            if(value == values[i]) {                                               \
                index = i;                                                         \
                break;                                                             \
            }                                                                      \
        }                                                                          \
        return index;                                                              \
    }

SPI_TERMINAL_VALUE_INDEX_IMPL(value_index_display_mode, TerminalDisplayMode);
SPI_TERMINAL_VALUE_INDEX_IMPL(value_index_size_t, size_t);
SPI_TERMINAL_VALUE_INDEX_IMPL(value_index_buffer_behaviour, TerminalBufferBehaviour);
