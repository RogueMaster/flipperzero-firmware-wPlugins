#pragma once

#include <furi.h>
//#define DEBUG_BUILD

#define M_PIX2    6.28318530717958647692 /* 2 pi */
#define l_abs(x)  ((x) < 0 ? -(x) : (x))
#define DEG_2_RAD 0.01745329251994329576f
#define RAD_2_DEG 565.48667764616278292327f

#ifdef DEBUG_BUILD
#define check_pointer(X) _check_ptr(X, __FILE__, __LINE__, __FUNCTION__)
#else
#define check_pointer(X) _test_ptr(X)
#define trace(X)         while(0)
#endif

char* get_basename(const char* path);

#ifndef basename
#define basename(path) get_basename(path)
#endif

bool _test_ptr(void* p);

bool _check_ptr(void* p, const char* file, int line, const char* func);

float inverse_tanh(double x);

float lerp_number(float a, float b, float t);

size_t curr_time();
