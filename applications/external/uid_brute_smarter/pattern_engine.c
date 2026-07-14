/*
 * Copyright (c) 2024, UID Brute Smarter Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pattern_engine.h"

// Include furi.h only when not in unit test mode
#ifndef UNIT_TEST
#include <furi.h>
#else
// Mock furi functionality for unit tests
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define TAG                    "PatternEngine"
#define FURI_LOG_E(tag, ...)   (void)(tag)
#define FURI_LOG_W(tag, ...)   (void)(tag)
#define FURI_LOG_D(tag, ...)   (void)(tag)
#define FURI_LOG_I(tag, ...)   (void)(tag)
#define furi_assert(condition) (void)(condition)
#define MIN(a, b)              ((a) < (b) ? (a) : (b))
#endif

#define TAG                     "PatternEngine"
#define MAX_RANGE_SIZE          1000
#define DEFAULT_RANGE_EXPANSION 256

static const uint32_t common_k_values[] = {16, 32, 64, 100, 256};
static const uint8_t common_k_count = 5;

bool pattern_engine_detect_bitmask(const uint32_t* uids, uint8_t count, PatternResult* result) {
    uint32_t and_mask = 0xFFFFFFFF;
    uint32_t or_mask = 0x00000000;

    for(uint8_t i = 0; i < count; i++) {
        and_mask &= uids[i];
        or_mask |= uids[i];
    }

    uint32_t bitmask = and_mask ^ or_mask;

    if(bitmask != 0 && (bitmask & (bitmask - 1)) != 0) {
        result->type = PatternBitmask;
        result->start_uid = and_mask;
        result->end_uid = or_mask;
        result->step = 1;
        result->bitmask = bitmask;
        result->range_size = 0;
        for(uint8_t i = 0; i < 32; i++) {
            if((bitmask >> i) & 1) {
                result->range_size++;
            }
        }
        result->range_size = 1 << result->range_size;
        return true;
    }

    return false;
}

bool pattern_engine_detect(const uint32_t* uids, uint8_t count, PatternResult* result) {
    furi_assert(uids);
    furi_assert(result);

    if(count == 0 || count > 5) {
        FURI_LOG_E(TAG, "Invalid UID count: %d", count);
        return false;
    }

    if(count == 1) {
        // Single card - use conservative range expansion
        result->type = PatternUnknown;
        result->start_uid = (uids[0] > 100) ? uids[0] - 100 : 0;
        result->end_uid = (uids[0] < 0xFFFFFF9B) ? uids[0] + 100 : 0xFFFFFFFF;
        result->step = 1;
        result->range_size = (result->end_uid - result->start_uid) + 1;

        // Ensure range doesn't exceed MAX_RANGE_SIZE
        if(result->range_size > MAX_RANGE_SIZE) {
            result->range_size = MAX_RANGE_SIZE;
            result->end_uid = result->start_uid + MAX_RANGE_SIZE - 1;
        }
        return true;
    }

    // Calculate deltas between consecutive UIDs
    int32_t deltas[4];
    for(uint8_t i = 0; i < count - 1; i++) {
        deltas[i] = (int32_t)(uids[i + 1] - uids[i]);
    }

    // Check for +1 linear pattern
    bool is_inc1 = true;
    for(uint8_t i = 0; i < count - 1; i++) {
        if(deltas[i] != 1) {
            is_inc1 = false;
            break;
        }
    }

    if(is_inc1) {
        result->type = PatternInc1;
        // Expand range by 100 in each direction (reduced from 128)
        result->start_uid = (uids[0] > 100) ? uids[0] - 100 : 0;
        result->end_uid = (uids[count - 1] < 0xFFFFFF9B) ? uids[count - 1] + 100 : 0xFFFFFFFF;
        result->step = 1;
        result->range_size = (result->end_uid - result->start_uid) + 1;

        // Ensure range doesn't exceed MAX_RANGE_SIZE
        if(result->range_size > MAX_RANGE_SIZE) {
            result->range_size = MAX_RANGE_SIZE;
            result->end_uid = result->start_uid + MAX_RANGE_SIZE - 1;
        }
        return true;
    }

    // Check for +k linear pattern
    for(uint8_t k_idx = 0; k_idx < common_k_count; k_idx++) {
        uint32_t k = common_k_values[k_idx];
        bool is_inc_k = true;

        for(uint8_t i = 0; i < count - 1; i++) {
            if(deltas[i] != (int32_t)k) {
                is_inc_k = false;
                break;
            }
        }

        if(is_inc_k) {
            result->type = PatternIncK;
            // Expand range by 5 steps in each direction (reduced from 10)
            uint32_t expand = 5 * k;
            result->start_uid = (uids[0] > expand) ? uids[0] - expand : 0;
            result->end_uid =
                (uids[count - 1] < (0xFFFFFFFF - expand)) ? uids[count - 1] + expand : 0xFFFFFFFF;
            result->step = k;
            result->range_size = ((result->end_uid - result->start_uid) / k) + 1;

            // Ensure range doesn't exceed MAX_RANGE_SIZE
            if(result->range_size > MAX_RANGE_SIZE) {
                result->range_size = MAX_RANGE_SIZE;
                result->end_uid = result->start_uid + (MAX_RANGE_SIZE - 1) * k;
            }
            return true;
        }
    }

    // Check for 16-bit little-endian counter pattern
    bool is_le16 = true;
    for(uint8_t i = 0; i < count - 1; i++) {
        if(deltas[i] != 0x10000) {
            is_le16 = false;
            break;
        }
    }

    if(is_le16) {
        result->type = PatternLe16;
        // For 16-bit counter, limit to MAX_RANGE_SIZE instead of full range
        uint32_t base = uids[0] & 0xFFFF0000;
        uint32_t offset = uids[0] & 0x0000FFFF;

        // Create a limited range around the detected offset
        uint32_t start_offset = (offset > 500) ? offset - 500 : 0;
        uint32_t end_offset = (offset < 65035) ? offset + 500 : 0xFFFF;

        // Ensure range doesn't exceed MAX_RANGE_SIZE
        if((end_offset - start_offset + 1) > MAX_RANGE_SIZE) {
            end_offset = start_offset + MAX_RANGE_SIZE - 1;
        }

        result->start_uid = base | start_offset;
        result->end_uid = base | end_offset;
        result->step = 1;
        result->range_size = (result->end_uid - result->start_uid) + 1;
        return true;
    }

    if(pattern_engine_detect_bitmask(uids, count, result)) {
        return true;
    }

    // Unknown pattern - expand around min/max values with constraints
    result->type = PatternUnknown;

    uint32_t min_uid = uids[0];
    uint32_t max_uid = uids[0];

    for(uint8_t i = 1; i < count; i++) {
        if(uids[i] < min_uid) min_uid = uids[i];
        if(uids[i] > max_uid) max_uid = uids[i];
    }

    // Calculate the gap between min and max
    uint32_t gap = max_uid - min_uid;

    // Use conservative expansion based on gap size
    uint32_t expansion = 100;
    if(gap > 1000) {
        // For very large gaps, use minimal expansion
        expansion = 50;
    } else if(gap > 100) {
        // For medium gaps, reduce expansion
        expansion = 75;
    }

    result->start_uid = (min_uid > expansion) ? min_uid - expansion : 0;
    result->end_uid = (max_uid < (0xFFFFFFFF - expansion)) ? max_uid + expansion : 0xFFFFFFFF;
    result->step = 1;
    result->range_size = (result->end_uid - result->start_uid) + 1;

    // Ensure range doesn't exceed MAX_RANGE_SIZE
    if(result->range_size > MAX_RANGE_SIZE) {
        result->range_size = MAX_RANGE_SIZE;
        result->end_uid = result->start_uid + MAX_RANGE_SIZE - 1;
    }

    return true;
}

bool pattern_engine_build_range(
    const PatternResult* result,
    uint32_t* range,
    uint16_t range_size,
    uint16_t* actual_size) {
    // Enhanced input validation
    if(!result || !range || !actual_size) {
        FURI_LOG_E(TAG, "[BUILD_RANGE] NULL pointer passed");
        return false;
    }

    if(range_size == 0 || range_size > MAX_RANGE_SIZE) {
        FURI_LOG_E(
            TAG, "[BUILD_RANGE] Invalid range_size: %d (max: %d)", range_size, MAX_RANGE_SIZE);
        return false;
    }

    FURI_LOG_D(
        TAG,
        "[BUILD_RANGE] Starting build: start=%08lX, end=%08lX, step=%lu, range_size=%lu",
        result->start_uid,
        result->end_uid,
        result->step,
        result->range_size);

    if(result->start_uid > result->end_uid) {
        FURI_LOG_E(TAG, "Invalid range: start=%lu > end=%lu", result->start_uid, result->end_uid);
        return false;
    }

    if(result->step == 0) {
        FURI_LOG_E(TAG, "Invalid step: %lu", result->step);
        return false;
    }

    if(!pattern_engine_validate_range(result->start_uid, result->end_uid, result->step)) {
        FURI_LOG_E(
            TAG,
            "Invalid range: start=%lu, end=%lu, step=%lu",
            result->start_uid,
            result->end_uid,
            result->step);
        return false;
    }

    *actual_size = 0;

    if(result->type == PatternBitmask) {
        uint32_t base = result->start_uid;
        uint32_t bitmask = result->bitmask;
        uint32_t i = 0;

        while(i < result->range_size && *actual_size < range_size) {
            uint32_t current_uid = base;
            uint32_t temp = i;
            for(uint8_t j = 0; j < 32; j++) {
                if((bitmask >> j) & 1) {
                    if(temp & 1) {
                        current_uid |= (1 << j);
                    }
                    temp >>= 1;
                }
            }
            range[*actual_size] = current_uid;
            (*actual_size)++;
            i++;
        }
    } else {
        uint32_t current = result->start_uid;

        // Safety check: ensure we don't overflow the range buffer
        uint16_t max_iterations = MIN(range_size, result->range_size);
        FURI_LOG_D(TAG, "[BUILD_RANGE] Max iterations: %d", max_iterations);

        while(current <= result->end_uid && *actual_size < max_iterations) {
            // Additional bounds checking
            if(*actual_size >= range_size) {
                FURI_LOG_W(TAG, "[BUILD_RANGE] Reached range_size limit: %d", range_size);
                break;
            }

            range[*actual_size] = current;
            (*actual_size)++;

            // Check for overflow before adding step
            if(current > (UINT32_MAX - result->step)) {
                FURI_LOG_W(TAG, "[BUILD_RANGE] UID overflow prevented at %08lX", current);
                break;
            }

            current += result->step;

            // Additional safety check to prevent infinite loops
            if(*actual_size >= MAX_RANGE_SIZE) {
                FURI_LOG_W(TAG, "[BUILD_RANGE] Reached MAX_RANGE_SIZE limit");
                break;
            }

            // Log progress every 50 iterations
            if(*actual_size % 50 == 0) {
                FURI_LOG_D(
                    TAG,
                    "[BUILD_RANGE] Progress: %d/%d, current=%08lX",
                    *actual_size,
                    max_iterations,
                    current);
            }
        }
    }

    FURI_LOG_I(TAG, "[BUILD_RANGE] Completed: built range with %d UIDs", *actual_size);
    if(*actual_size > 0) {
        FURI_LOG_D(
            TAG,
            "[BUILD_RANGE] First UID: %08lX, Last UID: %08lX",
            range[0],
            range[*actual_size - 1]);
    }
    return true;
}

const char* pattern_engine_get_name(PatternType type) {
    switch(type) {
    case PatternInc1:
        return "+1 Linear";
    case PatternIncK:
        return "+K Linear";
    case PatternLe16:
        return "16-bit LE Counter";
    case PatternBitmask:
        return "Bitmask";
    case PatternUnknown:
    default:
        return "Unknown (±256)";
    }
}

bool pattern_engine_validate_range(uint32_t start, uint32_t end, uint32_t step) {
    FURI_LOG_D(
        TAG, "[VALIDATE_RANGE] Validating: start=%08lX, end=%08lX, step=%lu", start, end, step);

    if(step == 0) {
        FURI_LOG_E(TAG, "[VALIDATE_RANGE] Invalid step: 0");
        return false;
    }

    if(start >= end) {
        FURI_LOG_E(TAG, "[VALIDATE_RANGE] Invalid range: start >= end");
        return false;
    }

    // Check if range is too large
    uint64_t range_size = ((uint64_t)end - (uint64_t)start) / (uint64_t)step + 1;
    FURI_LOG_D(TAG, "[VALIDATE_RANGE] Calculated range_size: %llu", range_size);

    if(range_size > MAX_RANGE_SIZE) {
        FURI_LOG_E(TAG, "[VALIDATE_RANGE] Range too large: %llu > %d", range_size, MAX_RANGE_SIZE);
        return false;
    }

    FURI_LOG_D(TAG, "[VALIDATE_RANGE] Range validation successful");
    return true;
}
