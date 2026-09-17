/* Host-side stub of furi.h for off-device unit tests. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#define FURI_LOG_E(...) ((void)0)
#define FURI_LOG_W(...) ((void)0)
#define FURI_LOG_I(...) ((void)0)
#define FURI_LOG_D(...) ((void)0)
#define FURI_LOG_T(...) ((void)0)
#define UNUSED(x) ((void)(x))
#define furi_assert(x) do { if(!(x)) { fprintf(stderr,"furi_assert failed: %s (%s:%d)\n",#x,__FILE__,__LINE__); abort(); } } while(0)
#define furi_check(x) furi_assert(x)
#define furi_crash(msg) do { fprintf(stderr,"furi_crash: %s\n",(msg)); abort(); } while(0)
