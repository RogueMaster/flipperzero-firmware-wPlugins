#pragma once

#include <furi.h>

#define TRIGGER_PATTERN_MAX (32u)

typedef struct Trigger Trigger;

Trigger* trigger_alloc(void);
void trigger_free(Trigger* tr);

/** Arm on a pattern. An empty string disarms.
 *
 * Matching is case-insensitive: you are hunting for "login:" on a board whose
 * capitalisation you have not seen yet.
 */
void trigger_set(Trigger* tr, const char* pattern);

const char* trigger_pattern(const Trigger* tr);
bool trigger_is_armed(const Trigger* tr);

/** Reset the match position and the hit count. */
void trigger_reset(Trigger* tr);

/** Feed received bytes. Returns true if the pattern completed in this chunk.
 *
 * Streaming, not line-based: the pattern is found even when it straddles two
 * DMA chunks, which it will whenever the target is talking quickly.
 */
bool trigger_feed(Trigger* tr, const uint8_t* data, size_t len);

uint32_t trigger_hits(const Trigger* tr);
