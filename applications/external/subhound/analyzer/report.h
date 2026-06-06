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

/* Section selector for the on-device drill-down view. */
typedef enum {
    ReportSectionMetrics = 0,
    ReportSectionReasoning,
    ReportSectionPayload,
    ReportSectionManchester,
    ReportSectionWarnings,
    ReportSectionFull,
} ReportSection;

/* Build a single section's text. Writes nothing if the section is empty
 * (e.g. ReportSectionWarnings when result has no warnings). */
void report_format_section(
    ReportSection section,
    const char* path,
    const SubFile* sub,
    const FeatureVector* fv,
    const ClassificationResult* result,
    FuriString* out);

/* Returns true when the given section has content to show.
 * Used to skip empty entries in the Sections submenu. */
bool report_section_has_content(
    ReportSection section,
    const FeatureVector* fv,
    const ClassificationResult* result);

/* Compact 3-4 line summary blob used by the on-device summary card.
 * Lines emitted (one per furi_string_cat_str line):
 *   freq · TE · seg_count
 *   sim · sig_quality · code-type (ROLLING/FIXED)
 *   first sub-protocol hint (if any)
 *   sub-second hint OR warning indicator (optional)
 */
void report_format_summary_lines(
    const FeatureVector* fv,
    const ClassificationResult* result,
    FuriString* out);
