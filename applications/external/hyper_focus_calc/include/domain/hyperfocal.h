#pragma once

#include "include/domain/sensor_data.h"

#define HFC_FOCAL_MM_MIN 8.0f
#define HFC_FOCAL_MM_MAX 600.0f

#define HFC_FSTOP_COUNT 9u

/** Full-stop sequence (approximate 1.4, 2, 2.8, ...). */
float hyperfocal_fstop_at_index(size_t idx);

size_t hyperfocal_fstop_index_of(float fstop);

void hyperfocal_fstop_step(size_t *idx, int delta);

/**
 * Hyperfocal distance in mm: (f^2)/(N*c) + f
 * f, N, c in mm; returns distance in mm.
 */
float hyperfocal_distance_mm(float focal_mm, float f_number, float coc_mm);

/** Result in meters for display. */
float hyperfocal_distance_m(float focal_mm, float f_number, float coc_mm);
