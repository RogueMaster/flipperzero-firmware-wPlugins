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

#ifndef UID_BRUTE_SMART_PATTERN_ENGINE_H
#define UID_BRUTE_SMART_PATTERN_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PatternUnknown,
    PatternInc1,
    PatternIncK,
    PatternLe16,
    PatternBitmask
} PatternType;

typedef struct {
    PatternType type;
    uint32_t start_uid;
    uint32_t end_uid;
    uint32_t step;
    uint32_t range_size;
    uint32_t bitmask;
} PatternResult;

/**
 * @brief Detect pattern from array of UIDs
 * @param uids Array of UIDs
 * @param count Number of UIDs in array
 * @param result Pointer to store pattern detection result
 * @return true if pattern detected, false otherwise
 */
bool pattern_engine_detect(const uint32_t* uids, uint8_t count, PatternResult* result);

/**
 * @brief Build range of UIDs based on pattern
 * @param result Pattern result containing range information
 * @param range Buffer to store generated UIDs
 * @param range_size Size of range buffer
 * @param actual_size Pointer to store actual number of UIDs generated
 * @return true if range built successfully, false otherwise
 */
bool pattern_engine_build_range(
    const PatternResult* result,
    uint32_t* range,
    uint16_t range_size,
    uint16_t* actual_size);

/**
 * @brief Get human-readable pattern name
 * @param type Pattern type
 * @return String representation of pattern type
 */
const char* pattern_engine_get_name(PatternType type);

/**
 * @brief Validate UID range
 * @param start Starting UID
 * @param end Ending UID
 * @param step Step size
 * @return true if range is valid, false otherwise
 */
bool pattern_engine_validate_range(uint32_t start, uint32_t end, uint32_t step);

#ifdef __cplusplus
}
#endif

#endif // UID_BRUTE_SMART_PATTERN_ENGINE_H
