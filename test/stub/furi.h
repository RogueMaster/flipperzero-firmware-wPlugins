/* Host stubs: just enough furi to compile the real helpers unmodified. */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define furi_assert(x) assert(x)
#define furi_check(x) assert(x)
#define UNUSED(x) ((void)(x))
