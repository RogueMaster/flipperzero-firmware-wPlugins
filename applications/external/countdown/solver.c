#include "solver.h"
#include <string.h>
#include <limits.h>

typedef struct {
    bool found_exact;
    int best_result;
    int best_diff;
    SolveStep steps[MAX_SOLVE_STEPS];
    uint8_t step_count;
} SolverState;

static void solver_update_best(
    SolverState* best,
    int value,
    int target,
    SolveStep current_steps[],
    uint8_t step_count) {
    int diff = value - target;
    if(diff < 0) diff = -diff;

    if(diff < best->best_diff) {
        best->best_diff = diff;
        best->best_result = value;
        best->step_count = step_count;
        memcpy(best->steps, current_steps, sizeof(SolveStep) * step_count);
        if(diff == 0) {
            best->found_exact = true;
        }
    }
}

static void solver_recursive(
    int values[],
    uint8_t n,
    int target,
    SolveStep current_steps[],
    uint8_t step_count,
    SolverState* best) {
    if(best->found_exact) return;

    if(n == 1) {
        solver_update_best(best, values[0], target, current_steps, step_count);
        return;
    }

    for(uint8_t i = 0; i < n; i++) {
        for(uint8_t j = i + 1; j < n; j++) {
            int a = values[i];
            int b = values[j];

            int remaining[6]; // hasta 6 números en este problema
            uint8_t r_idx = 0;

            for(uint8_t k = 0; k < n; k++) {
                if(k != i && k != j) {
                    remaining[r_idx++] = values[k];
                }
            }

            // Generamos todas las operaciones posibles guardando
            // SIEMPRE los operandos exactamente en el orden en que se usan.
            int left[6];
            int right[6];
            int results[6];
            char ops[6];
            uint8_t res_count = 0;

            // a + b
            left[res_count] = a;
            right[res_count] = b;
            results[res_count] = a + b;
            ops[res_count++] = '+';

            // a - b (solo si > 0)
            if(a - b > 0) {
                left[res_count] = a;
                right[res_count] = b;
                results[res_count] = a - b;
                ops[res_count++] = '-';
            }

            // b - a (solo si > 0)
            if(b - a > 0) {
                left[res_count] = b;
                right[res_count] = a;
                results[res_count] = b - a;
                ops[res_count++] = '-';
            }

            // a * b (evitamos algo triviales x*1)
            if(a != 1 && b != 1) {
                left[res_count] = a;
                right[res_count] = b;
                results[res_count] = a * b;
                ops[res_count++] = '*';
            }

            // a / b
            if(b != 0 && a % b == 0) {
                left[res_count] = a;
                right[res_count] = b;
                results[res_count] = a / b;
                ops[res_count++] = '/';
            }

            // b / a
            if(a != 0 && b % a == 0) {
                left[res_count] = b;
                right[res_count] = a;
                results[res_count] = b / a;
                ops[res_count++] = '/';
            }

            for(uint8_t ri = 0; ri < res_count; ri++) {
                if(best->found_exact) return;

                int r = results[ri];
                if(r <= 0) continue;
                if(step_count >= MAX_SOLVE_STEPS) continue;

                remaining[r_idx] = r;

                current_steps[step_count].a = left[ri];
                current_steps[step_count].b = right[ri];
                current_steps[step_count].result = r;
                current_steps[step_count].op = ops[ri];

                solver_recursive(
                    remaining, r_idx + 1, target, current_steps, step_count + 1, best);
            }
        }
    }
}

void countdown_solve(
    const int values[],
    uint8_t n,
    int target,
    bool* exact,
    int* best_result,
    int* best_diff,
    SolveStep steps_out[],
    uint8_t* step_count_out) {
    SolverState best;
    best.found_exact = false;
    best.best_result = 0;
    best.best_diff = INT_MAX;
    best.step_count = 0;

    int work[6];
    for(uint8_t i = 0; i < n; i++) {
        work[i] = values[i];
    }

    SolveStep current[MAX_SOLVE_STEPS];
    memset(current, 0, sizeof(current));

    solver_recursive(work, n, target, current, 0, &best);

    *exact = best.found_exact;
    *best_result = best.best_result;
    *best_diff = (best.best_diff == INT_MAX) ? -1 : best.best_diff;
    *step_count_out = best.step_count;

    if(best.step_count > 0) {
        memcpy(steps_out, best.steps, sizeof(SolveStep) * best.step_count);
    }
}
