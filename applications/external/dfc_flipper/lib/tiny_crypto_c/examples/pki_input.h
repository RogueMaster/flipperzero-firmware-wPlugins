/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_PKI_INPUT_H_
#define EXAMPLE_PKI_INPUT_H_
#include <tiny_crypto/x509_store.h>
#include <tiny_crypto/source.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Read a nonempty input through EOF. Oversized or unreadable inputs clear the
 * entire buffer; out changes only on success. Buffer, path and out are disjoint.
 * read_stream leaves the stream open; read_file owns its opened stream.
 * Invalid pointer/capacity arguments preserve buffer and out. */
int example_read_stream(FILE* stream, uint8_t* buffer, size_t capacity, TC_bytes* out);
int example_read_file(const char* path, uint8_t* buffer, size_t capacity, TC_bytes* out);
/* Read a nonempty regular file and its creation time in Unix seconds.
 * Returns 1 on success; out borrows buffer. Both outputs stay unchanged on
 * failure. Missing timestamps fail the read. Read/close failures clear buffer;
 * invalid arguments and open failures leave it unchanged. */
int example_read_created_file(const char* path, uint8_t* buffer, size_t capacity,
    TC_bytes* out, uint64_t* created);

/* Borrow a seekable binary stream for bounded source reads. Keep the file open
 * and immutable until processing finishes; serialize access to its seek position.
 * The size must fit both max_bytes and the host's long file offsets. Setup
 * restores the initial position and changes out only on success. */
int example_stream_source(FILE* stream, uint64_t max_bytes, TC_source* out);

typedef struct {
  const TC_bytes* candidates;
  size_t candidate_count;
  const TC_X509_store_anchor* anchors;
  size_t anchor_count;
} ExampleX509Source;

/* Borrow arrays of issuer candidates and application-authorized anchors.
 * Keep arrays, records and source context stable while the returned view is
 * used. Candidate certificates carry no local trust. Each read charges one
 * work unit. Read outputs and work must be disjoint from all source storage.
 * Certificate parsing and path validation occur at use time. */
TC_X509_store_source example_x509_source(ExampleX509Source* source);

#ifdef __cplusplus
}
#endif
#endif
