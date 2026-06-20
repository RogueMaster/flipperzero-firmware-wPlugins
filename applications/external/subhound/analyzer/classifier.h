#pragma once

#include "types.h"

void classifier_run(const FeatureVector* fv, ClassificationResult* out);

void classifier_add_reason(ClassificationResult* r, const char* fmt, ...);
void classifier_add_hint(ClassificationResult* r, const char* fmt, ...);
void classifier_add_warning(ClassificationResult* r, const char* fmt, ...);
