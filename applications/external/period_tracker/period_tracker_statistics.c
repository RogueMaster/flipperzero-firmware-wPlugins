#include "period_tracker_models.h"
#include "period_tracker_csv.h"
#include "period_tracker_string_utils.h"
#include <furi.h>
#include <math.h>
#include <string.h>
#include <storage/storage.h>

#define TAG "PeriodTrackerStats"

// Initialize cycle statistics structure
void cycle_statistics_init(CycleStatistics* stats) {
    memset(stats, 0, sizeof(CycleStatistics));
    stats->regularity = CYCLE_REGULAR;
    stats->has_sufficient_data = false;
}

// Calculate square root using integer approximation (Newton's method)
static uint32_t int_sqrt(uint32_t n) {
    if(n == 0) return 0;
    uint32_t x = n;
    uint32_t y = (x + 1) / 2;
    while(y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

// Calculate cycle statistics from historical data
bool calculate_cycle_statistics(
    Storage* storage,
    const char* girl_name,
    CycleStatistics* stats,
    uint8_t max_cycles) {
    cycle_statistics_init(stats);

    // Build path to events file
    char path[128];
    build_events_path(girl_name, path, sizeof(path));

    // Open file for streaming
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // No events file yet (new profile) — not an error
        FURI_LOG_I(TAG, "No events file yet for %s", girl_name);
        storage_file_free(file);
        return false;
    }

    // Extract period start dates by streaming through CSV line by line
    SimpleDate period_starts[32]; // Max 32 cycles (up to ~3 years of data) - 128 bytes on stack
    uint8_t period_count = 0;
    FileLineReader reader;
    char line[160];
    bool first_line = true;

    file_line_reader_init(&reader, file);
    while(period_count < 32 && file_line_reader_next(&reader, line, sizeof(line))) {
        // Skip header line
        if(first_line) {
            first_line = false;
            continue;
        }

        // Parse CSV: date,event_type,value
        char* fields[3];
        int field_count = string_split_csv(line, fields, 3);

        if(field_count >= 2) {
            // Only extract period_start events
            if(strcmp(fields[1], "period_start") == 0) {
                SimpleDate event_date;
                if(parse_date_string(fields[0], &event_date)) {
                    period_starts[period_count++] = event_date;
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    // Sort by date so CSV row order does not affect cycle pairing.
    // Simple insertion sort (n ≤ 32).
    for(uint8_t i = 1; i < period_count; i++) {
        SimpleDate key = period_starts[i];
        int8_t j = (int8_t)i - 1;
        while(j >= 0 && compare_dates(&period_starts[j], &key) > 0) {
            period_starts[j + 1] = period_starts[j];
            j--;
        }
        period_starts[j + 1] = key;
    }

    // Drop duplicate / same-day period starts (keep one)
    if(period_count > 1) {
        uint8_t w = 1;
        for(uint8_t r = 1; r < period_count; r++) {
            if(compare_dates(&period_starts[r], &period_starts[w - 1]) != 0) {
                period_starts[w++] = period_starts[r];
            }
        }
        period_count = w;
    }

    // Complete cycles = gaps between consecutive period starts (chronological).
    // Need at least 3 usable cycles ⇒ typically 4 period start dates.
    if(period_count < 4) {
        stats->has_sufficient_data = false;
        stats->num_cycles_analyzed = (period_count > 0) ? (uint8_t)(period_count - 1) : 0;
        FURI_LOG_W(
            TAG,
            "Insufficient data: %u period starts (%u gaps); need 4 starts / 3 cycles",
            period_count,
            stats->num_cycles_analyzed);
        return false;
    }

    stats->has_sufficient_data = true;

    // Calculate cycle lengths (most recent max_cycles after sort)
    uint8_t start_idx = period_count > max_cycles + 1 ? period_count - max_cycles - 1 : 0;
    uint8_t cycle_lengths[32];
    uint8_t num_cycles = 0;
    uint8_t skipped_short = 0;
    uint8_t skipped_long = 0;

    for(uint8_t i = start_idx; i < period_count - 1; i++) {
        int32_t cycle_len = days_between(&period_starts[i], &period_starts[i + 1]);
        // Sanity: real cycles are ~20–45 days in this app; drop junk pairs
        // (e.g. 2026-06-19 and 2026-06-22 as two "starts" for the same period).
        if(cycle_len >= 15 && cycle_len <= 60) {
            cycle_lengths[num_cycles++] = (uint8_t)cycle_len;
        } else if(cycle_len > 0 && cycle_len < 15) {
            skipped_short++;
            FURI_LOG_W(
                TAG,
                "Skipping implausibly short cycle (%ld days) between consecutive starts",
                (long)cycle_len);
        } else if(cycle_len > 60) {
            skipped_long++;
            FURI_LOG_W(
                TAG,
                "Skipping implausibly long cycle (%ld days) between consecutive starts",
                (long)cycle_len);
        } else {
            // cycle_len <= 0 should not happen after sort+dedupe
            FURI_LOG_W(TAG, "Skipping non-positive cycle length %ld", (long)cycle_len);
        }
    }

    if(skipped_short || skipped_long) {
        FURI_LOG_I(
            TAG,
            "Cycle filter: kept %u, skipped short=%u long=%u",
            num_cycles,
            skipped_short,
            skipped_long);
    }

    if(num_cycles < 3) {
        // Had enough starts but not enough plausible gaps
        stats->has_sufficient_data = false;
        stats->num_cycles_analyzed = num_cycles;
        return false;
    }

    stats->num_cycles_analyzed = num_cycles;

    // Calculate mean
    uint32_t sum = 0;
    stats->min_cycle_length = 255;
    stats->max_cycle_length = 0;

    for(uint8_t i = 0; i < num_cycles; i++) {
        sum += cycle_lengths[i];
        if(cycle_lengths[i] < stats->min_cycle_length) {
            stats->min_cycle_length = cycle_lengths[i];
        }
        if(cycle_lengths[i] > stats->max_cycle_length) {
            stats->max_cycle_length = cycle_lengths[i];
        }
    }

    stats->mean_cycle_length = (uint8_t)(sum / num_cycles);

    // Calculate standard deviation (using integer math)
    uint32_t variance_sum = 0;
    for(uint8_t i = 0; i < num_cycles; i++) {
        int16_t diff = cycle_lengths[i] - stats->mean_cycle_length;
        variance_sum += (uint32_t)(diff * diff);
    }

    uint32_t variance = variance_sum / num_cycles;
    uint32_t std_dev = int_sqrt(variance);
    stats->std_dev_cycle_length = (uint8_t)std_dev;

    // Calculate coefficient of variation (CV = std_dev / mean)
    // Store as fixed-point: CV * 1000
    if(stats->mean_cycle_length > 0) {
        stats->cycle_variability = (uint16_t)((std_dev * 1000) / stats->mean_cycle_length);
    } else {
        stats->cycle_variability = 0;
    }

    // Classify regularity based on CV
    if(stats->cycle_variability < 150) {
        // CV < 0.15: Regular (std dev < ~4 days for 28-day cycle)
        stats->regularity = CYCLE_REGULAR;
    } else if(stats->cycle_variability < 250) {
        // CV 0.15-0.25: Moderate irregularity
        stats->regularity = CYCLE_MODERATE;
    } else if(stats->cycle_variability < 400) {
        // CV 0.25-0.40: Irregular
        stats->regularity = CYCLE_IRREGULAR;
    } else {
        // CV > 0.40: Chaotic/very irregular
        stats->regularity = CYCLE_CHAOTIC;
    }

    FURI_LOG_I(
        TAG,
        "Cycle stats for %s: mean=%u, std=%u, CV=%u, regularity=%u",
        girl_name,
        stats->mean_cycle_length,
        stats->std_dev_cycle_length,
        stats->cycle_variability,
        stats->regularity);

    return true;
}

// Calculate adaptive ovulation day based on cycle statistics
uint8_t calculate_adaptive_ovulation_day(const CycleStatistics* stats) {
    if(!stats->has_sufficient_data) {
        // Default: ovulation typically 14 days before next period
        return 14;
    }

    // For regular cycles: ovulation is typically 14 days before next period
    // So ovulation day = mean_cycle_length - 14
    if(stats->mean_cycle_length > 14) {
        return stats->mean_cycle_length - 14;
    }

    // Very short cycles: use fixed day 10-12
    if(stats->mean_cycle_length <= 24) {
        return 10;
    }

    // Fallback
    return 14;
}

// Convert regularity enum to string
const char* cycle_regularity_to_string(CycleRegularity regularity) {
    switch(regularity) {
    case CYCLE_REGULAR:
        return "Regular";
    case CYCLE_MODERATE:
        return "Moderate";
    case CYCLE_IRREGULAR:
        return "Irregular";
    case CYCLE_CHAOTIC:
        return "Chaotic";
    default:
        return "Unknown";
    }
}
