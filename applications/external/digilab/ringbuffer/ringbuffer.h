/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <furi.h>

/**
 * @file ring_buffer.c
 * @brief Implementation of a ring buffer for statistical calculations on double data.
 */
#define MAX_VARIANCE_MV 100
typedef struct {
    double* buffer; // Dynamic array to hold the buffer data
    size_t bufferSize; // Total size of the buffer
    size_t head; // Index of the next element to update
    size_t count; // Number of elements currently stored
} RingBuffer;

double RingBuffer_get(const RingBuffer* ringBuffer, size_t idx);
double RingBuffer_getLast(RingBuffer* rb);
void RingBuffer_add(RingBuffer* rb, double value);
double RingBuffer_getAverage(RingBuffer* rb);
double RingBuffer_getVariance(RingBuffer* rb);
uint8_t RingBuffer_getVarianceMapped(RingBuffer* rb);
double RingBuffer_getStandardDeviation(RingBuffer* rb);
uint8_t RingBuffer_getStandardDeviationMapped(RingBuffer* rb, double minVal, double maxVal);
double RingBuffer_getMin(RingBuffer* rb);
double RingBuffer_getMax(RingBuffer* rb);
void RingBuffer_getMinMax(RingBuffer* rb, double* min, double* max);
void RingBuffer_reset(RingBuffer* rb);
void RingBuffer_changeSize(RingBuffer* rb, size_t newSize);
RingBuffer* RingBuffer_create(size_t initialSize);
void RingBuffer_free(RingBuffer* rb);

#endif /* end of include guard: _RINGBUFFER_H_ */
