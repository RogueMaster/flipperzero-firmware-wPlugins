/* Minimal host-side shim of <furi.h> so the pure-logic modules (models,
 * asset_manager, graph_engine) can be compiled and unit-tested with a normal
 * C compiler in CI. It only provides what those modules actually use. */
#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define furi_check(x)  assert(x)
#define furi_assert(x) assert(x)
#define UNUSED(x)      ((void)(x))
