/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXAMPLE_PIV_SM_WIRE_H_
#define EXAMPLE_PIV_SM_WIRE_H_
#include <tiny_crypto/piv_sm.h>
#include <tiny_crypto/piv_cvc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_PIV_SM_peer peer;
  TC_PIV_CVC cvc;
} ExamplePIVSMResponse;

typedef struct {
  TC_bytes data;
  uint8_t ins, p1, p2, has_le;
} ExamplePIVSMCommand;

typedef struct { size_t length; uint16_t status; } ExamplePIVSMResult;

/* These helpers own the PIV APDU and data-object formats. The cryptographic
 * session only receives decoded fields and ordered authenticated byte spans. */
TC_status example_piv_sm_begin(TC_PIV_SM* session, TC_PIV_SM_suite suite,
    const uint8_t host_identifier[8], TC_random_fn random, void* random_context,
    uint8_t* apdu, size_t capacity, size_t* written, TC_PIV_SM_workspace* workspace);
TC_status example_piv_sm_response_read(TC_PIV_SM_suite suite, TC_bytes encoded,
    ExamplePIVSMResponse* response);
TC_status example_piv_sm_finish(TC_PIV_SM* session, TC_bytes encoded,
    uint16_t transport_status, TC_bytes authenticated_key,
    TC_PIV_SM_workspace* workspace);
TC_status example_piv_sm_protect(TC_PIV_SM* session,
    const ExamplePIVSMCommand* command, uint8_t* output, size_t capacity,
    size_t* written, TC_PIV_SM_workspace* workspace);
TC_status example_piv_sm_unprotect(TC_PIV_SM* session, TC_bytes encoded,
    uint16_t transport_status, uint8_t* output, size_t capacity,
    ExamplePIVSMResult* result, TC_PIV_SM_workspace* workspace);

#ifdef __cplusplus
}
#endif
#endif
