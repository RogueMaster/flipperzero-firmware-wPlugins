/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_X509_REVOCATION_H_
#define EXAMPLE_X509_REVOCATION_H_
#include "x509_workspace.h"
#include <tiny_crypto/x509_revocation.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { EXAMPLE_CRL_CAPACITY = 4, EXAMPLE_REVOCATION_NODES = 8 };
/* Caller-owned storage; keep this outside a small task stack. */
typedef struct {
  ExampleX509SearchWorkspace search;
  uint8_t states[EXAMPLE_CRL_CAPACITY];
  TC_X509_revocation_node nodes[EXAMPLE_REVOCATION_NODES];
} ExampleX509RevocationWorkspace;

/* The path is already validated and held separately from this scratch. */
TC_TLV_result example_check_path_revocation(const TC_bytes* chain, size_t count,
    const TC_X509_revocation_options* options, size_t* work,
    ExampleX509RevocationWorkspace* storage, TC_X509_revocation_result* result);
#ifdef __cplusplus
}
#endif
#endif
