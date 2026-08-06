#pragma once

#include "../app_user.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration to avoid dependency cycle with scanner headers
typedef struct ScannerSession scanner_session_t;

typedef struct {
    // Protocol display name
    const char* (*get_display_name)(void);

    // Protocol-specific hardware/database setup
    void (*init)(App* app);

    // Blocking loop packet acquisition function
    bool (*run)(scanner_session_t* session, uint32_t timeout_ms);

    // Protocol-specific teardown and hardware restore
    void (*cleanup)(App* app);

    // Page count for the neighbor details view
    uint8_t (*get_details_page_count)(neighbor_t* neighbor);

    // Constructs details strings for a given page index
    void (*build_details_page)(
        neighbor_t* neighbor,
        uint8_t page,
        char* line1,
        size_t line1_size,
        char* line2,
        size_t line2_size,
        char* line3,
        size_t line3_size,
        char* line4,
        size_t line4_size);
} PassiveProtocolHandler;
