#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_SOLVE_STEPS 5 // up to 6 starting numbers -> 5 operations

typedef struct {
    int a;
    int b;
    int result;
    char op; // '+', '-', '*', '/'
} SolveStep;

/**
 * Countdown-style solver:
 *  - values: input numbers
 *  - n: number of elements in values[]
 *  - target: 3-digit target
 *
 * Output:
 *  - *exact: true if exact match found
 *  - *best_result: closest value found
 *  - *best_diff: abs(best_result - target), or -1 if no attempt
 *  - steps_out: sequence of operations (up to MAX_SOLVE_STEPS)
 *  - *step_count_out: number of valid steps in steps_out
 */
void countdown_solve(
    const int values[],
    uint8_t n,
    int target,
    bool* exact,
    int* best_result,
    int* best_diff,
    SolveStep steps_out[],
    uint8_t* step_count_out);
