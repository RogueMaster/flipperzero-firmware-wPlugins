#pragma once

#include <furi.h>
#include "types.h"

/* Build a human-readable text report. Mirrors analyze.py:format_report. */
void report_format(
    const char* path,
    const SubFile* sub,
    const FeatureVector* fv,
    const ClassificationResult* result,
    FuriString* out);
