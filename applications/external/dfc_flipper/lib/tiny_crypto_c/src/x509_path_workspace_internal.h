/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TC_X509_PATH_WORKSPACE_INTERNAL_H_
#define TC_X509_PATH_WORKSPACE_INTERNAL_H_
#include <tiny_crypto/x509_path.h>

/* offsetof(tc_x509_path_storage_alignment, storage) is
 * TC_X509_path_workspace_alignment as an integer constant expression, for
 * compile-time checks in composite arenas. */
typedef struct {
  char byte;
  TC_X509_path_storage storage;
} tc_x509_path_storage_alignment;

/* Reserve count elements of width bytes at the first multiple of alignment at
 * or after *offset. Write the array offset to *start and advance *offset past
 * the array. Composite arenas use this for the arrays that follow the path
 * layout. Returns 0 when the size overflows size_t, with *offset unchanged. */
int tc_x509_path_arena_reserve(size_t* offset, size_t count, size_t width, size_t alignment,
                               size_t* start);

/* Reserve the twelve path workspace arrays from *offset onward, each starting
 * at a multiple of alignment, and advance *offset past the last one. A NULL
 * arena sizes the layout and leaves the array pointers NULL. Arrays with a
 * zero count stay NULL. out receives the pointers and capacities. Composite
 * arenas call this with their own alignment, which must be a multiple of
 * TC_X509_path_workspace_alignment. Returns 0 when the size overflows size_t,
 * with *offset and out unspecified. The caller checks the capacity counts. */
int tc_x509_path_workspace_layout(const TC_X509_path_capacity* capacity, uint8_t* arena,
                                  size_t alignment, size_t* offset, TC_X509_path_workspace* out);
#endif
