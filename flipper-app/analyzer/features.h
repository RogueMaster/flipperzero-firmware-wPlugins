#pragma once

#include "types.h"

/* Compute every feature from a parsed SubFile. Mutates `sub` to populate
 * inner_start / inner_len for each segment (i.e. strip_padding ranges). */
void features_extract(SubFile* sub, FeatureVector* fv);
