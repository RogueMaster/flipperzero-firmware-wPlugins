#include "helpers.h"
#include <furi.h>
#include <math.h>

float inverse_tanh(double x) {
    return 0.5f * (float)log((1 + x) / (1 - x));
}

float lerp_number(float a, float b, float t) {
    if(t >= 1) return b;
    if(t <= 0) return a;
    return (1 - t) * a + t * b;
}

bool _test_ptr(void* p) {
    return p != NULL;
}

bool _check_ptr(void* p, const char* file, int line, const char* func) {
    UNUSED(file);
    UNUSED(line);
    UNUSED(func);
    if(p == NULL) {
        FURI_LOG_W("App", "[NULLPTR] %s:%s():%i", get_basename((char*)file), func, line);
    }

    return _test_ptr(p);
}

char* get_basename(const char* path) {
    const char* base = path;
    while(*path) {
        if(*path++ == '/') {
            base = path;
        }
    }
    return (char*)base;
}

size_t curr_time() {
    return DWT->CYCCNT;
}
